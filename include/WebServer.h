#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <mutex>
#include <vector>
#include <map>

namespace yuanta {

// 거래 로그 항목
struct TradeLogEntry {
    long long timestamp;
    std::string type;       // "BUY", "SELL", "SIGNAL"
    std::string code;
    std::string message;
    double price;
    int quantity;
    double pnl;
};

// 대시보드 상태 데이터
struct DashboardData {
    // 계좌 정보
    double dailyBudget = 0;
    double realizedPnL = 0;
    double unrealizedPnL = 0;
    double totalPnL = 0;
    double winRate = 0;
    int totalTrades = 0;
    int winTrades = 0;
    int lossTrades = 0;

    // 포지션 정보
    struct Position {
        std::string code;
        std::string name;
        int quantity;
        double avgPrice;
        double currentPrice;
        double pnl;
        double pnlRate;
    };
    std::vector<Position> positions;

    // 시세 정보
    struct Quote {
        std::string code;
        double price;
        double change;
        double changeRate;
        long long volume;
    };
    std::vector<Quote> quotes;

    // 전략 정보
    struct StrategyStatus {
        std::string name;
        bool enabled;
        int signals;
        int trades;
        double pnl;
    };
    std::vector<StrategyStatus> strategies;

    // 시스템 상태
    bool isRunning = false;
    bool isMarketOpen = false;
    bool isSimulationMode = true;
    std::string serverUrl;
    long long uptime = 0;

    // 거래 로그
    std::vector<TradeLogEntry> logs;
};

class WebServer {
public:
    WebServer(int port = 8080);
    ~WebServer();

    // 서버 시작/중지
    bool start();
    void stop();
    bool isRunning() const { return running; }

    // 데이터 업데이트 (메인 스레드에서 호출)
    void updateDashboardData(const DashboardData& data);

    // 로그 추가
    void addLog(const TradeLogEntry& entry);
    void addLog(const std::string& type, const std::string& code,
                const std::string& message, double price = 0,
                int quantity = 0, double pnl = 0);

    // 포트 설정
    void setPort(int port) { this->port = port; }
    int getPort() const { return port; }

    // 명령 처리
    using CommandCallback = std::function<void(const std::string&)>;
    void setCommandCallback(CommandCallback callback) { commandCallback = callback; }
    void checkCommands();  // 명령 파일 확인 (메인 루프에서 호출)

    // 거래 상태
    void setTradingActive(bool active) { tradingActive = active; }
    bool isTradingActive() const { return tradingActive; }

private:
    void serverThread();
    std::string generateDashboardHtml();
    std::string generateApiResponse();

    CommandCallback commandCallback;
    std::atomic<bool> tradingActive{false};

    int port;
    std::atomic<bool> running{false};
    std::thread serverWorker;

    DashboardData dashboardData;
    mutable std::mutex dataMutex;

    static const int MAX_LOGS = 100;
};

} // namespace yuanta

#endif // WEB_SERVER_H
