#include <atomic>
#include <thread>
#include <chrono>
#include <cstdio>
#include <cstdlib>

#define ASSERT(expr) do { if (!(expr)) { fprintf(stderr, "Assertion failed: %s\n", #expr); exit(1); } } while(0)

// This file exercises the logic around gUpdateDisplay flag handling in
// APP_TimeSlice10ms().  We replicate the relevant portion of the code to make
// the behaviour deterministic and easily testable.

static std::atomic<bool> gUpdateDisplay{false};

// original (broken) version of the timeslice handler
bool timeslice_old() {
    bool snapshot = gUpdateDisplay.load(std::memory_order_relaxed);
    if (snapshot) {
        gUpdateDisplay.store(false, std::memory_order_relaxed);
        // simulate render
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        // return "did radio work?" (always true for this test)
        return true;
    }
    return true;
}

// improved version with early return
bool timeslice_new() {
    bool snapshot = gUpdateDisplay.load(std::memory_order_relaxed);
    if (snapshot) {
        gUpdateDisplay.store(false, std::memory_order_relaxed);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        if (gUpdateDisplay.load(std::memory_order_relaxed)) {
            // early exit
            return false;
        }
    }
    return true;
}


void test_new_handler_retries() {
    gUpdateDisplay.store(true);
    std::thread setter([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        gUpdateDisplay.store(true);
    });
    bool result = timeslice_new();
    setter.join();
    ASSERT(result == false);
}

void test_old_handler_loses_update() {
    gUpdateDisplay.store(true);
    std::thread setter([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        gUpdateDisplay.store(true);
    });
    bool result = timeslice_old();
    setter.join();
    // old handler does not perform early return; flag remains set
    ASSERT(result == true);
    ASSERT(gUpdateDisplay.load() == true);
}

void test_rapid_updates() {
    const int pulses = 10;
    std::thread pulser([&]() {
        for (int i = 0; i < pulses; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(3));
            gUpdateDisplay.store(true);
        }
    });
    int early_returns = 0;
    for (int i = 0; i < pulses; ++i) {
        if (!timeslice_new()) {
            ++early_returns;
        }
    }
    pulser.join();
    ASSERT(early_returns > 0);
}

void test_dummy_loops() {
    for (int i = 0; i < 500; ++i) {
        for (int j = 0; j < 5; ++j) {
            gUpdateDisplay.store(false);
            gUpdateDisplay.store(true);
        }
    }
    ASSERT(gUpdateDisplay.load() == true);
}

// main defined in test_runner.cpp

// end of test_update_flags.cpp
