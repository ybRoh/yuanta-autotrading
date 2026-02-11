@echo off
REM Yuanta AutoTrading Portable Version Builder
REM 포터블(설치 불필요) 버전 생성

setlocal enabledelayedexpansion

echo ==================================
echo   Yuanta AutoTrading Portable
echo ==================================
echo.

set VERSION=1.0.0
set OUTPUT_DIR=dist\YuantaAutoTrading_Portable_%VERSION%

REM 빌드 확인
if not exist "build\bin\Release\yuanta_autotrading.exe" (
    echo [ERROR] Build not found. Running build first...
    call build.bat Release
    if errorlevel 1 (
        echo Build failed!
        pause
        exit /b 1
    )
)

REM 기존 포터블 폴더 삭제
if exist "%OUTPUT_DIR%" rmdir /s /q "%OUTPUT_DIR%"

REM 폴더 생성
echo Creating portable package...
mkdir "%OUTPUT_DIR%"
mkdir "%OUTPUT_DIR%\config"
mkdir "%OUTPUT_DIR%\logs"
mkdir "%OUTPUT_DIR%\data"
mkdir "%OUTPUT_DIR%\docs"

REM 파일 복사
echo Copying files...
copy "build\bin\Release\yuanta_autotrading.exe" "%OUTPUT_DIR%\"
copy "build\bin\Release\backtest.exe" "%OUTPUT_DIR%\"
copy "config\settings.json" "%OUTPUT_DIR%\config\"
copy "config\settings.ini" "%OUTPUT_DIR%\config\"
copy "README.md" "%OUTPUT_DIR%\docs\"
if exist "LICENSE" copy "LICENSE" "%OUTPUT_DIR%\docs\"

REM DLL 복사 (있는 경우)
if exist "lib\*.dll" copy "lib\*.dll" "%OUTPUT_DIR%\"

REM 실행 스크립트 생성
echo Creating launcher...
(
    echo @echo off
    echo cd /d "%%~dp0"
    echo start yuanta_autotrading.exe
) > "%OUTPUT_DIR%\Run.bat"

(
    echo @echo off
    echo cd /d "%%~dp0"
    echo start backtest.exe
    echo pause
) > "%OUTPUT_DIR%\Backtest.bat"

REM ZIP 파일 생성 (PowerShell 사용)
echo Creating ZIP archive...
powershell -Command "Compress-Archive -Path '%OUTPUT_DIR%\*' -DestinationPath 'dist\YuantaAutoTrading_Portable_%VERSION%.zip' -Force"

echo.
echo ==================================
echo   Portable Build Complete!
echo ==================================
echo.
echo Output:
echo   Folder: %OUTPUT_DIR%
echo   ZIP:    dist\YuantaAutoTrading_Portable_%VERSION%.zip
echo.
pause
