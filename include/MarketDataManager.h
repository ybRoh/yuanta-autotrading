#ifndef MARKET_DATA_MANAGER_H
#define MARKET_DATA_MANAGER_H

#include "YuantaAPI.h"
#include "TechnicalIndicators.h"
#include <string>
#include <vector>
#include <map>
#include <deque>
#include <mutex>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>

namespace yuanta {

// 시장 데이터 관리자
class MarketDataManager {
public:
    MarketDataManager();
    ~MarketDataManager();

    // API 설정
    void setAPI(YuantaAPI* api);

    // 종목 관리
    void addWatchlist(const std::string& code);
    void removeWatchlist(const std::string& code);
    std::vector<std::string> getWatchlist() const;

    // 실시간 시세 구독 시작/중지
    bool startRealtime();
    void stopRealtime();
    bool isRealtimeRunning() const;

    // 데이터 조회
    QuoteData getQuote(const std::string& code) const;
    std::vector<OHLCV> getMinuteCandles(const std::string& code,
                                         int minutes = 1,
                                         int count = 100) const;
    std::vector<OHLCV> getDailyCandles(const std::string& code,
                                        int count = 60) const;
    OrderbookData getOrderbook(const std::string& code) const;

    // 실시간 분봉 생성
    void processQuote(const QuoteData& quote);

    // 일중 분봉 데이터 (장 중 누적)
    std::vector<OHLCV> getIntradayCandles(const std::string& code,
                                           int minutes = 1) const;

    // 시세 변경 콜백
    using QuoteUpdateCallback = std::function<void(const std::string& code,
                                                     const QuoteData& quote)>;
    void setQuoteUpdateCallback(QuoteUpdateCallback callback);

    // 분봉 완성 콜백
    using CandleCompleteCallback = std::function<void(const std::string& code,
                                                       int minutes,
                                                       const OHLCV& candle)>;
    void setCandleCompleteCallback(CandleCompleteCallback callback);

    // 과거 데이터 로드
    bool loadHistoricalData(const std::string& code, int days = 20);

    // 데이터 캐시 관리
    void clearCache(const std::string& code = "");
    size_t getCacheSize() const;

    // 장 시간 관련
    bool isMarketOpen() const;
    int getMinutesSinceOpen() const;

private:
    YuantaAPI* api = nullptr;

    // 종목별 데이터 저장소
    struct StockData {
        QuoteData quote;
        OrderbookData orderbook;
        std::deque<OHLCV> minute1Candles;
        std::deque<OHLCV> minute5Candles;
        std::deque<OHLCV> dailyCandles;
        OHLCV currentCandle1;   // 현재 형성 중인 1분봉
        OHLCV currentCandle5;   // 현재 형성 중인 5분봉
        long long lastCandleTime1 = 0;
        long long lastCandleTime5 = 0;
    };

    std::map<std::string, StockData> stockData;
    std::vector<std::string> watchlist;
    mutable std::mutex dataMutex;

    // 콜백
    QuoteUpdateCallback quoteCallback;
    CandleCompleteCallback candleCallback;

    // 실시간 처리 스레드
    std::atomic<bool> realtimeRunning{false};
    std::thread realtimeThread;

    // 헬퍼 함수
    void updateCurrentCandle(StockData& data, const QuoteData& quote,
                             int minutes, OHLCV& currentCandle,
                             long long& lastTime);
    void completeCandle(const std::string& code, int minutes,
                        const OHLCV& candle, std::deque<OHLCV>& candles);
    long long getMinuteSlot(long long timestamp, int minutes) const;
};

// 과거 데이터 로더 (백테스팅용)
class HistoricalDataLoader {
public:
    HistoricalDataLoader();

    // CSV 파일에서 로드
    bool loadFromCSV(const std::string& filepath,
                     std::vector<OHLCV>& candles);

    // API에서 다운로드 및 저장
    bool downloadAndSave(YuantaAPI* api,
                         const std::string& code,
                         const std::string& outputPath,
                         int days = 180);

    // 날짜 범위로 필터
    std::vector<OHLCV> filterByDate(const std::vector<OHLCV>& candles,
                                     long long startTime,
                                     long long endTime);

private:
    std::string dataDirectory;
};

} // namespace yuanta

#endif // MARKET_DATA_MANAGER_H
