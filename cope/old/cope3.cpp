#include <coroutine>
#include <format>
#include "cope.h"

using namespace std;

/////////////////////////////////////////////////////////////////////

template<typename PromiseType>
class txn_handler_t {
public:
    using promise_type = PromiseType;
    using this_t = txn_handler_t<PromiseType>;
    using handle_t = std::coroutine_handle<promise_type>;

    struct awaitable
    {
        using promise_t = promise_type;

        promise_t* promise_;

        constexpr bool await_ready() { return false; }
        void await_suspend(this_t::handle_t h) { promise_ = &h.promise(); }
        promise_t& await_resume() { return *promise_; }
    };

public:
    txn_handler_t(this_t& other) = delete;
    txn_handler_t operator=(this_t& other) = delete;

    txn_handler_t(this_t&& other) noexcept : coro_handle_(std::exchange(other.coro_handle_, nullptr)) {}
    txn_handler_t& operator=(this_t&& other) noexcept {
        coro_handle_ = std::exchange(other.coro_handle_, nullptr);
        return *this;
    }

    explicit txn_handler_t(promise_type* p) noexcept : coro_handle_(handle_t::from_promise(*p)) {}
    ~txn_handler_t() { if (coro_handle_) coro_handle_.destroy(); }

    handle_t handle() const noexcept { return coro_handle_; }

private:
    handle_t coro_handle_;
};

struct AwaitPromiseType
{
    using handler_t = txn_handler_t<AwaitPromiseType>;

    int val_ = -1;

    std::suspend_always initial_suspend() noexcept { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    handler_t get_return_object() noexcept { return handler_t{ this }; }
    void unhandled_exception() { throw; }
    void return_void() { throw std::runtime_error("co_return (void) is not supported."); }
};

struct YieldPromiseType
{
    using handler_t = txn_handler_t<YieldPromiseType>;

    int val_ = -1;

    std::suspend_never initial_suspend() noexcept { puts("YPT: initial_suspend");  return {}; }
    std::suspend_always final_suspend() noexcept { puts("YPT: final_suspend");  return {}; }
    handler_t get_return_object() noexcept { puts("YPT: get_return_object");  return handler_t{ this }; }
    void unhandled_exception() { throw; }
    std::suspend_never yield_value(int value) {
        val_ = value;
        puts(std::format("yield_value: set value to {}", val_).c_str());
        return {};
    }
    void return_void() { throw std::runtime_error("co_return (void) is not supported."); }
};

txn_handler_t<YieldPromiseType>::awaitable promise_event;

txn_handler_t<YieldPromiseType>
txhandlerYield()
{
    while (true) {
        puts(std::format("txnHandlerYield before co_await").c_str());
        auto& promise = co_await promise_event;
        puts(std::format("txnHandlerYield after co_await: {}", promise.val_).c_str());
        co_yield 42;
    }
}

int main()
{
    puts("---");
    txn_handler_t<YieldPromiseType> tx_yield{ txhandlerYield() };
    tx_yield.handle().promise().val_ = 21;
    puts(std::format("main promise before resume: {}", tx_yield.handle().promise().val_).c_str());
    tx_yield.handle().resume();
    puts(std::format("main promise after resume: {}", tx_yield.handle().promise().val_).c_str());

    return 0;
}
