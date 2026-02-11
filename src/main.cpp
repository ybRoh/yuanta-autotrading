#include "../include/YuantaAPI.h"
#include "../include/RiskManager.h"
#include "../include/Strategy.h"
#include "../include/MarketDataManager.h"
#include "../include/OrderExecutor.h"
#include "../include/TechnicalIndicators.h"

#include <iostream>
#include <fstream>
#include <signal.h>
#include <thread>
#include <chrono>
#include <atomic>

// JSON 파싱용 간단한 헬퍼 (실제 환경에서는 nlohmann/json 등 사용)
#include <sstream>
#include <map>

using namespace yuanta;

// 전역 상태
std::atomic<bool> running{true};

// 시그널 핸들러
void signalHandler(int signum) {
    std::cout << "\nShutdown signal received..." << std::endl;
    running = false;
}

// 간단한 설정 로더
struct AppConfig {
    // API 설정
    std::string apiServer = "api.myasset.com";
    int apiPort = 8080;
    std::string userId = "";
    std::string userPassword = "";

    // 리스크 설정
    double dailyBudget = 10000000.0;
    double maxPositionRatio = 0.20;
    double maxDailyLossRatio = 0.03;
    int maxConcurrentPositions = 3;

    // 전략 설정
    bool enableGapPullback = true;
    bool enableMABreakout = true;
    bool enableBBSqueeze = true;

    // 관심 종목
    std::vector<std::string> watchlist;

    bool loadFromFile(const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            std::cout << "Config file not found, using defaults" << std::endl;
            return false;
        }

        std::string line;
        while (std::getline(file, line)) {
            // 간단한 key=value 파싱
            size_t pos = line.find('=');
            if (pos == std::string::npos) continue;

            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);

            if (key == "userId") userId = value;
            else if (key == "dailyBudget") dailyBudget = std::stod(value);
            else if (key == "maxPositionRatio") maxPositionRatio = std::stod(value);
            else if (key == "watchlist") {
                std::stringstream ss(value);
                std::string code;
                while (std::getline(ss, code, ',')) {
                    watchlist.push_back(code);
                }
            }
        }

        return true;
    }
};

void printStatus(RiskManager& rm, StrategyManager& sm) {
    std::cout << "\n========== Trading Status ==========" << std::endl;
    std::cout << "Realized P&L: " << std::fixed << std::setprecision(0)
              << rm.getRealizedPnL() << " KRW" << std::endl;
    std::cout << "Unrealized P&L: " << rm.getUnrealizedPnL() << " KRW" << std::endl;
    std::cout << "Total P&L: " << rm.getTotalPnL() << " KRW" << std::endl;
    std::cout << "Open Positions: " << rm.getOpenPositionCount() << std::endl;
    std::cout << "Win Rate: " << std::setprecision(1) << rm.getWinRate() << "%" << std::endl;

    if (rm.isDailyLossLimitReached()) {
        std::cout << "*** DAILY LOSS LIMIT REACHED - Trading Stopped ***" << std::endl;
    }

    std::cout << "======================================\n" << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "========================================" << std::endl;
    std::cout << "  Yuanta AutoTrading System v1.0" << std::endl;
    std::cout << "========================================\n" << std::endl;

    // 시그널 핸들러 설정
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // 설정 로드
    AppConfig config;
    config.loadFromFile("config/settings.ini");

    // 기본 관심 종목 (설정 파일에 없는 경우)
    if (config.watchlist.empty()) {
        config.watchlist = {"005930", "000660", "035420"};  // 삼성전자, SK하이닉스, NAVER
    }

    // 1. 리스크 매니저 초기화
    DailyBudgetConfig budgetConfig;
    budgetConfig.dailyBudget = config.dailyBudget;
    budgetConfig.maxPositionRatio = config.maxPositionRatio;
    budgetConfig.maxDailyLossRatio = config.maxDailyLossRatio;
    budgetConfig.maxConcurrentPositions = config.maxConcurrentPositions;

    RiskManager riskManager(budgetConfig);

    std::cout << "Risk Manager initialized:" << std::endl;
    std::cout << "  - Daily Budget: " << budgetConfig.dailyBudget << " KRW" << std::endl;
    std::cout << "  - Max Position: " << budgetConfig.getMaxPositionSize() << " KRW" << std::endl;
    std::cout << "  - Max Daily Loss: " << budgetConfig.getMaxDailyLoss() << " KRW" << std::endl;
    std::cout << "  - Max Positions: " << budgetConfig.maxConcurrentPositions << std::endl;
    std::cout << std::endl;

    // 2. API 초기화
    YuantaAPI api;
    if (!api.initialize()) {
        std::cerr << "Failed to initialize API" << std::endl;
        // 시뮬레이션 모드로 계속 진행
    }

    if (!api.connect(config.apiServer, config.apiPort)) {
        std::cerr << "Failed to connect to server" << std::endl;
        // 시뮬레이션 모드로 계속 진행
    }

    if (!config.userId.empty()) {
        if (!api.login(config.userId, config.userPassword)) {
            std::cerr << "Login failed" << std::endl;
        }
    }

    // 3. 시세 데이터 매니저 초기화
    MarketDataManager dataManager;
    dataManager.setAPI(&api);

    for (const auto& code : config.watchlist) {
        dataManager.addWatchlist(code);
        dataManager.loadHistoricalData(code, 60);  // 60일치 데이터 로드
    }

    // 4. 전략 매니저 초기화
    StrategyManager strategyManager;
    strategyManager.setRiskManager(&riskManager);

    if (config.enableGapPullback) {
        strategyManager.addStrategy(std::make_unique<GapPullbackStrategy>());
        std::cout << "Strategy enabled: Gap Pullback (65-70% win rate)" << std::endl;
    }

    if (config.enableMABreakout) {
        strategyManager.addStrategy(std::make_unique<MABreakoutStrategy>());
        std::cout << "Strategy enabled: MA Breakout (55-60% win rate)" << std::endl;
    }

    if (config.enableBBSqueeze) {
        strategyManager.addStrategy(std::make_unique<BBSqueezeStrategy>());
        std::cout << "Strategy enabled: BB Squeeze (60-65% win rate)" << std::endl;
    }

    std::cout << std::endl;

    // 5. 주문 실행기 초기화
    OrderExecutor orderExecutor;
    orderExecutor.setAPI(&api);
    orderExecutor.setRiskManager(&riskManager);
    orderExecutor.start();

    // 6. 손절/익절 모니터 초기화
    StopLossMonitor stopLossMonitor;
    stopLossMonitor.setOrderExecutor(&orderExecutor);
    stopLossMonitor.setRiskManager(&riskManager);
    stopLossMonitor.start();

    // 시세 업데이트 콜백 설정
    dataManager.setQuoteUpdateCallback([&](const std::string& code, const QuoteData& quote) {
        // 손절/익절 모니터에 시세 전달
        stopLossMonitor.onQuoteUpdate(code, quote);
    });

    // 7. 실시간 시세 시작
    dataManager.startRealtime();

    std::cout << "System started. Press Ctrl+C to stop.\n" << std::endl;

    // 8. 메인 루프
    int loopCount = 0;
    while (running) {
        // 장 시간 체크
        if (!dataManager.isMarketOpen()) {
            std::this_thread::sleep_for(std::chrono::seconds(60));
            continue;
        }

        // 일일 손실 한도 체크
        if (riskManager.isDailyLossLimitReached()) {
            std::cout << "Daily loss limit reached. Closing all positions..." << std::endl;
            orderExecutor.closeAllPositions();
            std::this_thread::sleep_for(std::chrono::seconds(60));
            continue;
        }

        // 강제 청산 시간 체크 (14:30 이후)
        if (riskManager.shouldForceClose()) {
            std::cout << "Market close approaching. Closing all positions..." << std::endl;
            orderExecutor.closeAllPositions();
            std::this_thread::sleep_for(std::chrono::seconds(60));
            continue;
        }

        // 각 종목 분석
        for (const auto& code : config.watchlist) {
            auto candles = dataManager.getMinuteCandles(code, 1, 100);
            auto quote = dataManager.getQuote(code);

            if (candles.empty()) continue;

            // 전략 분석
            auto signals = strategyManager.analyzeAll(code, candles, quote);

            // 신호 처리
            for (const auto& signal : signals) {
                if (signal.signal == Signal::BUY) {
                    // 진입 가능 여부 확인
                    int qty = riskManager.calculatePositionSize(signal.price);
                    if (riskManager.canOpenPosition(code, signal.price, qty)) {
                        std::cout << "BUY SIGNAL: " << code << " @ " << signal.price
                                  << " (" << signal.reason << ")" << std::endl;

                        orderExecutor.executeSignal(signal);
                    }
                }
            }

            // 청산 조건 확인
            std::map<std::string, QuoteData> quotes;
            quotes[code] = quote;

            auto closeSignals = strategyManager.checkCloseConditions(
                riskManager.getAllPositions(), quotes);

            for (const auto& closeSignal : closeSignals) {
                std::cout << "CLOSE SIGNAL: " << closeSignal.code << std::endl;
                orderExecutor.executeSignal(closeSignal);
            }
        }

        // 상태 출력 (60초마다)
        if (++loopCount % 60 == 0) {
            printStatus(riskManager, strategyManager);
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // 9. 종료 처리
    std::cout << "\nShutting down..." << std::endl;

    // 모든 포지션 청산
    orderExecutor.closeAllPositions();

    // 서비스 중지
    dataManager.stopRealtime();
    stopLossMonitor.stop();
    orderExecutor.stop();
    api.disconnect();

    // 최종 통계 출력
    std::cout << "\n========== Final Statistics ==========" << std::endl;
    printStatus(riskManager, strategyManager);

    auto trades = riskManager.getTodayTrades();
    std::cout << "Total Trades: " << trades.size() << std::endl;
    std::cout << "Profit Factor: " << riskManager.getProfitFactor() << std::endl;
    std::cout << "Average Win: " << riskManager.getAvgWin() << " KRW" << std::endl;
    std::cout << "Average Loss: " << riskManager.getAvgLoss() << " KRW" << std::endl;

    std::cout << "\nGoodbye!" << std::endl;

    return 0;
}
