#include "../../include/MarketDataManager.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <ctime>
#include <chrono>
#include <iomanip>
#include <algorithm>

namespace yuanta {

// ============================================================================
// MarketDataManager
// ============================================================================

MarketDataManager::MarketDataManager() {}

MarketDataManager::~MarketDataManager() {
    stopRealtime();
}

void MarketDataManager::setAPI(YuantaAPI* api) {
    this->api = api;
}

void MarketDataManager::addWatchlist(const std::string& code) {
    std::lock_guard<std::mutex> lock(dataMutex);

    auto it = std::find(watchlist.begin(), watchlist.end(), code);
    if (it == watchlist.end()) {
        watchlist.push_back(code);
        stockData[code] = StockData();
    }
}

void MarketDataManager::removeWatchlist(const std::string& code) {
    std::lock_guard<std::mutex> lock(dataMutex);

    auto it = std::find(watchlist.begin(), watchlist.end(), code);
    if (it != watchlist.end()) {
        watchlist.erase(it);
        stockData.erase(code);
    }
}

std::vector<std::string> MarketDataManager::getWatchlist() const {
    std::lock_guard<std::mutex> lock(dataMutex);
    return watchlist;
}

bool MarketDataManager::startRealtime() {
    if (realtimeRunning) return true;
    if (!api) return false;

    std::lock_guard<std::mutex> lock(dataMutex);

    // 각 종목 실시간 구독
    for (const auto& code : watchlist) {
        api->subscribeQuote(code);
        api->subscribeOrderbook(code);
    }

    // 콜백 설정
    api->setQuoteCallback([this](const QuoteData& quote) {
        processQuote(quote);
    });

    realtimeRunning = true;
    std::cout << "Realtime data started for " << watchlist.size() << " stocks" << std::endl;

    return true;
}

void MarketDataManager::stopRealtime() {
    if (!realtimeRunning) return;

    realtimeRunning = false;

    if (api) {
        std::lock_guard<std::mutex> lock(dataMutex);
        for (const auto& code : watchlist) {
            api->unsubscribeQuote(code);
        }
    }

    std::cout << "Realtime data stopped" << std::endl;
}

bool MarketDataManager::isRealtimeRunning() const {
    return realtimeRunning;
}

QuoteData MarketDataManager::getQuote(const std::string& code) const {
    std::lock_guard<std::mutex> lock(dataMutex);

    auto it = stockData.find(code);
    if (it != stockData.end()) {
        return it->second.quote;
    }

    QuoteData empty;
    empty.code = code;
    return empty;
}

std::vector<OHLCV> MarketDataManager::getMinuteCandles(const std::string& code,
                                                        int minutes, int count) const {
    std::lock_guard<std::mutex> lock(dataMutex);

    auto it = stockData.find(code);
    if (it == stockData.end()) {
        return {};
    }

    const std::deque<OHLCV>* candleSource = nullptr;
    if (minutes == 1) {
        candleSource = &it->second.minute1Candles;
    } else if (minutes == 5) {
        candleSource = &it->second.minute5Candles;
    } else {
        return {};
    }

    size_t actualCount = (std::min)(static_cast<size_t>(count), candleSource->size());
    std::vector<OHLCV> result;

    for (size_t i = candleSource->size() - actualCount; i < candleSource->size(); ++i) {
        result.push_back((*candleSource)[i]);
    }

    return result;
}

std::vector<OHLCV> MarketDataManager::getDailyCandles(const std::string& code,
                                                       int count) const {
    std::lock_guard<std::mutex> lock(dataMutex);

    auto it = stockData.find(code);
    if (it == stockData.end()) {
        return {};
    }

    size_t actualCount = (std::min)(static_cast<size_t>(count),
                                  it->second.dailyCandles.size());
    std::vector<OHLCV> result;

    for (size_t i = it->second.dailyCandles.size() - actualCount;
         i < it->second.dailyCandles.size(); ++i) {
        result.push_back(it->second.dailyCandles[i]);
    }

    return result;
}

OrderbookData MarketDataManager::getOrderbook(const std::string& code) const {
    std::lock_guard<std::mutex> lock(dataMutex);

    auto it = stockData.find(code);
    if (it != stockData.end()) {
        return it->second.orderbook;
    }

    OrderbookData empty;
    empty.code = code;
    return empty;
}

void MarketDataManager::processQuote(const QuoteData& quote) {
    std::lock_guard<std::mutex> lock(dataMutex);

    auto it = stockData.find(quote.code);
    if (it == stockData.end()) return;

    StockData& data = it->second;
    data.quote = quote;

    // 분봉 업데이트
    updateCurrentCandle(data, quote, 1, data.currentCandle1, data.lastCandleTime1);
    updateCurrentCandle(data, quote, 5, data.currentCandle5, data.lastCandleTime5);

    // 콜백 호출
    if (quoteCallback) {
        quoteCallback(quote.code, quote);
    }
}

void MarketDataManager::updateCurrentCandle(StockData& data, const QuoteData& quote,
                                             int minutes, OHLCV& currentCandle,
                                             long long& lastTime) {
    long long currentSlot = getMinuteSlot(quote.timestamp, minutes);

    if (currentSlot != lastTime) {
        // 이전 봉 완성
        if (lastTime > 0 && currentCandle.volume > 0) {
            std::deque<OHLCV>* candles = (minutes == 1) ?
                &data.minute1Candles : &data.minute5Candles;
            completeCandle(quote.code, minutes, currentCandle, *candles);
        }

        // 새 봉 시작
        currentCandle.code = quote.code;
        currentCandle.timestamp = currentSlot;
        currentCandle.open = quote.currentPrice;
        currentCandle.high = quote.currentPrice;
        currentCandle.low = quote.currentPrice;
        currentCandle.close = quote.currentPrice;
        currentCandle.volume = 0;

        lastTime = currentSlot;
    } else {
        // 현재 봉 업데이트
        currentCandle.high = (std::max)(currentCandle.high, quote.currentPrice);
        currentCandle.low = (std::min)(currentCandle.low, quote.currentPrice);
        currentCandle.close = quote.currentPrice;
        currentCandle.volume = quote.volume;
    }
}

void MarketDataManager::completeCandle(const std::string& code, int minutes,
                                        const OHLCV& candle, std::deque<OHLCV>& candles) {
    candles.push_back(candle);

    // 최대 500개 유지
    while (candles.size() > 500) {
        candles.pop_front();
    }

    // 콜백 호출
    if (candleCallback) {
        candleCallback(code, minutes, candle);
    }
}

long long MarketDataManager::getMinuteSlot(long long timestamp, int minutes) const {
    long long minuteMs = minutes * 60 * 1000LL;
    return (timestamp / minuteMs) * minuteMs;
}

std::vector<OHLCV> MarketDataManager::getIntradayCandles(const std::string& code,
                                                          int minutes) const {
    return getMinuteCandles(code, minutes, 500);
}

void MarketDataManager::setQuoteUpdateCallback(QuoteUpdateCallback callback) {
    quoteCallback = callback;
}

void MarketDataManager::setCandleCompleteCallback(CandleCompleteCallback callback) {
    candleCallback = callback;
}

bool MarketDataManager::loadHistoricalData(const std::string& code, int days) {
    if (!api) return false;

    std::lock_guard<std::mutex> lock(dataMutex);

    auto it = stockData.find(code);
    if (it == stockData.end()) {
        stockData[code] = StockData();
        it = stockData.find(code);
    }

    // 일봉 로드
    auto dailyCandles = api->getDailyCandles(code, days);
    for (const auto& c : dailyCandles) {
        OHLCV ohlcv;
        ohlcv.code = code;
        ohlcv.timestamp = c.timestamp;
        ohlcv.open = c.open;
        ohlcv.high = c.high;
        ohlcv.low = c.low;
        ohlcv.close = c.close;
        ohlcv.volume = c.volume;
        it->second.dailyCandles.push_back(ohlcv);
    }

    // 분봉 로드 (최근 20일치)
    int minuteCount = days * 390;  // 하루 390분 (9:00~15:30)
    auto minuteCandles = api->getMinuteCandles(code, 1, minuteCount);
    for (const auto& c : minuteCandles) {
        OHLCV ohlcv;
        ohlcv.code = code;
        ohlcv.timestamp = c.timestamp;
        ohlcv.open = c.open;
        ohlcv.high = c.high;
        ohlcv.low = c.low;
        ohlcv.close = c.close;
        ohlcv.volume = c.volume;
        it->second.minute1Candles.push_back(ohlcv);
    }

    std::cout << "Loaded " << dailyCandles.size() << " daily candles and "
              << minuteCandles.size() << " minute candles for " << code << std::endl;

    return true;
}

void MarketDataManager::clearCache(const std::string& code) {
    std::lock_guard<std::mutex> lock(dataMutex);

    if (code.empty()) {
        stockData.clear();
    } else {
        stockData.erase(code);
    }
}

size_t MarketDataManager::getCacheSize() const {
    std::lock_guard<std::mutex> lock(dataMutex);

    size_t totalSize = 0;
    for (const auto& entry : stockData) {
        totalSize += entry.second.minute1Candles.size() * sizeof(OHLCV);
        totalSize += entry.second.minute5Candles.size() * sizeof(OHLCV);
        totalSize += entry.second.dailyCandles.size() * sizeof(OHLCV);
    }

    return totalSize;
}

bool MarketDataManager::isMarketOpen() const {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm* tm = std::localtime(&t);

    // 주말 체크
    if (tm->tm_wday == 0 || tm->tm_wday == 6) return false;

    int minutes = tm->tm_hour * 60 + tm->tm_min;
    return minutes >= 540 && minutes <= 930;  // 09:00 ~ 15:30
}

int MarketDataManager::getMinutesSinceOpen() const {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm* tm = std::localtime(&t);

    int currentMinutes = tm->tm_hour * 60 + tm->tm_min;
    int marketOpen = 540;  // 09:00

    if (currentMinutes < marketOpen) return 0;
    return currentMinutes - marketOpen;
}

// ============================================================================
// HistoricalDataLoader
// ============================================================================

HistoricalDataLoader::HistoricalDataLoader()
    : dataDirectory("./data/") {
}

bool HistoricalDataLoader::loadFromCSV(const std::string& filepath,
                                        std::vector<OHLCV>& candles) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filepath << std::endl;
        return false;
    }

    std::string line;

    // 헤더 스킵
    std::getline(file, line);

    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string token;
        OHLCV candle;

        // CSV 형식: timestamp,open,high,low,close,volume
        int col = 0;
        while (std::getline(ss, token, ',')) {
            switch (col) {
                case 0: candle.timestamp = std::stoll(token); break;
                case 1: candle.open = std::stod(token); break;
                case 2: candle.high = std::stod(token); break;
                case 3: candle.low = std::stod(token); break;
                case 4: candle.close = std::stod(token); break;
                case 5: candle.volume = std::stoll(token); break;
            }
            col++;
        }

        if (col >= 6) {
            candles.push_back(candle);
        }
    }

    std::cout << "Loaded " << candles.size() << " candles from " << filepath << std::endl;
    return true;
}

bool HistoricalDataLoader::downloadAndSave(YuantaAPI* api,
                                            const std::string& code,
                                            const std::string& outputPath,
                                            int days) {
    if (!api) return false;

    auto candles = api->getDailyCandles(code, days);
    if (candles.empty()) {
        std::cerr << "No data retrieved for " << code << std::endl;
        return false;
    }

    std::ofstream file(outputPath);
    if (!file.is_open()) {
        std::cerr << "Failed to create file: " << outputPath << std::endl;
        return false;
    }

    // 헤더
    file << "timestamp,open,high,low,close,volume\n";

    // 데이터
    for (const auto& c : candles) {
        file << c.timestamp << ","
             << std::fixed << std::setprecision(2)
             << c.open << "," << c.high << "," << c.low << "," << c.close << ","
             << c.volume << "\n";
    }

    std::cout << "Saved " << candles.size() << " candles to " << outputPath << std::endl;
    return true;
}

std::vector<OHLCV> HistoricalDataLoader::filterByDate(const std::vector<OHLCV>& candles,
                                                       long long startTime,
                                                       long long endTime) {
    std::vector<OHLCV> filtered;

    for (const auto& candle : candles) {
        if (candle.timestamp >= startTime && candle.timestamp <= endTime) {
            filtered.push_back(candle);
        }
    }

    return filtered;
}

} // namespace yuanta
