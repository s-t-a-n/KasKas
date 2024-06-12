#if defined(ARDUINO) && defined(EMBEDDED)
#    include <Arduino.h>
#elif defined(UNITTEST)
#    include <ArduinoFake.h>
#endif

#include "kaskas/prompt/prompt.hpp"
#include "kaskas/prompt/rpc.hpp"

#include <limits.h>
#include <spine/eventsystem/eventsystem.hpp>
#include <unity.h>

using namespace spn::core;

#if defined(ARDUINO) && !defined(EMBEDDED)
using namespace fakeit;
#endif

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
                                       DBGF("roVariable accessed");
                                       return RPCResult(std::to_string(roVariable));
                                   }),
                          RPCModel("rwVariable",
                                   [this](const OptStringView& s) {
                                       const auto s_str = s ? std::string{*s} : std::string();
                                       DBGF("rwVariable accessed with arg: {%s}", s_str.c_str());
                                       rwVariable = s ? rwVariable + std::stod(std::string(*s)) : rwVariable;
                                       return RPCResult(std::to_string(rwVariable));
                                   }), //
                          RPCModel("foo",
                                   [this](const OptStringView& s) {
                                       if (!s || s->length() == 0) {
                                           return RPCResult(RPCResult::State::BAD_INPUT);
                                       }
                                       const auto s_str = s ? std::string{*s} : std::string();
                                       DBGF("foo called with arg: '%s'", s_str.c_str());

                                       return RPCResult(std::to_string(foo(std::stod(s_str))));
                                   }),
                      }));
        return std::move(model);
    }

    const double roVariable = 42;
    double rwVariable = 42;

    double foo(double variable) { return roVariable + rwVariable + variable; }
};

void ut_prompt_basics() {
    // ARDUINO_MOCK_ALL();

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
    const auto test_f = [&](const char* test_input, const char* expected_reply) {
        const auto test_input_len = strlen(test_input);
        auto buf = bufferpool->acquire();
        TEST_ASSERT(test_input_len < buf->capacity);
        strncpy(buf->raw, test_input, buf->capacity);
        buf->length = test_input_len;

        auto msg = Message::from_buffer(std::move(buf));
        TEST_ASSERT_EQUAL_STRING(msg->as_string().c_str(), test_input);
        TEST_ASSERT(msg != std::nullopt);

        dl->inject_message(*msg);
        prompt.update();

        const auto reply = dl->extract_reply();
        TEST_ASSERT(reply);
        if (reply) {
            TEST_ASSERT_EQUAL_STRING(expected_reply, reply->as_string().c_str());
        }
    };

    test_f("MOC!foo:1", "MOC<0:85.000000");
    test_f("MOC?roVariable", "MOC<0:42.000000");
    test_f("MOC=rwVariable:2", "MOC<0:44.000000");

    test_f("MOC!foo:", "MOC<1:BAD_INPUT");
}

int run_all_tests() {
    UNITY_BEGIN();
    RUN_TEST(ut_prompt_basics);
    return UNITY_END();
}

void setup() {
    // NOTE!!! Wait for >2 secs
    // if board doesn't support software reset via Serial.DTR/RTS
    delay(2000);

    run_all_tests();
}

void loop() {}

int main(int argc, char** argv) {
    run_all_tests();
    return 0;
}
