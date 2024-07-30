#include "kaskas/prompt/prompt.hpp"
#include "kaskas/prompt/rpc.hpp"

#include <spine/eventsystem/eventsystem.hpp>
#include <spine/platform/hal.hpp>
#include <unity.h>

#include <climits>

using namespace spn::core;
using namespace kaskas;

class MockController {
public:
    std::unique_ptr<prompt::RPCRecipe> rpc_recipe() {
        using namespace prompt;
        using kaskas::prompt::OptStringView;

        using spn::structure::Array;
        auto model = std::make_unique<RPCRecipe>(
            RPCRecipe("MOC", //
                      {
                          RPCModel("roVariable",
                                   [this](const OptStringView&) {
                                       DBG("roVariable accessed");
                                       return RPCResult(std::to_string(roVariable));
                                   }),
                          RPCModel("rwVariable",
                                   [this](const OptStringView& s) {
                                       const auto s_str = s ? std::string{*s} : std::string();
                                       DBG("rwVariable accessed with arg: {%s}", s_str.c_str());
                                       rwVariable = s ? rwVariable + spn::core::utils::to_double(*s) : rwVariable;
                                       return RPCResult(std::to_string(rwVariable));
                                   }), //
                          RPCModel("foo",
                                   [this](const OptStringView& s) {
                                       if (!s || s->length() == 0) {
                                           return RPCResult(RPCResult::Status::BAD_INPUT);
                                       }
                                       const auto s_str = s ? std::string{*s} : std::string();
                                       DBG("foo called with arg: '%s'", s_str.c_str());

                                       return RPCResult(std::to_string(foo(spn::core::utils::to_double(s_str))));
                                   }),
                      }));
        return std::move(model);
    }

    const double roVariable = 42;
    double rwVariable = 42;

    double foo(double variable) { return variable; }
};

void ut_prompt_basics() {
    using namespace kaskas::prompt;

    auto prompt_cfg = Prompt::Config{.message_length = 64, .pool_size = 20};

    auto dl = std::make_shared<MockDatalink>(
        MockDatalink::Config{.message_length = prompt_cfg.message_length, .pool_size = prompt_cfg.pool_size});
    auto prompt = Prompt(std::move(prompt_cfg));
    prompt.hotload_datalink(dl);

    auto mc = MockController();
    prompt.hotload_rpc_recipe(mc.rpc_recipe());

    prompt.initialize();

    auto bufferpool = dl->bufferpool();
    TEST_ASSERT_NOT_NULL(bufferpool);
    const auto test_f = [&](const char* test_input, const char* expected_reply, bool expected_to_be_valid) {
        const auto test_input_len = strlen(test_input);
        auto buf = bufferpool->acquire();
        TEST_ASSERT(test_input_len < buf->capacity);
        strncpy(buf->raw, test_input, buf->capacity);
        buf->length = test_input_len;

        auto msg = Message::from_buffer(std::move(buf));
        TEST_ASSERT_EQUAL(expected_to_be_valid, msg != std::nullopt);
        if (!expected_to_be_valid)
            return;
        TEST_ASSERT_EQUAL_STRING(test_input, msg->as_string().c_str());

        dl->inject_message(*msg);

        prompt.update();

        const auto reply = dl->extract_reply();
        TEST_ASSERT(reply);
        if (reply) {
            TEST_ASSERT_EQUAL_STRING(expected_reply, reply->as_string().c_str());
        }
    };

    test_f("MOC:roVariable", "MOC<1:42.000000", true);
    test_f("MOC:rwVariable:2", "MOC<1:44.000000", true);
    test_f("MOC:foo:1", "MOC<1:1.000000", true);
    test_f("MOC:foo", "MOC<2:BAD_INPUT", true);
    test_f("MOC:foo", "MOC<2:BAD_INPUT", true);

    test_f(":::", nullptr, false);
    test_f("1::", nullptr, false);
    test_f(":1:", nullptr, false);
    test_f(":::1", nullptr, false);
    test_f("1:1", nullptr, false);
}

/// test if repeat use of the prompt with erroneous input leads to memory corruption (fsanitize must be enabled)
void ut_prompt_stress_testing() {
    using namespace kaskas::prompt;

    auto prompt_cfg = Prompt::Config{.message_length = 64, .pool_size = 20};

    auto dl = std::make_shared<MockDatalink>(
        MockDatalink::Config{.message_length = prompt_cfg.message_length, .pool_size = prompt_cfg.pool_size});
    auto prompt = Prompt(std::move(prompt_cfg));
    prompt.hotload_datalink(dl);

    auto mc = MockController();
    prompt.hotload_rpc_recipe(mc.rpc_recipe());

    prompt.initialize();

    auto bufferpool = dl->bufferpool();
    TEST_ASSERT_NOT_NULL(bufferpool);
    const auto insert_f = [&](const std::string& test_input) { dl->inject_raw_string(test_input); };

    const auto extract_f = [&]() {
        prompt.update();
        const auto m = dl->receive_message();
    };

    insert_f("MOC:roVariable");
    insert_f("MOC:rwVariable:2");
    insert_f("MOC:foo:1");
    insert_f("MOC:foo");
    insert_f("MOC:foo");
    extract_f();

    insert_f("MOC:roVariable\n");
    insert_f("MOC:rwVariable:2\n");
    insert_f("MOC:foo:1\n");
    insert_f("MOC:foo\n");
    insert_f("MOC:foo\n");
    extract_f();

    insert_f("MOC:roVariable\n\r");
    insert_f("MOC:rwVariable:2\n\r");
    insert_f("MOC:foo:1\n\r");
    insert_f("MOC:foo\n\r");
    insert_f("MOC:foo\n\r");
    extract_f();

    insert_f(":::");
    insert_f("1::");
    insert_f(":1:");
    insert_f(":::1");
    insert_f("1:1");
    extract_f();

    insert_f("MOC:roVariable");
    insert_f("MOC:rwVariable:2");
    insert_f("MOC:foo:1");
    insert_f("MOC:foo");
    insert_f("MOC:foo");
    insert_f("MOC:foo\nMOC:foo");
    insert_f("MOC:foo\rMOC:foo");
    insert_f("MOC:foo\nMOC:foo\n");
    insert_f("\rMOC:foo\nMOC:foo\n");

    insert_f(":::");
    insert_f("1::");
    insert_f(":1:");
    insert_f(":::1");
    insert_f("1:1");
    extract_f();

    insert_f("");
    extract_f();
    insert_f("\n");
    extract_f();
    insert_f("\r");
    extract_f();

    insert_f("");
    insert_f("\n");
    insert_f("\r");
    extract_f();

    insert_f(":a\n:b");
    insert_f("\n:\r:\n");
    extract_f();
}

int run_all_tests() {
    UNITY_BEGIN();
    RUN_TEST(ut_prompt_basics);
    RUN_TEST(ut_prompt_stress_testing);
    return UNITY_END();
}

#if defined(ARDUINO) && defined(EMBEDDED)
#    include <Arduino.h>
void setup() {
    // NOTE!!! Wait for >2 secs
    // if board doesn't support software reset via Serial.DTR/RTS
    delay(2000);

    run_all_tests();
}

void loop() {}
#elif defined(ARDUINO)
#    include <ArduinoFake.h>
#endif

int main(int argc, char** argv) {
    run_all_tests();
    return 0;
}
