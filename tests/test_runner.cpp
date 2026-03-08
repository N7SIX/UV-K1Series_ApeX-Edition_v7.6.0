#include <cstdio>

// declare test functions from each file
void test_volatile_flag_observed();
void test_nonvolatile_may_miss();
void test_multiple_isr_pulses();
void test_stress_simulation();

void test_fillscreen_basic();
void test_fillscreen_values();
void test_stress_loop();
void test_dummy_expansion();

void test_new_handler_retries();
void test_old_handler_loses_update();
void test_rapid_updates();
void test_dummy_loops();

int main() {
    printf("Beginning unit test suite\n");
    test_volatile_flag_observed();
    test_nonvolatile_may_miss();
    test_multiple_isr_pulses();
    test_stress_simulation();
    printf("scheduler tests passed\n");

    test_fillscreen_basic();
    test_fillscreen_values();
    test_stress_loop();
    test_dummy_expansion();
    printf("st7565 tests passed\n");

    test_new_handler_retries();
    test_old_handler_loses_update();
    test_rapid_updates();
    test_dummy_loops();
    printf("update_flags tests passed\n");

    printf("All unit tests passed\n");
    return 0;
}
