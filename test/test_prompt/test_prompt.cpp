#include "kaskas/prompt/prompt.hpp"
#include "kaskas/prompt/rpc.hpp"

#include <spine/eventsystem/eventsystem.hpp>
#include <spine/io/stream/implementations/mock.hpp>
#include <spine/platform/hal.hpp>
#include <unity.h>

#include <climits>
#include <vector>

using namespace spn::core;
using namespace spn::io;
using namespace kaskas;
using namespace kaskas::prompt;

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

struct ResponsePattern {
    const char* input;
    const char* expected_response;
    bool expected_to_be_valid_message; // if it is a syntactically correct message
    bool expected_to_be_valid_mock_request; // if it is a semantically correct message (based on MockController above)
};

const auto g_pts = std::vector<ResponsePattern>{
    {"MOC:roVariable\n", "MOC<OK:42.000000", true, true},
    {"MOC:roVariable\r", "MOC<OK:42.000000", true, true},
    {"MOC:rwVariable:2\n", "MOC<OK:44.000000", true, true},
    {"MOC:foo:1\n", "MOC<OK:1.000000", true, true},
    {"MOC:foo\n", "MOC<BAD_INPUT", true, true},
    {"MOC:foo\n", "MOC<BAD_INPUT", true, true},
    {":::\n", nullptr, false, false},
    {"1::\n", "", true, false},
    {":1:\n", nullptr, false, false},
    {":::1\n", nullptr, false, false},
    {"1:1\n", "", true, false},
};

/// Mockstream is used in every test. For Mockstream's tests, see Spine.
const auto g_ms_io_buffer_size = 1024;
std::shared_ptr<MockStream> g_ms = nullptr;

const auto g_dl_cfg = Datalink::Config{
    .input_buffer_size = g_ms_io_buffer_size, .output_buffer_size = g_ms_io_buffer_size, .delimiters = "\r\n"};
std::shared_ptr<Datalink> g_dl = nullptr;
std::unique_ptr<MockController> g_mc = nullptr;
std::unique_ptr<Prompt> g_prompt = nullptr;

const auto prompt_cfg = Prompt::Config{.io_buffer_size = g_ms_io_buffer_size, .line_delimiters = "\r\n"};

void setUp(void) {
    g_ms = std::make_shared<MockStream>(
        MockStream::Config{.input_buffer_size = g_ms_io_buffer_size, .output_buffer_size = g_ms_io_buffer_size});
    g_ms->initialize();

    g_dl = std::make_shared<Datalink>(g_ms, g_dl_cfg);
    g_prompt = std::make_unique<Prompt>(std::move(prompt_cfg));
    g_prompt->hotload_datalink(g_dl);

    g_mc = std::make_unique<MockController>();
    g_prompt->hotload_rpc_recipe(g_mc->rpc_recipe());

    g_prompt->initialize();
}

void tearDown(void) {
    g_ms.reset();
    g_dl.reset();
    g_mc.reset();
    g_prompt.reset();
}

void ut_prompt_test_datalink() {
    const std::string reply_ok = "OK";
    const std::string reply_nok = "NOK";
    const auto test_f = [&](std::string test_input, bool expected_to_be_valid) {
        const auto byte_stream = std::vector<uint8_t>(test_input.begin(), test_input.end());
        g_ms->inject_bytestream(byte_stream);

        // pull in next transaction from datalink and verify that it has read a correct linelength
        TEST_ASSERT_EQUAL_MESSAGE(test_input.size(), g_dl->pull(), test_input.c_str());
        auto transaction = g_dl->incoming_transaction();
        TEST_ASSERT_EQUAL_MESSAGE(true, bool(transaction), test_input.c_str());

        // push out a reply through the transaction
        const auto reply = expected_to_be_valid ? reply_ok : reply_nok;
        transaction->outgoing(reply);
        transaction->commit();
        TEST_ASSERT_EQUAL_MESSAGE(reply.size(), g_dl->push(), test_input.c_str());

        // verify that reply was sent
        const auto reply_bytestream = g_ms->extract_bytestream();
        TEST_ASSERT_EQUAL_MESSAGE(true, bool(reply_bytestream), test_input.c_str());
        TEST_ASSERT_EQUAL_STRING(reply.c_str(),
                                 std::string(reply_bytestream->begin(), reply_bytestream->end()).c_str());
    };

    for (auto& rp : g_pts) {
        test_f(rp.input, rp.expected_to_be_valid_message);
    }
}

void ut_prompt_test_incoming_message_factory() {
    const std::string reply_ok = "OK";
    const std::string reply_nok = "NOK";
    const auto test_f = [&](std::string test_input, bool expected_to_be_valid) {
        // verify that an incoming message is recognized
        const auto msg = IncomingMessageFactory::from_view(test_input);
        TEST_ASSERT_EQUAL_MESSAGE(expected_to_be_valid, bool(msg), test_input.c_str());
        if (expected_to_be_valid) { // verify that a message was properly constructed from the input
            TEST_ASSERT_EQUAL_STRING(test_input.c_str(), msg->as_string().c_str());
        }
    };

    for (auto& rp : g_pts) {
        test_f(rp.input, rp.expected_to_be_valid_message);
    }
}

void ut_prompt_test_outgoing_message_factory() {
    const std::string module = "MOD";
    struct OutgoingResponsePattern {
        RPCResult result;
        std::string expected_response;
    };

    auto test_f = [&](const OutgoingResponsePattern& pattern) {
        auto msg = OutgoingMessageFactory::from_result(RPCResult(pattern.result), module);
        TEST_ASSERT_EQUAL_MESSAGE(true, bool(msg), pattern.expected_response.c_str());
        TEST_ASSERT_EQUAL_STRING(pattern.expected_response.c_str(), msg->as_string().c_str());
    };

    const auto patterns =
        std::vector<OutgoingResponsePattern>{{RPCResult("MY_RESULT"), "MOD<OK:MY_RESULT"},
                                             {RPCResult(RPCResult::Status::OK), "MOD<OK"},
                                             {RPCResult(RPCResult::Status::BAD_INPUT), "MOD<BAD_INPUT"},
                                             {RPCResult(RPCResult::Status::BAD_RESULT), "MOD<BAD_RESULT"},
                                             {RPCResult("NOK", RPCResult::Status::BAD_INPUT), "MOD<BAD_INPUT:NOK"}};

    for (const auto& pattern : patterns) {
        test_f(pattern);
    }
}

void ut_prompt_test_rpc_factory() {
    const auto factory_cfg = RPCFactory::Config{.directory_size = 32};
    auto factory = RPCFactory(factory_cfg);
    factory.hotload_rpc_recipe(g_mc->rpc_recipe());

    const auto test_f = [&](const Message& incoming_msg, const std::string& expected_response,
                            bool expected_to_be_valid) {
        auto rpc = factory.from_message(incoming_msg);
        TEST_ASSERT_EQUAL_MESSAGE(expected_to_be_valid, bool(rpc), incoming_msg.as_string().c_str());
        if (expected_to_be_valid) {
            auto result = rpc->invoke();
            TEST_ASSERT_EQUAL_STRING(expected_response.c_str(), result.as_string().c_str());
        }
    };

    for (auto& rp : g_pts) {
        if (rp.expected_to_be_valid_message) { // only legal messages are useful here
            std::string delimiter_stripped_input = rp.input;
            delimiter_stripped_input.erase(delimiter_stripped_input.find_last_not_of("\n\r") + 1);

            std::string expected_response_stripped = rp.expected_response;
            expected_response_stripped.erase(0, expected_response_stripped.find_first_of("<") + 1);
            auto msg = IncomingMessageFactory::from_view(delimiter_stripped_input);
            TEST_ASSERT(msg.has_value()); // make the IncomingMessageFactory does it's job
            test_f(*msg, expected_response_stripped, rp.expected_to_be_valid_mock_request);
        }
    }
}

void ut_prompt_test_integration() {
    const auto test_f = [&](std::string test_input, const char* expected_reply_raw, bool expected_to_be_valid) {
        auto stripped_test_input = test_input;
        stripped_test_input.erase(stripped_test_input.find_last_not_of("\n\r") + 1);
        const auto byte_stream = std::vector<uint8_t>(test_input.begin(), test_input.end());
        g_ms->inject_bytestream(byte_stream);

        g_prompt->update();

        auto reply = g_ms->extract_bytestream();
        if (expected_to_be_valid) {
            const auto expected_reply = std::string(expected_reply_raw) + std::string(Dialect::REPLY_CR);
            auto reply_as_string = std::string(reply->begin(), reply->end());
            TEST_ASSERT_EQUAL_STRING(expected_reply.c_str(), reply_as_string.c_str());
        }

        // extract empty lines
        while (g_ms->available()) {
            const auto residu = g_ms->extract_bytestream();
            TEST_ASSERT_EQUAL_MESSAGE(true, bool(residu), expected_reply_raw);
            TEST_ASSERT_EQUAL_STRING("", std::string(residu->begin(), residu->end()).c_str());
        }
    };

    for (auto& rp : g_pts) {
        test_f(rp.input, rp.expected_response, rp.expected_to_be_valid_mock_request);
    }
}

/// test if repeat use of the prompt with erroneous input leads to memory corruption (fsanitize must be enabled)
// void ut_prompt_stress_testing() {
//     using namespace kaskas::prompt;
//
//     auto prompt_cfg = Prompt::Config{.message_length = 64, .pool_size = 20};
//
//     auto dl = std::make_shared<MockDatalink>(
//         MockDatalink::Config{.message_length = prompt_cfg.message_length, .pool_size = prompt_cfg.pool_size});
//     auto prompt = Prompt(std::move(prompt_cfg));
//     prompt.hotload_datalink(dl);
//
//     auto mc = MockController();
//     prompt.hotload_rpc_recipe(mc.rpc_recipe());
//
//     prompt.initialize();
//
//     auto bufferpool = dl->bufferpool();
//     TEST_ASSERT_NOT_NULL(bufferpool);
//     const auto insert_f = [&](const std::string& test_input) { dl->inject_raw_string(test_input); };
//
//     const auto extract_f = [&]() {
//         prompt.update();
//         const auto m = dl->receive_message();
//     };
//
//     insert_f("MOC:roVariable");
//     insert_f("MOC:rwVariable:2");
//     insert_f("MOC:foo:1");
//     insert_f("MOC:foo");
//     insert_f("MOC:foo");
//     extract_f();
//
//     insert_f("MOC:roVariable\n");
//     insert_f("MOC:rwVariable:2\n");
//     insert_f("MOC:foo:1\n");
//     insert_f("MOC:foo\n");
//     insert_f("MOC:foo\n");
//     extract_f();
//
//     insert_f("MOC:roVariable\n\r");
//     insert_f("MOC:rwVariable:2\n\r");
//     insert_f("MOC:foo:1\n\r");
//     insert_f("MOC:foo\n\r");
//     insert_f("MOC:foo\n\r");
//     extract_f();
//
//     insert_f(":::");
//     insert_f("1::");
//     insert_f(":1:");
//     insert_f(":::1");
//     insert_f("1:1");
//     extract_f();
//
//     insert_f("MOC:roVariable");
//     insert_f("MOC:rwVariable:2");
//     insert_f("MOC:foo:1");
//     insert_f("MOC:foo");
//     insert_f("MOC:foo");
//     insert_f("MOC:foo\nMOC:foo");
//     insert_f("MOC:foo\rMOC:foo");
//     insert_f("MOC:foo\nMOC:foo\n");
//     insert_f("\rMOC:foo\nMOC:foo\n");
//
//     insert_f(":::");
//     insert_f("1::");
//     insert_f(":1:");
//     insert_f(":::1");
//     insert_f("1:1");
//     extract_f();
//
//     insert_f("");
//     extract_f();
//     insert_f("\n");
//     extract_f();
//     insert_f("\r");
//     extract_f();
//
//     insert_f("");
//     insert_f("\n");
//     insert_f("\r");
//     extract_f();
//
//     insert_f(":a\n:b");
//     insert_f("\n:\r:\n");
//     extract_f();
// }

int run_all_tests() {
    UNITY_BEGIN();
    RUN_TEST(ut_prompt_test_datalink);
    RUN_TEST(ut_prompt_test_incoming_message_factory);
    RUN_TEST(ut_prompt_test_outgoing_message_factory);
    RUN_TEST(ut_prompt_test_rpc_factory);
    RUN_TEST(ut_prompt_test_integration);
    //    RUN_TEST(ut_prompt_basics);
    //    RUN_TEST(ut_prompt_stress_testing);
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
