#include "../include/YuantaAPI.h"
#include "../include/RiskManager.h"
#include "../include/Strategy.h"
#include "../include/MarketDataManager.h"
#include "../include/OrderExecutor.h"
#include "../include/TechnicalIndicators.h"
#include "../include/WebServer.h"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <signal.h>
#include <thread>
#include <chrono>
#include <atomic>

#include <sstream>
#include <map>

using namespace yuanta;

// 전역 상태
std::atomic<bool> running{true};
WebServer* g_webServer = nullptr;

// 시그널 핸들러
void signalHandler(int signum) {
    std::cout << "\nShutdown signal received..." << std::endl;
    running = false;
}

// 간단한 설정 로더
struct AppConfig {
    // API 설정
    std::string apiServer = "simul.tradar.api.com";
    std::string dllPath = "";
    std::string userId = "";
    std::string userPassword = "";
    std::string certPassword = "";

    // 웹 대시보드 설정
    int webPort = 8080;
    bool enableWebDashboard = true;

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
            std::cout << "Config file not found: " << filepath << ", using defaults" << std::endl;
            return false;
        }

        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#' || line[0] == ';') continue;

            size_t pos = line.find('=');
            if (pos == std::string::npos) continue;

            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);

            while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
            while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.erase(0, 1);

            if (key == "apiServer") apiServer = value;
            else if (key == "dllPath") dllPath = value;
            else if (key == "userId") userId = value;
            else if (key == "userPassword") userPassword = value;
            else if (key == "certPassword") certPassword = value;
            else if (key == "webPort") webPort = std::stoi(value);
            else if (key == "enableWebDashboard") enableWebDashboard = (value == "true" || value == "1");
            else if (key == "dailyBudget") dailyBudget = std::stod(value);
            else if (key == "maxPositionRatio") maxPositionRatio = std::stod(value);
            else if (key == "maxDailyLossRatio") maxDailyLossRatio = std::stod(value);
            else if (key == "maxConcurrentPositions") maxConcurrentPositions = std::stoi(value);
            else if (key == "enableGapPullback") enableGapPullback = (value == "true" || value == "1");
            else if (key == "enableMABreakout") enableMABreakout = (value == "true" || value == "1");
            else if (key == "enableBBSqueeze") enableBBSqueeze = (value == "true" || value == "1");
            else if (key == "watchlist") {
                std::stringstream ss(value);
                std::string code;
                while (std::getline(ss, code, ',')) {
                    while (!code.empty() && code.front() == ' ') code.erase(0, 1);
                    while (!code.empty() && code.back() == ' ') code.pop_back();
                    if (!code.empty()) {
                        watchlist.push_back(code);
                    }
                }
            }
        }

        std::cout << "Config loaded from: " << filepath << std::endl;
        return true;
    }
};

// 대시보드 데이터 업데이트 함수
void updateDashboard(WebServer& webServer, RiskManager& rm, StrategyManager& sm,
                     MarketDataManager& dm, YuantaAPI& api, const AppConfig& config,
                     long long startTime) {
    DashboardData data;

    // 계좌 정보
    data.dailyBudget = config.dailyBudget;
    data.realizedPnL = rm.getRealizedPnL();
    data.unrealizedPnL = rm.getUnrealizedPnL();
    data.totalPnL = rm.getTotalPnL();
    data.winRate = rm.getWinRate();

    auto trades = rm.getTodayTrades();
    data.totalTrades = static_cast<int>(trades.size());
    data.winTrades = 0;
    data.lossTrades = 0;
    for (const auto& trade : trades) {
        if (trade.pnl > 0) data.winTrades++;
        else if (trade.pnl < 0) data.lossTrades++;
    }

    // 시스템 상태
    data.isRunning = running;
    data.isMarketOpen = dm.isMarketOpen();
    data.isSimulationMode = api.isSimulationMode();
    data.serverUrl = config.apiServer;
    data.uptime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() - startTime;

    // 포지션 정보
    auto positions = rm.getAllPositions();
    for (const auto& pos : positions) {
        DashboardData::Position p;
        p.code = pos.code;
        p.name = pos.code;  // TODO: 종목명 조회
        p.quantity = pos.quantity;
        p.avgPrice = pos.avgPrice;
        p.currentPrice = pos.currentPrice;
        p.pnl = (pos.currentPrice - pos.avgPrice) * pos.quantity;
        p.pnlRate = ((pos.currentPrice - pos.avgPrice) / pos.avgPrice) * 100;
        data.positions.push_back(p);
    }

    // 시세 정보
    for (const auto& code : config.watchlist) {
        auto quote = dm.getQuote(code);
        DashboardData::Quote q;
        q.code = code;
        q.price = quote.currentPrice;
        q.change = quote.currentPrice - quote.prevClose;
        q.changeRate = quote.changeRate;
        q.volume = quote.volume;
        data.quotes.push_back(q);
    }

    // 전략 정보
    DashboardData::StrategyStatus s1;
    s1.name = "Gap Pullback";
    s1.enabled = config.enableGapPullback;
    s1.signals = 0;
    s1.trades = 0;
    s1.pnl = 0;
    data.strategies.push_back(s1);

    DashboardData::StrategyStatus s2;
    s2.name = "MA Breakout";
    s2.enabled = config.enableMABreakout;
    s2.signals = 0;
    s2.trades = 0;
    s2.pnl = 0;
    data.strategies.push_back(s2);

    DashboardData::StrategyStatus s3;
    s3.name = "BB Squeeze";
    s3.enabled = config.enableBBSqueeze;
    s3.signals = 0;
    s3.trades = 0;
    s3.pnl = 0;
    data.strategies.push_back(s3);

    webServer.updateDashboardData(data);
}

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
    std::cout << "  with Web Dashboard" << std::endl;
    std::cout << "========================================\n" << std::endl;

    // 시그널 핸들러 설정
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // 시작 시간 기록
    long long startTime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    // 설정 로드
    AppConfig config;
    config.loadFromFile("config/settings.ini");

    // 기본 관심 종목
    if (config.watchlist.empty()) {
        config.watchlist = {"005930", "000660", "035420", "051910", "006400"};
    }

    // 1. 리스크 매니저 초기화
    DailyBudgetConfig budgetConfig;
    budgetConfig.dailyBudget = config.dailyBudget;
    budgetConfig.maxPositionRatio = config.maxPositionRatio;
    budgetConfig.maxDailyLossRatio = config.maxDailyLossRatio;
    budgetConfig.maxConcurrentPositions = config.maxConcurrentPositions;

    RiskManager riskManager(budgetConfig);

    std::cout << "Risk Manager initialized:" << std::endl;
    std::cout << "  - Daily Budget: " << std::fixed << std::setprecision(0)
              << budgetConfig.dailyBudget << " KRW" << std::endl;
    std::cout << "  - Max Position: " << budgetConfig.getMaxPositionSize() << " KRW" << std::endl;
    std::cout << "  - Max Daily Loss: " << budgetConfig.getMaxDailyLoss() << " KRW" << std::endl;
    std::cout << "  - Max Positions: " << budgetConfig.maxConcurrentPositions << std::endl;
    std::cout << std::endl;

    // 2. API 초기화
    YuantaAPI api;
    std::cout << "Initializing Yuanta API..." << std::endl;

    if (!api.initialize(config.dllPath)) {
        std::cerr << "Critical error: Failed to initialize API" << std::endl;
        return 1;
    }

    std::cout << "Connecting to server: " << config.apiServer << std::endl;
    if (!api.connect(config.apiServer)) {
        std::cerr << "Failed to connect to server" << std::endl;
        if (!api.isSimulationMode()) {
            return 1;
        }
    }

    if (!config.userId.empty()) {
        std::cout << "Logging in as: " << config.userId << std::endl;
        if (!api.login(config.userId, config.userPassword, config.certPassword)) {
            std::cerr << "Login failed" << std::endl;
            if (!api.isSimulationMode()) {
                return 1;
            }
        }
    } else if (api.isSimulationMode()) {
        std::cout << "No login credentials - running in demo mode" << std::endl;
    }

    if (api.isSimulationMode()) {
        std::cout << "\n*** SIMULATION MODE - No real trading ***\n" << std::endl;
    } else {
        std::cout << "\n*** LIVE MODE - Real trading enabled ***\n" << std::endl;
    }

    // 3. 시세 데이터 매니저 초기화
    MarketDataManager dataManager;
    dataManager.setAPI(&api);

    std::cout << "Loading market data for watchlist:" << std::endl;
    for (const auto& code : config.watchlist) {
        std::cout << "  - " << code;
        dataManager.addWatchlist(code);
        dataManager.loadHistoricalData(code, 60);
        auto dailyCandles = dataManager.getDailyCandles(code, 60);
        auto minuteCandles = dataManager.getMinuteCandles(code, 1, 100);

        std::cout << " (Daily: " << dailyCandles.size()
                  << ", Minute: " << minuteCandles.size() << ")" << std::endl;
    }
    std::cout << std::endl;

    // 4. 전략 매니저 초기화
    StrategyManager strategyManager;
    strategyManager.setRiskManager(&riskManager);

    std::cout << "Strategies:" << std::endl;
    if (config.enableGapPullback) {
        strategyManager.addStrategy(std::make_unique<GapPullbackStrategy>());
        std::cout << "  - Gap Pullback (Expected win rate: 65-70%)" << std::endl;
    }

    if (config.enableMABreakout) {
        strategyManager.addStrategy(std::make_unique<MABreakoutStrategy>());
        std::cout << "  - MA Breakout (Expected win rate: 55-60%)" << std::endl;
    }

    if (config.enableBBSqueeze) {
        strategyManager.addStrategy(std::make_unique<BBSqueezeStrategy>());
        std::cout << "  - BB Squeeze (Expected win rate: 60-65%)" << std::endl;
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

    dataManager.setQuoteUpdateCallback([&](const std::string& code, const QuoteData& quote) {
        stopLossMonitor.onQuoteUpdate(code, quote);
    });

    // 7. 웹 대시보드 시작
    WebServer webServer(config.webPort);
    g_webServer = &webServer;

    if (config.enableWebDashboard) {
        webServer.start();
        webServer.addLog("INFO", "", "System started", 0, 0, 0);
    }

    // 8. 실시간 시세 시작
    dataManager.startRealtime();

    std::cout << "========================================" << std::endl;
    std::cout << "System started. Press Ctrl+C to stop." << std::endl;
    if (config.enableWebDashboard) {
        std::cout << "Web Dashboard: http://localhost:" << config.webPort << std::endl;
    }
    std::cout << "========================================\n" << std::endl;

    // 9. 메인 루프
    int loopCount = 0;
    while (running) {
        // 장 시간 체크
        if (!api.isSimulationMode() && !dataManager.isMarketOpen()) {
            if (loopCount % 60 == 0) {
                std::cout << "Market closed. Waiting..." << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::seconds(60));
            loopCount++;
            continue;
        }

        // 일일 손실 한도 체크
        if (riskManager.isDailyLossLimitReached()) {
            std::cout << "Daily loss limit reached. Closing all positions..." << std::endl;
            webServer.addLog("ALERT", "", "Daily loss limit reached", 0, 0, riskManager.getTotalPnL());
            orderExecutor.closeAllPositions();
            std::this_thread::sleep_for(std::chrono::seconds(60));
            continue;
        }

        // 강제 청산 시간 체크
        if (!api.isSimulationMode() && riskManager.shouldForceClose()) {
            std::cout << "Market close approaching. Closing all positions..." << std::endl;
            webServer.addLog("ALERT", "", "Force close time reached", 0, 0, 0);
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
                    int qty = riskManager.calculatePositionSize(signal.price);
                    if (riskManager.canOpenPosition(code, signal.price, qty)) {
                        std::cout << "[" << code << "] BUY SIGNAL @ "
                                  << std::fixed << std::setprecision(0) << signal.price
                                  << " (" << signal.reason << ")" << std::endl;

                        webServer.addLog("SIGNAL", code, signal.reason, signal.price, qty, 0);

                        if (api.isSimulationMode()) {
                            std::cout << "  -> Simulated buy: " << qty << " shares" << std::endl;
                        }
                        orderExecutor.executeSignal(signal);
                        webServer.addLog("BUY", code, "Order executed", signal.price, qty, 0);
                    }
                }
            }

            // 청산 조건 확인
            std::map<std::string, QuoteData> quotes;
            quotes[code] = quote;

            auto closeSignals = strategyManager.checkCloseConditions(
                riskManager.getAllPositions(), quotes);

            for (const auto& closeSignal : closeSignals) {
                std::cout << "[" << closeSignal.code << "] CLOSE SIGNAL" << std::endl;
                webServer.addLog("SELL", closeSignal.code, "Position closed", closeSignal.price, 0, 0);
                orderExecutor.executeSignal(closeSignal);
            }
        }

        // 대시보드 업데이트 (2초마다)
        if (loopCount % 2 == 0) {
            updateDashboard(webServer, riskManager, strategyManager, dataManager, api, config, startTime);
        }

        // 콘솔 상태 출력 (30초마다)
        if (++loopCount % 30 == 0) {
            printStatus(riskManager, strategyManager);
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // 10. 종료 처리
    std::cout << "\nShutting down..." << std::endl;
    webServer.addLog("INFO", "", "System shutting down", 0, 0, 0);

    orderExecutor.closeAllPositions();
    dataManager.stopRealtime();
    stopLossMonitor.stop();
    orderExecutor.stop();
    webServer.stop();
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
