# 유안타증권 주식 자동매매 프로그램

C/C++ 기반 데이트레이딩 자동매매 시스템

## 개요

- **대상**: 국내 주식 (코스피, 코스닥)
- **매매 스타일**: 데이 트레이딩 (당일 청산)
- **API**: 유안타증권 티레이더 Open API

## 핵심 기능

### 1. 리스크 관리
| 항목 | 비율 | 예시 (1000만원) |
|------|------|-----------------|
| 종목당 최대 투자 | 20% | 200만원 |
| 일일 최대 손실 | 3% | 30만원 |
| 종목당 손절 | 1.5% | 3만원 |
| 동시 보유 종목 | 3개 | - |

### 2. 트레이딩 전략

#### 전략 1: 갭 상승 후 눌림목 매수 (승률 65-70%)
- 장 시작 시 갭 상승 1.5%~5%
- 고점 형성 후 0.5~1.5% 눌림
- 거래량 200% 이상
- VWAP 상회

#### 전략 2: 이동평균선 정배열 돌파 (승률 55-60%)
- 5분봉 5MA > 10MA > 20MA 정배열
- 20MA 돌파 + 거래량 300% 이상
- RSI 50~70, MACD 양전환

#### 전략 3: 볼린저밴드 스퀴즈 (승률 60-65%)
- 밴드폭 하위 20% (스퀴즈)
- 상단밴드 돌파 + 거래량 150%
- RSI 55~75

## 프로젝트 구조

```
yuanta-autotrading/
├── src/
│   ├── api/
│   │   └── YuantaAPIWrapper.cpp    # API 연동
│   ├── strategy/
│   │   ├── GapPullbackStrategy.cpp # 갭 눌림목 전략
│   │   ├── MABreakoutStrategy.cpp  # MA 돌파 전략
│   │   └── BBSqueezeStrategy.cpp   # 볼린저 스퀴즈
│   ├── core/
│   │   ├── RiskManager.cpp         # 리스크 관리
│   │   └── OrderExecutor.cpp       # 주문 실행
│   ├── indicator/
│   │   └── TechnicalIndicators.cpp # 기술적 지표
│   ├── data/
│   │   └── MarketDataManager.cpp   # 시세 데이터
│   ├── backtest/
│   │   └── BacktestMain.cpp        # 백테스팅
│   └── main.cpp
├── include/                         # 헤더 파일
├── lib/                            # 유안타 DLL
├── config/
│   └── settings.json               # 설정 파일
├── logs/                           # 로그
├── CMakeLists.txt
└── README.md
```

## 빌드

### 요구사항
- C++17 이상
- CMake 3.14 이상
- Windows (유안타 API 사용 시)

### 빌드 명령
```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

### 실행
```bash
# 자동매매
./bin/yuanta_autotrading

# 백테스팅
./bin/backtest
```

## 설정

`config/settings.json` 파일 수정:

```json
{
    "api": {
        "userId": "계정ID",
        "password": "비밀번호"
    },
    "risk": {
        "dailyBudget": 10000000,
        "maxPositionRatio": 0.20,
        "maxDailyLossRatio": 0.03
    },
    "watchlist": ["005930", "000660", "035420"]
}
```

## 사전 준비

1. 유안타증권 계좌 개설
2. 온라인 ID 등록
3. 티레이더 Open API 서비스 신청
4. API 모듈(DLL) 다운로드

## 주의사항

- 실전 투자 전 반드시 모의투자로 테스트
- 소액으로 시작하여 점진적으로 증액
- 거래 비용 고려 (수수료 0.015%, 세금 0.23%)
- 네트워크 장애 대비 필수
