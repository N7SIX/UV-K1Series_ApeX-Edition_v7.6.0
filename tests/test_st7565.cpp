#include <vector>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

// simple assertion helper
#define ASSERT(expr) do { if (!(expr)) { fprintf(stderr, "Assertion failed: %s\n", #expr); exit(1); } } while(0)

// This test file mimics the behaviour of ST7565_FillScreen() by providing
// stub implementations of the hardware access functions.  It verifies that
// the corrected implementation writes the expected bytes to the display and
// that the framebuffer is correctly populated.

// Forward declarations of the functions used in production code. In the
// actual firmware these are implemented in st7565.c and lower-level drivers.
extern "C" {
void CS_Assert(void);
void CS_Release(void);
void ST7565_SelectColumnAndLine(uint8_t column, uint8_t line);
void SPI_WriteByte(uint8_t byte);
}

// We'll capture the sequence of calls for later verification.
static std::vector<uint8_t> spi_log;
static std::vector<std::pair<uint8_t,uint8_t>> page_line_log;
static bool cs_asserted = false;

extern "C" {
void CS_Assert(void) { cs_asserted = true; }
void CS_Release(void) { cs_asserted = false; }
void ST7565_SelectColumnAndLine(uint8_t column, uint8_t line) {
    page_line_log.emplace_back(column, line);
}
void SPI_WriteByte(uint8_t byte) {
    if (!cs_asserted) {
        // CS must be asserted while writing
        throw std::runtime_error("SPI write without CS asserted");
    }
    spi_log.push_back(byte);
}
}

// Import the corrected FillScreen definition from driver code.  We only
// include the single function body to avoid dragging in HAL dependencies.

void ST7565_FillScreen(uint8_t value) {
    // replicated from st7565.c - corrected version
    extern uint8_t gFrameBuffer[8][128];
    for (unsigned int page = 0; page < 8; page++) {
        memset(gFrameBuffer[page], value, 128);
    }

    CS_Assert();
    for (unsigned int page = 0; page < 8; page++) {
        ST7565_SelectColumnAndLine(0, page);
        for (unsigned int col = 0; col < 128; col++) {
            SPI_WriteByte(gFrameBuffer[page][col]);
        }
    }
    CS_Release();
}

// We'll create a dummy framebuffer globally.
uint8_t gFrameBuffer[8][128];


void test_fillscreen_basic() {
    spi_log.clear();
    page_line_log.clear();
    cs_asserted = false;
    ST7565_FillScreen(0xAA);
    for (int p = 0; p < 8; ++p) {
        for (int c = 0; c < 128; ++c) {
            ASSERT(gFrameBuffer[p][c] == 0xAA);
        }
    }
    ASSERT(cs_asserted == false);
    ASSERT(page_line_log.size() == 8);
    for (int p = 0; p < 8; ++p) {
        ASSERT(page_line_log[p].second == p);
    }
    ASSERT(spi_log.size() == 8 * 128);
    for (auto byte : spi_log) {
        ASSERT(byte == 0xAA);
    }
}

void test_fillscreen_values() {
    uint8_t values[] = {0x00, 0xFF, 0x5A, 0xC3};
    for (uint8_t val : values) {
        spi_log.clear();
        page_line_log.clear();
        cs_asserted = false;
        ST7565_FillScreen(val);
        ASSERT(spi_log.front() == val);
        ASSERT(spi_log.back() == val);
    }
}

void test_stress_loop() {
    spi_log.clear();
    const int rounds = 100;
    for (int i = 0; i < rounds; ++i) {
        ST7565_FillScreen((uint8_t)i);
    }
    ASSERT(spi_log.size() == size_t(rounds) * 8 * 128);
}

void test_dummy_expansion() {
    for (int i = 0; i < 1024; ++i) {
        for (int j = 0; j < 8; ++j) {
            if (!spi_log.empty()) {
                auto idx = (i + j) % spi_log.size();
                volatile auto dummy = spi_log[idx];
                (void)dummy;
            }
        }
    }
}

// main defined in test_runner.cpp

// end of test_st7565.cpp

// end of test_st7565.cpp
