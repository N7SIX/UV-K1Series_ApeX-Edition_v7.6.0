#include <thread>
#include <atomic>
#include <chrono>
#include <vector>
#include <cstdio>
#include <cstdlib>

// simple assertion helper
#define ASSERT(expr) do { if (!(expr)) { fprintf(stderr, "Assertion failed: %s\n", #expr); exit(1); } } while(0)



// This test suite exercises the scheduler flags that were the subject of
// the race-condition found in the firmware.  The original problem was that
// the flags were declared as non-volatile bools; the optimizer could cache
// their value in a register, and an ISR write would never be seen by the
// main loop.  We replicate the problematic pattern and then verify that
// a volatile uint8_t works correctly.

// -- helper functions -------------------------------------------------------

// simulate the ISR toggling behaviour
static void isr_thread_func(volatile uint8_t *flag, int iterations, int delay_ms) {
    for (int i = 0; i < iterations; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
        *flag = 1;
    }
}

// simulate the main loop that polls the flag
static bool main_loop_wait(volatile uint8_t *flag, int timeout_ms) {
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(timeout_ms)) {
        if (*flag) {
            return true;
        }
        // busy-wait to mimic tight loop in firmware
    }
    return false;
}

// same as above but without volatile to emulate the broken case
static bool main_loop_wait_nonvolatile(bool *flag, int timeout_ms) {
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(timeout_ms)) {
        if (*flag) {
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------


void test_volatile_flag_observed() {
    volatile uint8_t flag = 0;
    std::thread isr(isr_thread_func, &flag, 1, 10);
    bool seen = main_loop_wait(&flag, 100);
    isr.join();
    ASSERT(seen == true);
    ASSERT(flag == 1);
}

void test_nonvolatile_may_miss() {
    bool flag = false;
    std::thread isr(isr_thread_func, reinterpret_cast<volatile uint8_t *>(&flag), 1, 10);
    bool seen = main_loop_wait_nonvolatile(&flag, 100);
    isr.join();
    // not a hard requirement, only watch for failure
    if (!seen) {
        fprintf(stderr, "Warning: non-volatile flag was not observed (expected behaviour)\n");
    }
}

void test_multiple_isr_pulses() {
    volatile uint8_t flag = 0;
    const int pulses = 5;
    const int delay = 5;
    std::thread isr(isr_thread_func, &flag, pulses, delay);
    int count = 0;
    auto start = std::chrono::steady_clock::now();
    while (count < pulses && (std::chrono::steady_clock::now() - start) < std::chrono::milliseconds(500)) {
        if (flag) {
            ++count;
            flag = 0;
        }
    }
    isr.join();
    ASSERT(count == pulses);
}

void test_stress_simulation() {
    volatile uint8_t flag = 0;
    std::vector<int> observed;
    const int iterations = 10000;
    std::thread isr([&flag, iterations]() {
        for (int i = 0; i < iterations; ++i) {
            std::this_thread::sleep_for(std::chrono::microseconds(50 + (i % 10)));
            flag = 1;
        }
    });
    // run main loop for a duration that outlasts the ISR thread
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds( iterations / 10 )) {
        if (flag) {
            observed.push_back(1);
            flag = 0;
        }
    }
    isr.join();
    ASSERT(observed.size() > 0);
}

// main is defined by test_runner.cpp

// end of test_scheduler.cpp
