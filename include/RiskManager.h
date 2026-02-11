#ifndef RISK_MANAGER_H
#define RISK_MANAGER_H

#include <string>
#include <map>
#include <chrono>
#include <mutex>

namespace yuanta {

// 일일 예산 관리 구조체
struct DailyBudgetConfig {
    double dailyBudget = 10000000.0;           // 일일 운영금액 (기본 1000만원)
    double maxPositionRatio = 0.20;            // 종목당 최대 투자 비율 (20%)
    double maxDailyLossRatio = 0.03;           // 일일 최대 손실 비율 (3%)
    double perTradeLossRatio = 0.015;          // 종목당 손절 비율 (1.5%)
    int maxConcurrentPositions = 3;            // 동시 보유 최대 종목 수

    // 계산된 값
    double getMaxPositionSize() const { return dailyBudget * maxPositionRatio; }
    double getMaxDailyLoss() const { return dailyBudget * maxDailyLossRatio; }
    double getPerTradeLoss() const { return dailyBudget * perTradeLossRatio; }
};

// 포지션 정보
struct Position {
    std::string code;
    int quantity;
    double avgPrice;
    double currentPrice;
    double unrealizedPnL;
    double stopLossPrice;
    double takeProfitPrice1;      // 1차 익절가
    double takeProfitPrice2;      // 2차 익절가 (있는 경우)
    int remainingQty;             // 익절 후 남은 수량
    std::chrono::system_clock::time_point entryTime;
};

// 거래 기록
struct TradeRecord {
    std::string code;
    bool isBuy;
    int quantity;
    double price;
    double pnl;
    std::chrono::system_clock::time_point timestamp;
};

class RiskManager {
public:
    RiskManager();
    explicit RiskManager(const DailyBudgetConfig& config);
    ~RiskManager();

    // 설정
    void setConfig(const DailyBudgetConfig& config);
    DailyBudgetConfig getConfig() const;
    void resetDaily();  // 일일 초기화

    // 진입 가능 여부 확인
    bool canOpenPosition(const std::string& code, double price, int quantity) const;
    bool canAddToPosition(const std::string& code, double price, int quantity) const;

    // 포지션 크기 계산
    int calculatePositionSize(double price) const;
    int calculateMaxQuantity(const std::string& code, double price) const;

    // 손절/익절가 계산
    double calculateStopLoss(double entryPrice, double stopLossPercent = 0.01) const;
    double calculateTakeProfit(double entryPrice, double takeProfitPercent = 0.015) const;

    // 포지션 관리
    void addPosition(const Position& position);
    void updatePosition(const std::string& code, double currentPrice);
    void closePosition(const std::string& code, double closePrice, int quantity);
    Position* getPosition(const std::string& code);
    const std::map<std::string, Position>& getAllPositions() const;
    int getOpenPositionCount() const;

    // 손익 관리
    double getRealizedPnL() const;
    double getUnrealizedPnL() const;
    double getTotalPnL() const;
    bool isDailyLossLimitReached() const;

    // 청산 조건 확인
    bool shouldStopLoss(const std::string& code) const;
    bool shouldTakeProfit(const std::string& code) const;
    bool shouldForceClose() const;  // 장 마감 전 강제 청산

    // 거래 기록
    void recordTrade(const TradeRecord& record);
    std::vector<TradeRecord> getTodayTrades() const;

    // 통계
    double getWinRate() const;
    double getAvgWin() const;
    double getAvgLoss() const;
    double getProfitFactor() const;

private:
    DailyBudgetConfig config;
    std::map<std::string, Position> positions;
    std::vector<TradeRecord> todayTrades;

    double realizedPnL = 0.0;
    double peakEquity = 0.0;
    double currentEquity = 0.0;

    mutable std::mutex mtx;

    // 내부 함수
    double calculateCommission(double amount) const;
    double calculateTax(double amount) const;
    bool isMarketOpen() const;
    bool isNearMarketClose() const;  // 14:30 이후
};

} // namespace yuanta

#endif // RISK_MANAGER_H
