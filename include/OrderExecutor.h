#ifndef ORDER_EXECUTOR_H
#define ORDER_EXECUTOR_H

#include "YuantaAPI.h"
#include "RiskManager.h"
#include "Strategy.h"
#include <string>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <functional>

namespace yuanta {

// 주문 타입
enum class OrderType {
    MARKET_BUY,
    MARKET_SELL,
    LIMIT_BUY,
    LIMIT_SELL,
    CANCEL,
    MODIFY
};

// 주문 요청
struct OrderRequest {
    OrderType type;
    std::string code;
    int quantity;
    double price = 0.0;         // 지정가일 경우
    double stopLoss = 0.0;
    double takeProfit1 = 0.0;
    double takeProfit2 = 0.0;
    std::string originalOrderId;  // 취소/수정 시
    int priority = 0;             // 높을수록 우선
    long long timestamp;
    std::string strategyName;
};

// 주문 상태
enum class OrderStatus {
    PENDING,        // 대기
    SUBMITTED,      // 전송됨
    FILLED,         // 체결
    PARTIAL,        // 부분체결
    CANCELLED,      // 취소됨
    REJECTED,       // 거부됨
    FAILED          // 실패
};

// 주문 결과 상세
struct OrderDetail {
    std::string orderId;
    OrderRequest request;
    OrderStatus status;
    int filledQuantity = 0;
    double filledPrice = 0.0;
    double commission = 0.0;
    std::string errorMessage;
    long long submitTime = 0;
    long long fillTime = 0;
};

// 주문 실행기
class OrderExecutor {
public:
    OrderExecutor();
    ~OrderExecutor();

    // 초기화
    void setAPI(YuantaAPI* api);
    void setRiskManager(RiskManager* rm);

    // 주문 큐 관리
    void start();
    void stop();
    bool isRunning() const;

    // 주문 요청
    std::string submitOrder(const OrderRequest& request);
    std::string submitBuy(const std::string& code, int quantity,
                          double stopLoss = 0.0, double takeProfit = 0.0);
    std::string submitSell(const std::string& code, int quantity);
    std::string submitLimitBuy(const std::string& code, int quantity,
                               double price, double stopLoss = 0.0);
    std::string submitLimitSell(const std::string& code, int quantity,
                                double price);

    // 시그널로부터 주문 생성
    std::string executeSignal(const SignalInfo& signal);

    // 주문 취소/수정
    bool cancelOrder(const std::string& orderId);
    bool modifyOrder(const std::string& orderId, double newPrice, int newQty);

    // 전체 청산
    void closeAllPositions();
    void closePosition(const std::string& code);

    // 주문 상태 조회
    OrderDetail getOrderStatus(const std::string& orderId) const;
    std::vector<OrderDetail> getPendingOrders() const;
    std::vector<OrderDetail> getTodayOrders() const;

    // 콜백 설정
    using OrderCallback = std::function<void(const OrderDetail&)>;
    void setOrderCallback(OrderCallback callback);

    // 슬리피지 설정
    void setMaxSlippage(double percent);  // 최대 허용 슬리피지

private:
    YuantaAPI* api = nullptr;
    RiskManager* riskManager = nullptr;

    // 주문 큐
    std::priority_queue<OrderRequest,
                        std::vector<OrderRequest>,
                        std::function<bool(const OrderRequest&,
                                           const OrderRequest&)>> orderQueue;

    // 주문 저장소
    std::map<std::string, OrderDetail> orders;
    mutable std::mutex orderMutex;

    // 처리 스레드
    std::atomic<bool> running{false};
    std::thread processingThread;
    std::condition_variable cv;
    std::mutex cvMutex;

    // 콜백
    OrderCallback orderCallback;

    // 설정
    double maxSlippage = 0.1;  // 0.1%

    // 내부 함수
    void processQueue();
    bool executeOrder(const OrderRequest& request);
    std::string generateOrderId();
    bool validateOrder(const OrderRequest& request);
    void updateOrderStatus(const std::string& orderId,
                           OrderStatus status,
                           const OrderResult& result = {});
};

// 손절/익절 모니터
class StopLossMonitor {
public:
    StopLossMonitor();
    ~StopLossMonitor();

    void setOrderExecutor(OrderExecutor* executor);
    void setRiskManager(RiskManager* rm);

    // 모니터링 시작/중지
    void start();
    void stop();

    // 시세 업데이트 수신
    void onQuoteUpdate(const std::string& code, const QuoteData& quote);

private:
    OrderExecutor* executor = nullptr;
    RiskManager* riskManager = nullptr;
    std::atomic<bool> running{false};
    std::thread monitorThread;
    std::mutex mtx;

    void monitorLoop();
    void checkStopLoss(const Position& pos, const QuoteData& quote);
    void checkTakeProfit(const Position& pos, const QuoteData& quote);
    void checkTimeStop(const Position& pos);  // 시간 기반 청산
};

} // namespace yuanta

#endif // ORDER_EXECUTOR_H
