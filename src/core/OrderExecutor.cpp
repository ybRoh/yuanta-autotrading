#include "../../include/OrderExecutor.h"
#include <iostream>
#include <sstream>
#include <random>
#include <iomanip>

namespace yuanta {

// ============================================================================
// OrderExecutor
// ============================================================================

OrderExecutor::OrderExecutor()
    : orderQueue([](const OrderRequest& a, const OrderRequest& b) {
        return a.priority < b.priority;  // 높은 우선순위가 먼저
      }) {
}

OrderExecutor::~OrderExecutor() {
    stop();
}

void OrderExecutor::setAPI(YuantaAPI* api) {
    this->api = api;
}

void OrderExecutor::setRiskManager(RiskManager* rm) {
    this->riskManager = rm;
}

void OrderExecutor::start() {
    if (running) return;

    running = true;
    processingThread = std::thread(&OrderExecutor::processQueue, this);
    std::cout << "OrderExecutor started" << std::endl;
}

void OrderExecutor::stop() {
    if (!running) return;

    running = false;
    cv.notify_all();

    if (processingThread.joinable()) {
        processingThread.join();
    }

    std::cout << "OrderExecutor stopped" << std::endl;
}

bool OrderExecutor::isRunning() const {
    return running;
}

std::string OrderExecutor::submitOrder(const OrderRequest& request) {
    if (!validateOrder(request)) {
        return "";
    }

    std::string orderId = generateOrderId();

    OrderDetail detail;
    detail.orderId = orderId;
    detail.request = request;
    detail.status = OrderStatus::PENDING;
    detail.submitTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    {
        std::lock_guard<std::mutex> lock(orderMutex);
        orders[orderId] = detail;
        orderQueue.push(request);
    }

    cv.notify_one();

    return orderId;
}

std::string OrderExecutor::submitBuy(const std::string& code, int quantity,
                                      double stopLoss, double takeProfit) {
    OrderRequest request;
    request.type = OrderType::MARKET_BUY;
    request.code = code;
    request.quantity = quantity;
    request.stopLoss = stopLoss;
    request.takeProfit1 = takeProfit;
    request.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    return submitOrder(request);
}

std::string OrderExecutor::submitSell(const std::string& code, int quantity) {
    OrderRequest request;
    request.type = OrderType::MARKET_SELL;
    request.code = code;
    request.quantity = quantity;
    request.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    return submitOrder(request);
}

std::string OrderExecutor::submitLimitBuy(const std::string& code, int quantity,
                                           double price, double stopLoss) {
    OrderRequest request;
    request.type = OrderType::LIMIT_BUY;
    request.code = code;
    request.quantity = quantity;
    request.price = price;
    request.stopLoss = stopLoss;
    request.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    return submitOrder(request);
}

std::string OrderExecutor::submitLimitSell(const std::string& code, int quantity,
                                            double price) {
    OrderRequest request;
    request.type = OrderType::LIMIT_SELL;
    request.code = code;
    request.quantity = quantity;
    request.price = price;
    request.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    return submitOrder(request);
}

std::string OrderExecutor::executeSignal(const SignalInfo& signal) {
    if (signal.signal == Signal::NONE) {
        return "";
    }

    OrderRequest request;
    request.code = signal.code;
    request.quantity = signal.quantity > 0 ? signal.quantity :
                       (riskManager ? riskManager->calculatePositionSize(signal.price) : 1);
    request.stopLoss = signal.stopLoss;
    request.takeProfit1 = signal.takeProfit1;
    request.takeProfit2 = signal.takeProfit2;
    request.strategyName = signal.reason;
    request.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    switch (signal.signal) {
        case Signal::BUY:
            request.type = OrderType::MARKET_BUY;
            break;
        case Signal::SELL:
        case Signal::CLOSE_LONG:
            request.type = OrderType::MARKET_SELL;
            break;
        case Signal::PARTIAL_CLOSE:
            request.type = OrderType::MARKET_SELL;
            request.quantity = request.quantity / 2;  // 50% 청산
            break;
        default:
            return "";
    }

    return submitOrder(request);
}

bool OrderExecutor::cancelOrder(const std::string& orderId) {
    std::lock_guard<std::mutex> lock(orderMutex);

    auto it = orders.find(orderId);
    if (it == orders.end()) {
        return false;
    }

    if (it->second.status != OrderStatus::PENDING &&
        it->second.status != OrderStatus::SUBMITTED) {
        return false;  // 이미 체결/취소된 주문
    }

    if (api) {
        api->cancelOrder(orderId);
    }

    it->second.status = OrderStatus::CANCELLED;
    return true;
}

bool OrderExecutor::modifyOrder(const std::string& orderId, double newPrice, int newQty) {
    std::lock_guard<std::mutex> lock(orderMutex);

    auto it = orders.find(orderId);
    if (it == orders.end()) {
        return false;
    }

    if (it->second.status != OrderStatus::PENDING &&
        it->second.status != OrderStatus::SUBMITTED) {
        return false;
    }

    if (api) {
        api->modifyOrder(orderId, newPrice, newQty);
    }

    return true;
}

void OrderExecutor::closeAllPositions() {
    if (!riskManager) return;

    const auto& positions = riskManager->getAllPositions();
    for (const auto& pos : positions) {
        closePosition(pos.first);
    }
}

void OrderExecutor::closePosition(const std::string& code) {
    if (!riskManager) return;

    Position* pos = riskManager->getPosition(code);
    if (!pos || pos->quantity <= 0) return;

    submitSell(code, pos->quantity);
}

OrderDetail OrderExecutor::getOrderStatus(const std::string& orderId) const {
    std::lock_guard<std::mutex> lock(orderMutex);

    auto it = orders.find(orderId);
    if (it != orders.end()) {
        return it->second;
    }

    OrderDetail empty;
    empty.status = OrderStatus::FAILED;
    return empty;
}

std::vector<OrderDetail> OrderExecutor::getPendingOrders() const {
    std::lock_guard<std::mutex> lock(orderMutex);

    std::vector<OrderDetail> pending;
    for (const auto& order : orders) {
        if (order.second.status == OrderStatus::PENDING ||
            order.second.status == OrderStatus::SUBMITTED) {
            pending.push_back(order.second);
        }
    }
    return pending;
}

std::vector<OrderDetail> OrderExecutor::getTodayOrders() const {
    std::lock_guard<std::mutex> lock(orderMutex);

    std::vector<OrderDetail> today;
    for (const auto& order : orders) {
        today.push_back(order.second);
    }
    return today;
}

void OrderExecutor::setOrderCallback(OrderCallback callback) {
    orderCallback = callback;
}

void OrderExecutor::setMaxSlippage(double percent) {
    maxSlippage = percent;
}

void OrderExecutor::processQueue() {
    while (running) {
        OrderRequest request;
        bool hasOrder = false;

        {
            std::unique_lock<std::mutex> lock(cvMutex);
            cv.wait_for(lock, std::chrono::milliseconds(100), [this] {
                std::lock_guard<std::mutex> orderLock(orderMutex);
                return !running || !orderQueue.empty();
            });

            if (!running) break;

            std::lock_guard<std::mutex> orderLock(orderMutex);
            if (!orderQueue.empty()) {
                request = orderQueue.top();
                orderQueue.pop();
                hasOrder = true;
            }
        }

        if (hasOrder) {
            executeOrder(request);
        }
    }
}

bool OrderExecutor::executeOrder(const OrderRequest& request) {
    if (!api) {
        std::cerr << "API not set" << std::endl;
        return false;
    }

    OrderResult result;

    switch (request.type) {
        case OrderType::MARKET_BUY:
            std::cout << "Executing Market Buy: " << request.code
                      << " x " << request.quantity << std::endl;
            result = api->buyMarket(request.code, request.quantity);
            break;

        case OrderType::MARKET_SELL:
            std::cout << "Executing Market Sell: " << request.code
                      << " x " << request.quantity << std::endl;
            result = api->sellMarket(request.code, request.quantity);
            break;

        case OrderType::LIMIT_BUY:
            std::cout << "Executing Limit Buy: " << request.code
                      << " x " << request.quantity << " @ " << request.price << std::endl;
            result = api->buyLimit(request.code, request.quantity, request.price);
            break;

        case OrderType::LIMIT_SELL:
            std::cout << "Executing Limit Sell: " << request.code
                      << " x " << request.quantity << " @ " << request.price << std::endl;
            result = api->sellLimit(request.code, request.quantity, request.price);
            break;

        default:
            return false;
    }

    // 주문 상태 업데이트
    updateOrderStatus(result.orderId, result.success ? OrderStatus::SUBMITTED : OrderStatus::FAILED, result);

    // 포지션 업데이트 (매수 체결 시)
    if (result.success && riskManager) {
        if (request.type == OrderType::MARKET_BUY || request.type == OrderType::LIMIT_BUY) {
            Position pos;
            pos.code = request.code;
            pos.quantity = request.quantity;
            pos.avgPrice = request.price > 0 ? request.price :
                           api->getCurrentQuote(request.code).currentPrice;
            pos.stopLossPrice = request.stopLoss;
            pos.takeProfitPrice1 = request.takeProfit1;
            pos.takeProfitPrice2 = request.takeProfit2;
            pos.remainingQty = pos.quantity;
            pos.entryTime = std::chrono::system_clock::now();

            riskManager->addPosition(pos);

            // 거래 기록
            TradeRecord record;
            record.code = request.code;
            record.isBuy = true;
            record.quantity = request.quantity;
            record.price = pos.avgPrice;
            record.timestamp = std::chrono::system_clock::now();
            riskManager->recordTrade(record);
        }
        else if (request.type == OrderType::MARKET_SELL || request.type == OrderType::LIMIT_SELL) {
            double closePrice = request.price > 0 ? request.price :
                               api->getCurrentQuote(request.code).currentPrice;
            riskManager->closePosition(request.code, closePrice, request.quantity);
        }
    }

    // 콜백 호출
    if (orderCallback) {
        OrderDetail detail = getOrderStatus(result.orderId);
        orderCallback(detail);
    }

    return result.success;
}

std::string OrderExecutor::generateOrderId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);

    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    std::stringstream ss;
    ss << "ORD" << ms << dis(gen);
    return ss.str();
}

bool OrderExecutor::validateOrder(const OrderRequest& request) {
    // 기본 검증
    if (request.code.empty()) {
        std::cerr << "Invalid order: empty code" << std::endl;
        return false;
    }

    if (request.quantity <= 0) {
        std::cerr << "Invalid order: quantity <= 0" << std::endl;
        return false;
    }

    // 리스크 매니저 검증
    if (riskManager) {
        if (request.type == OrderType::MARKET_BUY || request.type == OrderType::LIMIT_BUY) {
            double price = request.price > 0 ? request.price : 50000.0;  // 임시
            if (!riskManager->canOpenPosition(request.code, price, request.quantity)) {
                std::cerr << "Order rejected by risk manager" << std::endl;
                return false;
            }
        }
    }

    return true;
}

void OrderExecutor::updateOrderStatus(const std::string& orderId,
                                       OrderStatus status,
                                       const OrderResult& result) {
    std::lock_guard<std::mutex> lock(orderMutex);

    auto it = orders.find(orderId);
    if (it != orders.end()) {
        it->second.status = status;
        it->second.errorMessage = result.errorMessage;

        if (status == OrderStatus::FILLED) {
            it->second.fillTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
        }
    }
}

// ============================================================================
// StopLossMonitor
// ============================================================================

StopLossMonitor::StopLossMonitor() {}

StopLossMonitor::~StopLossMonitor() {
    stop();
}

void StopLossMonitor::setOrderExecutor(OrderExecutor* executor) {
    this->executor = executor;
}

void StopLossMonitor::setRiskManager(RiskManager* rm) {
    this->riskManager = rm;
}

void StopLossMonitor::start() {
    if (running) return;

    running = true;
    monitorThread = std::thread(&StopLossMonitor::monitorLoop, this);
    std::cout << "StopLossMonitor started" << std::endl;
}

void StopLossMonitor::stop() {
    if (!running) return;

    running = false;

    if (monitorThread.joinable()) {
        monitorThread.join();
    }

    std::cout << "StopLossMonitor stopped" << std::endl;
}

void StopLossMonitor::onQuoteUpdate(const std::string& code, const QuoteData& quote) {
    if (!riskManager || !executor) return;

    std::lock_guard<std::mutex> lock(mtx);

    Position* pos = riskManager->getPosition(code);
    if (!pos) return;

    // 시세 업데이트
    riskManager->updatePosition(code, quote.currentPrice);

    // 손절/익절 체크
    checkStopLoss(*pos, quote);
    checkTakeProfit(*pos, quote);
    checkTimeStop(*pos);
}

void StopLossMonitor::monitorLoop() {
    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (!riskManager) continue;

        // 시간 기반 체크 (14:30 강제청산)
        std::lock_guard<std::mutex> lock(mtx);

        const auto& positions = riskManager->getAllPositions();
        for (const auto& posEntry : positions) {
            checkTimeStop(posEntry.second);
        }
    }
}

void StopLossMonitor::checkStopLoss(const Position& pos, const QuoteData& quote) {
    if (quote.currentPrice <= pos.stopLossPrice) {
        std::cout << "Stop Loss triggered for " << pos.code
                  << " @ " << quote.currentPrice << std::endl;
        executor->closePosition(pos.code);
    }
}

void StopLossMonitor::checkTakeProfit(const Position& pos, const QuoteData& quote) {
    // 1차 익절 (전체 수량이 남아있을 때)
    if (pos.remainingQty == pos.quantity &&
        quote.currentPrice >= pos.takeProfitPrice1) {

        std::cout << "Take Profit 1 triggered for " << pos.code
                  << " @ " << quote.currentPrice << std::endl;

        // 50% 청산
        int halfQty = pos.quantity / 2;
        if (halfQty > 0) {
            executor->submitSell(pos.code, halfQty);
        }
    }

    // 2차 익절 (이미 부분 청산된 경우)
    if (pos.remainingQty < pos.quantity && pos.takeProfitPrice2 > 0 &&
        quote.currentPrice >= pos.takeProfitPrice2) {

        std::cout << "Take Profit 2 triggered for " << pos.code
                  << " @ " << quote.currentPrice << std::endl;

        executor->closePosition(pos.code);
    }
}

void StopLossMonitor::checkTimeStop(const Position& pos) {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm* tm = std::localtime(&t);

    int minutes = tm->tm_hour * 60 + tm->tm_min;

    // 14:30 이후 강제 청산
    if (minutes >= 870) {
        std::cout << "Time Stop triggered for " << pos.code << std::endl;
        executor->closePosition(pos.code);
    }
}

} // namespace yuanta
