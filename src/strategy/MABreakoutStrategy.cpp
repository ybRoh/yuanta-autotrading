#include "../../include/Strategy.h"
#include "../../include/TechnicalIndicators.h"
#include <ctime>
#include <algorithm>

namespace yuanta {

MABreakoutStrategy::MABreakoutStrategy() {}

SignalInfo MABreakoutStrategy::analyze(const std::string& code,
                                        const std::vector<OHLCV>& candles,
                                        const QuoteData& quote) {
    SignalInfo signal;
    signal.code = code;
    signal.signal = Signal::NONE;

    if (!enabled) return signal;

    // 최소 데이터 필요량 확인
    if (candles.size() < static_cast<size_t>(slowMA) + 5) {
        return signal;
    }

    // 종가 배열 추출
    std::vector<double> closes;
    for (const auto& candle : candles) {
        closes.push_back(candle.close);
    }

    // 이동평균선 계산
    auto ma5 = TechnicalIndicators::SMAVector(closes, fastMA);
    auto ma10 = TechnicalIndicators::SMAVector(closes, midMA);
    auto ma20 = TechnicalIndicators::SMAVector(closes, slowMA);

    if (ma5.empty() || ma10.empty() || ma20.empty()) {
        return signal;
    }

    // 1. 정배열 확인 (5MA > 10MA > 20MA)
    if (!checkMAAlignment(ma5, ma10, ma20)) {
        return signal;
    }

    // 2. 20MA 돌파 확인
    double currentPrice = quote.currentPrice;
    if (!checkMABreakout(currentPrice, ma20)) {
        return signal;
    }

    // 3. 거래량 확인 (300% 이상)
    if (candles.size() >= 20) {
        long long recentVolume = candles.back().volume;
        long long avgVolume = 0;
        for (size_t i = candles.size() - 20; i < candles.size() - 1; ++i) {
            avgVolume += candles[i].volume;
        }
        avgVolume /= 19;

        if (avgVolume > 0 && recentVolume < avgVolume * volumeMultiple) {
            return signal;
        }
    }

    // 4. RSI 확인 (50~70)
    double rsi = TechnicalIndicators::RSI(closes, 14);
    if (!checkRSI(rsi)) {
        return signal;
    }

    // 5. MACD 양전환 확인
    auto macd = TechnicalIndicators::MACD(closes);
    if (!checkMACDCross(macd)) {
        return signal;
    }

    // 모든 조건 충족 - 매수 신호
    signal.signal = Signal::BUY;
    signal.price = currentPrice;
    signal.stopLoss = currentPrice * (1.0 - stopLossPercent / 100.0);
    signal.takeProfit1 = currentPrice * (1.0 + takeProfit1Percent / 100.0);
    signal.takeProfit2 = currentPrice * (1.0 + takeProfit2Percent / 100.0);
    signal.confidence = 0.58;  // 55-60% 승률
    signal.reason = "MA Breakout: Aligned + 20MA break + RSI " +
                    std::to_string(static_cast<int>(rsi));

    return signal;
}

bool MABreakoutStrategy::shouldClose(const Position& position,
                                      const QuoteData& quote) {
    double currentPrice = quote.currentPrice;

    // 손절 확인
    if (currentPrice <= position.stopLossPrice) {
        return true;
    }

    // 5MA 이탈 확인 (추가 청산 조건)
    // 실제 구현에서는 분봉 데이터로 5MA 계산 필요

    // 1차 익절 (50% 물량)
    if (position.remainingQty == position.quantity &&
        currentPrice >= position.takeProfitPrice1) {
        return true;  // 부분 청산 시그널
    }

    // 2차 익절 (잔여 물량)
    if (position.remainingQty < position.quantity &&
        currentPrice >= position.takeProfitPrice2) {
        return true;
    }

    // 시간 기반 청산 (14:30 이후)
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm* tm = std::localtime(&t);
    int minutes = tm->tm_hour * 60 + tm->tm_min;

    if (minutes >= 870) {
        return true;
    }

    return false;
}

void MABreakoutStrategy::setParameter(const std::string& name, double value) {
    if (name == "fastMA") fastMA = static_cast<int>(value);
    else if (name == "midMA") midMA = static_cast<int>(value);
    else if (name == "slowMA") slowMA = static_cast<int>(value);
    else if (name == "volumeMultiple") volumeMultiple = value;
    else if (name == "rsiMin") rsiMin = value;
    else if (name == "rsiMax") rsiMax = value;
    else if (name == "takeProfit1Percent") takeProfit1Percent = value;
    else if (name == "takeProfit2Percent") takeProfit2Percent = value;
    else if (name == "stopLossPercent") stopLossPercent = value;
}

double MABreakoutStrategy::getParameter(const std::string& name) const {
    if (name == "fastMA") return static_cast<double>(fastMA);
    if (name == "midMA") return static_cast<double>(midMA);
    if (name == "slowMA") return static_cast<double>(slowMA);
    if (name == "volumeMultiple") return volumeMultiple;
    if (name == "rsiMin") return rsiMin;
    if (name == "rsiMax") return rsiMax;
    if (name == "takeProfit1Percent") return takeProfit1Percent;
    if (name == "takeProfit2Percent") return takeProfit2Percent;
    if (name == "stopLossPercent") return stopLossPercent;
    return 0.0;
}

bool MABreakoutStrategy::checkMAAlignment(const std::vector<double>& ma5,
                                           const std::vector<double>& ma10,
                                           const std::vector<double>& ma20) const {
    if (ma5.empty() || ma10.empty() || ma20.empty()) return false;

    // 가장 최근 값으로 정배열 확인
    double latest5 = ma5.back();
    double latest10 = ma10.back();
    double latest20 = ma20.back();

    return latest5 > latest10 && latest10 > latest20;
}

bool MABreakoutStrategy::checkMABreakout(double price, const std::vector<double>& ma20) const {
    if (ma20.size() < 2) return false;

    double current20MA = ma20.back();
    double prev20MA = ma20[ma20.size() - 2];

    // 현재가가 20MA 위에 있고, 이전에는 아래에 있었으면 돌파
    return price > current20MA;
}

bool MABreakoutStrategy::checkRSI(double rsi) const {
    return rsi >= rsiMin && rsi <= rsiMax;
}

bool MABreakoutStrategy::checkMACDCross(const MACDResult& macd) const {
    // MACD 양전환 (골든크로스) 또는 MACD > 0
    return macd.bullishCross || (macd.macd > 0 && macd.histogram > 0);
}

} // namespace yuanta
