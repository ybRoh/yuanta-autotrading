@echo off
REM Visual Studio 솔루션 파일 생성
REM 생성 후 build/YuantaAutoTrading.sln을 Visual Studio로 열기

echo ==================================
echo   Visual Studio Solution Generator
echo ==================================
echo.

if not exist "build" mkdir build
cd build

echo Detecting Visual Studio version...

REM VS2022 확인
where /q cl 2>NUL
if %ERRORLEVEL% EQU 0 (
    echo Found Visual Studio in PATH
    goto generate
)

REM VS2022 Community
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    echo Found: Visual Studio 2022 Community
    cmake .. -G "Visual Studio 17 2022" -A x64
    goto done
)

REM VS2022 Professional
if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" (
    echo Found: Visual Studio 2022 Professional
    cmake .. -G "Visual Studio 17 2022" -A x64
    goto done
)

REM VS2019 Community
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" (
    echo Found: Visual Studio 2019 Community
    cmake .. -G "Visual Studio 16 2019" -A x64
    goto done
)

REM VS2019 Professional
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat" (
    echo Found: Visual Studio 2019 Professional
    cmake .. -G "Visual Studio 16 2019" -A x64
    goto done
)

echo [ERROR] Visual Studio not found!
echo Please install Visual Studio 2019 or 2022 with C++ tools.
pause
exit /b 1

:generate
cmake .. -A x64

:done
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] CMake failed!
    pause
    exit /b 1
)

echo.
echo ==================================
echo   Solution Generated Successfully!
echo ==================================
echo.
echo Solution file: build\YuantaAutoTrading.sln
echo.
echo Opening in Visual Studio...
start YuantaAutoTrading.sln

cd ..
