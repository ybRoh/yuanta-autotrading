#ifndef YUANTA_API_H
#define YUANTA_API_H

#include <string>
#include <functional>
#include <vector>
#include <map>
#include <memory>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
// Cross-platform placeholder for development/testing
typedef void* HMODULE;
typedef void* HWND;
#endif

namespace yuanta {

// 시세 데이터 구조체
struct QuoteData {
    std::string code;           // 종목코드
    double currentPrice;        // 현재가
    double openPrice;           // 시가
    double highPrice;           // 고가
    double lowPrice;            // 저가
    double prevClose;           // 전일종가
    long long volume;           // 거래량
    long long prevVolume;       // 전일거래량
    double changeRate;          // 등락률
    long long timestamp;        // 타임스탬프
};

// 호가 데이터 구조체
struct OrderbookData {
    std::string code;
    double bidPrices[10];       // 매수호가
    double askPrices[10];       // 매도호가
    long long bidVolumes[10];   // 매수잔량
    long long askVolumes[10];   // 매도잔량
};

// 체결 데이터 구조체
struct TradeData {
    std::string code;
    double price;
    long long volume;
    long long timestamp;
    bool isBuy;                 // 매수/매도 구분
};

// 분봉 데이터 구조체
struct CandleData {
    std::string code;
    long long timestamp;
    double open;
    double high;
    double low;
    double close;
    long long volume;
};

// 주문 결과 구조체
struct OrderResult {
    bool success;
    std::string orderId;
    std::string errorMessage;
    int errorCode;
};

// 콜백 함수 타입 정의
using QuoteCallback = std::function<void(const QuoteData&)>;
using OrderbookCallback = std::function<void(const OrderbookData&)>;
using TradeCallback = std::function<void(const TradeData&)>;
using OrderCallback = std::function<void(const OrderResult&)>;

// 유안타 API 래퍼 클래스
class YuantaAPI {
public:
    YuantaAPI();
    ~YuantaAPI();

    // 초기화 및 연결
    bool initialize(const std::string& dllPath = "");
    bool connect(const std::string& server, int port);
    bool login(const std::string& id, const std::string& password,
               const std::string& certPassword = "");
    void disconnect();
    bool isConnected() const;

    // 실시간 시세 구독
    bool subscribeQuote(const std::string& code);
    bool unsubscribeQuote(const std::string& code);
    bool subscribeOrderbook(const std::string& code);
    bool subscribeTradeData(const std::string& code);

    // 데이터 조회
    std::vector<CandleData> getMinuteCandles(const std::string& code,
                                              int minutes, int count);
    std::vector<CandleData> getDailyCandles(const std::string& code, int count);
    QuoteData getCurrentQuote(const std::string& code);

    // 주문 실행
    OrderResult buyMarket(const std::string& code, int quantity);
    OrderResult buyLimit(const std::string& code, int quantity, double price);
    OrderResult sellMarket(const std::string& code, int quantity);
    OrderResult sellLimit(const std::string& code, int quantity, double price);
    bool cancelOrder(const std::string& orderId);
    bool modifyOrder(const std::string& orderId, double newPrice, int newQty);

    // 계좌 정보
    double getBalance();
    double getBuyingPower();
    std::map<std::string, int> getPositions();

    // 콜백 등록
    void setQuoteCallback(QuoteCallback callback);
    void setOrderbookCallback(OrderbookCallback callback);
    void setTradeCallback(TradeCallback callback);
    void setOrderCallback(OrderCallback callback);

    // 윈도우 핸들 설정 (메시지 수신용)
    void setWindowHandle(HWND hwnd);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;

    // DLL 함수 포인터들
    HMODULE hDll;
    bool initialized;
    bool connected;

    // 콜백 함수들
    QuoteCallback quoteCallback;
    OrderbookCallback orderbookCallback;
    TradeCallback tradeCallback;
    OrderCallback orderCallback;

    // 내부 함수
    bool loadDll(const std::string& path);
    void bindFunctions();
    void processMessage(int msgType, void* data);
};

} // namespace yuanta

#endif // YUANTA_API_H
