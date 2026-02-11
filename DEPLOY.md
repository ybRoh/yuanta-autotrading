# 배포 파일 생성 가이드

## 방법 1: GitHub Actions 자동 빌드 (권장)

GitHub에 프로젝트를 올리면 자동으로 Windows 설치파일이 생성됩니다.

### 단계별 안내

#### 1. GitHub 저장소 생성
1. https://github.com 접속 및 로그인
2. 우측 상단 "+" > "New repository"
3. Repository name: `yuanta-autotrading`
4. "Create repository" 클릭

#### 2. 프로젝트 업로드
```bash
cd yuanta-autotrading
git init
git add .
git commit -m "Initial commit"
git branch -M main
git remote add origin https://github.com/사용자명/yuanta-autotrading.git
git push -u origin main
```

#### 3. 자동 빌드 확인
1. GitHub 저장소 페이지에서 "Actions" 탭 클릭
2. "Build Windows Installer" 워크플로우 실행 확인
3. 완료되면 (약 5-10분) Artifacts에서 다운로드:
   - `YuantaAutoTrading-Portable-Windows` - 포터블 ZIP
   - `YuantaAutoTrading-Installer-Windows` - 설치 파일
   - `YuantaAutoTrading-Binaries` - 실행 파일만

#### 4. Release 생성 (선택)
태그를 붙이면 자동으로 Release가 생성됩니다:
```bash
git tag v1.0.0
git push origin v1.0.0
```

---

## 방법 2: 로컬 Windows에서 직접 빌드

### 원클릭 빌드
```cmd
build.bat
make_portable.bat
```

### 설치파일 생성 (Inno Setup 필요)
```cmd
make_installer.bat
```

---

## 방법 3: GitHub Codespaces 사용

1. GitHub 저장소에서 "Code" > "Codespaces" > "Create codespace"
2. 터미널에서:
```bash
# 크로스 컴파일러 설치
sudo apt update
sudo apt install mingw-w64

# 빌드
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/windows-toolchain.cmake
make
```

---

## 배포 파일 목록

| 파일명 | 설명 | 크기 (예상) |
|--------|------|------------|
| `YuantaAutoTrading_Setup_1.0.0.exe` | 설치 프로그램 | ~2MB |
| `YuantaAutoTrading_Portable_1.0.0.zip` | 포터블 버전 | ~1.5MB |
| `yuanta_autotrading.exe` | 메인 실행 파일 | ~500KB |
| `backtest.exe` | 백테스트 도구 | ~400KB |

---

## GitHub Actions 수동 실행

자동 빌드가 안 될 경우:
1. GitHub 저장소 > Actions 탭
2. 왼쪽에서 "Build Windows Installer" 선택
3. "Run workflow" 버튼 클릭
4. "Run workflow" 확인

---

## 트러블슈팅

### Actions가 실행되지 않음
- `.github/workflows/build-windows.yml` 파일이 있는지 확인
- 저장소 Settings > Actions > General에서 권한 확인

### 빌드 실패
- Actions 탭에서 실패한 job 클릭
- 로그 확인하여 오류 원인 파악

### 아티팩트가 없음
- 빌드가 완료될 때까지 기다림 (보통 5-10분)
- Summary 페이지 하단의 Artifacts 섹션 확인
