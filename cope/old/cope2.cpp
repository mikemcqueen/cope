#include <coroutine>
#include <format>
#include "cope.h"

using namespace std;

class txn_handler {
public:
    struct promise_type
    {
        int val_ = -1;
        std::string str_;

        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        txn_handler get_return_object() noexcept { return txn_handler{ this }; }
        void unhandled_exception() { throw; }
        std::suspend_never yield_value(int value) {
            val_ = value;
            return {};
        }

        void return_void() { throw std::runtime_error("co_return (void) is not supported."); }
    };

public:
    txn_handler(txn_handler& other) = delete;
    txn_handler operator=(txn_handler& other) = delete;

    txn_handler(txn_handler&& other) noexcept : coro_handle_(std::exchange(other.coro_handle_, nullptr)) {}
    txn_handler& operator=(txn_handler&& other) noexcept {
        coro_handle_ = std::exchange(other.coro_handle_, nullptr);
        return *this;
    }
    explicit txn_handler(promise_type* p) noexcept : coro_handle_(handle_t::from_promise(*p)) {}

    ~txn_handler() {
        if (coro_handle_) {
            coro_handle_.destroy();
        }
    }

    using handle_t = std::coroutine_handle<promise_type>;

    handle_t handle() const noexcept { return coro_handle_; }

private:
    handle_t coro_handle_;
};

struct promise_awaitable
{
    using promise_t = txn_handler::promise_type;

    promise_t* promise_;

    constexpr bool await_ready() { return false; }
    void await_suspend(txn_handler::handle_t h) { promise_ = &h.promise(); }
    promise_t& await_resume() { return *promise_; }
};

promise_awaitable promise_event;

txn_handler
txnhandlerYield() {
    //using promise_t = txn_handler::promise_type;

    while (true) {
        puts(std::format("txnHandlerPromise before co_await").c_str());
        auto& promise = co_await promise_event;
        puts(std::format("txnHanlderPromise after co_await: {}", promise.val_).c_str());
        co_yield 42;
    }
}

int main()
{
    puts("---");
    txn_handler tx{ txnhandlerYield() };
    auto& p = tx.handle().promise();
    p.val_ = 21;
    puts(std::format("main promise before resume, val_:{} str_:{}", p.val_, p.str_).c_str());
    tx.handle().resume();
    puts(std::format("main promise after resume, val_:{}, str_:{}", p.val_, p.str_).c_str());
}