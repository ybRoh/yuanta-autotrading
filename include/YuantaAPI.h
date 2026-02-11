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
typedef const char* LPCTSTR;
#endif

namespace yuanta {

// API 결과 코드
const long RESULT_SUCCESS = 1000;
const long RESULT_FAIL = -1;
const long ERROR_MAX_CODE = 0;

// 메시지 명령 (nStartMsgID + CMD_XXX)
enum MessageCommand {
    CMD_YOA_FAIL = -1,
    CMD_YOA_SUCCESS = 1,
    CMD_SYSTEM_MESSAGE = 2,
    CMD_LOGIN = 3,
    CMD_RECEIVE_ERROR = 4,
    CMD_RECEIVE_DATA = 5,
    CMD_RECEIVE_REAL_DATA = 6
};

// 시세 데이터 구조체
struct QuoteData {
    std::string code;           // 종목코드
    double currentPrice = 0;    // 현재가
    double openPrice = 0;       // 시가
    double highPrice = 0;       // 고가
    double lowPrice = 0;        // 저가
    double prevClose = 0;       // 전일종가
    long long volume = 0;       // 거래량
    long long prevVolume = 0;   // 전일거래량
    double changeRate = 0;      // 등락률
    long long timestamp = 0;    // 타임스탬프
};

// 호가 데이터 구조체
struct OrderbookData {
    std::string code;
    double bidPrices[10] = {0}; // 매수호가
    double askPrices[10] = {0}; // 매도호가
    long long bidVolumes[10] = {0}; // 매수잔량
    long long askVolumes[10] = {0}; // 매도잔량
};

// 체결 데이터 구조체
struct TradeData {
    std::string code;
    double price = 0;
    long long volume = 0;
    long long timestamp = 0;
    bool isBuy = false;         // 매수/매도 구분
};

// 분봉 데이터 구조체
struct CandleData {
    std::string code;
    long long timestamp = 0;
    double open = 0;
    double high = 0;
    double low = 0;
    double close = 0;
    long long volume = 0;
};

// 주문 결과 구조체
struct OrderResult {
    bool success = false;
    std::string orderId;
    std::string errorMessage;
    int errorCode = 0;
};

// 콜백 함수 타입 정의
using QuoteCallback = std::function<void(const QuoteData&)>;
using OrderbookCallback = std::function<void(const OrderbookData&)>;
using TradeCallback = std::function<void(const TradeData&)>;
using OrderCallback = std::function<void(const OrderResult&)>;
using LoginCallback = std::function<void(bool success, const std::string& message)>;
using DataCallback = std::function<void(int reqId, const std::string& trCode)>;

// 유안타 API 래퍼 클래스
class YuantaAPI {
public:
    YuantaAPI();
    ~YuantaAPI();

    // 초기화 및 연결
    // dllPath: yuantaapi.dll이 있는 경로 (비어있으면 현재 디렉토리)
    // server: simul.tradar.api.com (모의투자) 또는 real.tradar.api.com (실투자)
    bool initialize(const std::string& dllPath = "");
    bool connect(const std::string& server = "simul.tradar.api.com", int port = 0);
    bool login(const std::string& id, const std::string& password,
               const std::string& certPassword = "");
    void disconnect();
    bool isConnected() const;
    bool isLoggedIn() const { return loggedIn; }
    bool isSimulationMode() const { return simulationMode; }

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
    void setLoginCallback(LoginCallback callback);
    void setDataCallback(DataCallback callback);

    // 윈도우 핸들 설정 (메시지 수신용)
    void setWindowHandle(HWND hwnd);

    // 메시지 처리 (Windows 메시지 루프에서 호출)
    void processMessage(int msgType, void* data);

    // TR 필드 설정/조회 (고급 사용자용)
    bool setTRFieldString(const std::string& trCode, const std::string& blockName,
                          const std::string& fieldName, const std::string& value, int index = 0);
    bool setTRFieldLong(const std::string& trCode, const std::string& blockName,
                        const std::string& fieldName, long value, int index = 0);
    std::string getTRFieldString(const std::string& trCode, const std::string& blockName,
                                  const std::string& fieldName, int index = 0);
    long getTRFieldLong(const std::string& trCode, const std::string& blockName,
                        const std::string& fieldName, int index = 0);
    int getTRRecordCount(const std::string& trCode, const std::string& blockName);

    // TR 요청
    int request(const std::string& trCode, bool releaseData = true, int nextReqId = -1);

    // 실시간 등록/해제
    int registAuto(const std::string& autoCode, const std::string& key);
    bool unregistAuto(const std::string& autoCode, const std::string& key);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;

    // DLL 핸들
    HMODULE hDll;
    bool initialized;
    bool connected;
    bool loggedIn;
    bool simulationMode;
    std::string serverUrl;

    // 콜백 함수들
    QuoteCallback quoteCallback;
    OrderbookCallback orderbookCallback;
    TradeCallback tradeCallback;
    OrderCallback orderCallback;
    LoginCallback loginCallback;
    DataCallback dataCallback;

    // 내부 함수
    bool loadDll(const std::string& path);
    void bindFunctions();
    void enableSimulationMode();
};

} // namespace yuanta

#endif // YUANTA_API_H
