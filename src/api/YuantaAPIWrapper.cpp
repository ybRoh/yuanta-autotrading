#include "../../include/YuantaAPI.h"
#include <iostream>
#include <cstring>
#include <chrono>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>

// 유안타 tRadar Open API 함수 타입 정의
// yuantaapi.dll 에서 export되는 함수들
typedef long (*YOA_INITIAL)(LPCTSTR szURL, HWND hWnd, LPCTSTR szPath, long nStartMsgID);
typedef long (*YOA_UNINITIAL)();
typedef long (*YOA_LOGIN)(HWND hWnd, LPCTSTR szUserID, LPCTSTR szUserPWD, LPCTSTR szCertPWD);
typedef long (*YOA_LOGOUT)(HWND hWnd);
typedef long (*YOA_REQUEST)(HWND hWnd, LPCTSTR szDSOID, BOOL bReleaseData, long nNextReqID);
typedef long (*YOA_REGISTAUTO)(HWND hWnd, LPCTSTR szAutoID, LPCTSTR szKey);
typedef long (*YOA_UNREGISTAUTO)(LPCTSTR szAutoID, LPCTSTR szKey);
typedef long (*YOA_UNREGISTALLAUTO)();
typedef long (*YOA_SETTRTIELDSTRING)(LPCTSTR szDSOID, LPCTSTR szBlockName, LPCTSTR szFieldName, LPCTSTR szValue, long nRowIndex);
typedef long (*YOA_SETTRFIELDLONG)(LPCTSTR szDSOID, LPCTSTR szBlockName, LPCTSTR szFieldName, long nValue, long nRowIndex);
typedef long (*YOA_SETTRFIELDBYTE)(LPCTSTR szDSOID, LPCTSTR szBlockName, LPCTSTR szFieldName, BYTE nValue, long nRowIndex);
typedef LPCTSTR (*YOA_GETTRFIELDSTRING)(LPCTSTR szDSOID, LPCTSTR szBlockName, LPCTSTR szFieldName, long nRowIndex);
typedef long (*YOA_GETTRFIELDLONG)(LPCTSTR szDSOID, LPCTSTR szBlockName, LPCTSTR szFieldName, long nRowIndex);
typedef BYTE (*YOA_GETTRFIELDBYTE)(LPCTSTR szDSOID, LPCTSTR szBlockName, LPCTSTR szFieldName, long nRowIndex);
typedef double (*YOA_GETTRFIELDDOUBLE)(LPCTSTR szDSOID, LPCTSTR szBlockName, LPCTSTR szFieldName, long nRowIndex);
typedef long (*YOA_GETTRRECORDCOUNT)(LPCTSTR szDSOID, LPCTSTR szBlockName);
typedef long (*YOA_GETERRORMESSAGE)(long nErrorCode, LPCTSTR szMessage, long nBufferSize);
typedef long (*YOA_RELEASEDATA)(long nReqID);
typedef long (*YOA_RESET)(LPCTSTR szDSOID);

#endif

namespace yuanta {

class YuantaAPI::Impl {
public:
#ifdef _WIN32
    // API 함수 포인터
    YOA_INITIAL fnInitial = nullptr;
    YOA_UNINITIAL fnUnInitial = nullptr;
    YOA_LOGIN fnLogin = nullptr;
    YOA_LOGOUT fnLogout = nullptr;
    YOA_REQUEST fnRequest = nullptr;
    YOA_REGISTAUTO fnRegistAuto = nullptr;
    YOA_UNREGISTAUTO fnUnRegistAuto = nullptr;
    YOA_UNREGISTALLAUTO fnUnRegistAllAuto = nullptr;
    YOA_SETTRTIELDSTRING fnSetTRFieldString = nullptr;
    YOA_SETTRFIELDLONG fnSetTRFieldLong = nullptr;
    YOA_SETTRFIELDBYTE fnSetTRFieldByte = nullptr;
    YOA_GETTRFIELDSTRING fnGetTRFieldString = nullptr;
    YOA_GETTRFIELDLONG fnGetTRFieldLong = nullptr;
    YOA_GETTRFIELDBYTE fnGetTRFieldByte = nullptr;
    YOA_GETTRFIELDDOUBLE fnGetTRFieldDouble = nullptr;
    YOA_GETTRRECORDCOUNT fnGetTRRecordCount = nullptr;
    YOA_GETERRORMESSAGE fnGetErrorMessage = nullptr;
    YOA_RELEASEDATA fnReleaseData = nullptr;
    YOA_RESET fnReset = nullptr;
#endif

    HWND hwnd = nullptr;
    std::string accountNo;
    std::string userId;
    long startMsgId = WM_USER + 100;
};

YuantaAPI::YuantaAPI()
    : pImpl(std::make_unique<Impl>())
    , hDll(nullptr)
    , initialized(false)
    , connected(false)
    , loggedIn(false)
    , simulationMode(false) {
}

YuantaAPI::~YuantaAPI() {
    disconnect();
    if (hDll) {
#ifdef _WIN32
        FreeLibrary(hDll);
#endif
        hDll = nullptr;
    }
}

bool YuantaAPI::initialize(const std::string& dllPath) {
#ifdef _WIN32
    // DLL 경로 설정
    std::string path = dllPath.empty() ? "yuantaapi.dll" : dllPath + "\\yuantaapi.dll";

    // DLL 로드
    hDll = LoadLibraryA(path.c_str());
    if (!hDll) {
        DWORD err = GetLastError();
        std::cerr << "Failed to load DLL: " << path << std::endl;
        std::cerr << "Error code: " << err << std::endl;

        // DLL 없으면 시뮬레이션 모드로 전환
        std::cout << "[Simulation Mode] Running without Yuanta API DLL" << std::endl;
        enableSimulationMode();
        return true;  // 시뮬레이션 모드로 계속 진행
    }

    // 함수 바인딩
    bindFunctions();

    // 필수 함수 체크
    if (!pImpl->fnInitial || !pImpl->fnLogin || !pImpl->fnRequest) {
        std::cerr << "Failed to bind required API functions" << std::endl;
        FreeLibrary(hDll);
        hDll = nullptr;
        enableSimulationMode();
        return true;  // 시뮬레이션 모드로 진행
    }

    initialized = true;
    std::cout << "Yuanta API initialized successfully (DLL loaded)" << std::endl;
    return true;

#else
    // Non-Windows: 시뮬레이션 모드
    std::cout << "[Simulation Mode] Yuanta API initialized (non-Windows platform)" << std::endl;
    enableSimulationMode();
    return true;
#endif
}

void YuantaAPI::bindFunctions() {
#ifdef _WIN32
    if (!hDll) return;

    pImpl->fnInitial = (YOA_INITIAL)GetProcAddress(hDll, "YOA_Initial");
    pImpl->fnUnInitial = (YOA_UNINITIAL)GetProcAddress(hDll, "YOA_UnInitial");
    pImpl->fnLogin = (YOA_LOGIN)GetProcAddress(hDll, "YOA_Login");
    pImpl->fnLogout = (YOA_LOGOUT)GetProcAddress(hDll, "YOA_Logout");
    pImpl->fnRequest = (YOA_REQUEST)GetProcAddress(hDll, "YOA_Request");
    pImpl->fnRegistAuto = (YOA_REGISTAUTO)GetProcAddress(hDll, "YOA_RegistAuto");
    pImpl->fnUnRegistAuto = (YOA_UNREGISTAUTO)GetProcAddress(hDll, "YOA_UnRegistAuto");
    pImpl->fnUnRegistAllAuto = (YOA_UNREGISTALLAUTO)GetProcAddress(hDll, "YOA_UnRegistAllAuto");
    pImpl->fnSetTRFieldString = (YOA_SETTRTIELDSTRING)GetProcAddress(hDll, "YOA_SetTRFieldString");
    pImpl->fnSetTRFieldLong = (YOA_SETTRFIELDLONG)GetProcAddress(hDll, "YOA_SetTRFieldLong");
    pImpl->fnSetTRFieldByte = (YOA_SETTRFIELDBYTE)GetProcAddress(hDll, "YOA_SetTRFieldByte");
    pImpl->fnGetTRFieldString = (YOA_GETTRFIELDSTRING)GetProcAddress(hDll, "YOA_GetTRFieldString");
    pImpl->fnGetTRFieldLong = (YOA_GETTRFIELDLONG)GetProcAddress(hDll, "YOA_GetTRFieldLong");
    pImpl->fnGetTRFieldByte = (YOA_GETTRFIELDBYTE)GetProcAddress(hDll, "YOA_GetTRFieldByte");
    pImpl->fnGetTRFieldDouble = (YOA_GETTRFIELDDOUBLE)GetProcAddress(hDll, "YOA_GetTRFieldDouble");
    pImpl->fnGetTRRecordCount = (YOA_GETTRRECORDCOUNT)GetProcAddress(hDll, "YOA_GetTRRecordCount");
    pImpl->fnGetErrorMessage = (YOA_GETERRORMESSAGE)GetProcAddress(hDll, "YOA_GetErrorMessage");
    pImpl->fnReleaseData = (YOA_RELEASEDATA)GetProcAddress(hDll, "YOA_ReleaseData");
    pImpl->fnReset = (YOA_RESET)GetProcAddress(hDll, "YOA_Reset");

    // 함수 바인딩 결과 출력
    std::cout << "API functions bound:" << std::endl;
    std::cout << "  - YOA_Initial: " << (pImpl->fnInitial ? "OK" : "FAIL") << std::endl;
    std::cout << "  - YOA_Login: " << (pImpl->fnLogin ? "OK" : "FAIL") << std::endl;
    std::cout << "  - YOA_Request: " << (pImpl->fnRequest ? "OK" : "FAIL") << std::endl;
#endif
}

void YuantaAPI::enableSimulationMode() {
    simulationMode = true;
    initialized = true;
    std::cout << "Simulation mode enabled - using generated test data" << std::endl;
}

bool YuantaAPI::connect(const std::string& server, int port) {
    if (!initialized) {
        std::cerr << "API not initialized" << std::endl;
        return false;
    }

    serverUrl = server;

    if (simulationMode) {
        std::cout << "[Simulation] Connected to " << server << std::endl;
        connected = true;
        return true;
    }

#ifdef _WIN32
    if (!pImpl->hwnd) {
        std::cerr << "Window handle not set - using NULL (console mode)" << std::endl;
    }

    if (pImpl->fnInitial) {
        // YOA_Initial(szURL, hWnd, szPath, nStartMsgID)
        // szPath를 빈 문자열로 설정하면 실행 파일 위치 기준
        long result = pImpl->fnInitial(server.c_str(), pImpl->hwnd, "", pImpl->startMsgId);

        if (result == RESULT_SUCCESS) {
            connected = true;
            std::cout << "Connected to Yuanta server: " << server << std::endl;
            return true;
        } else {
            std::cerr << "Failed to connect. Error code: " << result << std::endl;
            return false;
        }
    }
#endif

    return false;
}

bool YuantaAPI::login(const std::string& id, const std::string& password,
                      const std::string& certPassword) {
    if (!connected) {
        std::cerr << "Not connected to server" << std::endl;
        return false;
    }

    if (simulationMode) {
        std::cout << "[Simulation] Login successful: " << id << std::endl;
        pImpl->userId = id;
        loggedIn = true;
        return true;
    }

#ifdef _WIN32
    if (pImpl->fnLogin) {
        // YOA_Login(hWnd, szUserID, szUserPWD, szCertPWD)
        long result = pImpl->fnLogin(pImpl->hwnd, id.c_str(), password.c_str(), certPassword.c_str());

        if (result == RESULT_SUCCESS) {
            std::cout << "Login request sent for user: " << id << std::endl;
            pImpl->userId = id;
            // 실제 로그인 결과는 메시지로 수신됨 (CMD_LOGIN)
            return true;
        } else {
            std::cerr << "Login request failed. Error code: " << result << std::endl;
            return false;
        }
    }
#endif

    return false;
}

void YuantaAPI::disconnect() {
    if (connected) {
#ifdef _WIN32
        if (!simulationMode && pImpl->fnUnRegistAllAuto) {
            pImpl->fnUnRegistAllAuto();
        }
        if (!simulationMode && pImpl->fnUnInitial) {
            pImpl->fnUnInitial();
        }
#endif
        connected = false;
        loggedIn = false;
        std::cout << "Disconnected from Yuanta server" << std::endl;
    }
}

bool YuantaAPI::isConnected() const {
    return connected;
}

bool YuantaAPI::subscribeQuote(const std::string& code) {
    if (!connected) return false;

    if (simulationMode) {
        std::cout << "[Simulation] Subscribed to quote: " << code << std::endl;
        return true;
    }

#ifdef _WIN32
    // 실시간 시세 등록 (예: 주식현재가)
    // AutoID는 종목코드, Key는 종목코드
    if (pImpl->fnRegistAuto) {
        long result = pImpl->fnRegistAuto(pImpl->hwnd, "41", code.c_str());
        return result > ERROR_MAX_CODE;
    }
#endif

    return true;
}

bool YuantaAPI::unsubscribeQuote(const std::string& code) {
    if (!connected) return false;

    if (simulationMode) {
        std::cout << "[Simulation] Unsubscribed from quote: " << code << std::endl;
        return true;
    }

#ifdef _WIN32
    if (pImpl->fnUnRegistAuto) {
        long result = pImpl->fnUnRegistAuto("41", code.c_str());
        return result == RESULT_SUCCESS;
    }
#endif

    return true;
}

bool YuantaAPI::subscribeOrderbook(const std::string& code) {
    if (!connected) return false;

    if (simulationMode) {
        std::cout << "[Simulation] Subscribed to orderbook: " << code << std::endl;
        return true;
    }

#ifdef _WIN32
    // 호가 실시간 등록
    if (pImpl->fnRegistAuto) {
        long result = pImpl->fnRegistAuto(pImpl->hwnd, "42", code.c_str());
        return result > ERROR_MAX_CODE;
    }
#endif

    return true;
}

bool YuantaAPI::subscribeTradeData(const std::string& code) {
    if (!connected) return false;

    if (simulationMode) {
        std::cout << "[Simulation] Subscribed to trade data: " << code << std::endl;
        return true;
    }

#ifdef _WIN32
    // 체결 실시간 등록
    if (pImpl->fnRegistAuto) {
        long result = pImpl->fnRegistAuto(pImpl->hwnd, "43", code.c_str());
        return result > ERROR_MAX_CODE;
    }
#endif

    return true;
}

std::vector<CandleData> YuantaAPI::getMinuteCandles(const std::string& code,
                                                     int minutes, int count) {
    std::vector<CandleData> candles;

    if (!connected) return candles;

    if (simulationMode) {
        // 시뮬레이션 데이터 생성
        auto now = std::chrono::system_clock::now();
        auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();

        double basePrice = 50000.0;
        if (code == "005930") basePrice = 70000.0;      // 삼성전자
        else if (code == "000660") basePrice = 130000.0; // SK하이닉스
        else if (code == "035420") basePrice = 180000.0; // NAVER

        for (int i = count - 1; i >= 0; --i) {
            CandleData candle;
            candle.code = code;
            candle.timestamp = nowMs - (i * minutes * 60000);

            double change = (rand() % 200 - 100) / 10000.0;
            candle.open = basePrice;
            candle.close = basePrice * (1 + change);
            candle.high = (std::max)(candle.open, candle.close) * 1.002;
            candle.low = (std::min)(candle.open, candle.close) * 0.998;
            candle.volume = 1000 + rand() % 9000;

            basePrice = candle.close;
            candles.push_back(candle);
        }
        return candles;
    }

#ifdef _WIN32
    // TR: 분봉 조회 (예: 250102)
    if (pImpl->fnSetTRFieldString && pImpl->fnRequest) {
        pImpl->fnSetTRFieldString("250102", "InBlock1", "jongcode", code.c_str(), 0);
        // 분 단위 설정
        char minStr[10];
        sprintf_s(minStr, "%d", minutes);
        pImpl->fnSetTRFieldString("250102", "InBlock1", "ncnt", minStr, 0);

        long reqId = pImpl->fnRequest(pImpl->hwnd, "250102", FALSE, -1);
        // 실제로는 메시지로 데이터 수신 후 처리
    }
#endif

    return candles;
}

std::vector<CandleData> YuantaAPI::getDailyCandles(const std::string& code, int count) {
    std::vector<CandleData> candles;

    if (!connected) return candles;

    if (simulationMode) {
        // 시뮬레이션 데이터
        auto now = std::chrono::system_clock::now();
        auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();

        double basePrice = 50000.0;
        if (code == "005930") basePrice = 70000.0;
        else if (code == "000660") basePrice = 130000.0;
        else if (code == "035420") basePrice = 180000.0;

        for (int i = count - 1; i >= 0; --i) {
            CandleData candle;
            candle.code = code;
            candle.timestamp = nowMs - (i * 24 * 60 * 60 * 1000LL);

            double change = (rand() % 600 - 300) / 10000.0;
            candle.open = basePrice;
            candle.close = basePrice * (1 + change);
            candle.high = (std::max)(candle.open, candle.close) * 1.01;
            candle.low = (std::min)(candle.open, candle.close) * 0.99;
            candle.volume = 100000 + rand() % 900000;

            basePrice = candle.close;
            candles.push_back(candle);
        }
        return candles;
    }

#ifdef _WIN32
    // TR: 일봉 조회 (예: 250101)
    if (pImpl->fnSetTRFieldString && pImpl->fnRequest) {
        pImpl->fnSetTRFieldString("250101", "InBlock1", "jongcode", code.c_str(), 0);
        long reqId = pImpl->fnRequest(pImpl->hwnd, "250101", FALSE, -1);
    }
#endif

    return candles;
}

QuoteData YuantaAPI::getCurrentQuote(const std::string& code) {
    QuoteData quote;
    quote.code = code;

    if (!connected) return quote;

    if (simulationMode) {
        // 시뮬레이션
        double basePrice = 50000.0;
        if (code == "005930") basePrice = 70000.0;
        else if (code == "000660") basePrice = 130000.0;
        else if (code == "035420") basePrice = 180000.0;

        quote.currentPrice = basePrice + (rand() % 1000 - 500);
        quote.openPrice = basePrice;
        quote.highPrice = quote.openPrice * 1.02;
        quote.lowPrice = quote.openPrice * 0.98;
        quote.prevClose = basePrice - 500;
        quote.volume = 500000 + rand() % 500000;
        quote.changeRate = ((quote.currentPrice - quote.prevClose) / quote.prevClose) * 100;
        quote.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        return quote;
    }

#ifdef _WIN32
    // TR: 주식현재가 조회 (예: 300001)
    if (pImpl->fnSetTRFieldString && pImpl->fnRequest) {
        pImpl->fnSetTRFieldString("300001", "InBlock1", "jongcode", code.c_str(), 0);
        long reqId = pImpl->fnRequest(pImpl->hwnd, "300001", TRUE, -1);
    }
#endif

    return quote;
}

OrderResult YuantaAPI::buyMarket(const std::string& code, int quantity) {
    OrderResult result;
    result.success = false;

    if (!connected || !loggedIn) {
        result.errorMessage = "Not connected or not logged in";
        return result;
    }

    if (simulationMode) {
        std::cout << "[Simulation] Market Buy: " << code << " x " << quantity << std::endl;
        result.success = true;
        result.orderId = "SIM" + std::to_string(rand() % 1000000);
        return result;
    }

#ifdef _WIN32
    // TR: 현금매수 (시장가)
    if (pImpl->fnSetTRFieldString && pImpl->fnRequest) {
        // 주문 정보 설정
        pImpl->fnSetTRFieldString("160001", "InBlock1", "jongcode", code.c_str(), 0);
        char qtyStr[20];
        sprintf_s(qtyStr, "%d", quantity);
        pImpl->fnSetTRFieldString("160001", "InBlock1", "qty", qtyStr, 0);
        pImpl->fnSetTRFieldString("160001", "InBlock1", "price", "0", 0);       // 시장가
        pImpl->fnSetTRFieldString("160001", "InBlock1", "hogagb", "03", 0);     // 03: 시장가

        long reqId = pImpl->fnRequest(pImpl->hwnd, "160001", TRUE, -1);
        if (reqId > ERROR_MAX_CODE) {
            result.success = true;
            result.orderId = std::to_string(reqId);
        }
    }
#endif

    return result;
}

OrderResult YuantaAPI::buyLimit(const std::string& code, int quantity, double price) {
    OrderResult result;
    result.success = false;

    if (!connected || !loggedIn) {
        result.errorMessage = "Not connected or not logged in";
        return result;
    }

    if (simulationMode) {
        std::cout << "[Simulation] Limit Buy: " << code << " x " << quantity
                  << " @ " << price << std::endl;
        result.success = true;
        result.orderId = "SIM" + std::to_string(rand() % 1000000);
        return result;
    }

#ifdef _WIN32
    if (pImpl->fnSetTRFieldString && pImpl->fnRequest) {
        pImpl->fnSetTRFieldString("160001", "InBlock1", "jongcode", code.c_str(), 0);
        char qtyStr[20], priceStr[20];
        sprintf_s(qtyStr, "%d", quantity);
        sprintf_s(priceStr, "%.0f", price);
        pImpl->fnSetTRFieldString("160001", "InBlock1", "qty", qtyStr, 0);
        pImpl->fnSetTRFieldString("160001", "InBlock1", "price", priceStr, 0);
        pImpl->fnSetTRFieldString("160001", "InBlock1", "hogagb", "00", 0);     // 00: 지정가

        long reqId = pImpl->fnRequest(pImpl->hwnd, "160001", TRUE, -1);
        if (reqId > ERROR_MAX_CODE) {
            result.success = true;
            result.orderId = std::to_string(reqId);
        }
    }
#endif

    return result;
}

OrderResult YuantaAPI::sellMarket(const std::string& code, int quantity) {
    OrderResult result;
    result.success = false;

    if (!connected || !loggedIn) {
        result.errorMessage = "Not connected or not logged in";
        return result;
    }

    if (simulationMode) {
        std::cout << "[Simulation] Market Sell: " << code << " x " << quantity << std::endl;
        result.success = true;
        result.orderId = "SIM" + std::to_string(rand() % 1000000);
        return result;
    }

#ifdef _WIN32
    if (pImpl->fnSetTRFieldString && pImpl->fnRequest) {
        pImpl->fnSetTRFieldString("160002", "InBlock1", "jongcode", code.c_str(), 0);
        char qtyStr[20];
        sprintf_s(qtyStr, "%d", quantity);
        pImpl->fnSetTRFieldString("160002", "InBlock1", "qty", qtyStr, 0);
        pImpl->fnSetTRFieldString("160002", "InBlock1", "price", "0", 0);
        pImpl->fnSetTRFieldString("160002", "InBlock1", "hogagb", "03", 0);

        long reqId = pImpl->fnRequest(pImpl->hwnd, "160002", TRUE, -1);
        if (reqId > ERROR_MAX_CODE) {
            result.success = true;
            result.orderId = std::to_string(reqId);
        }
    }
#endif

    return result;
}

OrderResult YuantaAPI::sellLimit(const std::string& code, int quantity, double price) {
    OrderResult result;
    result.success = false;

    if (!connected || !loggedIn) {
        result.errorMessage = "Not connected or not logged in";
        return result;
    }

    if (simulationMode) {
        std::cout << "[Simulation] Limit Sell: " << code << " x " << quantity
                  << " @ " << price << std::endl;
        result.success = true;
        result.orderId = "SIM" + std::to_string(rand() % 1000000);
        return result;
    }

#ifdef _WIN32
    if (pImpl->fnSetTRFieldString && pImpl->fnRequest) {
        pImpl->fnSetTRFieldString("160002", "InBlock1", "jongcode", code.c_str(), 0);
        char qtyStr[20], priceStr[20];
        sprintf_s(qtyStr, "%d", quantity);
        sprintf_s(priceStr, "%.0f", price);
        pImpl->fnSetTRFieldString("160002", "InBlock1", "qty", qtyStr, 0);
        pImpl->fnSetTRFieldString("160002", "InBlock1", "price", priceStr, 0);
        pImpl->fnSetTRFieldString("160002", "InBlock1", "hogagb", "00", 0);

        long reqId = pImpl->fnRequest(pImpl->hwnd, "160002", TRUE, -1);
        if (reqId > ERROR_MAX_CODE) {
            result.success = true;
            result.orderId = std::to_string(reqId);
        }
    }
#endif

    return result;
}

bool YuantaAPI::cancelOrder(const std::string& orderId) {
    if (!connected || !loggedIn) return false;

    if (simulationMode) {
        std::cout << "[Simulation] Cancel Order: " << orderId << std::endl;
        return true;
    }

#ifdef _WIN32
    // TR: 주문취소
    if (pImpl->fnSetTRFieldString && pImpl->fnRequest) {
        pImpl->fnSetTRFieldString("160003", "InBlock1", "orgordno", orderId.c_str(), 0);
        long reqId = pImpl->fnRequest(pImpl->hwnd, "160003", TRUE, -1);
        return reqId > ERROR_MAX_CODE;
    }
#endif

    return true;
}

bool YuantaAPI::modifyOrder(const std::string& orderId, double newPrice, int newQty) {
    if (!connected || !loggedIn) return false;

    if (simulationMode) {
        std::cout << "[Simulation] Modify Order: " << orderId
                  << " -> Price: " << newPrice << ", Qty: " << newQty << std::endl;
        return true;
    }

#ifdef _WIN32
    // TR: 주문정정
    if (pImpl->fnSetTRFieldString && pImpl->fnRequest) {
        pImpl->fnSetTRFieldString("160004", "InBlock1", "orgordno", orderId.c_str(), 0);
        char qtyStr[20], priceStr[20];
        sprintf_s(qtyStr, "%d", newQty);
        sprintf_s(priceStr, "%.0f", newPrice);
        pImpl->fnSetTRFieldString("160004", "InBlock1", "qty", qtyStr, 0);
        pImpl->fnSetTRFieldString("160004", "InBlock1", "price", priceStr, 0);

        long reqId = pImpl->fnRequest(pImpl->hwnd, "160004", TRUE, -1);
        return reqId > ERROR_MAX_CODE;
    }
#endif

    return true;
}

double YuantaAPI::getBalance() {
    if (!connected) return 0.0;

    if (simulationMode) {
        return 10000000.0;  // 시뮬레이션: 1000만원
    }

#ifdef _WIN32
    // TR: 예수금 조회
    if (pImpl->fnRequest) {
        long reqId = pImpl->fnRequest(pImpl->hwnd, "170001", TRUE, -1);
    }
#endif

    return 0.0;
}

double YuantaAPI::getBuyingPower() {
    if (!connected) return 0.0;

    if (simulationMode) {
        return 10000000.0;
    }

#ifdef _WIN32
    // TR: 주문가능금액 조회
#endif

    return 0.0;
}

std::map<std::string, int> YuantaAPI::getPositions() {
    std::map<std::string, int> positions;

    if (!connected) return positions;

    if (simulationMode) {
        // 시뮬레이션: 빈 포지션
        return positions;
    }

#ifdef _WIN32
    // TR: 잔고 조회
    if (pImpl->fnRequest) {
        long reqId = pImpl->fnRequest(pImpl->hwnd, "170002", TRUE, -1);
    }
#endif

    return positions;
}

void YuantaAPI::setQuoteCallback(QuoteCallback callback) {
    quoteCallback = callback;
}

void YuantaAPI::setOrderbookCallback(OrderbookCallback callback) {
    orderbookCallback = callback;
}

void YuantaAPI::setTradeCallback(TradeCallback callback) {
    tradeCallback = callback;
}

void YuantaAPI::setOrderCallback(OrderCallback callback) {
    orderCallback = callback;
}

void YuantaAPI::setLoginCallback(LoginCallback callback) {
    loginCallback = callback;
}

void YuantaAPI::setDataCallback(DataCallback callback) {
    dataCallback = callback;
}

void YuantaAPI::setWindowHandle(HWND hwnd) {
    pImpl->hwnd = hwnd;
}

void YuantaAPI::processMessage(int msgType, void* data) {
    // 메시지 타입에 따른 처리
    int cmd = msgType - pImpl->startMsgId;

    switch (cmd) {
        case CMD_LOGIN:
            loggedIn = true;
            if (loginCallback) {
                loginCallback(true, "Login successful");
            }
            break;

        case CMD_RECEIVE_DATA:
            // 조회 데이터 수신
            if (dataCallback) {
                // dataCallback(reqId, trCode);
            }
            break;

        case CMD_RECEIVE_REAL_DATA:
            // 실시간 데이터 수신
            break;

        case CMD_RECEIVE_ERROR:
            // 에러 수신
            break;
    }
}

// TR 필드 설정/조회 함수들
bool YuantaAPI::setTRFieldString(const std::string& trCode, const std::string& blockName,
                                  const std::string& fieldName, const std::string& value, int index) {
#ifdef _WIN32
    if (pImpl->fnSetTRFieldString) {
        long result = pImpl->fnSetTRFieldString(trCode.c_str(), blockName.c_str(),
                                                 fieldName.c_str(), value.c_str(), index);
        return result == RESULT_SUCCESS;
    }
#endif
    return simulationMode;
}

bool YuantaAPI::setTRFieldLong(const std::string& trCode, const std::string& blockName,
                                const std::string& fieldName, long value, int index) {
#ifdef _WIN32
    if (pImpl->fnSetTRFieldLong) {
        long result = pImpl->fnSetTRFieldLong(trCode.c_str(), blockName.c_str(),
                                               fieldName.c_str(), value, index);
        return result == RESULT_SUCCESS;
    }
#endif
    return simulationMode;
}

std::string YuantaAPI::getTRFieldString(const std::string& trCode, const std::string& blockName,
                                         const std::string& fieldName, int index) {
#ifdef _WIN32
    if (pImpl->fnGetTRFieldString) {
        const char* result = pImpl->fnGetTRFieldString(trCode.c_str(), blockName.c_str(),
                                                        fieldName.c_str(), index);
        if (result) return std::string(result);
    }
#endif
    return "";
}

long YuantaAPI::getTRFieldLong(const std::string& trCode, const std::string& blockName,
                                const std::string& fieldName, int index) {
#ifdef _WIN32
    if (pImpl->fnGetTRFieldLong) {
        return pImpl->fnGetTRFieldLong(trCode.c_str(), blockName.c_str(),
                                        fieldName.c_str(), index);
    }
#endif
    return 0;
}

int YuantaAPI::getTRRecordCount(const std::string& trCode, const std::string& blockName) {
#ifdef _WIN32
    if (pImpl->fnGetTRRecordCount) {
        return pImpl->fnGetTRRecordCount(trCode.c_str(), blockName.c_str());
    }
#endif
    return 0;
}

int YuantaAPI::request(const std::string& trCode, bool releaseData, int nextReqId) {
#ifdef _WIN32
    if (pImpl->fnRequest) {
        return pImpl->fnRequest(pImpl->hwnd, trCode.c_str(), releaseData ? TRUE : FALSE, nextReqId);
    }
#endif
    return simulationMode ? 1 : -1;
}

int YuantaAPI::registAuto(const std::string& autoCode, const std::string& key) {
#ifdef _WIN32
    if (pImpl->fnRegistAuto) {
        return pImpl->fnRegistAuto(pImpl->hwnd, autoCode.c_str(), key.c_str());
    }
#endif
    return simulationMode ? 1 : -1;
}

bool YuantaAPI::unregistAuto(const std::string& autoCode, const std::string& key) {
#ifdef _WIN32
    if (pImpl->fnUnRegistAuto) {
        return pImpl->fnUnRegistAuto(autoCode.c_str(), key.c_str()) == RESULT_SUCCESS;
    }
#endif
    return simulationMode;
}

} // namespace yuanta
