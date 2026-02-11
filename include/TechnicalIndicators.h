#ifndef TECHNICAL_INDICATORS_H
#define TECHNICAL_INDICATORS_H

#include <vector>
#include <deque>
#include <cmath>
#include <algorithm>
#include <numeric>

namespace yuanta {

// OHLCV 데이터 구조체
struct OHLCV {
    double open;
    double high;
    double low;
    double close;
    long long volume;
    long long timestamp;
};

// 볼린저 밴드 결과
struct BollingerBands {
    double upper;
    double middle;
    double lower;
    double bandwidth;      // (upper - lower) / middle
    double percentB;       // (close - lower) / (upper - lower)
};

// MACD 결과
struct MACDResult {
    double macd;           // MACD 라인
    double signal;         // 시그널 라인
    double histogram;      // 히스토그램
    bool bullishCross;     // 골든크로스
    bool bearishCross;     // 데드크로스
};

// 지표 계산 클래스
class TechnicalIndicators {
public:
    // 단순 이동평균 (SMA)
    static double SMA(const std::vector<double>& prices, int period);
    static std::vector<double> SMAVector(const std::vector<double>& prices, int period);

    // 지수 이동평균 (EMA)
    static double EMA(const std::vector<double>& prices, int period);
    static std::vector<double> EMAVector(const std::vector<double>& prices, int period);

    // 가중 이동평균 (WMA)
    static double WMA(const std::vector<double>& prices, int period);

    // VWAP (Volume Weighted Average Price)
    static double VWAP(const std::vector<OHLCV>& candles);
    static std::vector<double> VWAPVector(const std::vector<OHLCV>& candles);

    // RSI (Relative Strength Index)
    static double RSI(const std::vector<double>& prices, int period = 14);
    static std::vector<double> RSIVector(const std::vector<double>& prices, int period = 14);

    // MACD
    static MACDResult MACD(const std::vector<double>& prices,
                           int fastPeriod = 12, int slowPeriod = 26, int signalPeriod = 9);
    static std::vector<MACDResult> MACDVector(const std::vector<double>& prices,
                                               int fastPeriod = 12, int slowPeriod = 26,
                                               int signalPeriod = 9);

    // 볼린저 밴드
    static BollingerBands BollingerBand(const std::vector<double>& prices,
                                         int period = 20, double stdDev = 2.0);
    static std::vector<BollingerBands> BollingerBandVector(const std::vector<double>& prices,
                                                            int period = 20, double stdDev = 2.0);

    // ATR (Average True Range)
    static double ATR(const std::vector<OHLCV>& candles, int period = 14);
    static std::vector<double> ATRVector(const std::vector<OHLCV>& candles, int period = 14);

    // 표준편차
    static double StdDev(const std::vector<double>& prices, int period);

    // 정배열 확인 (MA1 > MA2 > MA3 ...)
    static bool isMAAligned(const std::vector<double>& maValues);

    // 볼린저 스퀴즈 확인
    static bool isBollingerSqueeze(const std::vector<BollingerBands>& bands,
                                    int lookback = 50, double percentile = 0.2);

    // 최고가/최저가
    static double Highest(const std::vector<double>& prices, int period);
    static double Lowest(const std::vector<double>& prices, int period);

    // 변화율
    static double ROC(const std::vector<double>& prices, int period);

    // 모멘텀
    static double Momentum(const std::vector<double>& prices, int period);

    // 스토캐스틱
    struct Stochastic {
        double k;
        double d;
    };
    static Stochastic StochasticOscillator(const std::vector<OHLCV>& candles,
                                            int kPeriod = 14, int dPeriod = 3);

private:
    // True Range 계산
    static double TrueRange(const OHLCV& current, const OHLCV& previous);
};

// 실시간 지표 계산기 (스트리밍용)
class StreamingIndicators {
public:
    StreamingIndicators();

    // 데이터 추가
    void addPrice(double price, long long volume = 0);
    void addOHLCV(const OHLCV& candle);

    // 실시간 지표 조회
    double getCurrentSMA(int period) const;
    double getCurrentEMA(int period) const;
    double getCurrentRSI(int period = 14) const;
    double getCurrentVWAP() const;
    MACDResult getCurrentMACD() const;
    BollingerBands getCurrentBB(int period = 20, double stdDev = 2.0) const;
    double getCurrentATR(int period = 14) const;

    // 데이터 클리어
    void clear();
    void setMaxSize(size_t size);

private:
    std::deque<double> prices;
    std::deque<OHLCV> candles;
    std::deque<double> volumes;
    size_t maxSize = 500;

    // EMA 캐시
    mutable std::map<int, double> emaCache;
    mutable std::map<int, double> rsiCache;
};

} // namespace yuanta

#endif // TECHNICAL_INDICATORS_H
