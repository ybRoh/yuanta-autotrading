#include "../include/TechnicalIndicators.h"
#include <iostream>
#include <cassert>
#include <cmath>

using namespace yuanta;

#define TEST(name) std::cout << "Testing " << name << "... ";
#define PASS() std::cout << "PASSED" << std::endl;
#define FAIL(msg) std::cout << "FAILED: " << msg << std::endl; failures++;

int failures = 0;

bool approxEqual(double a, double b, double epsilon = 0.01) {
    return std::abs(a - b) < epsilon;
}

void testSMA() {
    TEST("SMA");

    std::vector<double> prices = {10, 20, 30, 40, 50};
    double sma = TechnicalIndicators::SMA(prices, 5);

    if (approxEqual(sma, 30.0)) {
        PASS();
    } else {
        FAIL("Expected 30, got " << sma);
    }
}

void testEMA() {
    TEST("EMA");

    std::vector<double> prices = {22.27, 22.19, 22.08, 22.17, 22.18,
                                   22.13, 22.23, 22.43, 22.24, 22.29};
    double ema = TechnicalIndicators::EMA(prices, 10);

    // EMA 10 should be close to SMA for first calculation
    if (ema > 22.0 && ema < 23.0) {
        PASS();
    } else {
        FAIL("EMA out of expected range: " << ema);
    }
}

void testRSI() {
    TEST("RSI");

    // RSI with uptrend should be > 50
    std::vector<double> uptrend;
    for (int i = 0; i < 20; i++) {
        uptrend.push_back(100 + i * 2);
    }

    double rsi = TechnicalIndicators::RSI(uptrend, 14);

    if (rsi > 50 && rsi <= 100) {
        PASS();
    } else {
        FAIL("Uptrend RSI should be > 50, got " << rsi);
    }
}

void testMACD() {
    TEST("MACD");

    std::vector<double> prices;
    for (int i = 0; i < 50; i++) {
        prices.push_back(100 + std::sin(i * 0.2) * 10);
    }

    auto macd = TechnicalIndicators::MACD(prices, 12, 26, 9);

    // Just check it doesn't crash and returns valid values
    if (!std::isnan(macd.macd) && !std::isnan(macd.signal)) {
        PASS();
    } else {
        FAIL("MACD returned NaN");
    }
}

void testBollingerBands() {
    TEST("Bollinger Bands");

    std::vector<double> prices;
    for (int i = 0; i < 30; i++) {
        prices.push_back(50000 + (i % 5) * 100);
    }

    auto bb = TechnicalIndicators::BollingerBand(prices, 20, 2.0);

    if (bb.upper > bb.middle && bb.middle > bb.lower && bb.bandwidth > 0) {
        PASS();
    } else {
        FAIL("Invalid BB values: U=" << bb.upper << " M=" << bb.middle << " L=" << bb.lower);
    }
}

void testATR() {
    TEST("ATR");

    std::vector<OHLCV> candles;
    for (int i = 0; i < 20; i++) {
        OHLCV c;
        c.open = 50000 + i * 100;
        c.high = c.open + 500;
        c.low = c.open - 300;
        c.close = c.open + 200;
        c.volume = 10000;
        candles.push_back(c);
    }

    double atr = TechnicalIndicators::ATR(candles, 14);

    if (atr > 0 && atr < 1000) {
        PASS();
    } else {
        FAIL("ATR out of range: " << atr);
    }
}

void testVWAP() {
    TEST("VWAP");

    std::vector<OHLCV> candles;
    for (int i = 0; i < 10; i++) {
        OHLCV c;
        c.high = 50500;
        c.low = 49500;
        c.close = 50000;
        c.volume = 10000;
        candles.push_back(c);
    }

    double vwap = TechnicalIndicators::VWAP(candles);

    // VWAP should be around typical price
    if (approxEqual(vwap, 50000, 100)) {
        PASS();
    } else {
        FAIL("VWAP should be ~50000, got " << vwap);
    }
}

void testMAAlignment() {
    TEST("MA Alignment");

    std::vector<double> aligned = {100, 90, 80};  // 정배열
    std::vector<double> notAligned = {80, 90, 100};  // 역배열

    if (TechnicalIndicators::isMAAligned(aligned) &&
        !TechnicalIndicators::isMAAligned(notAligned)) {
        PASS();
    } else {
        FAIL("MA alignment detection failed");
    }
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Technical Indicators Test Suite" << std::endl;
    std::cout << "========================================\n" << std::endl;

    testSMA();
    testEMA();
    testRSI();
    testMACD();
    testBollingerBands();
    testATR();
    testVWAP();
    testMAAlignment();

    std::cout << "\n========================================" << std::endl;
    if (failures == 0) {
        std::cout << "  All tests PASSED!" << std::endl;
    } else {
        std::cout << "  " << failures << " test(s) FAILED!" << std::endl;
    }
    std::cout << "========================================" << std::endl;

    return failures;
}
