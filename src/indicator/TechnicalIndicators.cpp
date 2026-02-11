#include "../../include/TechnicalIndicators.h"
#include <cmath>
#include <stdexcept>
#include <algorithm>

namespace yuanta {

// ============================================================================
// Static Helper Functions
// ============================================================================

static double mean(const std::vector<double>& data, size_t start, size_t count) {
    if (count == 0) return 0.0;
    double sum = 0.0;
    for (size_t i = start; i < start + count && i < data.size(); ++i) {
        sum += data[i];
    }
    return sum / count;
}

// ============================================================================
// SMA (Simple Moving Average)
// ============================================================================

double TechnicalIndicators::SMA(const std::vector<double>& prices, int period) {
    if (prices.size() < static_cast<size_t>(period)) {
        throw std::invalid_argument("Not enough data for SMA calculation");
    }

    double sum = 0.0;
    for (int i = prices.size() - period; i < static_cast<int>(prices.size()); ++i) {
        sum += prices[i];
    }
    return sum / period;
}

std::vector<double> TechnicalIndicators::SMAVector(const std::vector<double>& prices, int period) {
    std::vector<double> result;
    if (prices.size() < static_cast<size_t>(period)) return result;

    double sum = 0.0;
    for (int i = 0; i < period; ++i) {
        sum += prices[i];
    }
    result.push_back(sum / period);

    for (size_t i = period; i < prices.size(); ++i) {
        sum = sum - prices[i - period] + prices[i];
        result.push_back(sum / period);
    }

    return result;
}

// ============================================================================
// EMA (Exponential Moving Average)
// ============================================================================

double TechnicalIndicators::EMA(const std::vector<double>& prices, int period) {
    if (prices.size() < static_cast<size_t>(period)) {
        throw std::invalid_argument("Not enough data for EMA calculation");
    }

    double multiplier = 2.0 / (period + 1);
    double ema = SMA(std::vector<double>(prices.begin(), prices.begin() + period), period);

    for (size_t i = period; i < prices.size(); ++i) {
        ema = (prices[i] - ema) * multiplier + ema;
    }

    return ema;
}

std::vector<double> TechnicalIndicators::EMAVector(const std::vector<double>& prices, int period) {
    std::vector<double> result;
    if (prices.size() < static_cast<size_t>(period)) return result;

    double multiplier = 2.0 / (period + 1);

    // 첫 EMA는 SMA로 시작
    double sum = 0.0;
    for (int i = 0; i < period; ++i) {
        sum += prices[i];
    }
    double ema = sum / period;
    result.push_back(ema);

    for (size_t i = period; i < prices.size(); ++i) {
        ema = (prices[i] - ema) * multiplier + ema;
        result.push_back(ema);
    }

    return result;
}

// ============================================================================
// WMA (Weighted Moving Average)
// ============================================================================

double TechnicalIndicators::WMA(const std::vector<double>& prices, int period) {
    if (prices.size() < static_cast<size_t>(period)) {
        throw std::invalid_argument("Not enough data for WMA calculation");
    }

    double weightedSum = 0.0;
    double weightSum = 0.0;

    for (int i = 0; i < period; ++i) {
        int weight = i + 1;
        weightedSum += prices[prices.size() - period + i] * weight;
        weightSum += weight;
    }

    return weightedSum / weightSum;
}

// ============================================================================
// VWAP (Volume Weighted Average Price)
// ============================================================================

double TechnicalIndicators::VWAP(const std::vector<OHLCV>& candles) {
    if (candles.empty()) return 0.0;

    double cumulativeTPV = 0.0;  // TP * Volume
    double cumulativeVolume = 0.0;

    for (const auto& candle : candles) {
        double typicalPrice = (candle.high + candle.low + candle.close) / 3.0;
        cumulativeTPV += typicalPrice * candle.volume;
        cumulativeVolume += candle.volume;
    }

    if (cumulativeVolume == 0) return 0.0;
    return cumulativeTPV / cumulativeVolume;
}

std::vector<double> TechnicalIndicators::VWAPVector(const std::vector<OHLCV>& candles) {
    std::vector<double> result;
    if (candles.empty()) return result;

    double cumulativeTPV = 0.0;
    double cumulativeVolume = 0.0;

    for (const auto& candle : candles) {
        double typicalPrice = (candle.high + candle.low + candle.close) / 3.0;
        cumulativeTPV += typicalPrice * candle.volume;
        cumulativeVolume += candle.volume;

        if (cumulativeVolume > 0) {
            result.push_back(cumulativeTPV / cumulativeVolume);
        } else {
            result.push_back(0.0);
        }
    }

    return result;
}

// ============================================================================
// RSI (Relative Strength Index)
// ============================================================================

double TechnicalIndicators::RSI(const std::vector<double>& prices, int period) {
    if (prices.size() < static_cast<size_t>(period) + 1) {
        throw std::invalid_argument("Not enough data for RSI calculation");
    }

    std::vector<double> gains, losses;

    for (size_t i = 1; i < prices.size(); ++i) {
        double change = prices[i] - prices[i - 1];
        if (change > 0) {
            gains.push_back(change);
            losses.push_back(0.0);
        } else {
            gains.push_back(0.0);
            losses.push_back(-change);
        }
    }

    // 첫 평균
    double avgGain = 0.0, avgLoss = 0.0;
    for (int i = 0; i < period; ++i) {
        avgGain += gains[i];
        avgLoss += losses[i];
    }
    avgGain /= period;
    avgLoss /= period;

    // 스무딩
    for (size_t i = period; i < gains.size(); ++i) {
        avgGain = (avgGain * (period - 1) + gains[i]) / period;
        avgLoss = (avgLoss * (period - 1) + losses[i]) / period;
    }

    if (avgLoss == 0) return 100.0;
    double rs = avgGain / avgLoss;
    return 100.0 - (100.0 / (1.0 + rs));
}

std::vector<double> TechnicalIndicators::RSIVector(const std::vector<double>& prices, int period) {
    std::vector<double> result;
    if (prices.size() < static_cast<size_t>(period) + 1) return result;

    std::vector<double> gains, losses;

    for (size_t i = 1; i < prices.size(); ++i) {
        double change = prices[i] - prices[i - 1];
        if (change > 0) {
            gains.push_back(change);
            losses.push_back(0.0);
        } else {
            gains.push_back(0.0);
            losses.push_back(-change);
        }
    }

    double avgGain = 0.0, avgLoss = 0.0;
    for (int i = 0; i < period; ++i) {
        avgGain += gains[i];
        avgLoss += losses[i];
    }
    avgGain /= period;
    avgLoss /= period;

    if (avgLoss == 0) {
        result.push_back(100.0);
    } else {
        double rs = avgGain / avgLoss;
        result.push_back(100.0 - (100.0 / (1.0 + rs)));
    }

    for (size_t i = period; i < gains.size(); ++i) {
        avgGain = (avgGain * (period - 1) + gains[i]) / period;
        avgLoss = (avgLoss * (period - 1) + losses[i]) / period;

        if (avgLoss == 0) {
            result.push_back(100.0);
        } else {
            double rs = avgGain / avgLoss;
            result.push_back(100.0 - (100.0 / (1.0 + rs)));
        }
    }

    return result;
}

// ============================================================================
// MACD
// ============================================================================

MACDResult TechnicalIndicators::MACD(const std::vector<double>& prices,
                                      int fastPeriod, int slowPeriod, int signalPeriod) {
    MACDResult result = {0, 0, 0, false, false};

    if (prices.size() < static_cast<size_t>(slowPeriod)) {
        return result;
    }

    auto fastEMA = EMAVector(prices, fastPeriod);
    auto slowEMA = EMAVector(prices, slowPeriod);

    // MACD 라인 계산
    std::vector<double> macdLine;
    size_t offset = slowPeriod - fastPeriod;
    for (size_t i = 0; i < slowEMA.size(); ++i) {
        macdLine.push_back(fastEMA[i + offset] - slowEMA[i]);
    }

    if (macdLine.size() < static_cast<size_t>(signalPeriod)) {
        return result;
    }

    // 시그널 라인
    auto signalLine = EMAVector(macdLine, signalPeriod);

    if (signalLine.empty()) return result;

    result.macd = macdLine.back();
    result.signal = signalLine.back();
    result.histogram = result.macd - result.signal;

    // 크로스 체크
    if (macdLine.size() >= 2 && signalLine.size() >= 2) {
        double prevMACD = macdLine[macdLine.size() - 2];
        double prevSignal = signalLine[signalLine.size() - 2];

        result.bullishCross = (prevMACD < prevSignal) && (result.macd > result.signal);
        result.bearishCross = (prevMACD > prevSignal) && (result.macd < result.signal);
    }

    return result;
}

std::vector<MACDResult> TechnicalIndicators::MACDVector(const std::vector<double>& prices,
                                                         int fastPeriod, int slowPeriod,
                                                         int signalPeriod) {
    std::vector<MACDResult> results;

    if (prices.size() < static_cast<size_t>(slowPeriod)) {
        return results;
    }

    auto fastEMA = EMAVector(prices, fastPeriod);
    auto slowEMA = EMAVector(prices, slowPeriod);

    std::vector<double> macdLine;
    size_t offset = slowPeriod - fastPeriod;
    for (size_t i = 0; i < slowEMA.size(); ++i) {
        macdLine.push_back(fastEMA[i + offset] - slowEMA[i]);
    }

    if (macdLine.size() < static_cast<size_t>(signalPeriod)) {
        return results;
    }

    auto signalLine = EMAVector(macdLine, signalPeriod);

    for (size_t i = 0; i < signalLine.size(); ++i) {
        MACDResult r;
        size_t macdIdx = i + signalPeriod - 1;
        r.macd = macdLine[macdIdx];
        r.signal = signalLine[i];
        r.histogram = r.macd - r.signal;

        if (i > 0) {
            double prevMACD = macdLine[macdIdx - 1];
            double prevSignal = signalLine[i - 1];
            r.bullishCross = (prevMACD < prevSignal) && (r.macd > r.signal);
            r.bearishCross = (prevMACD > prevSignal) && (r.macd < r.signal);
        }

        results.push_back(r);
    }

    return results;
}

// ============================================================================
// Bollinger Bands
// ============================================================================

double TechnicalIndicators::StdDev(const std::vector<double>& prices, int period) {
    if (prices.size() < static_cast<size_t>(period)) {
        throw std::invalid_argument("Not enough data for StdDev calculation");
    }

    double avg = SMA(prices, period);
    double sumSq = 0.0;

    for (size_t i = prices.size() - period; i < prices.size(); ++i) {
        double diff = prices[i] - avg;
        sumSq += diff * diff;
    }

    return std::sqrt(sumSq / period);
}

BollingerBands TechnicalIndicators::BollingerBand(const std::vector<double>& prices,
                                                   int period, double stdDev) {
    BollingerBands bb = {0, 0, 0, 0, 0};

    if (prices.size() < static_cast<size_t>(period)) {
        return bb;
    }

    bb.middle = SMA(prices, period);
    double sd = StdDev(prices, period);

    bb.upper = bb.middle + (sd * stdDev);
    bb.lower = bb.middle - (sd * stdDev);
    bb.bandwidth = (bb.upper - bb.lower) / bb.middle;

    double currentPrice = prices.back();
    if (bb.upper != bb.lower) {
        bb.percentB = (currentPrice - bb.lower) / (bb.upper - bb.lower);
    }

    return bb;
}

std::vector<BollingerBands> TechnicalIndicators::BollingerBandVector(
    const std::vector<double>& prices, int period, double stdDev) {

    std::vector<BollingerBands> results;
    if (prices.size() < static_cast<size_t>(period)) return results;

    for (size_t i = period; i <= prices.size(); ++i) {
        std::vector<double> window(prices.begin() + i - period, prices.begin() + i);
        BollingerBands bb = BollingerBand(window, period, stdDev);
        results.push_back(bb);
    }

    return results;
}

// ============================================================================
// ATR (Average True Range)
// ============================================================================

double TechnicalIndicators::TrueRange(const OHLCV& current, const OHLCV& previous) {
    double hl = current.high - current.low;
    double hpc = std::abs(current.high - previous.close);
    double lpc = std::abs(current.low - previous.close);
    return (std::max)({hl, hpc, lpc});
}

double TechnicalIndicators::ATR(const std::vector<OHLCV>& candles, int period) {
    if (candles.size() < static_cast<size_t>(period) + 1) {
        throw std::invalid_argument("Not enough data for ATR calculation");
    }

    std::vector<double> trueRanges;
    for (size_t i = 1; i < candles.size(); ++i) {
        trueRanges.push_back(TrueRange(candles[i], candles[i - 1]));
    }

    // 첫 ATR은 단순 평균
    double atr = 0.0;
    for (int i = 0; i < period; ++i) {
        atr += trueRanges[i];
    }
    atr /= period;

    // 스무딩
    for (size_t i = period; i < trueRanges.size(); ++i) {
        atr = (atr * (period - 1) + trueRanges[i]) / period;
    }

    return atr;
}

std::vector<double> TechnicalIndicators::ATRVector(const std::vector<OHLCV>& candles, int period) {
    std::vector<double> result;
    if (candles.size() < static_cast<size_t>(period) + 1) return result;

    std::vector<double> trueRanges;
    for (size_t i = 1; i < candles.size(); ++i) {
        trueRanges.push_back(TrueRange(candles[i], candles[i - 1]));
    }

    double atr = 0.0;
    for (int i = 0; i < period; ++i) {
        atr += trueRanges[i];
    }
    atr /= period;
    result.push_back(atr);

    for (size_t i = period; i < trueRanges.size(); ++i) {
        atr = (atr * (period - 1) + trueRanges[i]) / period;
        result.push_back(atr);
    }

    return result;
}

// ============================================================================
// 유틸리티 함수들
// ============================================================================

bool TechnicalIndicators::isMAAligned(const std::vector<double>& maValues) {
    if (maValues.size() < 2) return false;

    for (size_t i = 0; i < maValues.size() - 1; ++i) {
        if (maValues[i] <= maValues[i + 1]) {
            return false;  // 정배열이 아님
        }
    }
    return true;
}

bool TechnicalIndicators::isBollingerSqueeze(const std::vector<BollingerBands>& bands,
                                              int lookback, double percentile) {
    if (bands.size() < static_cast<size_t>(lookback)) return false;

    std::vector<double> bandwidths;
    for (size_t i = bands.size() - lookback; i < bands.size(); ++i) {
        bandwidths.push_back(bands[i].bandwidth);
    }

    std::sort(bandwidths.begin(), bandwidths.end());

    double currentBandwidth = bands.back().bandwidth;
    size_t percentileIdx = static_cast<size_t>(lookback * percentile);

    return currentBandwidth <= bandwidths[percentileIdx];
}

double TechnicalIndicators::Highest(const std::vector<double>& prices, int period) {
    if (prices.size() < static_cast<size_t>(period)) {
        return prices.empty() ? 0.0 : *std::max_element(prices.begin(), prices.end());
    }

    return *std::max_element(prices.end() - period, prices.end());
}

double TechnicalIndicators::Lowest(const std::vector<double>& prices, int period) {
    if (prices.size() < static_cast<size_t>(period)) {
        return prices.empty() ? 0.0 : *std::min_element(prices.begin(), prices.end());
    }

    return *std::min_element(prices.end() - period, prices.end());
}

double TechnicalIndicators::ROC(const std::vector<double>& prices, int period) {
    if (prices.size() < static_cast<size_t>(period) + 1) {
        return 0.0;
    }

    double current = prices.back();
    double past = prices[prices.size() - period - 1];

    if (past == 0) return 0.0;
    return ((current - past) / past) * 100.0;
}

double TechnicalIndicators::Momentum(const std::vector<double>& prices, int period) {
    if (prices.size() < static_cast<size_t>(period) + 1) {
        return 0.0;
    }

    return prices.back() - prices[prices.size() - period - 1];
}

TechnicalIndicators::Stochastic TechnicalIndicators::StochasticOscillator(
    const std::vector<OHLCV>& candles, int kPeriod, int dPeriod) {

    Stochastic result = {50.0, 50.0};

    if (candles.size() < static_cast<size_t>(kPeriod)) {
        return result;
    }

    std::vector<double> kValues;

    for (size_t i = kPeriod - 1; i < candles.size(); ++i) {
        double highest = candles[i].high;
        double lowest = candles[i].low;

        for (size_t j = i - kPeriod + 1; j < i; ++j) {
            highest = (std::max)(highest, candles[j].high);
            lowest = (std::min)(lowest, candles[j].low);
        }

        double k = 50.0;
        if (highest != lowest) {
            k = ((candles[i].close - lowest) / (highest - lowest)) * 100.0;
        }
        kValues.push_back(k);
    }

    result.k = kValues.back();

    // %D = %K의 이동평균
    if (kValues.size() >= static_cast<size_t>(dPeriod)) {
        result.d = SMA(kValues, dPeriod);
    } else {
        result.d = result.k;
    }

    return result;
}

// ============================================================================
// StreamingIndicators
// ============================================================================

StreamingIndicators::StreamingIndicators() {}

void StreamingIndicators::addPrice(double price, long long volume) {
    prices.push_back(price);
    if (volume > 0) volumes.push_back(static_cast<double>(volume));

    if (prices.size() > maxSize) {
        prices.pop_front();
    }
    if (volumes.size() > maxSize) {
        volumes.pop_front();
    }

    // EMA 캐시 무효화
    emaCache.clear();
    rsiCache.clear();
}

void StreamingIndicators::addOHLCV(const OHLCV& candle) {
    candles.push_back(candle);
    if (candles.size() > maxSize) {
        candles.pop_front();
    }

    addPrice(candle.close, candle.volume);
}

double StreamingIndicators::getCurrentSMA(int period) const {
    std::vector<double> priceVec(prices.begin(), prices.end());
    if (priceVec.size() < static_cast<size_t>(period)) return 0.0;
    return TechnicalIndicators::SMA(priceVec, period);
}

double StreamingIndicators::getCurrentEMA(int period) const {
    auto it = emaCache.find(period);
    if (it != emaCache.end()) return it->second;

    std::vector<double> priceVec(prices.begin(), prices.end());
    if (priceVec.size() < static_cast<size_t>(period)) return 0.0;

    double ema = TechnicalIndicators::EMA(priceVec, period);
    emaCache[period] = ema;
    return ema;
}

double StreamingIndicators::getCurrentRSI(int period) const {
    auto it = rsiCache.find(period);
    if (it != rsiCache.end()) return it->second;

    std::vector<double> priceVec(prices.begin(), prices.end());
    if (priceVec.size() < static_cast<size_t>(period) + 1) return 50.0;

    double rsi = TechnicalIndicators::RSI(priceVec, period);
    rsiCache[period] = rsi;
    return rsi;
}

double StreamingIndicators::getCurrentVWAP() const {
    std::vector<OHLCV> candleVec(candles.begin(), candles.end());
    if (candleVec.empty()) return 0.0;
    return TechnicalIndicators::VWAP(candleVec);
}

MACDResult StreamingIndicators::getCurrentMACD() const {
    std::vector<double> priceVec(prices.begin(), prices.end());
    return TechnicalIndicators::MACD(priceVec);
}

BollingerBands StreamingIndicators::getCurrentBB(int period, double stdDev) const {
    std::vector<double> priceVec(prices.begin(), prices.end());
    if (priceVec.size() < static_cast<size_t>(period)) {
        return {0, 0, 0, 0, 0};
    }
    return TechnicalIndicators::BollingerBand(priceVec, period, stdDev);
}

double StreamingIndicators::getCurrentATR(int period) const {
    std::vector<OHLCV> candleVec(candles.begin(), candles.end());
    if (candleVec.size() < static_cast<size_t>(period) + 1) return 0.0;
    return TechnicalIndicators::ATR(candleVec, period);
}

void StreamingIndicators::clear() {
    prices.clear();
    candles.clear();
    volumes.clear();
    emaCache.clear();
    rsiCache.clear();
}

void StreamingIndicators::setMaxSize(size_t size) {
    maxSize = size;
}

} // namespace yuanta
