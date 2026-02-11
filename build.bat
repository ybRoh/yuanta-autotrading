@echo off
REM Yuanta AutoTrading Windows Build Script
REM Requires: Visual Studio 2019/2022 with C++ tools, CMake

setlocal enabledelayedexpansion

echo ==================================
echo   Yuanta AutoTrading Build
echo   Windows Build Script
echo ==================================
echo.

REM 빌드 타입 설정
set BUILD_TYPE=%1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=Release

REM Visual Studio 환경 설정 (VS2022)
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
) else (
    echo Warning: Visual Studio not found. Make sure vcvars64.bat is in PATH.
)

REM 빌드 디렉토리 생성
if not exist "build" mkdir build
cd build

REM CMake 설정
echo.
echo Configuring with CMake...
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
if errorlevel 1 (
    echo CMake configuration failed. Trying VS2019...
    cmake .. -G "Visual Studio 16 2019" -A x64 -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
)

if errorlevel 1 (
    echo CMake configuration failed!
    pause
    exit /b 1
)

REM 빌드
echo.
echo Building %BUILD_TYPE%...
cmake --build . --config %BUILD_TYPE% --parallel

if errorlevel 1 (
    echo Build failed!
    pause
    exit /b 1
)

echo.
echo ==================================
echo   Build Complete!
echo ==================================
echo.
echo Executables:
echo   - bin\%BUILD_TYPE%\yuanta_autotrading.exe
echo   - bin\%BUILD_TYPE%\backtest.exe
echo.

cd ..
pause
