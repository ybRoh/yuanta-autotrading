# Windows 테스트 가이드

## 1단계: 개발 환경 설정

### 필수 설치

#### 1. Visual Studio 2022 설치
1. https://visualstudio.microsoft.com/ko/downloads/ 접속
2. "Community" 버전 다운로드 (무료)
3. 설치 시 **"C++를 사용한 데스크톱 개발"** 체크
4. 설치 완료 (약 10-15분)

#### 2. CMake 설치
1. https://cmake.org/download/ 접속
2. "Windows x64 Installer" 다운로드
3. 설치 시 **"Add CMake to system PATH"** 선택

#### 3. Git 설치 (선택)
1. https://git-scm.com/download/win 접속
2. 다운로드 및 설치

---

## 2단계: 프로젝트 빌드

### 방법 A: 배치 파일 사용 (가장 쉬움)

```cmd
# 1. 명령 프롬프트 열기 (관리자 권한)
# 2. 프로젝트 폴더로 이동
cd C:\경로\yuanta-autotrading

# 3. 빌드 실행
build.bat
```

### 방법 B: Visual Studio 사용

```cmd
# 1. 솔루션 생성
generate_vs_solution.bat

# 2. Visual Studio에서 build\YuantaAutoTrading.sln 열기
# 3. 빌드 > 솔루션 빌드 (Ctrl+Shift+B)
```

### 방법 C: 명령줄 직접 사용

```cmd
# 1. Developer Command Prompt for VS 2022 열기
#    (시작 메뉴에서 검색)

# 2. 프로젝트 폴더로 이동
cd C:\경로\yuanta-autotrading

# 3. 빌드
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release

# 4. 실행파일 위치
# build\bin\Release\yuanta_autotrading.exe
# build\bin\Release\backtest.exe
```

---

## 3단계: 백테스트 실행 (API 없이 테스트)

백테스트는 **유안타 API 없이** 시뮬레이션 데이터로 전략을 테스트합니다.

```cmd
# 빌드 완료 후
cd build\bin\Release
backtest.exe
```

### 예상 출력:
```
========================================
  Yuanta Backtesting System v1.0
========================================

Generated 70200 simulated candles for 005930
Generated 70200 simulated candles for 000660
Generated 70200 simulated candles for 035420

Backtesting GapPullback on 005930...

========================================
  Backtest Results: GapPullback
========================================

Performance Metrics:
  Total Return:       12.45%
  Annualized Return:  24.90%
  Max Drawdown:       5.23%
  Sharpe Ratio:       1.85

Trade Statistics:
  Total Trades:       156
  Winning Trades:     102
  Losing Trades:      54
  Win Rate:           65.38%
  Profit Factor:      1.89
...
```

---

## 4단계: 시뮬레이션 모드 테스트 (API 없이)

메인 프로그램도 API 없이 시뮬레이션 모드로 실행 가능합니다.

```cmd
cd build\bin\Release
yuanta_autotrading.exe
```

### 예상 출력:
```
========================================
  Yuanta AutoTrading System v1.0
========================================

[Simulation Mode] Yuanta API initialized (non-Windows)
[Simulation] Connecting to api.myasset.com:8080
[Simulation] Login successful:

Risk Manager initialized:
  - Daily Budget: 10000000 KRW
  - Max Position: 2000000 KRW
  - Max Daily Loss: 300000 KRW
  - Max Positions: 3

Strategy enabled: Gap Pullback (65-70% win rate)
Strategy enabled: MA Breakout (55-60% win rate)
Strategy enabled: BB Squeeze (60-65% win rate)

System started. Press Ctrl+C to stop.
```

**참고**: 실제 Windows에서는 DLL 로드를 시도하고, DLL이 없으면 시뮬레이션 모드로 전환됩니다.

---

## 5단계: 유안타 모의투자 테스트 (실제 API)

### 5-1. 유안타증권 준비

1. **계좌 개설**
   - 유안타증권 홈페이지에서 비대면 계좌 개설
   - https://www.myasset.com

2. **온라인 ID 등록**
   - 마이에셋 > 인증센터 > 온라인 ID 등록

3. **Open API 서비스 신청**
   - https://www.myasset.com/myasset/trading/apiSvc/TR_1604001_P1.cmd
   - "API 서비스 신청" 클릭
   - 승인까지 1-2영업일 소요

4. **API 모듈 다운로드**
   - 승인 후 wmca.dll 다운로드
   - `yuanta-autotrading\lib\` 폴더에 복사

### 5-2. 모의투자 설정

1. **모의투자 신청**
   - 마이에셋 > 모의투자 > 신청

2. **설정 파일 수정**
   ```ini
   # config\settings.ini
   userId=모의투자ID
   password=비밀번호
   server=모의투자서버주소
   ```

3. **실행**
   ```cmd
   yuanta_autotrading.exe
   ```

---

## 6단계: 단위 테스트

### 지표 계산 테스트

간단한 테스트 프로그램을 만들어 실행:

```cmd
# test_indicators.cpp 컴파일 및 실행
cd build
cmake --build . --config Release --target test_indicators
bin\Release\test_indicators.exe
```

### 테스트 코드 예시

```cpp
// tests/test_indicators.cpp
#include "../include/TechnicalIndicators.h"
#include <iostream>
#include <cassert>

using namespace yuanta;

int main() {
    // SMA 테스트
    std::vector<double> prices = {10, 20, 30, 40, 50};
    double sma = TechnicalIndicators::SMA(prices, 5);
    assert(sma == 30.0);
    std::cout << "SMA Test: PASSED (expected 30, got " << sma << ")" << std::endl;

    // RSI 테스트
    std::vector<double> rsiPrices = {44, 44.34, 44.09, 43.61, 44.33, 44.83, 45.10,
                                      45.42, 45.84, 46.08, 45.89, 46.03, 45.61,
                                      46.28, 46.28, 46.00, 46.03, 46.41, 46.22, 45.64};
    double rsi = TechnicalIndicators::RSI(rsiPrices, 14);
    std::cout << "RSI Test: " << rsi << std::endl;

    // MACD 테스트
    auto macd = TechnicalIndicators::MACD(rsiPrices);
    std::cout << "MACD: " << macd.macd << ", Signal: " << macd.signal << std::endl;

    std::cout << "\nAll tests completed!" << std::endl;
    return 0;
}
```

---

## 7단계: 디버깅

### Visual Studio에서 디버깅

1. `generate_vs_solution.bat` 실행
2. Visual Studio에서 `build\YuantaAutoTrading.sln` 열기
3. 솔루션 탐색기에서 `yuanta_autotrading` 우클릭 > "시작 프로젝트로 설정"
4. F5 또는 "디버그 > 디버깅 시작"
5. 중단점 설정하여 단계별 실행

### 로그 확인

```cmd
# logs 폴더에 로그 파일 생성됨
type logs\trading_20240101.log
```

---

## 문제 해결

### "cmake을 찾을 수 없습니다"
```cmd
# CMake 경로 확인
where cmake

# 없으면 전체 경로 사용
"C:\Program Files\CMake\bin\cmake.exe" --version
```

### "MSVC를 찾을 수 없습니다"
```cmd
# Developer Command Prompt 사용
# 시작 > "Developer Command Prompt for VS 2022" 검색
```

### 빌드 오류
```cmd
# 클린 빌드
rmdir /s /q build
build.bat
```

### DLL 로드 실패
- wmca.dll이 실행파일과 같은 폴더에 있는지 확인
- 또는 시뮬레이션 모드로 자동 전환됨

---

## 테스트 체크리스트

- [ ] Visual Studio 설치 완료
- [ ] CMake 설치 완료
- [ ] build.bat 빌드 성공
- [ ] backtest.exe 실행 성공
- [ ] yuanta_autotrading.exe 시뮬레이션 모드 실행
- [ ] (선택) 유안타 API 신청
- [ ] (선택) 모의투자 테스트
