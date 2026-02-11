#include "../../include/Strategy.h"
#include "../../include/TechnicalIndicators.h"
#include <ctime>
#include <algorithm>

namespace yuanta {

BBSqueezeStrategy::BBSqueezeStrategy() {}

SignalInfo BBSqueezeStrategy::analyze(const std::string& code,
                                       const std::vector<OHLCV>& candles,
                                       const QuoteData& quote) {
    SignalInfo signal;
    signal.code = code;
    signal.signal = Signal::NONE;

    if (!enabled) return signal;

    // 최소 데이터 필요량 확인
    if (candles.size() < static_cast<size_t>(squeezeLookback)) {
        return signal;
    }

    // 종가 배열 추출
    std::vector<double> closes;
    for (const auto& candle : candles) {
        closes.push_back(candle.close);
    }

    // 볼린저 밴드 계산
    auto bbVector = TechnicalIndicators::BollingerBandVector(closes, bbPeriod, bbStdDev);

    if (bbVector.size() < static_cast<size_t>(squeezeLookback)) {
        return signal;
    }

    // 1. 스퀴즈 확인 (밴드폭이 하위 20%)
    if (!checkSqueeze(bbVector)) {
        return signal;
    }

    // 2. 상단밴드 돌파 확인
    double currentPrice = quote.currentPrice;
    auto currentBB = bbVector.back();

    if (!checkBreakout(currentPrice, currentBB)) {
        return signal;
    }

    // 3. 거래량 확인 (150% 이상)
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

    // 4. RSI 확인 (55~75)
    double rsi = TechnicalIndicators::RSI(closes, 14);
    if (rsi < rsiMin || rsi > rsiMax) {
        return signal;
    }

    // ATR 계산 (목표가 설정용)
    double atr = TechnicalIndicators::ATR(candles, 14);

    // 모든 조건 충족 - 매수 신호
    signal.signal = Signal::BUY;
    signal.price = currentPrice;
    signal.stopLoss = currentBB.middle;  // 중심선 이탈 시 손절

    // 또는 고정 손절
    double fixedStopLoss = currentPrice * (1.0 - stopLossPercent / 100.0);
    signal.stopLoss = (std::max)(signal.stopLoss, fixedStopLoss);

    signal.takeProfit1 = calculateTarget(currentBB, atr);
    signal.confidence = 0.62;  // 60-65% 승률
    signal.reason = "BB Squeeze: Bandwidth percentile low + Upper break + RSI " +
                    std::to_string(static_cast<int>(rsi));

    return signal;
}

bool BBSqueezeStrategy::shouldClose(const Position& position,
                                     const QuoteData& quote) {
    double currentPrice = quote.currentPrice;

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

    if (minutes >= 870) {
        return true;
    }

    return false;
}

void BBSqueezeStrategy::setParameter(const std::string& name, double value) {
    if (name == "bbPeriod") bbPeriod = static_cast<int>(value);
    else if (name == "bbStdDev") bbStdDev = value;
    else if (name == "squeezeLookback") squeezeLookback = static_cast<int>(value);
    else if (name == "squeezePercentile") squeezePercentile = value;
    else if (name == "volumeMultiple") volumeMultiple = value;
    else if (name == "rsiMin") rsiMin = value;
    else if (name == "rsiMax") rsiMax = value;
    else if (name == "stopLossPercent") stopLossPercent = value;
}

double BBSqueezeStrategy::getParameter(const std::string& name) const {
    if (name == "bbPeriod") return static_cast<double>(bbPeriod);
    if (name == "bbStdDev") return bbStdDev;
    if (name == "squeezeLookback") return static_cast<double>(squeezeLookback);
    if (name == "squeezePercentile") return squeezePercentile;
    if (name == "volumeMultiple") return volumeMultiple;
    if (name == "rsiMin") return rsiMin;
    if (name == "rsiMax") return rsiMax;
    if (name == "stopLossPercent") return stopLossPercent;
    return 0.0;
}

bool BBSqueezeStrategy::checkSqueeze(const std::vector<BollingerBands>& bands) const {
    if (bands.size() < static_cast<size_t>(squeezeLookback)) return false;

    // 최근 N개 밴드폭 수집
    std::vector<double> bandwidths;
    for (size_t i = bands.size() - squeezeLookback; i < bands.size(); ++i) {
        bandwidths.push_back(bands[i].bandwidth);
    }

    // 정렬하여 백분위 계산
    std::vector<double> sorted = bandwidths;
    std::sort(sorted.begin(), sorted.end());

    double currentBandwidth = bandwidths.back();
    size_t percentileIdx = static_cast<size_t>(squeezeLookback * squeezePercentile);

    // 현재 밴드폭이 하위 N% 이내인지 확인
    return currentBandwidth <= sorted[percentileIdx];
}

bool BBSqueezeStrategy::checkBreakout(double price, const BollingerBands& bb) const {
    return price > bb.upper;
}

double BBSqueezeStrategy::calculateTarget(const BollingerBands& bb, double atr) const {
    // 목표가: 상단밴드 + ATR
    return bb.upper + atr;
}

// ============================================================================
// StrategyManager
// ============================================================================

StrategyManager::StrategyManager() {}

StrategyManager::~StrategyManager() {}

void StrategyManager::addStrategy(std::unique_ptr<Strategy> strategy) {
    strategies.push_back(std::move(strategy));
}

void StrategyManager::removeStrategy(const std::string& name) {
    strategies.erase(
        std::remove_if(strategies.begin(), strategies.end(),
            [&name](const std::unique_ptr<Strategy>& s) {
                return s->getName() == name;
            }),
        strategies.end()
    );
}

Strategy* StrategyManager::getStrategy(const std::string& name) {
    for (auto& s : strategies) {
        if (s->getName() == name) {
            return s.get();
        }
    }
    return nullptr;
}

std::vector<SignalInfo> StrategyManager::analyzeAll(const std::string& code,
                                                     const std::vector<OHLCV>& candles,
                                                     const QuoteData& quote) {
    std::vector<SignalInfo> signals;

    for (auto& strategy : strategies) {
        if (strategy->isEnabled()) {
            auto signal = strategy->analyze(code, candles, quote);
            if (signal.signal != Signal::NONE) {
                signals.push_back(signal);
            }
        }
    }

    // 신뢰도 순으로 정렬
    std::sort(signals.begin(), signals.end(),
        [](const SignalInfo& a, const SignalInfo& b) {
            return a.confidence > b.confidence;
        });

    return signals;
}

std::vector<SignalInfo> StrategyManager::checkCloseConditions(
    const std::map<std::string, Position>& positions,
    const std::map<std::string, QuoteData>& quotes) {

    std::vector<SignalInfo> closeSignals;

    for (const auto& posEntry : positions) {
        const std::string& code = posEntry.first;
        const Position& position = posEntry.second;

        auto quoteIt = quotes.find(code);
        if (quoteIt == quotes.end()) continue;

        const QuoteData& quote = quoteIt->second;

        for (auto& strategy : strategies) {
            if (strategy->isEnabled() && strategy->shouldClose(position, quote)) {
                SignalInfo signal;
                signal.signal = Signal::CLOSE_LONG;
                signal.code = code;
                signal.price = quote.currentPrice;
                signal.quantity = position.quantity;
                closeSignals.push_back(signal);
                break;  // 하나의 전략에서 청산 시그널 나오면 충분
            }
        }
    }

    return closeSignals;
}

void StrategyManager::setRiskManager(RiskManager* rm) {
    riskManager = rm;
}

} // namespace yuanta
