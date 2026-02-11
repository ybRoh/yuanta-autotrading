#ifndef STRATEGY_H
#define STRATEGY_H

#include "TechnicalIndicators.h"
#include "RiskManager.h"
#include "YuantaAPI.h"
#include <string>
#include <memory>
#include <functional>

namespace yuanta {

// 매매 신호
enum class Signal {
    NONE,
    BUY,
    SELL,
    CLOSE_LONG,     // 롱 포지션 청산
    PARTIAL_CLOSE   // 부분 청산
};

// 신호 상세 정보
struct SignalInfo {
    Signal signal = Signal::NONE;
    std::string code;
    double price = 0.0;
    int quantity = 0;
    double stopLoss = 0.0;
    double takeProfit1 = 0.0;
    double takeProfit2 = 0.0;
    double confidence = 0.0;    // 신뢰도 (0~1)
    std::string reason;
};

// 전략 기본 클래스
class Strategy {
public:
    virtual ~Strategy() = default;

    // 전략 이름
    virtual std::string getName() const = 0;

    // 신호 생성
    virtual SignalInfo analyze(const std::string& code,
                               const std::vector<OHLCV>& candles,
                               const QuoteData& quote) = 0;

    // 청산 조건 확인
    virtual bool shouldClose(const Position& position,
                             const QuoteData& quote) = 0;

    // 전략 활성화/비활성화
    void setEnabled(bool enabled) { this->enabled = enabled; }
    bool isEnabled() const { return enabled; }

    // 파라미터 설정
    virtual void setParameter(const std::string& name, double value) {}
    virtual double getParameter(const std::string& name) const { return 0.0; }

protected:
    bool enabled = true;
    RiskManager* riskManager = nullptr;
};

// 전략 1: 갭 상승 후 눌림목 매수 (승률 65-70%)
class GapPullbackStrategy : public Strategy {
public:
    GapPullbackStrategy();

    std::string getName() const override { return "GapPullback"; }

    SignalInfo analyze(const std::string& code,
                       const std::vector<OHLCV>& candles,
                       const QuoteData& quote) override;

    bool shouldClose(const Position& position,
                     const QuoteData& quote) override;

    void setParameter(const std::string& name, double value) override;
    double getParameter(const std::string& name) const override;

private:
    // 파라미터
    double minGapPercent = 1.5;      // 최소 갭 상승률 (%)
    double maxGapPercent = 5.0;      // 최대 갭 상승률 (%)
    double pullbackMin = 0.5;        // 최소 눌림폭 (%)
    double pullbackMax = 1.5;        // 최대 눌림폭 (%)
    double volumeMultiple = 2.0;     // 거래량 배수 (200%)
    double takeProfitPercent = 2.0;  // 익절률 (%)
    double stopLossPercent = 1.0;    // 손절률 (%)
    int entryWindowMinutes = 15;     // 진입 시간 윈도우 (장 시작 후)

    // 내부 상태
    std::map<std::string, double> morningHighs;      // 아침 고점
    std::map<std::string, bool> pullbackDetected;    // 눌림 감지 여부

    // 헬퍼 함수
    bool checkGapUp(const QuoteData& quote, double prevClose) const;
    bool checkPullback(const std::string& code, double currentPrice) const;
    bool checkVolume(const std::vector<OHLCV>& candles) const;
    bool checkVWAP(double currentPrice, const std::vector<OHLCV>& candles) const;
    bool isWithinEntryWindow() const;
};

// 전략 2: 이동평균선 정배열 돌파 (승률 55-60%)
class MABreakoutStrategy : public Strategy {
public:
    MABreakoutStrategy();

    std::string getName() const override { return "MABreakout"; }

    SignalInfo analyze(const std::string& code,
                       const std::vector<OHLCV>& candles,
                       const QuoteData& quote) override;

    bool shouldClose(const Position& position,
                     const QuoteData& quote) override;

    void setParameter(const std::string& name, double value) override;
    double getParameter(const std::string& name) const override;

private:
    // 파라미터
    int fastMA = 5;
    int midMA = 10;
    int slowMA = 20;
    double volumeMultiple = 3.0;     // 거래량 배수 (300%)
    double rsiMin = 50.0;
    double rsiMax = 70.0;
    double takeProfit1Percent = 1.5; // 1차 익절 (50% 물량)
    double takeProfit2Percent = 3.0; // 2차 익절 (잔여)
    double stopLossPercent = 1.2;    // 손절률

    // 헬퍼 함수
    bool checkMAAlignment(const std::vector<double>& ma5,
                          const std::vector<double>& ma10,
                          const std::vector<double>& ma20) const;
    bool checkMABreakout(double price, const std::vector<double>& ma20) const;
    bool checkRSI(double rsi) const;
    bool checkMACDCross(const MACDResult& macd) const;
};

// 전략 3: 볼린저밴드 스퀴즈 (승률 60-65%)
class BBSqueezeStrategy : public Strategy {
public:
    BBSqueezeStrategy();

    std::string getName() const override { return "BBSqueeze"; }

    SignalInfo analyze(const std::string& code,
                       const std::vector<OHLCV>& candles,
                       const QuoteData& quote) override;

    bool shouldClose(const Position& position,
                     const QuoteData& quote) override;

    void setParameter(const std::string& name, double value) override;
    double getParameter(const std::string& name) const override;

private:
    // 파라미터
    int bbPeriod = 20;
    double bbStdDev = 2.0;
    int squeezeLookback = 50;
    double squeezePercentile = 0.20;  // 하위 20%
    double volumeMultiple = 1.5;       // 거래량 배수 (150%)
    double rsiMin = 55.0;
    double rsiMax = 75.0;
    double stopLossPercent = 1.5;

    // 헬퍼 함수
    bool checkSqueeze(const std::vector<BollingerBands>& bands) const;
    bool checkBreakout(double price, const BollingerBands& bb) const;
    double calculateTarget(const BollingerBands& bb, double atr) const;
};

// 전략 매니저
class StrategyManager {
public:
    StrategyManager();
    ~StrategyManager();

    // 전략 등록
    void addStrategy(std::unique_ptr<Strategy> strategy);
    void removeStrategy(const std::string& name);
    Strategy* getStrategy(const std::string& name);

    // 모든 전략 분석 실행
    std::vector<SignalInfo> analyzeAll(const std::string& code,
                                        const std::vector<OHLCV>& candles,
                                        const QuoteData& quote);

    // 청산 조건 확인
    std::vector<SignalInfo> checkCloseConditions(
        const std::map<std::string, Position>& positions,
        const std::map<std::string, QuoteData>& quotes);

    // 리스크 매니저 설정
    void setRiskManager(RiskManager* rm);

private:
    std::vector<std::unique_ptr<Strategy>> strategies;
    RiskManager* riskManager = nullptr;
};

} // namespace yuanta

#endif // STRATEGY_H
