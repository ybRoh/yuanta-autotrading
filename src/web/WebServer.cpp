#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define _CRT_SECURE_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

#include "../../include/WebServer.h"
#include "../../include/third_party/httplib.h"
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>

namespace yuanta {

WebServer::WebServer(int port) : port(port) {
}

WebServer::~WebServer() {
    stop();
}

bool WebServer::start() {
    if (running) return true;

    running = true;
    serverWorker = std::thread(&WebServer::serverThread, this);

    return true;
}

void WebServer::stop() {
    running = false;
    if (serverWorker.joinable()) {
        serverWorker.join();
    }
}

void WebServer::updateDashboardData(const DashboardData& data) {
    std::lock_guard<std::mutex> lock(dataMutex);
    // 로그는 유지하면서 다른 데이터만 업데이트
    auto oldLogs = dashboardData.logs;
    dashboardData = data;
    dashboardData.logs = oldLogs;
}

void WebServer::addLog(const TradeLogEntry& entry) {
    std::lock_guard<std::mutex> lock(dataMutex);
    dashboardData.logs.insert(dashboardData.logs.begin(), entry);
    if (dashboardData.logs.size() > MAX_LOGS) {
        dashboardData.logs.resize(MAX_LOGS);
    }
}

void WebServer::addLog(const std::string& type, const std::string& code,
                       const std::string& message, double price,
                       int quantity, double pnl) {
    TradeLogEntry entry;
    entry.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    entry.type = type;
    entry.code = code;
    entry.message = message;
    entry.price = price;
    entry.quantity = quantity;
    entry.pnl = pnl;
    addLog(entry);
}

void WebServer::serverThread() {
    httplib::Server svr;

    // 메인 대시보드 페이지
    svr.Get("/", [this](const httplib::Request&, httplib::Response& res) {
        res.set_content(generateDashboardHtml(), "text/html; charset=utf-8");
    });

    // API 엔드포인트 - JSON 데이터
    svr.Get("/api/status", [this](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content(generateApiResponse(), "application/json; charset=utf-8");
    });

    // 정적 파일 (CSS, JS)
    svr.Get("/style.css", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(R"(
body {
    font-family: 'Segoe UI', Tahoma, sans-serif;
    margin: 0;
    padding: 20px;
    background: #1a1a2e;
    color: #eee;
}
.header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 20px;
    padding: 15px 20px;
    background: #16213e;
    border-radius: 10px;
}
.header h1 { margin: 0; color: #00d9ff; }
.status-badge {
    padding: 8px 16px;
    border-radius: 20px;
    font-weight: bold;
}
.status-running { background: #00c853; color: #000; }
.status-stopped { background: #ff5252; color: #fff; }
.status-simulation { background: #ffc107; color: #000; }
.grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
    gap: 20px;
    margin-bottom: 20px;
}
.card {
    background: #16213e;
    border-radius: 10px;
    padding: 20px;
    box-shadow: 0 4px 6px rgba(0,0,0,0.3);
}
.card h2 {
    margin: 0 0 15px 0;
    color: #00d9ff;
    font-size: 1.1em;
    border-bottom: 1px solid #0f3460;
    padding-bottom: 10px;
}
.stat-row {
    display: flex;
    justify-content: space-between;
    padding: 8px 0;
    border-bottom: 1px solid #0f3460;
}
.stat-row:last-child { border-bottom: none; }
.stat-label { color: #888; }
.stat-value { font-weight: bold; }
.positive { color: #00c853; }
.negative { color: #ff5252; }
.neutral { color: #ffc107; }
table {
    width: 100%;
    border-collapse: collapse;
}
th, td {
    padding: 10px;
    text-align: left;
    border-bottom: 1px solid #0f3460;
}
th {
    color: #00d9ff;
    font-weight: normal;
    font-size: 0.9em;
}
.log-entry {
    padding: 8px 12px;
    margin: 5px 0;
    border-radius: 5px;
    font-size: 0.9em;
    display: flex;
    justify-content: space-between;
}
.log-buy { background: rgba(0,200,83,0.2); border-left: 3px solid #00c853; }
.log-sell { background: rgba(255,82,82,0.2); border-left: 3px solid #ff5252; }
.log-signal { background: rgba(0,217,255,0.2); border-left: 3px solid #00d9ff; }
.log-info { background: rgba(255,255,255,0.1); border-left: 3px solid #888; }
.log-time { color: #888; font-size: 0.85em; }
.refresh-info {
    text-align: center;
    color: #666;
    font-size: 0.8em;
    margin-top: 20px;
}
)", "text/css");
    });

    std::cout << "\n========================================" << std::endl;
    std::cout << "  Web Dashboard started at:" << std::endl;
    std::cout << "  http://localhost:" << port << std::endl;
    std::cout << "========================================\n" << std::endl;

    // 서버 실행 (블로킹)
    while (running) {
        svr.listen("0.0.0.0", port);
    }
}

std::string WebServer::generateApiResponse() {
    std::lock_guard<std::mutex> lock(dataMutex);
    std::ostringstream json;

    json << std::fixed << std::setprecision(0);
    json << "{";

    // 계좌 정보
    json << "\"account\":{";
    json << "\"dailyBudget\":" << dashboardData.dailyBudget << ",";
    json << "\"realizedPnL\":" << dashboardData.realizedPnL << ",";
    json << "\"unrealizedPnL\":" << dashboardData.unrealizedPnL << ",";
    json << "\"totalPnL\":" << dashboardData.totalPnL << ",";
    json << "\"winRate\":" << std::setprecision(1) << dashboardData.winRate << ",";
    json << "\"totalTrades\":" << dashboardData.totalTrades << ",";
    json << "\"winTrades\":" << dashboardData.winTrades << ",";
    json << "\"lossTrades\":" << dashboardData.lossTrades;
    json << "},";

    // 시스템 상태
    json << "\"system\":{";
    json << "\"isRunning\":" << (dashboardData.isRunning ? "true" : "false") << ",";
    json << "\"isMarketOpen\":" << (dashboardData.isMarketOpen ? "true" : "false") << ",";
    json << "\"isSimulationMode\":" << (dashboardData.isSimulationMode ? "true" : "false") << ",";
    json << "\"serverUrl\":\"" << dashboardData.serverUrl << "\",";
    json << "\"uptime\":" << dashboardData.uptime;
    json << "},";

    // 포지션
    json << "\"positions\":[";
    for (size_t i = 0; i < dashboardData.positions.size(); i++) {
        const auto& pos = dashboardData.positions[i];
        if (i > 0) json << ",";
        json << std::setprecision(0);
        json << "{\"code\":\"" << pos.code << "\",";
        json << "\"name\":\"" << pos.name << "\",";
        json << "\"quantity\":" << pos.quantity << ",";
        json << "\"avgPrice\":" << pos.avgPrice << ",";
        json << "\"currentPrice\":" << pos.currentPrice << ",";
        json << "\"pnl\":" << pos.pnl << ",";
        json << "\"pnlRate\":" << std::setprecision(2) << pos.pnlRate << "}";
    }
    json << "],";

    // 시세
    json << "\"quotes\":[";
    for (size_t i = 0; i < dashboardData.quotes.size(); i++) {
        const auto& q = dashboardData.quotes[i];
        if (i > 0) json << ",";
        json << std::setprecision(0);
        json << "{\"code\":\"" << q.code << "\",";
        json << "\"price\":" << q.price << ",";
        json << "\"change\":" << q.change << ",";
        json << "\"changeRate\":" << std::setprecision(2) << q.changeRate << ",";
        json << "\"volume\":" << q.volume << "}";
    }
    json << "],";

    // 전략
    json << "\"strategies\":[";
    for (size_t i = 0; i < dashboardData.strategies.size(); i++) {
        const auto& s = dashboardData.strategies[i];
        if (i > 0) json << ",";
        json << "{\"name\":\"" << s.name << "\",";
        json << "\"enabled\":" << (s.enabled ? "true" : "false") << ",";
        json << "\"signals\":" << s.signals << ",";
        json << "\"trades\":" << s.trades << ",";
        json << "\"pnl\":" << std::setprecision(0) << s.pnl << "}";
    }
    json << "],";

    // 로그
    json << "\"logs\":[";
    for (size_t i = 0; i < dashboardData.logs.size() && i < 20; i++) {
        const auto& log = dashboardData.logs[i];
        if (i > 0) json << ",";
        json << "{\"timestamp\":" << log.timestamp << ",";
        json << "\"type\":\"" << log.type << "\",";
        json << "\"code\":\"" << log.code << "\",";
        json << "\"message\":\"" << log.message << "\",";
        json << "\"price\":" << std::setprecision(0) << log.price << ",";
        json << "\"quantity\":" << log.quantity << ",";
        json << "\"pnl\":" << log.pnl << "}";
    }
    json << "]";

    json << "}";

    return json.str();
}

std::string WebServer::generateDashboardHtml() {
    std::lock_guard<std::mutex> lock(dataMutex);

    std::ostringstream html;
    html << std::fixed;

    html << R"(<!DOCTYPE html>
<html lang="ko">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Yuanta AutoTrading Dashboard</title>
    <link rel="stylesheet" href="/style.css">
</head>
<body>
    <div class="header">
        <h1>📈 Yuanta AutoTrading</h1>
        <div>
            <span class="status-badge )" << (dashboardData.isSimulationMode ? "status-simulation" : "status-running") << R"(">
                )" << (dashboardData.isSimulationMode ? "🔬 시뮬레이션" : "🔴 실거래") << R"(
            </span>
            <span class="status-badge )" << (dashboardData.isRunning ? "status-running" : "status-stopped") << R"(">
                )" << (dashboardData.isRunning ? "● 실행중" : "○ 중지") << R"(
            </span>
        </div>
    </div>

    <div class="grid">
        <!-- 계좌 현황 -->
        <div class="card">
            <h2>💰 계좌 현황</h2>
            <div class="stat-row">
                <span class="stat-label">일일 운영금액</span>
                <span class="stat-value">)" << std::setprecision(0) << dashboardData.dailyBudget << R"( 원</span>
            </div>
            <div class="stat-row">
                <span class="stat-label">실현 손익</span>
                <span class="stat-value )" << (dashboardData.realizedPnL >= 0 ? "positive" : "negative") << R"(">)"
        << (dashboardData.realizedPnL >= 0 ? "+" : "") << dashboardData.realizedPnL << R"( 원</span>
            </div>
            <div class="stat-row">
                <span class="stat-label">평가 손익</span>
                <span class="stat-value )" << (dashboardData.unrealizedPnL >= 0 ? "positive" : "negative") << R"(">)"
        << (dashboardData.unrealizedPnL >= 0 ? "+" : "") << dashboardData.unrealizedPnL << R"( 원</span>
            </div>
            <div class="stat-row">
                <span class="stat-label">총 손익</span>
                <span class="stat-value )" << (dashboardData.totalPnL >= 0 ? "positive" : "negative") << R"(" style="font-size:1.2em">)"
        << (dashboardData.totalPnL >= 0 ? "+" : "") << dashboardData.totalPnL << R"( 원</span>
            </div>
        </div>

        <!-- 거래 통계 -->
        <div class="card">
            <h2>📊 거래 통계</h2>
            <div class="stat-row">
                <span class="stat-label">승률</span>
                <span class="stat-value )" << (dashboardData.winRate >= 50 ? "positive" : "negative") << R"(">)"
        << std::setprecision(1) << dashboardData.winRate << R"(%</span>
            </div>
            <div class="stat-row">
                <span class="stat-label">총 거래</span>
                <span class="stat-value">)" << dashboardData.totalTrades << R"( 건</span>
            </div>
            <div class="stat-row">
                <span class="stat-label">수익 거래</span>
                <span class="stat-value positive">)" << dashboardData.winTrades << R"( 건</span>
            </div>
            <div class="stat-row">
                <span class="stat-label">손실 거래</span>
                <span class="stat-value negative">)" << dashboardData.lossTrades << R"( 건</span>
            </div>
        </div>
    </div>

    <!-- 보유 포지션 -->
    <div class="card" style="margin-bottom:20px">
        <h2>📋 보유 포지션</h2>
        <table>
            <thead>
                <tr>
                    <th>종목코드</th>
                    <th>수량</th>
                    <th>평균단가</th>
                    <th>현재가</th>
                    <th>평가손익</th>
                    <th>수익률</th>
                </tr>
            </thead>
            <tbody id="positions-table">)";

    if (dashboardData.positions.empty()) {
        html << R"(<tr><td colspan="6" style="text-align:center;color:#666">보유 포지션 없음</td></tr>)";
    } else {
        for (const auto& pos : dashboardData.positions) {
            html << "<tr>";
            html << "<td>" << pos.code << "</td>";
            html << "<td>" << pos.quantity << "</td>";
            html << "<td>" << std::setprecision(0) << pos.avgPrice << "</td>";
            html << "<td>" << pos.currentPrice << "</td>";
            html << "<td class=\"" << (pos.pnl >= 0 ? "positive" : "negative") << "\">"
                 << (pos.pnl >= 0 ? "+" : "") << pos.pnl << "</td>";
            html << "<td class=\"" << (pos.pnlRate >= 0 ? "positive" : "negative") << "\">"
                 << (pos.pnlRate >= 0 ? "+" : "") << std::setprecision(2) << pos.pnlRate << "%</td>";
            html << "</tr>";
        }
    }

    html << R"(
            </tbody>
        </table>
    </div>

    <div class="grid">
        <!-- 관심 종목 시세 -->
        <div class="card">
            <h2>📈 관심 종목</h2>
            <table>
                <thead>
                    <tr>
                        <th>종목</th>
                        <th>현재가</th>
                        <th>등락</th>
                        <th>거래량</th>
                    </tr>
                </thead>
                <tbody>)";

    for (const auto& q : dashboardData.quotes) {
        html << "<tr>";
        html << "<td>" << q.code << "</td>";
        html << "<td>" << std::setprecision(0) << q.price << "</td>";
        html << "<td class=\"" << (q.changeRate >= 0 ? "positive" : "negative") << "\">"
             << (q.changeRate >= 0 ? "+" : "") << std::setprecision(2) << q.changeRate << "%</td>";
        html << "<td>" << std::setprecision(0) << q.volume << "</td>";
        html << "</tr>";
    }

    html << R"(
                </tbody>
            </table>
        </div>

        <!-- 전략 현황 -->
        <div class="card">
            <h2>🎯 전략 현황</h2>
            <table>
                <thead>
                    <tr>
                        <th>전략</th>
                        <th>상태</th>
                        <th>신호</th>
                        <th>거래</th>
                    </tr>
                </thead>
                <tbody>)";

    for (const auto& s : dashboardData.strategies) {
        html << "<tr>";
        html << "<td>" << s.name << "</td>";
        html << "<td>" << (s.enabled ? "<span class=\"positive\">활성</span>" : "<span class=\"negative\">비활성</span>") << "</td>";
        html << "<td>" << s.signals << "</td>";
        html << "<td>" << s.trades << "</td>";
        html << "</tr>";
    }

    html << R"(
                </tbody>
            </table>
        </div>
    </div>

    <!-- 거래 로그 -->
    <div class="card">
        <h2>📝 거래 로그</h2>
        <div id="trade-logs">)";

    for (const auto& log : dashboardData.logs) {
        std::string logClass = "log-info";
        if (log.type == "BUY") logClass = "log-buy";
        else if (log.type == "SELL") logClass = "log-sell";
        else if (log.type == "SIGNAL") logClass = "log-signal";

        // 시간 포맷팅
        time_t t = log.timestamp / 1000;
        struct tm* tm_info = localtime(&t);
        char timeStr[20];
        strftime(timeStr, 20, "%H:%M:%S", tm_info);

        html << "<div class=\"log-entry " << logClass << "\">";
        html << "<span>[" << log.type << "] " << log.code << " - " << log.message;
        if (log.price > 0) {
            html << " @ " << std::setprecision(0) << log.price;
        }
        if (log.pnl != 0) {
            html << " (P&L: " << (log.pnl >= 0 ? "+" : "") << log.pnl << ")";
        }
        html << "</span>";
        html << "<span class=\"log-time\">" << timeStr << "</span>";
        html << "</div>";
    }

    if (dashboardData.logs.empty()) {
        html << R"(<div class="log-entry log-info">거래 로그가 없습니다.</div>)";
    }

    html << R"(
        </div>
    </div>

    <div class="refresh-info">
        페이지는 5초마다 자동 새로고침됩니다 |
        <a href="/api/status" style="color:#00d9ff">API JSON</a>
    </div>

    <script>
        // 5초마다 자동 새로고침
        setTimeout(function() {
            location.reload();
        }, 5000);

        // 또는 AJAX로 부분 업데이트
        // setInterval(async () => {
        //     const res = await fetch('/api/status');
        //     const data = await res.json();
        //     // 데이터 업데이트 로직
        // }, 2000);
    </script>
</body>
</html>)";

    return html.str();
}

} // namespace yuanta
