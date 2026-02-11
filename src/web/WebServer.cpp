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
            std::remove("command.txt");
            std::cout << "[WebServer] Command received: " << command << std::endl;
            if (commandCallback) {
                commandCallback(command);
            }
        }
    }
}

void WebServer::serverThread() {
    std::string htmlPath = "dashboard.html";

    std::cout << "\n========================================" << std::endl;
    std::cout << "  Web Dashboard: dashboard.html" << std::endl;
    std::cout << "  Press F5 to refresh in browser" << std::endl;
    std::cout << "========================================\n" << std::endl;

    {
        std::ofstream file(htmlPath);
        if (file.is_open()) {
            file << generateDashboardHtml();
            file.close();
        }
    }

#ifdef _WIN32
    ShellExecuteA(NULL, "open", htmlPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
#endif

    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(2));

        std::ofstream file(htmlPath);
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

    auto now = std::chrono::system_clock::now();
    auto nowTime = std::chrono::system_clock::to_time_t(now);
    struct tm* tm_info = localtime(&nowTime);

    std::string timeStr = (tm_info->tm_hour < 12) ? "AM " : "PM ";
    char hourMin[10];
    int hour12 = tm_info->tm_hour % 12;
    if (hour12 == 0) hour12 = 12;
    sprintf(hourMin, "%02d:%02d:%02d", hour12, tm_info->tm_min, tm_info->tm_sec);
    timeStr += hourMin;

    std::ostringstream html;
    html << std::fixed;

    // HTML header
    html << "<!DOCTYPE html>\n";
    html << "<html lang=\"en\">\n";
    html << "<head>\n";
    html << "    <meta charset=\"UTF-8\">\n";
    html << "    <meta http-equiv=\"refresh\" content=\"2\">\n";
    html << "    <title>Yuanta AutoTrading v1.0.4</title>\n";
    html << "    <style>\n";
    html << "        * { margin: 0; padding: 0; box-sizing: border-box; }\n";
    html << "        body { font-family: Arial, sans-serif; background: #0d1421; color: #e1e5eb; padding: 20px; }\n";
    html << "        .header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px; flex-wrap: wrap; gap: 10px; }\n";
    html << "        .title { font-size: 1.5em; color: #4ecdc4; }\n";
    html << "        .status { padding: 8px 15px; background: #1a2332; border-radius: 20px; }\n";
    html << "        .trading-active { color: #2ecc71; }\n";
    html << "        .trading-inactive { color: #e74c3c; }\n";
    html << "        .stats-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 15px; margin-bottom: 20px; }\n";
    html << "        .stat-card { background: #141d2b; border-radius: 12px; padding: 20px; }\n";
    html << "        .stat-label { color: #7a8a9a; font-size: 0.85em; margin-bottom: 8px; }\n";
    html << "        .stat-value { font-size: 1.5em; font-weight: bold; }\n";
    html << "        .positive { color: #e74c3c; }\n";
    html << "        .negative { color: #3498db; }\n";
    html << "        .data-section { display: grid; grid-template-columns: 1.5fr 1fr; gap: 20px; margin-bottom: 20px; }\n";
    html << "        .data-card { background: #141d2b; border-radius: 12px; padding: 20px; }\n";
    html << "        .card-title { color: #f1c40f; margin-bottom: 15px; }\n";
    html << "        table { width: 100%; border-collapse: collapse; }\n";
    html << "        th { text-align: left; padding: 10px 8px; color: #7a8a9a; font-weight: normal; border-bottom: 1px solid #2a3a4a; }\n";
    html << "        td { padding: 12px 8px; border-bottom: 1px solid #1a2a3a; }\n";
    html << "        .log-section { background: #141d2b; border-radius: 12px; padding: 20px; }\n";
    html << "        .log-entry { padding: 8px 12px; margin: 5px 0; border-radius: 6px; display: flex; justify-content: space-between; }\n";
    html << "        .log-buy { background: rgba(231,76,60,0.15); border-left: 3px solid #e74c3c; }\n";
    html << "        .log-sell { background: rgba(52,152,219,0.15); border-left: 3px solid #3498db; }\n";
    html << "        .log-info { background: rgba(127,140,141,0.15); border-left: 3px solid #7f8c8d; }\n";
    html << "        .footer { text-align: center; color: #5a6a7a; margin-top: 20px; }\n";
    html << "        @media (max-width: 768px) { .data-section { grid-template-columns: 1fr; } }\n";
    html << "    </style>\n";
    html << "</head>\n";
    html << "<body>\n";

    // Header
    html << "    <div class=\"header\">\n";
    html << "        <div class=\"title\">Yuanta AutoTrading v1.0.4</div>\n";
    html << "        <div class=\"status\">\n";
    html << "            <span>" << (dashboardData.isSimulationMode ? "SIMULATION" : "LIVE") << "</span> | \n";
    html << "            <span class=\"" << (tradingActive ? "trading-active" : "trading-inactive") << "\">";
    html << (tradingActive ? "[ON] Trading Active" : "[OFF] Standby") << "</span>\n";
    html << "        </div>\n";
    html << "    </div>\n";

    // Stats grid
    html << "    <div class=\"stats-grid\">\n";
    html << "        <div class=\"stat-card\"><div class=\"stat-label\">Balance</div>";
    html << "<div class=\"stat-value\">" << std::setprecision(0) << dashboardData.dailyBudget << " KRW</div></div>\n";
    html << "        <div class=\"stat-card\"><div class=\"stat-label\">Total Assets</div>";
    html << "<div class=\"stat-value\">" << (dashboardData.dailyBudget + dashboardData.totalPnL) << " KRW</div></div>\n";
    html << "        <div class=\"stat-card\"><div class=\"stat-label\">P&L</div>";
    html << "<div class=\"stat-value " << (dashboardData.totalPnL >= 0 ? "positive" : "negative") << "\">";
    html << (dashboardData.totalPnL >= 0 ? "+" : "") << dashboardData.totalPnL << " KRW</div></div>\n";
    html << "        <div class=\"stat-card\"><div class=\"stat-label\">Time</div>";
    html << "<div class=\"stat-value\">" << timeStr << "</div></div>\n";
    html << "    </div>\n";

    // Data section
    html << "    <div class=\"data-section\">\n";

    // Quotes table
    html << "        <div class=\"data-card\">\n";
    html << "            <div class=\"card-title\">Real-time Quotes</div>\n";
    html << "            <table>\n";
    html << "                <thead><tr><th>Code</th><th>Name</th><th>Price</th><th>Change</th><th>Volume</th></tr></thead>\n";
    html << "                <tbody>\n";

    std::map<std::string, std::string> stockNames = {
        {"005930", "Samsung"}, {"000660", "SK Hynix"}, {"035420", "NAVER"},
        {"051910", "LG Chem"}, {"006400", "Samsung SDI"}, {"005380", "Hyundai"}
    };

    for (const auto& q : dashboardData.quotes) {
        std::string name = stockNames.count(q.code) ? stockNames[q.code] : q.code;
        html << "                <tr>";
        html << "<td>" << q.code << "</td>";
        html << "<td>" << name << "</td>";
        html << "<td>" << std::setprecision(0) << q.price << "</td>";
        html << "<td class=\"" << (q.changeRate >= 0 ? "positive" : "negative") << "\">";
        html << std::setprecision(2) << q.changeRate << "%</td>";
        html << "<td>" << std::setprecision(0) << q.volume << "</td>";
        html << "</tr>\n";
    }

    if (dashboardData.quotes.empty()) {
        html << "                <tr><td colspan=\"5\" style=\"text-align:center;color:#5a6a7a;\">Loading...</td></tr>\n";
    }

    html << "                </tbody>\n";
    html << "            </table>\n";
    html << "        </div>\n";

    // Positions table
    html << "        <div class=\"data-card\">\n";
    html << "            <div class=\"card-title\">Positions</div>\n";
    html << "            <table>\n";
    html << "                <thead><tr><th>Name</th><th>Qty</th><th>Avg</th><th>Current</th><th>P&L</th></tr></thead>\n";
    html << "                <tbody>\n";

    for (const auto& pos : dashboardData.positions) {
        std::string name = stockNames.count(pos.code) ? stockNames[pos.code] : pos.code;
        html << "                <tr>";
        html << "<td>" << name << "</td>";
        html << "<td>" << pos.quantity << "</td>";
        html << "<td>" << std::setprecision(0) << pos.avgPrice << "</td>";
        html << "<td>" << pos.currentPrice << "</td>";
        html << "<td class=\"" << (pos.pnl >= 0 ? "positive" : "negative") << "\">";
        html << (pos.pnl >= 0 ? "+" : "") << pos.pnl << "</td>";
        html << "</tr>\n";
    }

    if (dashboardData.positions.empty()) {
        html << "                <tr><td colspan=\"5\" style=\"text-align:center;color:#5a6a7a;\">No positions</td></tr>\n";
    }

    html << "                </tbody>\n";
    html << "            </table>\n";
    html << "        </div>\n";
    html << "    </div>\n";

    // Log section
    html << "    <div class=\"log-section\">\n";
    html << "        <div class=\"card-title\">Trade Log</div>\n";

    for (size_t i = 0; i < dashboardData.logs.size() && i < 10; i++) {
        const auto& log = dashboardData.logs[i];
        std::string logClass = "log-info";
        if (log.type == "BUY") logClass = "log-buy";
        else if (log.type == "SELL") logClass = "log-sell";

        time_t t = log.timestamp / 1000;
        struct tm* tm_log = localtime(&t);
        char logTimeStr[20];
        strftime(logTimeStr, 20, "%H:%M:%S", tm_log);

        html << "        <div class=\"log-entry " << logClass << "\">";
        html << "<span>[" << log.type << "] " << log.code << " - " << log.message;
        if (log.price > 0) {
            html << " @ " << std::setprecision(0) << log.price << " KRW";
        }
        html << "</span>";
        html << "<span>" << logTimeStr << "</span>";
        html << "</div>\n";
    }

    if (dashboardData.logs.empty()) {
        html << "        <div class=\"log-entry log-info\"><span>No trade logs</span></div>\n";
    }

    html << "    </div>\n";

    // Footer
    html << "    <div class=\"footer\">Auto-refresh every 2 seconds | Use console to control trading</div>\n";
    html << "</body>\n";
    html << "</html>\n";

    return html.str();
}

} // namespace yuanta
