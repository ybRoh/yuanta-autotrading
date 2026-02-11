#include "../../include/Strategy.h"
#include "../../include/TechnicalIndicators.h"
#include <ctime>
#include <algorithm>
#include <iostream>

namespace yuanta {

GapPullbackStrategy::GapPullbackStrategy() {}

SignalInfo GapPullbackStrategy::analyze(const std::string& code,
                                         const std::vector<OHLCV>& candles,
                                         const QuoteData& quote) {
    SignalInfo signal;
    signal.code = code;
    signal.signal = Signal::NONE;

    if (!enabled || candles.empty()) {
        return signal;
    }

    // 장 시작 15분 이내인지 확인
    if (!isWithinEntryWindow()) {
        return signal;
    }

    // 전일 종가 (일봉 데이터 필요, 여기선 분봉의 첫 봉 시가 사용)
    double prevClose = quote.prevClose;
    if (prevClose <= 0) {
        prevClose = candles.front().open;
    }

    // 1. 갭 상승 확인 (1.5% ~ 5%)
    if (!checkGapUp(quote, prevClose)) {
        return signal;
    }

    // 2. 아침 고점 기록 및 눌림목 확인
    double currentPrice = quote.currentPrice;

    // 아침 고점 갱신
    auto it = morningHighs.find(code);
    if (it == morningHighs.end()) {
        morningHighs[code] = currentPrice;
    } else if (currentPrice > it->second) {
        morningHighs[code] = currentPrice;
        pullbackDetected[code] = false;  // 고점 갱신 시 눌림 리셋
    }

    // 3. 눌림목 확인 (고점 대비 0.5% ~ 1.5% 하락)
    if (!checkPullback(code, currentPrice)) {
        return signal;
    }

    // 4. 거래량 확인 (전일 동시간대 대비 200% 이상)
    if (!checkVolume(candles)) {
        return signal;
    }

    // 5. VWAP 위에 있는지 확인
    if (!checkVWAP(currentPrice, candles)) {
        return signal;
    }

    // 모든 조건 충족 - 매수 신호
    signal.signal = Signal::BUY;
    signal.price = currentPrice;
    signal.stopLoss = currentPrice * (1.0 - stopLossPercent / 100.0);
    signal.takeProfit1 = currentPrice * (1.0 + takeProfitPercent / 100.0);
    signal.confidence = 0.7;  // 65-70% 승률
    signal.reason = "Gap pullback: Gap " +
                    std::to_string(quote.changeRate) + "%, " +
                    "Pullback from high";

    return signal;
}

bool GapPullbackStrategy::shouldClose(const Position& position,
                                       const QuoteData& quote) {
    double currentPrice = quote.currentPrice;
    double entryPrice = position.avgPrice;

    // 손절 확인
    if (currentPrice <= position.stopLossPrice) {
        return true;
    }

    // 익절 확인
    if (currentPrice >= position.takeProfitPrice1) {
        return true;
    }

    // 시간 기반 청산 (14:30 이후)
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm* tm = std::localtime(&t);
    int minutes = tm->tm_hour * 60 + tm->tm_min;

    if (minutes >= 870) {  // 14:30
        return true;
    }

    return false;
}

void GapPullbackStrategy::setParameter(const std::string& name, double value) {
    if (name == "minGapPercent") minGapPercent = value;
    else if (name == "maxGapPercent") maxGapPercent = value;
    else if (name == "pullbackMin") pullbackMin = value;
    else if (name == "pullbackMax") pullbackMax = value;
    else if (name == "volumeMultiple") volumeMultiple = value;
    else if (name == "takeProfitPercent") takeProfitPercent = value;
    else if (name == "stopLossPercent") stopLossPercent = value;
    else if (name == "entryWindowMinutes") entryWindowMinutes = static_cast<int>(value);
}

double GapPullbackStrategy::getParameter(const std::string& name) const {
    if (name == "minGapPercent") return minGapPercent;
    if (name == "maxGapPercent") return maxGapPercent;
    if (name == "pullbackMin") return pullbackMin;
    if (name == "pullbackMax") return pullbackMax;
    if (name == "volumeMultiple") return volumeMultiple;
    if (name == "takeProfitPercent") return takeProfitPercent;
    if (name == "stopLossPercent") return stopLossPercent;
    if (name == "entryWindowMinutes") return static_cast<double>(entryWindowMinutes);
    return 0.0;
}

bool GapPullbackStrategy::checkGapUp(const QuoteData& quote, double prevClose) const {
    if (prevClose <= 0) return false;

    double gapPercent = ((quote.openPrice - prevClose) / prevClose) * 100.0;

    return gapPercent >= minGapPercent && gapPercent <= maxGapPercent;
}

bool GapPullbackStrategy::checkPullback(const std::string& code, double currentPrice) const {
    auto it = morningHighs.find(code);
    if (it == morningHighs.end()) return false;

    double morningHigh = it->second;
    if (morningHigh <= 0) return false;

    double pullbackPercent = ((morningHigh - currentPrice) / morningHigh) * 100.0;

    // 눌림폭이 적정 범위 내인지 확인
    return pullbackPercent >= pullbackMin && pullbackPercent <= pullbackMax;
}

bool GapPullbackStrategy::checkVolume(const std::vector<OHLCV>& candles) const {
    if (candles.size() < 20) return false;

    // 최근 5분 평균 거래량
    long long recentVolume = 0;
    for (size_t i = candles.size() - 5; i < candles.size(); ++i) {
        recentVolume += candles[i].volume;
    }
    recentVolume /= 5;

    // 이전 15분 평균 거래량 (비교 기준)
    long long prevVolume = 0;
    size_t startIdx = candles.size() > 20 ? candles.size() - 20 : 0;
    for (size_t i = startIdx; i < startIdx + 15 && i < candles.size() - 5; ++i) {
        prevVolume += candles[i].volume;
    }
    prevVolume /= 15;

    if (prevVolume <= 0) return true;  // 비교 데이터 없으면 통과

    return static_cast<double>(recentVolume) >= static_cast<double>(prevVolume) * volumeMultiple;
}

bool GapPullbackStrategy::checkVWAP(double currentPrice, const std::vector<OHLCV>& candles) const {
    if (candles.empty()) return false;

    double vwap = TechnicalIndicators::VWAP(candles);
    return currentPrice > vwap;
}

bool GapPullbackStrategy::isWithinEntryWindow() const {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm* tm = std::localtime(&t);

    int minutes = tm->tm_hour * 60 + tm->tm_min;
    int marketOpen = 540;  // 09:00

    return minutes >= marketOpen && minutes <= (marketOpen + entryWindowMinutes);
}

} // namespace yuanta
