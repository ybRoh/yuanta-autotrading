#!/bin/bash

# Yuanta AutoTrading Build Script

set -e

BUILD_TYPE=${1:-Release}
BUILD_DIR="build"

echo "=================================="
echo "  Yuanta AutoTrading Build"
echo "  Build Type: $BUILD_TYPE"
echo "=================================="

# 빌드 디렉토리 생성
if [ ! -d "$BUILD_DIR" ]; then
    mkdir -p "$BUILD_DIR"
fi

cd "$BUILD_DIR"

# CMake 설정
echo ""
echo "Configuring with CMake..."
cmake .. -DCMAKE_BUILD_TYPE=$BUILD_TYPE

# 빌드
echo ""
echo "Building..."
cmake --build . --config $BUILD_TYPE -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo ""
echo "=================================="
echo "  Build Complete!"
echo "=================================="
echo ""
echo "Executables:"
echo "  - bin/yuanta_autotrading (Main trading program)"
echo "  - bin/backtest (Backtesting tool)"
echo ""
echo "To run:"
echo "  cd build && ./bin/yuanta_autotrading"
echo "  cd build && ./bin/backtest"
