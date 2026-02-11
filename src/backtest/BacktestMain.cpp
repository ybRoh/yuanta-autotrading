#include "../../include/TechnicalIndicators.h"
#include "../../include/RiskManager.h"
#include "../../include/Strategy.h"
#include "../../include/MarketDataManager.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <map>

using namespace yuanta;

// 백테스트 결과 구조체
struct BacktestResult {
    double totalReturn = 0.0;
    double annualizedReturn = 0.0;
    double maxDrawdown = 0.0;
    double sharpeRatio = 0.0;
    double winRate = 0.0;
    double profitFactor = 0.0;
    int totalTrades = 0;
    int winTrades = 0;
    int lossTrades = 0;
    double avgWin = 0.0;
    double avgLoss = 0.0;
    double avgHoldingPeriod = 0.0;  // 분 단위
};

// 백테스트 거래 기록
struct BacktestTrade {
    std::string code;
    long long entryTime;
    long long exitTime;
    double entryPrice;
    double exitPrice;
    int quantity;
    double pnl;
    double pnlPercent;
    std::string strategy;
    std::string exitReason;
};

// 백테스터 클래스
class Backtester {
public:
    Backtester() {
        // 기본 설정
        config.dailyBudget = 10000000.0;
        config.maxPositionRatio = 0.20;
        config.maxDailyLossRatio = 0.03;
        config.maxConcurrentPositions = 3;

        // 거래 비용 설정
        slippage = 0.001;      // 0.1%
        commission = 0.00015;  // 0.015%
        tax = 0.0023;          // 0.23%
    }

    // 데이터 로드
    bool loadData(const std::string& filepath, const std::string& code) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "Failed to open: " << filepath << std::endl;
            return false;
        }

        std::vector<OHLCV> candles;
        std::string line;

        // 헤더 스킵
        std::getline(file, line);

        while (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string token;
            OHLCV candle;
            candle.code = code;

            int col = 0;
            while (std::getline(ss, token, ',')) {
                try {
                    switch (col) {
                        case 0: candle.timestamp = std::stoll(token); break;
                        case 1: candle.open = std::stod(token); break;
                        case 2: candle.high = std::stod(token); break;
                        case 3: candle.low = std::stod(token); break;
                        case 4: candle.close = std::stod(token); break;
                        case 5: candle.volume = std::stoll(token); break;
                    }
                } catch (...) {
                    continue;
                }
                col++;
            }

            if (col >= 6) {
                candles.push_back(candle);
            }
        }

        historicalData[code] = candles;
        std::cout << "Loaded " << candles.size() << " candles for " << code << std::endl;
        return true;
    }

    // 시뮬레이션 데이터 생성
    void generateSimulatedData(const std::string& code, int days = 180) {
        std::vector<OHLCV> candles;

        // 분봉 데이터 생성 (하루 390분)
        int candlesPerDay = 390;
        int totalCandles = days * candlesPerDay;

        double basePrice = 50000.0;
        long long baseTime = 1704067200000LL;  // 2024-01-01

        srand(42);  // 재현 가능한 결과

        for (int i = 0; i < totalCandles; ++i) {
            OHLCV candle;
            candle.code = code;
            candle.timestamp = baseTime + (i * 60000LL);

            // 가격 변동 시뮬레이션
            double dailyTrend = sin(i / (double)candlesPerDay * 0.1) * 0.02;
            double noise = ((rand() % 200) - 100) / 10000.0;
            double change = dailyTrend + noise;

            // 갭 발생 시뮬레이션 (장 시작 시)
            if (i % candlesPerDay == 0 && rand() % 10 < 3) {
                change += ((rand() % 40) - 10) / 1000.0;
            }

            candle.open = basePrice;
            basePrice *= (1.0 + change);
            candle.close = basePrice;
            candle.high = std::max(candle.open, candle.close) * (1.0 + (rand() % 30) / 10000.0);
            candle.low = std::min(candle.open, candle.close) * (1.0 - (rand() % 30) / 10000.0);
            candle.volume = 10000 + rand() % 90000;

            // 거래량 스파이크
            if (rand() % 20 == 0) {
                candle.volume *= 3;
            }

            candles.push_back(candle);
        }

        historicalData[code] = candles;
        std::cout << "Generated " << candles.size() << " simulated candles for " << code << std::endl;
    }

    // 백테스트 실행
    BacktestResult run(const std::string& strategyName) {
        BacktestResult result;

        if (historicalData.empty()) {
            std::cerr << "No data loaded" << std::endl;
            return result;
        }

        // 전략 초기화
        std::unique_ptr<Strategy> strategy;
        if (strategyName == "GapPullback") {
            strategy = std::make_unique<GapPullbackStrategy>();
        } else if (strategyName == "MABreakout") {
            strategy = std::make_unique<MABreakoutStrategy>();
        } else if (strategyName == "BBSqueeze") {
            strategy = std::make_unique<BBSqueezeStrategy>();
        } else {
            std::cerr << "Unknown strategy: " << strategyName << std::endl;
            return result;
        }

        // 리스크 매니저 초기화
        RiskManager rm(config);

        // 각 종목별 백테스트
        for (auto& [code, candles] : historicalData) {
            std::cout << "\nBacktesting " << strategyName << " on " << code << "..." << std::endl;

            // 시뮬레이션
            simulateTrading(strategy.get(), rm, code, candles);
        }

        // 결과 계산
        calculateResults(result);

        return result;
    }

    // 거래 기록 조회
    const std::vector<BacktestTrade>& getTrades() const {
        return trades;
    }

    // 일별 자산 곡선 조회
    const std::vector<std::pair<long long, double>>& getEquityCurve() const {
        return equityCurve;
    }

private:
    DailyBudgetConfig config;
    double slippage;
    double commission;
    double tax;

    std::map<std::string, std::vector<OHLCV>> historicalData;
    std::vector<BacktestTrade> trades;
    std::vector<std::pair<long long, double>> equityCurve;

    // 현재 포지션
    struct SimPosition {
        std::string code;
        int quantity;
        double entryPrice;
        long long entryTime;
        double stopLoss;
        double takeProfit1;
        double takeProfit2;
        std::string strategy;
    };
    std::map<std::string, SimPosition> positions;
    double cash = 10000000.0;
    double peakEquity = 10000000.0;
    double maxDrawdown = 0.0;

    void simulateTrading(Strategy* strategy, RiskManager& rm,
                         const std::string& code,
                         const std::vector<OHLCV>& candles) {
        int lookback = 100;  // 지표 계산용 lookback

        for (size_t i = lookback; i < candles.size(); ++i) {
            // 시세 데이터 생성
            QuoteData quote;
            quote.code = code;
            quote.currentPrice = candles[i].close;
            quote.openPrice = candles[i].open;
            quote.highPrice = candles[i].high;
            quote.lowPrice = candles[i].low;
            quote.volume = candles[i].volume;
            quote.timestamp = candles[i].timestamp;

            // 전일 종가 (전날 마지막 봉)
            int candlesPerDay = 390;
            if (i >= candlesPerDay) {
                quote.prevClose = candles[i - candlesPerDay].close;
            } else {
                quote.prevClose = candles[0].open;
            }
            quote.changeRate = ((quote.currentPrice - quote.prevClose) / quote.prevClose) * 100.0;

            // lookback 데이터
            std::vector<OHLCV> lookbackCandles(candles.begin() + i - lookback,
                                                candles.begin() + i + 1);

            // 포지션 확인 및 청산 조건 체크
            auto posIt = positions.find(code);
            if (posIt != positions.end()) {
                SimPosition& pos = posIt->second;

                // 손절 체크
                if (quote.currentPrice <= pos.stopLoss) {
                    closeTrade(pos, quote.currentPrice, candles[i].timestamp, "StopLoss");
                    positions.erase(posIt);
                    continue;
                }

                // 익절 체크
                if (quote.currentPrice >= pos.takeProfit1) {
                    closeTrade(pos, quote.currentPrice, candles[i].timestamp, "TakeProfit");
                    positions.erase(posIt);
                    continue;
                }

                // 시간 기반 청산 (장 마감 1시간 전)
                int candleInDay = i % 390;
                if (candleInDay >= 330) {  // 14:30
                    closeTrade(pos, quote.currentPrice, candles[i].timestamp, "TimeStop");
                    positions.erase(posIt);
                    continue;
                }
            }

            // 새 진입 신호 분석
            if (positions.find(code) == positions.end()) {
                // 장 시작 시간 체크 (갭 전략용)
                int candleInDay = i % 390;
                if (candleInDay < 15 || candleInDay > 300) {
                    continue;  // 장 시작 15분 또는 장 마감 90분 전에는 진입 안함
                }

                SignalInfo signal = strategy->analyze(code, lookbackCandles, quote);

                if (signal.signal == Signal::BUY) {
                    // 포지션 크기 계산
                    double maxPosition = cash * config.maxPositionRatio;
                    int qty = static_cast<int>(maxPosition / quote.currentPrice);

                    if (qty > 0 && positions.size() < static_cast<size_t>(config.maxConcurrentPositions)) {
                        double fillPrice = quote.currentPrice * (1.0 + slippage);
                        double cost = fillPrice * qty * (1.0 + commission);

                        if (cost <= cash) {
                            SimPosition pos;
                            pos.code = code;
                            pos.quantity = qty;
                            pos.entryPrice = fillPrice;
                            pos.entryTime = candles[i].timestamp;
                            pos.stopLoss = signal.stopLoss > 0 ? signal.stopLoss :
                                           fillPrice * (1.0 - 0.01);
                            pos.takeProfit1 = signal.takeProfit1 > 0 ? signal.takeProfit1 :
                                              fillPrice * (1.0 + 0.02);
                            pos.strategy = strategy->getName();

                            positions[code] = pos;
                            cash -= cost;
                        }
                    }
                }
            }

            // 자산 곡선 업데이트
            double equity = cash;
            for (const auto& [c, pos] : positions) {
                equity += pos.quantity * quote.currentPrice;
            }

            // 일별로 기록 (장 마감 시)
            if (i % 390 == 389) {
                equityCurve.push_back({candles[i].timestamp, equity});

                // 최대 낙폭 계산
                peakEquity = std::max(peakEquity, equity);
                double drawdown = (peakEquity - equity) / peakEquity;
                maxDrawdown = std::max(maxDrawdown, drawdown);
            }
        }

        // 남은 포지션 청산
        for (auto& [c, pos] : positions) {
            if (c == code && !historicalData[code].empty()) {
                double lastPrice = historicalData[code].back().close;
                long long lastTime = historicalData[code].back().timestamp;
                closeTrade(pos, lastPrice, lastTime, "EndOfTest");
            }
        }
        positions.erase(code);
    }

    void closeTrade(const SimPosition& pos, double exitPrice, long long exitTime,
                    const std::string& reason) {
        double fillPrice = exitPrice * (1.0 - slippage);
        double proceeds = pos.quantity * fillPrice;
        double sellCommission = proceeds * commission;
        double sellTax = proceeds * tax;

        proceeds -= (sellCommission + sellTax);
        cash += proceeds;

        double pnl = proceeds - (pos.entryPrice * pos.quantity);
        double pnlPercent = (fillPrice - pos.entryPrice) / pos.entryPrice * 100.0;

        BacktestTrade trade;
        trade.code = pos.code;
        trade.entryTime = pos.entryTime;
        trade.exitTime = exitTime;
        trade.entryPrice = pos.entryPrice;
        trade.exitPrice = fillPrice;
        trade.quantity = pos.quantity;
        trade.pnl = pnl;
        trade.pnlPercent = pnlPercent;
        trade.strategy = pos.strategy;
        trade.exitReason = reason;

        trades.push_back(trade);
    }

    void calculateResults(BacktestResult& result) {
        if (trades.empty()) {
            return;
        }

        // 기본 통계
        result.totalTrades = trades.size();
        double totalPnL = 0.0;
        double totalWin = 0.0;
        double totalLoss = 0.0;
        double totalHoldingTime = 0.0;

        for (const auto& trade : trades) {
            totalPnL += trade.pnl;

            if (trade.pnl > 0) {
                result.winTrades++;
                totalWin += trade.pnl;
            } else {
                result.lossTrades++;
                totalLoss += std::abs(trade.pnl);
            }

            totalHoldingTime += (trade.exitTime - trade.entryTime) / 60000.0;  // 분 단위
        }

        // 수익률
        result.totalReturn = totalPnL / config.dailyBudget * 100.0;

        // 승률
        result.winRate = result.totalTrades > 0 ?
            (double)result.winTrades / result.totalTrades * 100.0 : 0.0;

        // 평균 손익
        result.avgWin = result.winTrades > 0 ? totalWin / result.winTrades : 0.0;
        result.avgLoss = result.lossTrades > 0 ? totalLoss / result.lossTrades : 0.0;

        // 손익비
        result.profitFactor = result.avgLoss > 0 ? result.avgWin / result.avgLoss : 0.0;

        // 평균 보유 시간
        result.avgHoldingPeriod = result.totalTrades > 0 ?
            totalHoldingTime / result.totalTrades : 0.0;

        // 최대 낙폭
        result.maxDrawdown = maxDrawdown * 100.0;

        // 연환산 수익률 (가정: 250 거래일)
        if (!equityCurve.empty()) {
            int tradingDays = equityCurve.size();
            result.annualizedReturn = result.totalReturn * (250.0 / tradingDays);
        }

        // 샤프 비율 (단순화)
        if (!equityCurve.empty() && equityCurve.size() > 1) {
            std::vector<double> dailyReturns;
            for (size_t i = 1; i < equityCurve.size(); ++i) {
                double ret = (equityCurve[i].second - equityCurve[i-1].second) /
                             equityCurve[i-1].second;
                dailyReturns.push_back(ret);
            }

            double avgReturn = 0.0;
            for (double r : dailyReturns) avgReturn += r;
            avgReturn /= dailyReturns.size();

            double variance = 0.0;
            for (double r : dailyReturns) {
                variance += (r - avgReturn) * (r - avgReturn);
            }
            variance /= dailyReturns.size();
            double stdDev = std::sqrt(variance);

            result.sharpeRatio = stdDev > 0 ?
                (avgReturn * std::sqrt(250.0)) / (stdDev * std::sqrt(250.0)) : 0.0;
        }
    }
};

void printResults(const BacktestResult& result, const std::string& strategyName) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "  Backtest Results: " << strategyName << std::endl;
    std::cout << "========================================\n" << std::endl;

    std::cout << std::fixed << std::setprecision(2);

    std::cout << "Performance Metrics:" << std::endl;
    std::cout << "  Total Return:       " << result.totalReturn << "%" << std::endl;
    std::cout << "  Annualized Return:  " << result.annualizedReturn << "%" << std::endl;
    std::cout << "  Max Drawdown:       " << result.maxDrawdown << "%" << std::endl;
    std::cout << "  Sharpe Ratio:       " << result.sharpeRatio << std::endl;
    std::cout << std::endl;

    std::cout << "Trade Statistics:" << std::endl;
    std::cout << "  Total Trades:       " << result.totalTrades << std::endl;
    std::cout << "  Winning Trades:     " << result.winTrades << std::endl;
    std::cout << "  Losing Trades:      " << result.lossTrades << std::endl;
    std::cout << "  Win Rate:           " << result.winRate << "%" << std::endl;
    std::cout << "  Profit Factor:      " << result.profitFactor << std::endl;
    std::cout << std::endl;

    std::cout << "Average Trade:" << std::endl;
    std::cout << "  Avg Win:            " << std::setprecision(0) << result.avgWin << " KRW" << std::endl;
    std::cout << "  Avg Loss:           " << result.avgLoss << " KRW" << std::endl;
    std::cout << "  Avg Holding Time:   " << std::setprecision(1) << result.avgHoldingPeriod << " min" << std::endl;

    std::cout << "\n========================================\n" << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "========================================" << std::endl;
    std::cout << "  Yuanta Backtesting System v1.0" << std::endl;
    std::cout << "========================================\n" << std::endl;

    Backtester backtester;

    // 시뮬레이션 데이터 생성 (실제 사용 시 loadData 사용)
    std::vector<std::string> codes = {"005930", "000660", "035420"};

    for (const auto& code : codes) {
        // 데이터 파일이 있으면 로드, 없으면 시뮬레이션 데이터 생성
        std::string filepath = "data/" + code + "_1m.csv";
        if (!backtester.loadData(filepath, code)) {
            backtester.generateSimulatedData(code, 180);  // 6개월
        }
    }

    // 각 전략별 백테스트
    std::vector<std::string> strategies = {"GapPullback", "MABreakout", "BBSqueeze"};

    for (const auto& strategyName : strategies) {
        // 새 백테스터로 각 전략 테스트
        Backtester bt;
        for (const auto& code : codes) {
            bt.generateSimulatedData(code, 180);
        }

        BacktestResult result = bt.run(strategyName);
        printResults(result, strategyName);

        // 거래 기록 저장
        std::string outputPath = "logs/backtest_" + strategyName + ".csv";
        std::ofstream outFile(outputPath);
        if (outFile.is_open()) {
            outFile << "Code,EntryTime,ExitTime,EntryPrice,ExitPrice,Quantity,PnL,PnL%,Strategy,ExitReason\n";
            for (const auto& trade : bt.getTrades()) {
                outFile << trade.code << ","
                        << trade.entryTime << ","
                        << trade.exitTime << ","
                        << trade.entryPrice << ","
                        << trade.exitPrice << ","
                        << trade.quantity << ","
                        << trade.pnl << ","
                        << trade.pnlPercent << ","
                        << trade.strategy << ","
                        << trade.exitReason << "\n";
            }
            std::cout << "Trade log saved to " << outputPath << std::endl;
        }
    }

    std::cout << "\nBacktesting completed!" << std::endl;

    return 0;
}
