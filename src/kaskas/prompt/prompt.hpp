#pragma once

#include <magic_enum/magic_enum.hpp>
#include <magic_enum/magic_enum_all.hpp>
#include <spine/core/assert.hpp>
#include <spine/core/debugging.hpp>
#include <spine/platform/hal.hpp>
#include <spine/structure/list.hpp>
#include <spine/structure/pool.hpp>

#include <AH/STL/memory>
#include <cstring>
#include <initializer_list>
#include <optional>
#include <string_view>
#include <variant>

namespace kaskas::prompt {

class CharBuffer {
private:
    const std::unique_ptr<char> _buffer;

public:
    char* const raw;
    const size_t capacity;
    size_t length = 0;

    explicit CharBuffer(size_t buffer_length)
        : _buffer(std::unique_ptr<char>(new char[buffer_length])), raw(_buffer.get()), capacity(buffer_length) {}
};

struct Dialect {
    enum class OP { NOP, FUNCTION_CALL, ASSIGNMENT, ACCESS, RETURN_VALUE, SIZE };
    static OP optype_for_operant(const char operant) {
        magic_enum::enum_for_each<OP>([operant](OP optype) {
            if (operant == magic_enum::enum_index(optype)) {
                return optype;
            }
        });
    }

    // todo: constexpr from OPERANTS
    static constexpr auto OPERANTS = "!=?<";
    static constexpr auto MINIMAL_CMD_LENGTH = 2;
    static constexpr auto MAXIMAL_CMD_LENGTH = 3;
};

class RPCResult;

class Message {
public:
    /*
     * The following patterns are legal messages:
     * CMD=             : list all variables than can be set for CMD
     * CMD?             : list all variable that can be requested for CMD
     * CMD!             : list all functions that can be called for CMD
     * CMD=var:1        : set a variable
     * CMD?var          : request a variable
     * CMD!function     : function call
     * CMD<arguments    : reply to request
     */

    const std::string_view& cmd() const { return _cmd; }
    const std::string_view& operant() const { return _operant; }
    const std::string_view& argument() const { return _argument; }

    static std::optional<Message> from_buffer(std::shared_ptr<CharBuffer>&& buffer) {
        assert(buffer->capacity > 0);

        // ensure null termination
        buffer->raw[buffer->capacity - 1] = '\0';
        const auto operant_idx = strcspn(buffer->raw, Dialect::OPERANTS);

        if (operant_idx == buffer->length - 1) {
            // no operant found
            return std::nullopt;
        }
        if (operant_idx < Dialect::MINIMAL_CMD_LENGTH || operant_idx > Dialect::MAXIMAL_CMD_LENGTH) {
            // command size is too small or too big
            return std::nullopt;
        }

        Message m;
        m._cmd = std::string_view(buffer->raw, operant_idx);
        m._operant = std::string_view(buffer->raw + operant_idx, 1);
        assert(operant_idx + 1 >= buffer->length);
        m._argument = std::string_view(buffer->raw + operant_idx + 1, buffer->length - operant_idx - 1);
        m._buffer = std::move(buffer);
        return m;
    }

    static std::optional<Message> from_result(std::shared_ptr<CharBuffer>&& buffer, const RPCResult& result) {
        assert(buffer->capacity > 0);

        Message m;
    }

    // protected:
    // Message() = default;

private:
    std::string_view _cmd;
    std::string_view _operant;
    std::string_view _argument;

    std::shared_ptr<CharBuffer> _buffer;
};

using spn::structure::Pool;

class Datalink {
public:
    struct Config {
        const size_t message_length;
        size_t pool_size;
    };

    Datalink(const Config&& cfg) : _cfg(cfg), _bufferpool(nullptr) {}

    void hotload_bufferpool(const std::shared_ptr<Pool<CharBuffer>>& bufferpool) { _bufferpool = bufferpool; }

public:
    virtual ~Datalink() = default;
    virtual void initialize() = 0;
    virtual void send_message(const Message& msg) = 0;
    virtual std::optional<Message> receive_message() = 0;

protected:
    Config _cfg;
    std::shared_ptr<Pool<CharBuffer>> _bufferpool;
};

class SerialDatalink : public Datalink {
public:
    SerialDatalink(const Config&& cfg, HAL::UART&& uart) : Datalink(std::move(cfg)), _uart(uart){};
    ~SerialDatalink() override = default;

public:
    void initialize() override { assert(_bufferpool); }

    void send_message(const Message& msg) override {
        _uart.write(reinterpret_cast<const uint8_t*>(msg.cmd().data()), msg.cmd().length());
        _uart.write(reinterpret_cast<const uint8_t*>(msg.operant().data()), msg.operant().length());
        _uart.write(reinterpret_cast<const uint8_t*>(msg.argument().data()), msg.argument().length());
        _uart.write('\n');
    }
    std::optional<Message> receive_message() override {
        if (_uart.available() == 0)
            return std::nullopt;
        assert(_bufferpool);
        auto buffer = _bufferpool->acquire();
        if (!buffer)
            return std::nullopt;
        buffer->length = _uart.read(reinterpret_cast<uint8_t*>(buffer->raw), buffer->capacity);
        return Message::from_buffer(std::move(buffer));
    }

private:
    HAL::UART& _uart;
};

class MockDatalink : public Datalink {
public:
    MockDatalink(const Config&& cfg) : Datalink(std::move(cfg)), _stdin(_cfg.pool_size), _stdout(_cfg.pool_size){};
    ~MockDatalink() override = default;

public:
    void initialize() override { assert(_bufferpool); }

    void send_message(const Message& msg) override { send_message_impl(msg, _stdout); }
    std::optional<Message> receive_message() override { return receive_message_impl(_stdin); }

    void inject_message(Message& msg) { send_message_impl(msg, _stdin); }
    std::optional<Message> extract_reply() { return receive_message_impl(_stdout); }

    std::shared_ptr<Pool<CharBuffer>>& bufferpool() { return _bufferpool; }

protected:
    using Vector = spn::structure::Vector<std::string>;

    void send_message_impl(const Message& msg, Vector& out) {
        std::string s =
            std::string(msg.cmd()) + std::string(msg.operant()) + std::string(msg.argument()) + std::string("\n");
        out.push_back(s);
    }

    std::optional<Message> receive_message_impl(Vector& in) {
        if (_stdin.empty())
            return std::nullopt;
        assert(_bufferpool);
        auto buffer = _bufferpool->acquire();
        if (!buffer)
            return std::nullopt;
        const auto s = _stdin.pop_front();
        std::memcpy(buffer->raw, s.c_str(), s.length());
        buffer->length = s.length();
        return Message::from_buffer(std::move(buffer));
    }

private:
    Vector _stdin;
    Vector _stdout;
};

using OptString = std::optional<std::string>;
using OptStringView = std::optional<std::string_view>;
using OptInt = std::optional<int>;

struct RPCResult {
    RPCResult(const RPCResult& other) : return_value(other.return_value), status(other.status) {}
    RPCResult(RPCResult&& other) noexcept : return_value(other.return_value), status(std::move(other.status)) {}
    RPCResult& operator=(const RPCResult& other) {
        if (this == &other)
            return *this;
        return_value = other.return_value;
        status = other.status;
        return *this;
    }
    RPCResult& operator=(RPCResult&& other) noexcept {
        if (this == &other)
            return *this;
        return_value = other.return_value;
        status = std::move(other.status);
        return *this;
    }
    enum class State { OK, BAD_INPUT, BAD_RESULT };

    RPCResult(OptStringView&& return_value)
        : return_value(std::move(return_value)), status(magic_enum::enum_index(State::OK)) {}
    RPCResult(OptStringView&& return_value, uint8_t status) : return_value(std::move(return_value)), status(status) {}
    RPCResult(uint8_t status) : return_value(std::nullopt), status(status) {}
    RPCResult(State state)
        : return_value(std::string(magic_enum::enum_name(state))), status(magic_enum::enum_index(state)) {}

    OptString return_value;
    OptInt status;
};

class RPCModel {
public:
    explicit RPCModel(const std::string_view& name) : _name(name) {}
    RPCModel(const std::string_view& name, const std::function<RPCResult(const OptString&)>& call)
        : _name(name), _call(call) {}

    std::string_view name() const { return _name; }
    std::function<RPCResult(const OptString&)> call() const { return _call; }

private:
    const std::string_view _name;
    const std::function<RPCResult(const OptString&)> _call;
};

// stubs
class RWVariable : public RPCModel {
public:
    using RPCModel::RPCModel;
};

class ROVariable : public RPCModel {
public:
    using RPCModel::RPCModel;
};

class FunctionCall : public RPCModel {
public:
    using RPCModel::RPCModel;
};

using spn::structure::List;

struct RPCRecipe {
    using RPCVariant = std::variant<RWVariable, ROVariable, FunctionCall>;

    RPCRecipe(const char* command, const std::initializer_list<const RPCVariant>& rpcs)
        : _command(command), _rpcs(std::move(rpcs)){};

    std::optional<const RPCVariant> rpc_from_arguments(const std::string_view& arguments) const {
        for (const auto& rpc_v : _rpcs) {
            const auto cmd = std::visit([](auto&& v) { return v.name(); }, rpc_v).data();
        }
    }
    // protected:
    // private:
    const char* _command;
    const std::initializer_list<const RPCVariant> _rpcs;
};

class RPCFactory;

static std::optional<const RPCModel*> model_for_op(const RPCRecipe::RPCVariant& v, Dialect::OP op) {
    switch (op) {
    case Dialect::OP::ASSIGNMENT: return &std::get<RWVariable>(v);
    case Dialect::OP::ACCESS: return &std::get<ROVariable>(v);
    case Dialect::OP::FUNCTION_CALL: return &std::get<FunctionCall>(v);
    default: return {};
    }
}

class RPC {
public:
    const Dialect::OP op;
    const RPCRecipe::RPCVariant model;

    const OptStringView key;
    const OptStringView value;

    RPCResult invoke() const {
        const auto& m = model_for_op(model, op);
        assert(m);
        const auto model = *m;
        const auto args = value ? std::make_optional(std::string(*value)) : std::nullopt;
        const auto res = model->call()(args);
        return res;
    }

public:
protected:
    RPC(Dialect::OP op, const RPCRecipe::RPCVariant& model) : op(op), model(model) {}

    friend RPCFactory;
};

class RPCFactory {
public:
    struct Config {
        size_t directory_size = 32;
    };
    RPCFactory(const Config&& cfg) : _cfg(cfg), _rpcs(_cfg.directory_size) {}

    std::optional<RPC> from_message(const Message& msg) {
        assert(msg.operant().length() == 1);
        const auto recipe = recipe_for_command(msg.cmd());
        if (!recipe)
            return {};
        assert(*recipe != nullptr);

        const auto optype = Dialect::optype_for_operant(msg.operant()[0]);
        switch (optype) {
        case Dialect::OP::ACCESS: return build_ro_variable(**recipe, optype, msg.argument());
        case Dialect::OP::ASSIGNMENT: return build_rw_variable(**recipe, optype, msg.argument());
        case Dialect::OP::FUNCTION_CALL: return build_function_call(**recipe, optype, msg.argument());
        default: return {};
        }
    }

protected:
    std::optional<RPC> build_function_call(const RPCRecipe& recipe, Dialect::OP optype,
                                           const std::string_view& arguments) {}
    std::optional<RPC> build_ro_variable(const RPCRecipe& recipe, Dialect::OP optype,
                                         const std::string_view& arguments) {
        const auto seperator_idx = arguments.find(':');
        if (seperator_idx == std::string::npos) {
            // no separator found
            return {};
        }
        // found separator
        const auto key = arguments.substr(0, seperator_idx);
        const auto value = arguments.substr(seperator_idx, arguments.length());

        const auto model =
            std::find_if(std::begin(recipe._rpcs), std::end(recipe._rpcs), [key](const RPCRecipe::RPCVariant& m) {
                const auto name = std::visit([](auto&& v) { return v.name(); }, m);
                return key == name;
            });
        if (model == std::end(recipe._rpcs)) {
            // no model found
            return {};
        }
        return RPC(optype, *model);
    }
    std::optional<RPC> build_rw_variable(const RPCRecipe& recipe, Dialect::OP optype,
                                         const std::string_view& arguments) {
        const auto seperator_idx = arguments.find(':');
        if (seperator_idx == std::string::npos) {
            // no separator found
            return {};
        }
        // found separator
        const auto key = arguments.substr(0, seperator_idx);
        const auto value = arguments.substr(seperator_idx, arguments.length());

        const auto model =
            std::find_if(std::begin(recipe._rpcs), std::end(recipe._rpcs), [key](const RPCRecipe::RPCVariant& m) {
                const auto name = std::visit([](auto&& v) { return v.name(); }, m);
                return key == name;
            });
        if (model == std::end(recipe._rpcs)) {
            // no model found
            return {};
        }
        return RPC(optype, *model);
    }

    std::optional<const RPCRecipe*> recipe_for_command(const std::string_view& cmd) {
        for (auto& recipe : _rpcs) {
            if (std::string_view(recipe->_command) == cmd) {
                const auto cmd_str = std::string(cmd);
                DBGF("RPCHandler: found recipe: %s for %s", recipe->_command, cmd_str.c_str())
                return recipe.get();
            }
        }
        return {};
    }

    void hotload_rpc_recipe(std::unique_ptr<RPCRecipe> recipe) { _rpcs.push_back(std::move(recipe)); }

private:
    const Config _cfg;

    std::vector<std::unique_ptr<RPCRecipe>> _rpcs;
};

class Prompt {
public:
    struct Config {
        size_t message_length;
        size_t pool_size;
    };

    Prompt(const Config&& cfg)
        : _cfg(cfg), _rpc_factory(RPCFactory::Config{.directory_size = 16}),
          _bufferpool(std::make_shared<Pool<CharBuffer>>(_cfg.pool_size)) {
        for (int i = 0; i < _cfg.pool_size; ++i) {
            _bufferpool->populate(new CharBuffer(_cfg.message_length));
        }
        assert(_bufferpool->is_fully_populated());
    }

public:
    void initialize() { _dl->initialize(); }
    void update() {
        if (const auto message = _dl->receive_message()) {
            if (const auto rpc = _rpc_factory.from_message(*message)) {
                const auto res = rpc->invoke();
                const auto reply = Message::from_result(_bufferpool->acquire(), res);
            }
        }
    }

    void hotload_datalink(std::unique_ptr<Datalink> dl) {
        assert(_dl == nullptr);
        _dl.swap(dl);
        assert(dl == nullptr);

        assert(_bufferpool.get() != nullptr);
        _dl->hotload_bufferpool(_bufferpool);
    }
    void hotload_rpc_recipe(std::unique_ptr<RPCRecipe> recipe) {}

private:
    const Config _cfg;

    RPCFactory _rpc_factory;

    std::unique_ptr<Datalink> _dl;
    std::shared_ptr<Pool<CharBuffer>> _bufferpool;
};

} // namespace kaskas::prompt