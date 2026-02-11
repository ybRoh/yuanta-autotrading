#include "../../include/RiskManager.h"
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace yuanta {

RiskManager::RiskManager() {
    resetDaily();
}

RiskManager::RiskManager(const DailyBudgetConfig& config)
    : config(config) {
    resetDaily();
}

RiskManager::~RiskManager() {}

void RiskManager::setConfig(const DailyBudgetConfig& config) {
    std::lock_guard<std::mutex> lock(mtx);
    this->config = config;
}

DailyBudgetConfig RiskManager::getConfig() const {
    std::lock_guard<std::mutex> lock(mtx);
    return config;
}

void RiskManager::resetDaily() {
    std::lock_guard<std::mutex> lock(mtx);
    realizedPnL = 0.0;
    peakEquity = config.dailyBudget;
    currentEquity = config.dailyBudget;
    todayTrades.clear();
    positions.clear();
}

bool RiskManager::canOpenPosition(const std::string& code, double price, int quantity) const {
    std::lock_guard<std::mutex> lock(mtx);

    // 일일 손실 한도 체크
    if (isDailyLossLimitReached()) {
        return false;
    }

    // 동시 보유 종목 수 체크
    if (static_cast<int>(positions.size()) >= config.maxConcurrentPositions) {
        return false;
    }

    // 이미 보유 중인 종목인지 체크
    if (positions.find(code) != positions.end()) {
        return false;
    }

    // 포지션 크기 체크
    double positionValue = price * quantity;
    double maxPosition = config.getMaxPositionSize();

    if (positionValue > maxPosition) {
        return false;
    }

    // 총 투자금액 체크
    double totalInvested = 0.0;
    for (const auto& pos : positions) {
        totalInvested += pos.second.avgPrice * pos.second.quantity;
    }

    if (totalInvested + positionValue > config.dailyBudget) {
        return false;
    }

    return true;
}

bool RiskManager::canAddToPosition(const std::string& code, double price, int quantity) const {
    std::lock_guard<std::mutex> lock(mtx);

    auto it = positions.find(code);
    if (it == positions.end()) {
        return false;  // 포지션 없음
    }

    double currentValue = it->second.avgPrice * it->second.quantity;
    double additionalValue = price * quantity;
    double maxPosition = config.getMaxPositionSize();

    return (currentValue + additionalValue) <= maxPosition;
}

int RiskManager::calculatePositionSize(double price) const {
    std::lock_guard<std::mutex> lock(mtx);

    if (price <= 0) return 0;

    double maxPosition = config.getMaxPositionSize();
    int quantity = static_cast<int>(maxPosition / price);

    return std::max(1, quantity);
}

int RiskManager::calculateMaxQuantity(const std::string& code, double price) const {
    std::lock_guard<std::mutex> lock(mtx);

    if (price <= 0) return 0;

    // 남은 투자 가능 금액 계산
    double totalInvested = 0.0;
    for (const auto& pos : positions) {
        totalInvested += pos.second.avgPrice * pos.second.quantity;
    }

    double remainingBudget = config.dailyBudget - totalInvested;
    double maxPosition = std::min(remainingBudget, config.getMaxPositionSize());

    return static_cast<int>(maxPosition / price);
}

double RiskManager::calculateStopLoss(double entryPrice, double stopLossPercent) const {
    return entryPrice * (1.0 - stopLossPercent);
}

double RiskManager::calculateTakeProfit(double entryPrice, double takeProfitPercent) const {
    return entryPrice * (1.0 + takeProfitPercent);
}

void RiskManager::addPosition(const Position& position) {
    std::lock_guard<std::mutex> lock(mtx);
    positions[position.code] = position;
}

void RiskManager::updatePosition(const std::string& code, double currentPrice) {
    std::lock_guard<std::mutex> lock(mtx);

    auto it = positions.find(code);
    if (it == positions.end()) return;

    Position& pos = it->second;
    pos.currentPrice = currentPrice;
    pos.unrealizedPnL = (currentPrice - pos.avgPrice) * pos.quantity;

    // 수수료 및 세금 반영
    double commission = calculateCommission(pos.avgPrice * pos.quantity);
    double tax = calculateTax(currentPrice * pos.quantity);
    pos.unrealizedPnL -= (commission + tax);
}

void RiskManager::closePosition(const std::string& code, double closePrice, int quantity) {
    std::lock_guard<std::mutex> lock(mtx);

    auto it = positions.find(code);
    if (it == positions.end()) return;

    Position& pos = it->second;
    int closeQty = std::min(quantity, pos.quantity);

    // 손익 계산
    double pnl = (closePrice - pos.avgPrice) * closeQty;

    // 거래비용 차감
    double buyValue = pos.avgPrice * closeQty;
    double sellValue = closePrice * closeQty;
    pnl -= calculateCommission(buyValue);
    pnl -= calculateCommission(sellValue);
    pnl -= calculateTax(sellValue);

    realizedPnL += pnl;

    // 거래 기록
    TradeRecord record;
    record.code = code;
    record.isBuy = false;
    record.quantity = closeQty;
    record.price = closePrice;
    record.pnl = pnl;
    record.timestamp = std::chrono::system_clock::now();
    todayTrades.push_back(record);

    // 포지션 업데이트 또는 제거
    pos.quantity -= closeQty;
    pos.remainingQty = pos.quantity;

    if (pos.quantity <= 0) {
        positions.erase(it);
    }

    // 자산 업데이트
    currentEquity = config.dailyBudget + realizedPnL + getUnrealizedPnL();
    peakEquity = std::max(peakEquity, currentEquity);
}

Position* RiskManager::getPosition(const std::string& code) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = positions.find(code);
    if (it != positions.end()) {
        return &it->second;
    }
    return nullptr;
}

const std::map<std::string, Position>& RiskManager::getAllPositions() const {
    return positions;
}

int RiskManager::getOpenPositionCount() const {
    std::lock_guard<std::mutex> lock(mtx);
    return static_cast<int>(positions.size());
}

double RiskManager::getRealizedPnL() const {
    std::lock_guard<std::mutex> lock(mtx);
    return realizedPnL;
}

double RiskManager::getUnrealizedPnL() const {
    std::lock_guard<std::mutex> lock(mtx);
    double unrealized = 0.0;
    for (const auto& pos : positions) {
        unrealized += pos.second.unrealizedPnL;
    }
    return unrealized;
}

double RiskManager::getTotalPnL() const {
    return getRealizedPnL() + getUnrealizedPnL();
}

bool RiskManager::isDailyLossLimitReached() const {
    double totalPnL = getTotalPnL();
    return totalPnL <= -config.getMaxDailyLoss();
}

bool RiskManager::shouldStopLoss(const std::string& code) const {
    std::lock_guard<std::mutex> lock(mtx);

    auto it = positions.find(code);
    if (it == positions.end()) return false;

    const Position& pos = it->second;
    return pos.currentPrice <= pos.stopLossPrice;
}

bool RiskManager::shouldTakeProfit(const std::string& code) const {
    std::lock_guard<std::mutex> lock(mtx);

    auto it = positions.find(code);
    if (it == positions.end()) return false;

    const Position& pos = it->second;

    // 1차 익절 체크
    if (pos.remainingQty == pos.quantity) {
        return pos.currentPrice >= pos.takeProfitPrice1;
    }

    // 2차 익절 체크
    return pos.currentPrice >= pos.takeProfitPrice2;
}

bool RiskManager::shouldForceClose() const {
    return isNearMarketClose();
}

void RiskManager::recordTrade(const TradeRecord& record) {
    std::lock_guard<std::mutex> lock(mtx);
    todayTrades.push_back(record);
}

std::vector<TradeRecord> RiskManager::getTodayTrades() const {
    std::lock_guard<std::mutex> lock(mtx);
    return todayTrades;
}

double RiskManager::getWinRate() const {
    std::lock_guard<std::mutex> lock(mtx);

    if (todayTrades.empty()) return 0.0;

    int wins = 0;
    int totalTrades = 0;

    for (const auto& trade : todayTrades) {
        if (!trade.isBuy) {  // 청산 거래만 카운트
            totalTrades++;
            if (trade.pnl > 0) wins++;
        }
    }

    if (totalTrades == 0) return 0.0;
    return static_cast<double>(wins) / totalTrades * 100.0;
}

double RiskManager::getAvgWin() const {
    std::lock_guard<std::mutex> lock(mtx);

    double totalWin = 0.0;
    int wins = 0;

    for (const auto& trade : todayTrades) {
        if (!trade.isBuy && trade.pnl > 0) {
            totalWin += trade.pnl;
            wins++;
        }
    }

    if (wins == 0) return 0.0;
    return totalWin / wins;
}

double RiskManager::getAvgLoss() const {
    std::lock_guard<std::mutex> lock(mtx);

    double totalLoss = 0.0;
    int losses = 0;

    for (const auto& trade : todayTrades) {
        if (!trade.isBuy && trade.pnl < 0) {
            totalLoss += std::abs(trade.pnl);
            losses++;
        }
    }

    if (losses == 0) return 0.0;
    return totalLoss / losses;
}

double RiskManager::getProfitFactor() const {
    double avgWin = getAvgWin();
    double avgLoss = getAvgLoss();

    if (avgLoss == 0) return avgWin > 0 ? 999.0 : 0.0;
    return avgWin / avgLoss;
}

double RiskManager::calculateCommission(double amount) const {
    // 유안타증권 수수료: 약 0.015%
    return amount * 0.00015;
}

double RiskManager::calculateTax(double amount) const {
    // 거래세: 0.23% (코스피), 0.23% (코스닥) - 2024년 기준
    return amount * 0.0023;
}

bool RiskManager::isMarketOpen() const {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm* tm = std::localtime(&t);

    // 주말 체크
    if (tm->tm_wday == 0 || tm->tm_wday == 6) return false;

    // 장 시간: 09:00 ~ 15:30
    int minutes = tm->tm_hour * 60 + tm->tm_min;
    return minutes >= 540 && minutes <= 930;  // 9:00 ~ 15:30
}

bool RiskManager::isNearMarketClose() const {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm* tm = std::localtime(&t);

    // 14:30 이후 (데이트레이딩 강제청산 시간)
    int minutes = tm->tm_hour * 60 + tm->tm_min;
    return minutes >= 870;  // 14:30
}

} // namespace yuanta
