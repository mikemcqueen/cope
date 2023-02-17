#include <coroutine>
#include <format>
#include "cope.h"

using namespace std;

class txn_handler {
public:
    struct promise_type
    {
        int val_ = -1;

        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        txn_handler get_return_object() noexcept { return txn_handler{ this }; }
        void unhandled_exception() { throw; }
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

struct void_awaitable
{
    constexpr bool await_ready() { return false; }
    void await_suspend(typename txn_handler::handle_t) {}
    void await_resume() {}
};

struct promise_awaitable
{
    using promise_t = txn_handler::promise_type;

    promise_t* promise_;

    constexpr bool await_ready() { return false; }
    void await_suspend(txn_handler::handle_t h) { promise_ = &h.promise(); }
    promise_t& await_resume() { return *promise_; }
};

void_awaitable void_event;
promise_awaitable promise_event;

txn_handler
txnHandlerVoid() {
    while (true) {
        puts("void before co_await");
        co_await void_event;
        puts(std::format("Void after co_await").c_str());
    }
}

txn_handler
txnHandlerPromise() {
    using promise_t = txn_handler::promise_type;

    while (true) {
        puts(std::format("txnHandlerPromise before co_await").c_str());
        auto& promise = co_await promise_event;
        puts(std::format("txnHanlderPromise after co_await: {}", promise.val_).c_str());
        promise.val_ = 42;
    }
}

int main()
{
    puts("---");
    txn_handler th_void{ txnHandlerVoid() };
    th_void.handle().resume();

    puts("---");
    txn_handler th_promise{ txnHandlerPromise() };
    th_promise.handle().promise().val_ = 21;
    puts(std::format("main promise before resume: {}", th_promise.handle().promise().val_).c_str());
    th_promise.handle().resume();
    puts(std::format("main promise after resume: {}", th_promise.handle().promise().val_).c_str());

    return 0;
}
