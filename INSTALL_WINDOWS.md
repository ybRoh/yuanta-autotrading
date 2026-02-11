# Windows 설치 가이드

## 사전 요구사항

### 1. 개발 도구 설치

#### Visual Studio 2022 (권장) 또는 2019
1. https://visualstudio.microsoft.com/ko/downloads/ 에서 다운로드
2. "C++를 사용한 데스크톱 개발" 워크로드 선택
3. 설치 완료

#### CMake
1. https://cmake.org/download/ 에서 Windows x64 Installer 다운로드
2. 설치 시 "Add CMake to the system PATH" 선택

### 2. 설치 파일 제작 도구 (선택)

#### Inno Setup (권장)
1. https://jrsoftware.org/isdl.php 에서 다운로드
2. 설치

#### NSIS (대안)
1. https://nsis.sourceforge.io/Download 에서 다운로드
2. 설치

---

## 빌드 방법

### 방법 1: 배치 파일 사용 (간단)

```cmd
cd yuanta-autotrading
build.bat
```

### 방법 2: CMake 직접 사용

```cmd
cd yuanta-autotrading
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

### 방법 3: Visual Studio에서 열기

1. Visual Studio 실행
2. 파일 > 열기 > CMake
3. yuanta-autotrading 폴더의 CMakeLists.txt 선택
4. 빌드 > 모두 빌드

---

## 설치 파일 생성

### 방법 1: make_installer.bat 사용

```cmd
make_installer.bat
```
- Inno Setup 또는 NSIS 선택 가능
- dist 폴더에 설치파일 생성

### 방법 2: 포터블 버전 생성

```cmd
make_portable.bat
```
- 설치 없이 사용 가능한 ZIP 파일 생성

### 방법 3: CMake CPack 사용

```cmd
cd build
cpack -G NSIS
```
또는
```cmd
cpack -G ZIP
```

---

## 설치 파일 위치

생성된 파일은 다음 위치에 저장됩니다:

```
yuanta-autotrading/
├── dist/
│   ├── YuantaAutoTrading_Setup_1.0.0.exe    # Inno Setup 설치파일
│   ├── YuantaAutoTrading_Setup_1.0.0.exe    # NSIS 설치파일
│   └── YuantaAutoTrading_Portable_1.0.0.zip # 포터블 버전
└── build/
    └── YuantaAutoTrading-1.0.0-win64.exe    # CPack 설치파일
```

---

## 아이콘 추가

설치 파일에 아이콘을 추가하려면:

1. 256x256 PNG 이미지 준비
2. ICO 파일로 변환 (https://convertico.com/)
3. `assets/icon.ico`로 저장

---

## 문제 해결

### CMake를 찾을 수 없음
- 시스템 환경 변수 PATH에 CMake 경로 추가
- 또는 전체 경로 사용: `"C:\Program Files\CMake\bin\cmake.exe"`

### Visual Studio를 찾을 수 없음
- Developer Command Prompt for VS 2022 사용
- 또는 vcvars64.bat 직접 실행

### NSIS/Inno Setup을 찾을 수 없음
- 프로그램 설치 후 기본 경로에 설치되었는지 확인
- make_installer.bat에서 경로 수정

### 빌드 오류
```cmd
# 클린 빌드
rmdir /s /q build
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

---

## 유안타 API 설정

1. 유안타증권 계좌 개설
2. 온라인 ID 등록: https://www.myasset.com
3. Open API 서비스 신청: https://www.myasset.com/myasset/trading/apiSvc/TR_1604001_P1.cmd
4. API 모듈(wmca.dll) 다운로드
5. wmca.dll을 실행파일과 같은 폴더에 복사

---

## Visual C++ 런타임

설치 파일에 VC++ 런타임을 포함하려면:

1. https://aka.ms/vs/17/release/vc_redist.x64.exe 다운로드
2. `redist/vc_redist.x64.exe`로 저장
3. 설치 파일 재생성
