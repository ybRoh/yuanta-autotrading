cmake_minimum_required(VERSION 3.14)
project(YuantaAutoTrading VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 빌드 타입
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 컴파일러 플래그
if(MSVC)
    add_compile_options(/W4 /WX-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -O0)
    else()
        add_compile_options(-O3)
    endif()
endif()

# 헤더 파일 경로
include_directories(${CMAKE_SOURCE_DIR}/include)

# 소스 파일 수집
set(API_SOURCES
    src/api/YuantaAPIWrapper.cpp
)

set(CORE_SOURCES
    src/core/RiskManager.cpp
    src/core/OrderExecutor.cpp
)

set(STRATEGY_SOURCES
    src/strategy/GapPullbackStrategy.cpp
    src/strategy/MABreakoutStrategy.cpp
    src/strategy/BBSqueezeStrategy.cpp
)

set(INDICATOR_SOURCES
    src/indicator/TechnicalIndicators.cpp
)

set(DATA_SOURCES
    src/data/MarketDataManager.cpp
)

# 라이브러리 빌드
add_library(yuanta_trading STATIC
    ${API_SOURCES}
    ${CORE_SOURCES}
    ${STRATEGY_SOURCES}
    ${INDICATOR_SOURCES}
    ${DATA_SOURCES}
)

# 메인 실행 파일
add_executable(yuanta_autotrading src/main.cpp)
target_link_libraries(yuanta_autotrading PRIVATE yuanta_trading)

# 스레드 라이브러리 링크
find_package(Threads REQUIRED)
target_link_libraries(yuanta_trading PUBLIC Threads::Threads)

# Windows 전용 링크
if(WIN32)
    target_link_libraries(yuanta_trading PUBLIC ws2_32)
endif()

# 백테스트 실행 파일
add_executable(backtest src/backtest/BacktestMain.cpp)
target_link_libraries(backtest PRIVATE yuanta_trading)

# 설치
install(TARGETS yuanta_autotrading backtest
    RUNTIME DESTINATION bin
)

install(DIRECTORY config/
    DESTINATION config
)

# 테스트 (옵션)
option(BUILD_TESTS "Build unit tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# 출력 디렉토리 설정
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 빌드 정보 출력
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")

# ============================================================================
# CPack 설정 (설치 파일 생성)
# ============================================================================

set(CPACK_PACKAGE_NAME "YuantaAutoTrading")
set(CPACK_PACKAGE_VENDOR "Your Company")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "유안타증권 주식 자동매매 시스템")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_INSTALL_DIRECTORY "YuantaAutoTrading")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")

# Windows 설정
if(WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP")

    # NSIS 설정
    set(CPACK_NSIS_DISPLAY_NAME "유안타 자동매매")
    set(CPACK_NSIS_PACKAGE_NAME "Yuanta AutoTrading")
    set(CPACK_NSIS_HELP_LINK "https://github.com/your-repo/yuanta-autotrading")
    set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/your-repo/yuanta-autotrading")
    set(CPACK_NSIS_CONTACT "your-email@example.com")
    set(CPACK_NSIS_MODIFY_PATH ON)
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/assets/icon.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/assets/icon.ico")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\yuanta_autotrading.exe")

    # 시작 메뉴 바로가기
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\유안타 자동매매.lnk' '$INSTDIR\\\\bin\\\\yuanta_autotrading.exe'
         CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\백테스트.lnk' '$INSTDIR\\\\bin\\\\backtest.exe'
         CreateShortCut '$DESKTOP\\\\유안타 자동매매.lnk' '$INSTDIR\\\\bin\\\\yuanta_autotrading.exe'"
    )
    set(CPACK_NSIS_DELETE_ICONS_EXTRA
        "Delete '$SMPROGRAMS\\\\$START_MENU\\\\유안타 자동매매.lnk'
         Delete '$SMPROGRAMS\\\\$START_MENU\\\\백테스트.lnk'
         Delete '$DESKTOP\\\\유안타 자동매매.lnk'"
    )

    # WiX 설정 (대안)
    set(CPACK_WIX_UPGRADE_GUID "A1B2C3D4-E5F6-7890-ABCD-EF1234567890")
    set(CPACK_WIX_PRODUCT_ICON "${CMAKE_SOURCE_DIR}/assets/icon.ico")
else()
    set(CPACK_GENERATOR "TGZ;ZIP")
endif()

# 컴포넌트 설정
set(CPACK_COMPONENTS_ALL applications config)
set(CPACK_COMPONENT_APPLICATIONS_DISPLAY_NAME "실행 파일")
set(CPACK_COMPONENT_CONFIG_DISPLAY_NAME "설정 파일")

include(CPack)
