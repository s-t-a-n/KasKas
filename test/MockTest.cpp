#include <limits.h>
#include <unity.h>

namespace {

void ut_basics() { TEST_ASSERT_EQUAL(0, 0); }

void run_all_tests() { RUN_TEST(ut_basics); }

} // namespace

#ifdef ARDUINO

#    include <Arduino.h>
void setup() {
    // NOTE!!! Wait for >2 secs
    // if board doesn't support software reset via Serial.DTR/RTS
    delay(2000);

    run_all_tests();
}

void loop() {}

#else

int main(int argc, char** argv) {
    run_all_tests();
    return 0;
}

#endif
