@echo off
REM Yuanta AutoTrading Installer Build Script
REM 설치 파일 생성 스크립트

setlocal enabledelayedexpansion

echo ==================================
echo   Yuanta AutoTrading Installer
echo ==================================
echo.

REM 출력 디렉토리 생성
if not exist "dist" mkdir dist
if not exist "assets" mkdir assets

REM 아이콘 파일 확인
if not exist "assets\icon.ico" (
    echo [WARNING] assets\icon.ico not found. Creating placeholder...
    echo Please replace with your own icon file.
    copy NUL "assets\icon.ico" >NUL 2>&1
)

REM LICENSE 파일 확인
if not exist "LICENSE" (
    echo Creating LICENSE file...
    (
        echo MIT License
        echo.
        echo Copyright ^(c^) 2024 Your Company
        echo.
        echo 이 소프트웨어를 사용함에 있어 발생하는 모든 손실에 대해
        echo 제작자는 책임지지 않습니다.
        echo.
        echo 주식 투자는 원금 손실의 위험이 있습니다.
        echo 자동매매 시스템 사용 전 반드시 모의투자로 충분히 테스트하세요.
    ) > LICENSE
)

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

echo.
echo Select installer type:
echo   1. Inno Setup (Recommended)
echo   2. NSIS
echo   3. Both
echo.
set /p CHOICE="Enter choice (1-3): "

if "%CHOICE%"=="1" goto inno
if "%CHOICE%"=="2" goto nsis
if "%CHOICE%"=="3" goto both
goto inno

:inno
echo.
echo Building Inno Setup installer...
if exist "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" (
    "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer\YuantaAutoTrading.iss
) else if exist "C:\Program Files\Inno Setup 6\ISCC.exe" (
    "C:\Program Files\Inno Setup 6\ISCC.exe" installer\YuantaAutoTrading.iss
) else (
    echo [ERROR] Inno Setup not found!
    echo Download from: https://jrsoftware.org/isdl.php
    pause
    exit /b 1
)
goto done

:nsis
echo.
echo Building NSIS installer...
if exist "C:\Program Files (x86)\NSIS\makensis.exe" (
    "C:\Program Files (x86)\NSIS\makensis.exe" installer\YuantaAutoTrading.nsi
) else if exist "C:\Program Files\NSIS\makensis.exe" (
    "C:\Program Files\NSIS\makensis.exe" installer\YuantaAutoTrading.nsi
) else (
    echo [ERROR] NSIS not found!
    echo Download from: https://nsis.sourceforge.io/Download
    pause
    exit /b 1
)
goto done

:both
call :inno
call :nsis
goto done

:done
echo.
echo ==================================
echo   Installer Build Complete!
echo ==================================
echo.
echo Output files in 'dist' folder:
dir /b dist\*.exe 2>NUL
echo.
pause
