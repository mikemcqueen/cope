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

  class handler_t {
  public:
    struct initial_awaitable;
    struct promise_type;
    using handle_t = std::coroutine_handle<promise_type>;

    struct promise_type : message_in_t
    {
      handler_t::initial_awaitable initial_suspend() noexcept { return {}; }
      std::suspend_always final_suspend() noexcept { return {}; }
      auto /*handler_t*/ get_return_object() noexcept { return handler_t{this}; }
      void unhandled_exception() { throw; }
      void return_void() { throw std::runtime_error("co_return not allowed"); }
      std::suspend_never yield_value(std::unique_ptr<DP::Message::Data_t> value) {
        puts(std::format("in_t::yield_value").c_str());
        root_handle_.promise().in_ = std::move(value);
        return {};
      }

      auto active_handle() const { return active_handle_; }
      void set_active_handle(handle_t h) { active_handle_ = h; }

      auto root_handle() const { return root_handle_; }
      void set_root_handle(handle_t h) { root_handle_ = h; }

      auto prev_handle() const { return prev_handle_.value(); }
      void set_prev_handle(handle_t h) { prev_handle_.emplace(h); }
      void clear_prev_handle() { prev_handle_.reset(); }

      auto& txn_handler() { return *txn_handler_;  }
      void set_txn_handler(handler_t* handler) { txn_handler_ = handler; }

      //   State state;
       //private
      handle_t active_handle_; // maybe this one doesn't need to be optional
      handle_t root_handle_;   // or this for that matter
      std::optional<handle_t> prev_handle_;
      handler_t* txn_handler_;
    };

    struct awaitable {
      constexpr bool await_ready() { return false; }
      // note: possible threading / lifetime issue in await_suspend
      void await_suspend(handle_t h) { promise_ = &h.promise(); }
      promise_type& await_resume() { return *promise_; }
    private:
      promise_type* promise_;
    };

    struct initial_awaitable {
      bool await_ready() { return false; }
      bool await_suspend(handle_t h) {
        puts("initial_awaitable::await_suspend()");
        // TODO: this obviously restricts the promise type and I should use
        // a concept or just make it an inner class to make that (more) explict
        h.promise().set_active_handle(h);
        h.promise().set_root_handle(h);
        return false;
      }
      void await_resume() { /* will never be called */ }
    };

  public:
    handler_t(handler_t& other) = delete;
    handler_t operator=(handler_t& other) = delete;
    /*
      handler_t(this_t&& other) noexcept : coro_handle_(std::exchange(other.coro_handle_, nullptr)) {}
      handler_t& operator=(this_t&& other) noexcept {
        coro_handle_ = std::exchange(other.coro_handle_, nullptr);
        return *this;
      }
    */
    explicit handler_t(promise_type* p) noexcept :
      coro_handle_(handle_t::from_promise(*p)) { p->set_txn_handler(this); }
    ~handler_t() { if (coro_handle_) coro_handle_.destroy(); }

    handle_t handle() const noexcept { return coro_handle_; }

    // this logic should be in an awaitable.
    auto send_value(handle_t handle,
      std::unique_ptr<DP::Message::Data_t> value)
    {
      auto& p = handle.promise();
      p.send_value(std::move(value));
      puts(std::format("send_value, before in_ msg_name: {}",
        p.in_->msg_name).c_str());
      handle.resume();
      if (p.in_) {
        puts(std::format("send_value, after in_ msg_name: {}",
          p.in_->msg_name).c_str());
      } else {
        puts(std::format("send_value, after in_ empty").c_str());
      }
      return std::move(p.in_);
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
  }; // struct handler_t

  // this couuld probably be: txn_transfer_awaitable that works both directions
  template<bool Start>
  struct transfer_txn_awaitable {
    using handle_t = handler_t::handle_t;

    transfer_txn_awaitable(handle_t handle, DP::msg_ptr_t msg) :
      dst_handle_(handle), msg_(std::move(msg)) {}

    auto await_ready() { return false; }
    auto await_suspend(handle_t h) {
      puts(std::format("transfer_txn_awaitable<{}>::await_suspend()", start_).c_str());
      src_promise_ = &h.promise();

      auto& dst_p = dst_handle_.promise();
      dst_p.in_ = std::move(msg_);
      puts(std::format("  setting dst_p.in_, msg_name: {}",
        dst_p.in_->msg_name).c_str());

      // note there is some funny business here. because we are not storing
      // the "actual handle" in a promise, we infer that active_handle is the
      // "actual handle", which is true for all cases except root handle,
      // where, the root_handle the "actual". we can tell when we're at the
      // root because there is no previous_handle.

      // setting root and prev should only be done for "start_txn"; this will
      // break things if this is used for "finish_txn".
      if (start_) {
        dst_p.set_root_handle(src_promise_->root_handle());
        dst_p.set_prev_handle(src_promise_->active_handle());
      } else {
        src_promise_->clear_prev_handle();
      }
      auto& root_p = src_promise_->root_handle().promise();
      root_p.set_active_handle(dst_p.active_handle());

      return dst_handle_; // symmetric transfer to dst
    }
    auto& await_resume() {
      puts(std::format("transfer_txn_awaitable<{}>::await_resume()", start_).c_str());
      return *src_promise_;
    }
  private:
    handler_t::promise_type* src_promise_;
    handle_t dst_handle_;
    DP::msg_ptr_t msg_;
    bool start_ = Start;
  };

  using start_txn_awaitable = transfer_txn_awaitable<true>;
  using complete_txn_awaitable = transfer_txn_awaitable<false>;
} // namespace txn

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
  using namespace dp::txn;

  auto process_txn_message(const DP::Message::Data_t& msg)
    // -> std::unique_ptr<DP::Message::Data_t>
  {
    puts(std::format("setprice::process_txn_message: {}", msg.msg_name).c_str());
    // todo: save state
    return std::make_unique<DP::txn::data_t>("setprice-txn-result", "balls");
  }

  auto process_message(const DP::Message::Data_t& msg,
    const txn::state_t& state)
    -> std::unique_ptr<DP::Message::Data_t>
  {
    puts(std::format("setprice::process_message: {}", msg.msg_name).c_str());
    return std::make_unique<DP::txn::success_result_t>(txn::name);
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
        // TODO: this comes from promise->txn_handler->state
        txn::state_t state;
        auto result_ptr = process_message(msg, state); // this could be member func of inner class of txn_handler
        if (result_ptr) {
          auto& result = *result_ptr.get();
          puts(std::format("  process_message, result: {}", result.msg_name).c_str());
          if (DP::is_txn_message(result)) {
            if (DP::is_complete_txn(result)) {
              co_await dp::txn::complete_txn_awaitable{ promise.prev_handle(),
                std::move(result_ptr) };
            } else {
              puts("unhandled txn message");
            }
          }
          else {
            co_yield std::move(result_ptr);
          }
        } else {
          puts("  process_message, empty result");
        }
      }
    }
  }
}

namespace sellitem
{
  using handler_t = dp::txn::handler_t;

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
        // TODO this comes from promise->handler->state
        txn::state_t state;
        auto result = process_message(msg, state);
        // i think there is some room for improvement here on next line
        if (auto& txn = *result.get(); DP::is_start_txn(txn)) {
          //txn_handler_map.at(get_txn_name(msg)).send_value(std::move(result));
          auto name = DP::get_txn_name(txn);
          if (name == setprice::txn::name) {
            // the whole idea of a "txn_start" message might be unnecessary, as
            // we can(?) reset destination handler state within the logic of the
            // start_txn_awaitable
            /*auto txn_result = */
            co_await dp::txn::start_txn_awaitable{ setprice_handler.handle(),
              std::move(promise.in_) };
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
    dp::txn::handler_t tx_sell{ sellitem::txn_handler() };
/*
    sellitem::txn::state_t s1{"some item", 1};
    auto m1 = std::make_unique<sellitem::txn::start_t>(sellitem::txn::name, s1);
    // resume_with_value
    auto r1 = tx_sell.send_value(std::move(m1));
*/

    auto m2 = std::make_unique<setprice::msg::data_t>(1);
    auto r2 = tx_sell.send_value(std::move(m2));

    return 0;
}
