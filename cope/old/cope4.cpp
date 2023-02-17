#include <coroutine>
#include <format>
#include "cope.h"
#include "dp.h"
#include "txsetprice.h"
#include "txsellitem.h"

//using namespace std;

/////////////////////////////////////////////////////////////////////

namespace dp::txn {

    template<typename PromiseType>
    class handler_t {
    public:
        using promise_type = PromiseType; // coroutine promise_type

        using this_t = handler_t<PromiseType>;
        using handle_t = std::coroutine_handle<promise_type>;

        struct awaitable
        {
            promise_type* promise_;

            constexpr bool await_ready() { return false; }
            // note: possible threading / lifetime issue in await_suspend
            void await_suspend(this_t::handle_t h) { promise_ = &h.promise(); }
            promise_type& await_resume() { return *promise_; }
        };

    public:
        handler_t(this_t& other) = delete;
        handler_t operator=(this_t& other) = delete;

        handler_t(this_t&& other) noexcept : coro_handle_(std::exchange(other.coro_handle_, nullptr)) {}
        handler_t& operator=(this_t&& other) noexcept {
            coro_handle_ = std::exchange(other.coro_handle_, nullptr);
            return *this;
        }

        explicit handler_t(promise_type* p) noexcept : coro_handle_(handle_t::from_promise(*p)) {}
        ~handler_t() { if (coro_handle_) coro_handle_.destroy(); }

        handle_t handle() const noexcept { return coro_handle_; }

    private:
        handle_t coro_handle_;
    };

    template<typename T>
    struct in_t
    {
        std::unique_ptr<T> in_;

        void send_value(std::unique_ptr<T>&& value) {
            in_ = std::move(value);
            puts(std::format("in_t::send_value").c_str());
        }
    };

    template<typename T>
    struct out_t
    {
        std::unique_ptr<T> out_;

        std::suspend_never yield_value(std::unique_ptr<T>&& value) {
            out_ = std::move(value);
            puts(std::format("out_t::yield_value").c_str());
            return {};
        }
    };

    struct message_in_t : in_t<DP::Message::Data_t> {};
    struct message_out_t : out_t<DP::Message::Data_t> {};
    struct message_inout_t : message_in_t, message_out_t {};

    template<typename State>
    struct message_inout_promise_t : message_inout_t
    {
    private:
        using this_t = message_inout_promise_t<State>;
    public:
        using handler_t = handler_t<this_t>;

        State state_;

        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        handler_t get_return_object() noexcept { return handler_t{ this }; }
        void unhandled_exception() { throw; }
        void return_void() { throw std::runtime_error("co_return not allowed"); }
    };

} // namespace txn

namespace SellItem
{
    using promise_t = dp::txn::message_inout_promise_t<txn::state_t>;
    using handler_t = dp::txn::handler_t<promise_t>;

    constexpr void process_txn_message(const DP::Message::Data_t& data) {}

    struct row_data_t {
        int index;
        bool price_matches;
        bool selected;
        bool listed;
    };

    auto get_matching_row(const DP::Message::Data_t& msg, const txn::state_t& state)
        -> row_data_t
    {
        row_data_t row_data{ -1 };

        return row_data;
    }

    auto process_message(const DP::Message::Data_t& msg, const txn::state_t& state)
        -> std::unique_ptr<DP::Message::Data_t>
    {
        auto row_data = get_matching_row(msg, state);
        
        return std::make_unique<DP::Transaction::start_t>(SetPrice::Window::Name);
    }

    auto txn_handler() -> handler_t
    {
        handler_t::awaitable event;
        while (true) {
            //puts(std::format("SellItem::txn_handler before co_await").c_str());
            auto& promise = co_await event;
            auto& msg = *promise.in_.get();
            puts(std::format("sell_item::txn_handler after co_await, msg_name: {}", msg.msg_name).c_str());
            if (DP::is_txn_message(msg)) {
                process_txn_message(msg); // this could be member func of inner class of txn_handler
            } else {
                auto result = process_message(msg, promise.state_); // this could be member func of inner class of txn_handler
                co_yield std::move(result);
            }
        }
    }
} // namespace SellItem

int main()
{
    // 1. txn sell item. reset_state(). 
    // 2. send message with wrong item name. resume.
    // 2.5 send message with correct item name. resume.
    // 3. return setprice::start_t with expected price.
    // 4. push txn setprice. reset_state(). pause old? set to "active"?
    // 5. send message with wrong price. resume.
    // 6. send message with correct price. resume.
    // 7. responds with complete_t or something like that.
    // 8. pop setprice txn. send anything to prev txn here? set to "resume"?
    // 9. 
    puts("---");
    SellItem::handler_t tx_sell{ SellItem::txn_handler() };
    auto& sell_p = tx_sell.handle().promise();
    sell_p.send_value(std::move(std::make_unique<SetPrice::msg::data_t>("1")));
    puts(std::format("main promise.in_ before resume, msg_name: {}", sell_p.in_->msg_name).c_str());
    tx_sell.handle().resume();
    puts(std::format("main promise.out_ after resume, msg_name: {}", sell_p.out_->msg_name).c_str());

    return 0;
}
