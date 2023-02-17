#include <coroutine>
#include <format>
#include <optional>
#include <unordered_map>
#include "cope.h"
#include "dp.h"
#include "txsetprice.h"
#include "txsellitem.h"

/////////////////////////////////////////////////////////////////////

namespace dp::txn {

  template<typename PromiseType>
  class handler_t {
  private:
    using this_t = handler_t<PromiseType>;
  public:
    using promise_type = PromiseType; // coroutine promise_type
    using handle_t = std::coroutine_handle<promise_type>;

    struct awaitable {
      constexpr bool await_ready() { return false; }
      // note: possible threading / lifetime issue in await_suspend
      void await_suspend(this_t::handle_t h) { promise_ = &h.promise(); }
      promise_type& await_resume() { return *promise_; }
    private:
      promise_type* promise_;
    };

    struct initial_awaitable {
      bool await_ready() { return false; }
      bool await_suspend(this_t::handle_t h) {
        puts("initial_awaitable::await_suspend()");
        // TODO: this obviously restricts the promise type and I should use
        // a concept or just make it an inner class to make that (more) explict
        h.promise().set_active_handle(h);
        return false;
      }
      void await_resume() { /* will never be called */ }
    };

  public:
    handler_t(this_t& other) = delete;
    handler_t operator=(this_t& other) = delete;
    /*
      handler_t(this_t&& other) noexcept : coro_handle_(std::exchange(other.coro_handle_, nullptr)) {}
      handler_t& operator=(this_t&& other) noexcept {
        coro_handle_ = std::exchange(other.coro_handle_, nullptr);
        return *this;
      }
    */
    explicit handler_t(promise_type* p) noexcept :
      coro_handle_(handle_t::from_promise(*p)) {}
    ~handler_t() { if (coro_handle_) coro_handle_.destroy(); }

    handle_t handle() const noexcept { return coro_handle_; }

    // this logic should be in an awaitable.
    auto send_value(handle_t handle,
      std::unique_ptr<DP::Message::Data_t> value)
    {
      auto& p = handle.promise();
      p.send_value(std::move(value));
      puts(std::format("send_value, in_ before resume, msg_name: {}",
        p.in_->msg_name).c_str());
      handle.resume();
      puts(std::format("send_value, out_ after resume, msg_name: {}",
        p.out_->msg_name).c_str());
      return std::move(p.out_);
    }

    auto send_value(std::unique_ptr<DP::Message::Data_t> value) {
      return send_value(active_handle(), std::move(value));
    }

  private:
    handle_t active_handle() const noexcept {
      return coro_handle_.promise().active_handle();
    }
    handle_t root_handle() const noexcept {
      return coro_handle_.promise().root_handle();
    }

    handle_t coro_handle_;
  };

  template<typename T>
  struct in_t
  {
    std::unique_ptr<T> in_;

    // send_message?
    void send_value(std::unique_ptr<T> value) {
      in_ = std::move(value);
      puts(std::format("in_t::send_value").c_str());
    }
  };

  template<typename T>
  struct out_t
  {
    std::unique_ptr<T> out_;

    std::suspend_never yield_value(std::unique_ptr<T> value) {
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
    using handle_t = std::coroutine_handle<this_t>;
  public:
    using handler_t = handler_t<this_t>;

    handler_t::initial_awaitable initial_suspend() noexcept { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    handler_t get_return_object() noexcept { return handler_t{ this }; }
    void unhandled_exception() { throw; }
    void return_void() { throw std::runtime_error("co_return not allowed"); }

    handle_t active_handle() const { return active_handle_.value(); }
    void set_active_handle(handle_t h) { active_handle_.emplace(h); }

    handle_t root_handle() const { return root_handle_.value(); }
    void set_root_handle(handle_t h) { root_handle_.emplace(h); }

    handle_t previous_handle() const { return previous_handle_.value(); }
    void set_previous_handle(handle_t h) { previous_handle_.emplace(h); }

    State state;
  //private:
    std::optional<handle_t> active_handle_; // maybe this one doesn't need to be optional
    std::optional<handle_t> root_handle_;
    std::optional<handle_t> previous_handle_;
  };
} // namespace txn

template<typename SrcPromiseT, typename DstPromiseT>
struct start_txn_awaitable {
  using dst_handle_t = std::coroutine_handle<DstPromiseT>;

  start_txn_awaitable(dst_handle_t handle, DP::msg_ptr_t msg) :
    dst_handle_(handle), msg_(std::move(msg)) {}

  auto await_ready() { return false; }
  auto await_suspend(/*this_t::handle_t*/ auto h) {
    puts("start_txn_awaitable::await_suspend()");
    src_promise_ = &h.promise();

    auto& dst_p = dst_handle_.promise();
    dst_p.in_ = std::move(msg_);
    puts(std::format("dst_p.in_, msg_name: {}",
      dst_p.in_->msg_name).c_str());

    // TODO: update root, prev handle state in dst_

    return dst_handle_; // symmetric transfer to dst
  }
  auto& await_resume() {
    return *src_promise_;
  }
private:
  SrcPromiseT* src_promise_;
  dst_handle_t dst_handle_;
  DP::msg_ptr_t msg_;
};

/*template<typename FromState, typename ToState>
auto start_txn(message_inout_promise_t<FromState>& fromPromise,
  message_inout_promise_t<ToState>& toPromise,
  ToState state)
*/
// this function is probably unnecessary and could be subsumed by
// start_txn_awaitable 
/*
template<typename DstPromiseT>
auto start_txn(DstPromiseT& dst_promise, DP::msg_ptr_t msg) {
  using dst_handle_t = std::coroutine_handle<DstPromiseT>;
  puts("start_txn");
  dst_promise.in_ = std::move(msg);
  return start_txn_awaitable<SrcPromiseT, DstPromiseT>
    { dst_handle_t::from_promise(dst_promise) };
}
*/

namespace setprice {
  using promise_t = dp::txn::message_inout_promise_t<txn::state_t>;
  using handler_t = dp::txn::handler_t<promise_t>;

  auto process_txn_message(const DP::Message::Data_t& msg)
    // -> std::unique_ptr<DP::Message::Data_t>
  {
    puts(std::format("setprice::process_txn_message: {}", msg.msg_name).c_str());
    // todo: save state
    return std::make_unique<DP::txn::data_t>("setprice-txn-result", "balls");
  }

  auto process_message(const DP::Message::Data_t& msg,
    const txn::state_t& state)
    // -> std::unique_ptr<DP::Message::Data_t>
  {
    puts(std::format("setprice::process_message: {}", msg.msg_name).c_str());
    return std::make_unique<DP::Message::Data_t>("setprice-msg-result");
  }

  auto txn_handler() -> handler_t
  {
    handler_t::awaitable event;
    while (true) {
      puts(std::format("setprice::txn_handler before co_await").c_str());
      auto& promise = co_await event;
      auto& msg = *promise.in_.get();
      puts(std::format("setprice::txn_handler, in_ after co_await, msg_name: {}", msg.msg_name).c_str());
      if (DP::is_txn_message(msg)) {
        // wonder if i can use a single if constexpr() function to choose a path txn/msg here
        auto result = process_txn_message(msg); // this could be member func of inner class of txn_handler
        co_yield std::move(result);
        // if is txn_complete, symmetric transfer back to previous
      } else {
        auto result = process_message(msg, promise.state); // this could be member func of inner class of txn_handler
        co_yield std::move(result);
      }
    }
  }
}

namespace sellitem
{
  using promise_t = dp::txn::message_inout_promise_t<txn::state_t>;
  using handler_t = dp::txn::handler_t<promise_t>;

  auto process_txn_message(const DP::Message::Data_t& msg)
    // -> std::unique_ptr<DP::Message::Data_t>
  {
    puts(std::format("sellitem::process_txn_message: {}", msg.msg_name).c_str());
    // todo: save state
    return std::make_unique<DP::txn::data_t>("sellitem-txn-result", "balls");
  }

  struct row_data_t {
    int index;
    bool price_matches;
    bool selected;
    bool listed;
  };

  auto get_matching_row(const DP::Message::Data_t& msg,
    const txn::state_t& state)
    // -> row_data_t
  {
    row_data_t row_data{ -1 };
    return row_data;
  }

  auto process_message(const DP::Message::Data_t& msg,
    const txn::state_t& state)
    -> std::unique_ptr<DP::Message::Data_t>
  {
    puts(std::format("sellitem::process_message: {}", msg.msg_name).c_str());

    auto row_data = get_matching_row(msg, state);

    // if message.name == setprice, return txn_start::setprice
    // BUT! we don't want to lose the cuurrent message either.
    // how do we send two messages? (txn_start, set_price::data_t)
    // seems we need promise.in_ to be a deque?
    if (msg.msg_name == setprice::msg::name) {
      setprice::txn::state_t setprice_state{ state.item_price };
      return std::make_unique<setprice::txn::start_t>(setprice::txn::name, setprice_state);
    }
    return std::make_unique<DP::Message::Data_t>("sellitem-msg-result");
  }

  auto txn_handler() -> handler_t
  {
    // i'm not sure if this should be  map of handlers or a map of handles
    // or of awaitables. maybe forget about the map for a second and focus
    // on implementing the awaitable replacement for send_message.
    //std::unordered_map<handler_t> txn_handler_map;
    setprice::handler_t setprice_handler{ setprice::txn_handler() };

    handler_t::awaitable event;
    while (true) {
      puts(std::format("sellitem::txn_handler, outer co_await").c_str());
      auto& promise = co_await event;
      auto& msg = *promise.in_.get();
      if (DP::is_txn_message(msg)) {
        auto result = process_txn_message(msg);
        co_yield std::move(result);
      } else {
        auto result = process_message(msg, promise.state);
        // i think there is some room for improvement here on next line
        if (auto& txn = *result.get(); DP::is_start_txn(txn)) {
          //txn_handler_map.at(get_txn_name(msg)).send_value(std::move(result));
          auto name = DP::get_txn_name(txn);
          if (name == setprice::txn::name) {
            // NOTE: moving "msg" here instead of "result". i'm not even sure
            // what "result" would be in the deque_t case? the queue.front()?
            // or the entire queue?
            // can this be co_yield deqeue<dp::msg_ptr_t>? yes? to handle both
            // a) xfer of state to dst, and b) xfer of msg(s) to dst.  note that
            // in current conceived design state is in txt_start.
            // i'm a little worried about this "deqeue" of messages though, because
            // it seems to imply special case handling of an "outer loop co_await"
            // within the txn_handler. generally we just process one message on a
            // co_await/co_yield.
            // maybe we should promise.out_ = std::move(msg) here, in prep for
            // transfer to new tx. instead of passing msg?
            /*auto txn_result = */
            co_await start_txn_awaitable<sellitem::handler_t::promise_type,
              setprice::handler_t::promise_type>
              { setprice_handler.handle(), std::move(promise.in_) };
          }
          else {
            throw std::runtime_error(std::format("unsupported txn: {}", name).c_str());
          }
          // note that, at this (resume) point, we have the return value from
          // the txn, and, what to do with it?
          // of course the structure of this function is wrong, with one "global"
          // event listener. in fact their will be many, within "stateful" loops.
          // but maybe i can hack it to make it work with this structure for PoC.
        }
        else {
          co_yield std::move(result);
        }
      }
    }
  }
} // namespace sellitem

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
    sellitem::handler_t tx_sell{ sellitem::txn_handler() };
    sellitem::txn::state_t s1{ "some item", 1 };
    auto m1 = std::make_unique<sellitem::txn::start_t>(sellitem::txn::name, s1);
    // resume_with_value
    auto r1 = tx_sell.send_value(std::move(m1));

    auto m2 = std::make_unique<setprice::msg::data_t>(1);
    auto r2 = tx_sell.send_value(std::move(m2));

    return 0;
}
