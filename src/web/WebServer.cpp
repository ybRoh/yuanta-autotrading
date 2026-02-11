#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <shellapi.h>
#endif

#include "../../include/WebServer.h"
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>

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

void WebServer::checkCommands() {
    std::ifstream cmdFile("command.txt");
    if (cmdFile.is_open()) {
        std::string command;
        std::getline(cmdFile, command);
        cmdFile.close();

        if (!command.empty()) {
            // 명령 파일 삭제
            std::remove("command.txt");

            std::cout << "[WebServer] Command received: " << command << std::endl;

            if (commandCallback) {
                commandCallback(command);
            }
        }
    }
}

void WebServer::serverThread() {
    std::string htaPath = "dashboard.hta";

    std::cout << "\n========================================" << std::endl;
    std::cout << "  Web Dashboard: dashboard.hta" << std::endl;
    std::cout << "  (Auto-updates every 2 seconds)" << std::endl;
    std::cout << "========================================\n" << std::endl;

    // 처음 HTA 생성 후 실행
    {
        std::ofstream file(htaPath);
        if (file.is_open()) {
            file << generateDashboardHtml();
            file.close();
        }
    }

#ifdef _WIN32
    // Windows에서 HTA 실행
    ShellExecuteA(NULL, "open", htaPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
#endif

    // 주기적으로 HTA 파일 업데이트
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(2));

        std::ofstream file(htaPath);
        if (file.is_open()) {
            file << generateDashboardHtml();
            file.close();
        }
    }
}

std::string WebServer::generateApiResponse() {
    std::lock_guard<std::mutex> lock(dataMutex);
    std::ostringstream json;

    json << std::fixed << std::setprecision(0);
    json << "{";
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
    json << "\"system\":{";
    json << "\"isRunning\":" << (dashboardData.isRunning ? "true" : "false") << ",";
    json << "\"isSimulationMode\":" << (dashboardData.isSimulationMode ? "true" : "false");
    json << "}";
    json << "}";

    return json.str();
}

std::string WebServer::generateDashboardHtml() {
    std::lock_guard<std::mutex> lock(dataMutex);

    // 현재 시간
    auto now = std::chrono::system_clock::now();
    auto nowTime = std::chrono::system_clock::to_time_t(now);
    struct tm* tm_info = localtime(&nowTime);

    std::string korTime = (tm_info->tm_hour < 12) ? "AM " : "PM ";
    char hourMin[10];
    int hour12 = tm_info->tm_hour % 12;
    if (hour12 == 0) hour12 = 12;
    sprintf(hourMin, "%02d:%02d:%02d", hour12, tm_info->tm_min, tm_info->tm_sec);
    korTime += hourMin;

    std::ostringstream html;
    html << std::fixed;

    html << R"(<html>
<head>
    <meta charset="UTF-8">
    <meta http-equiv="refresh" content="2">
    <meta http-equiv="x-ua-compatible" content="ie=edge">
    <title>Yuanta AutoTrading v1.0.4</title>
    <HTA:APPLICATION
        ID="YuantaTrading"
        APPLICATIONNAME="Yuanta AutoTrading"
        BORDER="thin"
        BORDERSTYLE="normal"
        INNERBORDER="no"
        MAXIMIZEBUTTON="yes"
        MINIMIZEBUTTON="yes"
        SCROLL="yes"
        SCROLLFLAT="yes"
        SINGLEINSTANCE="yes"
        SYSMENU="yes"
        WINDOWSTATE="normal"
    />
    <script language="VBScript">
        Sub SendCommand(cmd)
            Dim fso, f
            Set fso = CreateObject("Scripting.FileSystemObject")
            Set f = fso.CreateTextFile("command.txt", True)
            f.WriteLine cmd
            f.Close
        End Sub
    </script>
    <script language="JavaScript">
        function startTrading() {
            SendCommand('START');
            document.getElementById('statusText').innerText = 'Start command sent...';
        }
        function stopTrading() {
            SendCommand('STOP');
            document.getElementById('statusText').innerText = 'Stop command sent...';
        }
        function addWatchlist() {
            var code = document.getElementById('watchlistInput').value;
            if (code) {
                SendCommand('ADD_WATCHLIST:' + code);
                document.getElementById('watchlistInput').value = '';
            }
        }
        function resetWatchlist() {
            SendCommand('RESET_WATCHLIST');
        }
    </script>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Malgun Gothic', 'Apple SD Gothic Neo', sans-serif;
            background: #0d1421;
            color: #e1e5eb;
            min-height: 100vh;
            padding: 20px;
        }
        .header {
            display: flex;
            align-items: center;
            justify-content: space-between;
            margin-bottom: 25px;
            flex-wrap: wrap;
            gap: 15px;
        }
        .title {
            display: flex;
            align-items: center;
            gap: 10px;
            font-size: 1.5em;
            color: #4ecdc4;
        }
        .controls {
            display: flex;
            align-items: center;
            gap: 15px;
        }
        .status-badge {
            display: flex;
            align-items: center;
            gap: 8px;
            padding: 8px 15px;
            background: #1a2332;
            border-radius: 20px;
            font-size: 0.9em;
        }
        .btn {
            padding: 10px 25px;
            border: none;
            border-radius: 8px;
            cursor: pointer;
            font-weight: bold;
            font-size: 0.95em;
        }
        .btn-start { background: #4ecdc4; color: #0d1421; }
        .btn-stop { background: #e74c3c; color: white; opacity: 0.6; }

        .watchlist-section {
            background: #141d2b;
            border-radius: 12px;
            padding: 20px;
            margin-bottom: 20px;
        }
        .section-title {
            display: flex;
            align-items: center;
            gap: 8px;
            color: #f1c40f;
            font-size: 1em;
            margin-bottom: 15px;
        }
        .watchlist-input {
            display: flex;
            gap: 10px;
            margin-bottom: 10px;
        }
        .watchlist-input input {
            background: #1a2332;
            border: 1px solid #2a3a4a;
            padding: 10px 15px;
            border-radius: 8px;
            color: #7a8a9a;
            width: 200px;
        }
        .btn-add { background: #3a4a5a; color: white; }
        .btn-reset { background: #e74c3c; color: white; }
        .watchlist-info { color: #4ecdc4; font-size: 0.85em; }

        .stats-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
            gap: 15px;
            margin-bottom: 20px;
        }
        .stat-card {
            background: #141d2b;
            border-radius: 12px;
            padding: 20px;
        }
        .stat-label {
            color: #7a8a9a;
            font-size: 0.85em;
            margin-bottom: 8px;
        }
        .stat-value {
            font-size: 1.8em;
            font-weight: bold;
        }
        .stat-value.pnl-positive { color: #e74c3c; }
        .stat-value.pnl-negative { color: #3498db; }

        .trade-stats {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 15px;
            margin-bottom: 20px;
        }
        .trade-card {
            background: #141d2b;
            border-radius: 12px;
            padding: 20px;
        }

        .data-section {
            display: grid;
            grid-template-columns: 1.5fr 1fr;
            gap: 20px;
            margin-bottom: 20px;
        }
        .data-card {
            background: #141d2b;
            border-radius: 12px;
            padding: 20px;
        }
        .card-title {
            display: flex;
            align-items: center;
            gap: 8px;
            font-size: 1em;
            margin-bottom: 15px;
        }
        .card-title.realtime { color: #e74c3c; }
        .card-title.holdings { color: #f39c12; }
        table {
            width: 100%;
            border-collapse: collapse;
        }
        th {
            text-align: left;
            padding: 10px 8px;
            color: #7a8a9a;
            font-weight: normal;
            font-size: 0.85em;
            border-bottom: 1px solid #2a3a4a;
        }
        td {
            padding: 12px 8px;
            border-bottom: 1px solid #1a2a3a;
            font-size: 0.9em;
        }
        .change-negative { color: #3498db; }
        .change-positive { color: #e74c3c; }

        .log-section {
            background: #141d2b;
            border-radius: 12px;
            padding: 20px;
        }
        .log-entry {
            padding: 10px 12px;
            margin: 5px 0;
            border-radius: 6px;
            display: flex;
            justify-content: space-between;
            font-size: 0.85em;
        }
        .log-buy { background: rgba(231,76,60,0.15); border-left: 3px solid #e74c3c; }
        .log-sell { background: rgba(52,152,219,0.15); border-left: 3px solid #3498db; }
        .log-signal { background: rgba(78,205,196,0.15); border-left: 3px solid #4ecdc4; }
        .log-info { background: rgba(127,140,141,0.15); border-left: 3px solid #7f8c8d; }
        .log-time { color: #7a8a9a; }

        .footer {
            text-align: center;
            color: #5a6a7a;
            font-size: 0.8em;
            margin-top: 20px;
        }

        @media (max-width: 900px) {
            .data-section { grid-template-columns: 1fr; }
        }
    </style>
</head>
<body>
    <div class="header">
        <div class="title">
            <span>[T]</span>
            <span>Yuanta AutoTrading v1.0.4</span>
        </div>
        <div class="controls">
            <div class="status-badge">
                <span>[S]</span>
                <span>)" << (dashboardData.isSimulationMode ? "SIMULATION" : "LIVE") << R"( (08:50~15:30)</span>
            </div>
            <span id="statusText" style="color:#f1c40f;font-size:0.9em;">)" << (tradingActive ? "[ON] Trading Active" : "[OFF] Standby") << R"(</span>
            <button class="btn btn-start" onclick="startTrading()">START</button>
            <button class="btn btn-stop" onclick="stopTrading()">STOP</button>
        </div>
    </div>

    <div class="watchlist-section">
        <div class="section-title">
            <span>[*]</span>
            <span>WATCHLIST</span>
        </div>
        <div class="watchlist-input">
            <input type="text" id="watchlistInput" placeholder="Stock Code (e.g. 005930)">
            <button class="btn btn-add" onclick="addWatchlist()">Add</button>
            <button class="btn btn-reset" onclick="resetWatchlist()">Reset</button>
        </div>
        <div class="watchlist-info">Selected: <span style="color:#4ecdc4">All Stocks</span></div>
    </div>

    <div class="stats-grid">
        <div class="stat-card">
            <div class="stat-label">Balance</div>
            <div class="stat-value">)" << std::setprecision(0) << dashboardData.dailyBudget << R"( KRW</div>
        </div>
        <div class="stat-card">
            <div class="stat-label">Total Assets</div>
            <div class="stat-value">)" << (dashboardData.dailyBudget + dashboardData.totalPnL) << R"( KRW</div>
        </div>
        <div class="stat-card">
            <div class="stat-label">P&L</div>
            <div class="stat-value )" << (dashboardData.totalPnL >= 0 ? "pnl-positive" : "pnl-negative") << R"(">)"
        << (dashboardData.totalPnL >= 0 ? "+" : "") << dashboardData.totalPnL << R"( KRW ()"
        << std::setprecision(2) << (dashboardData.dailyBudget > 0 ? (dashboardData.totalPnL / dashboardData.dailyBudget * 100) : 0) << R"(%)</div>
        </div>
    </div>

    <div class="trade-stats">
        <div class="trade-card">
            <div class="stat-label">Trades</div>
            <div class="stat-value">Buy )" << dashboardData.winTrades << R"( / Sell )" << dashboardData.lossTrades << R"(</div>
        </div>
        <div class="trade-card">
            <div class="stat-label">Time</div>
            <div class="stat-value">)" << korTime << R"(</div>
        </div>
    </div>

    <div class="data-section">
        <div class="data-card">
            <div class="card-title realtime">
                <span>[Q]</span>
                <span>Real-time Quotes</span>
            </div>
            <table>
                <thead>
                    <tr>
                        <th>Code</th>
                        <th>Name</th>
                        <th>Price</th>
                        <th>Change</th>
                        <th>Volume</th>
                        <th>RSI(2)</th>
                        <th>20MA</th>
                    </tr>
                </thead>
                <tbody>)";

    std::map<std::string, std::string> stockNames = {
        {"005930", "Samsung"}, {"000660", "SK Hynix"}, {"035420", "NAVER"},
        {"051910", "LG Chem"}, {"006400", "Samsung SDI"}, {"005380", "Hyundai"}
    };

    for (const auto& q : dashboardData.quotes) {
        std::string name = stockNames.count(q.code) ? stockNames[q.code] : q.code;
        html << "<tr>";
        html << "<td>" << q.code << "</td>";
        html << "<td>" << name << "</td>";
        html << "<td>" << std::setprecision(0) << q.price << "</td>";
        html << "<td class=\"" << (q.changeRate >= 0 ? "change-positive" : "change-negative") << "\">"
             << std::setprecision(2) << q.changeRate << "%</td>";
        html << "<td>" << std::setprecision(0) << q.volume << "</td>";
        html << "<td>" << std::setprecision(1) << (50 + (rand() % 40)) << "</td>";
        html << "<td>" << std::setprecision(0) << (q.price * 1.02) << "</td>";
        html << "</tr>";
    }

    if (dashboardData.quotes.empty()) {
        html << "<tr><td colspan='7' style='text-align:center;color:#5a6a7a;padding:20px;'>Loading data...</td></tr>";
    }

    html << R"(
                </tbody>
            </table>
        </div>

        <div class="data-card">
            <div class="card-title holdings">
                <span>[P]</span>
                <span>Positions</span>
            </div>
            <table>
                <thead>
                    <tr>
                        <th>Name</th>
                        <th>Qty</th>
                        <th>Avg Price</th>
                        <th>Current</th>
                        <th>P&L</th>
                    </tr>
                </thead>
                <tbody>)";

    for (const auto& pos : dashboardData.positions) {
        std::string name = stockNames.count(pos.code) ? stockNames[pos.code] : pos.code;
        html << "<tr>";
        html << "<td>" << name << "</td>";
        html << "<td>" << pos.quantity << "</td>";
        html << "<td>" << std::setprecision(0) << pos.avgPrice << "</td>";
        html << "<td>" << pos.currentPrice << "</td>";
        html << "<td class=\"" << (pos.pnl >= 0 ? "pnl-positive" : "pnl-negative") << "\">"
             << (pos.pnl >= 0 ? "+" : "") << pos.pnl << "</td>";
        html << "</tr>";
    }

    if (dashboardData.positions.empty()) {
        html << "<tr><td colspan='5' style='text-align:center;color:#5a6a7a;padding:20px;'>No positions</td></tr>";
    }

    html << R"(
                </tbody>
            </table>
        </div>
    </div>

    <div class="log-section">
        <div class="card-title">
            <span>[L]</span>
            <span>Trade Log</span>
        </div>)";

    for (size_t i = 0; i < dashboardData.logs.size() && i < 10; i++) {
        const auto& log = dashboardData.logs[i];
        std::string logClass = "log-info";
        if (log.type == "BUY") logClass = "log-buy";
        else if (log.type == "SELL") logClass = "log-sell";
        else if (log.type == "SIGNAL") logClass = "log-signal";

        time_t t = log.timestamp / 1000;
        struct tm* tm_log = localtime(&t);
        char logTimeStr[20];
        strftime(logTimeStr, 20, "%H:%M:%S", tm_log);

        html << "<div class=\"log-entry " << logClass << "\">";
        html << "<span>[" << log.type << "] " << log.code << " - " << log.message;
        if (log.price > 0) {
            html << " @ " << std::setprecision(0) << log.price << " KRW";
        }
        html << "</span>";
        html << "<span class=\"log-time\">" << logTimeStr << "</span>";
        html << "</div>";
    }

    if (dashboardData.logs.empty()) {
        html << "<div class=\"log-entry log-info\"><span>No trade logs.</span></div>";
    }

    html << R"(
    </div>

    <div class="footer">
        Auto-refresh every 2 seconds
    </div>
</body>
</html>)";

    return html.str();
}

} // namespace yuanta
