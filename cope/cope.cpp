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
    }
  };

/*
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
*/

  struct message_in_t : in_t<DP::Message::Data_t> {};
//  struct message_out_t : out_t<DP::Message::Data_t> {};
//  struct message_inout_t : message_in_t, message_out_t {};

  class handler_t {
  public:
    struct initial_awaitable;
    struct promise_type;
    using handle_t = std::coroutine_handle<promise_type>;

    struct awaitable {
      bool await_ready() const { return false; }
      void await_suspend(handle_t h) { promise_ = &h.promise(); }
      promise_type& await_resume() const { return *promise_; }

      const promise_type& promise() const { return *promise_; }
      promise_type& promise() { return *promise_; }
    private:
      promise_type* promise_;
    };

    struct promise_type : message_in_t
    {
      handler_t::initial_awaitable initial_suspend() noexcept { return {}; }
      std::suspend_always final_suspend() noexcept { return {}; }
      auto /*handler_t*/ get_return_object() noexcept { return handler_t{this}; }
      void unhandled_exception() { throw; }
      void return_void() { throw std::runtime_error("co_return not allowed"); }
      awaitable yield_value(DP::msg_ptr_t value) {
        puts(std::format("yield_value, value: {}", value.get()->msg_name).c_str());
        root_handle_.promise().out_ = std::move(value);
        //puts(std::format("yield_value, out_: {}", out_.get() ? out_.get()->msg_name : "empty").c_str());
        return {};
      }

      auto active_handle() const { return active_handle_; }
      void set_active_handle(handle_t h) { active_handle_ = h; }

      auto root_handle() const { return root_handle_; }
      void set_root_handle(handle_t h) { root_handle_ = h; }

      auto prev_handle() const { return prev_handle_; }
      void set_prev_handle(handle_t h) { prev_handle_.emplace(h); }
      void clear_prev_handle() { prev_handle_.reset(); }

      auto& txn_handler() { return *txn_handler_;  }
      void set_txn_handler(handler_t* handler) { txn_handler_ = handler; }

      //   State state;
       //private
      handle_t active_handle_;
      handle_t root_handle_;
      std::optional<handle_t> prev_handle_;
      handler_t* txn_handler_;
      DP::msg_ptr_t out_;
    };

    struct initial_awaitable {
      bool await_ready() { return false; }
      bool await_suspend(handle_t h) {
        puts("initial_awaitable::await_suspend()");
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
    [[nodiscard]] auto send_value(DP::msg_ptr_t value)
    {
      auto& ap = active_handle().promise();
      ap.send_value(std::move(value));
      puts(std::format("send_value, in_ msg_name: {}",
        ap.in_->msg_name).c_str());
      active_handle().resume();
      auto& rp = root_handle().promise();
      if (rp.out_) {
        puts(std::format("send_value, out_ msg_name: {}",
          rp.out_->msg_name).c_str());
      } else {
        puts(std::format("send_value, out_ empty").c_str());
      }
      return std::move(rp.out_);
    }

/*
    [[nodiscard]] auto send_value(DP::msg_ptr_t value) {
      return send_value(active_handle(), std::move(value));
    }
*/

  private:
    handle_t active_handle() const noexcept {
      return coro_handle_.promise().active_handle();
    }
    handle_t root_handle() const noexcept {
      return coro_handle_.promise().root_handle();
    }

    handle_t coro_handle_;
  }; // struct handler_t

  struct transfer_txn_awaitable {
    using handle_t = handler_t::handle_t;

    transfer_txn_awaitable(handle_t handle, DP::msg_ptr_t msg) :
      dst_handle_(handle), msg_(std::move(msg)) {}

    auto await_ready() { return false; }
    auto& await_resume() {
      puts("transfer_txn_awaitable::await_resume()");
      return *src_promise_;
    }
  protected:
    handler_t::promise_type* src_promise_ = nullptr;
    handle_t dst_handle_;
    DP::msg_ptr_t msg_;
  };

  template<typename State>
  struct start_txn_awaitable : transfer_txn_awaitable {
    using state_ptr_t = std::unique_ptr<State>;
    start_txn_awaitable(handle_t handle, DP::msg_ptr_t msg, state_ptr_t state) :
      transfer_txn_awaitable(handle, std::move(msg)),
      state_(std::move(state)) {}

    auto await_suspend(handle_t h) {
      puts("start_txn_awaitable::await_suspend()");
      src_promise_ = &h.promise();
      auto & dst_p = dst_handle_.promise();
      dst_p.in_ = std::move(msg_);
      puts(std::format("  setting dst_p.in_, msg_name: {}",
        dst_p.in_->msg_name).c_str());

      dst_p.set_root_handle(src_promise_->root_handle());
      dst_p.set_prev_handle(src_promise_->active_handle());
      auto & root_p = src_promise_->root_handle().promise();
      root_p.set_active_handle(dst_p.active_handle());

      return dst_handle_; // symmetric transfer to dst
    }

    state_ptr_t state_;
  };

  struct complete_txn_awaitable : transfer_txn_awaitable {
    complete_txn_awaitable(handle_t handle, DP::msg_ptr_t msg) :
      transfer_txn_awaitable(handle, std::move(msg)) {}

    auto await_suspend(handle_t h) {
      puts("start_txn_awaitable::await_suspend()");
      src_promise_ = &h.promise();
      auto & dst_p = dst_handle_.promise();
      dst_p.in_ = std::move(msg_);
      puts(std::format("  setting dst_p.in_, msg_name: {}",
        dst_p.in_->msg_name).c_str());
      // todo: fix this comment. we just use src->prev because:
      // note there is some funny business here. because we are not storing
      // the "actual handle" in a promise, we infer that active_handle is the
      // "actual handle", which is true for all cases except root handle,
      // where, the root_handle the "actual". we can tell when we're at the
      // root because there is no prev_handle.
      auto& root_p = src_promise_->root_handle().promise();
      root_p.set_active_handle(src_promise_->prev_handle().value());
      src_promise_->clear_prev_handle();

      return dst_handle_; // symmetric transfer to dst
    }
  };


} // namespace txn

namespace setprice {
  using DP::txn::result_code;
  using dp::txn::handler_t;
  using promise_type = handler_t::promise_type;
  using msg_t = DP::Message::Data_t;

//  using namespace dp::txn;

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
    return std::make_unique<DP::txn::complete_t>(txn::name, DP::txn::result_code::success);
  }

  // txn_complete are candidates for moving to dp::txn namespace
  auto txn_complete(promise_type& promise, DP::msg_ptr_t msg_ptr) {
    return dp::txn::complete_txn_awaitable{
      promise.prev_handle().value(),
      std::move(msg_ptr)
    };
  }

  auto txn_complete(promise_type& promise) {
    return txn_complete(promise, std::move(promise.in_));
  }

  auto txn_complete(promise_type& promise, result_code rc) {
      return txn_complete(promise,
        std::move(std::make_unique<DP::txn::complete_t>(txn::name, rc)));
  }

  auto validate_message(const msg_t& msg) {
    result_code rc = result_code::success;
    if (msg.msg_name != msg::name) {
      puts("setprice::validate_message, message name mismatch");
      rc = result_code::error; // start_txn_expected;
    }
    return rc;
  }

  auto validate_price(const promise_type& promise, int price) {
    const auto& msg = *promise.in_.get();
    result_code rc = validate_message(msg);
    if (rc == result_code::success) {
      const msg::data_t* spmsg = dynamic_cast<const msg::data_t*>(&msg);
      if (!spmsg) {
        puts("setprice::validate_price: message type mismatch");
        rc = result_code::error; // msg_type_mismatch (this is a biger deal isn't it)
      } else if (spmsg->price != price) {
        puts("setprice::validate_price: price mismatch");
        rc = result_code::error; // price mismatch? (this is expected)
      } else {
        puts("setprice::validate_price: prices match");
      }
    }
    return rc;
  }

  auto validate_complete(const promise_type& promise) {
    result_code rc = result_code::success;
    // const txn::state_t& state = promise.txn_handler
    // check state.prev_msg_name == promise.in_->msg_name
    if (promise.in_->msg_name != sellitem::msg::name) {
      puts("set_price::validate_complete: message name mismatch");
      rc = result_code::error;
    } else {
      puts("set_price::validate_complete: message name matches");
    }
    return rc;
  }

  auto get_price(const promise_type& promise) {
    return 2; // TODO: from state
  }

  auto input_price(const promise_type& promise, int) {
    return std::make_unique<msg_t>("input_price");
  }

  auto click_ok() {
    return std::make_unique<msg_t>("click_ok");
  }

  auto txn_handler() -> handler_t
  {
    result_code rc = result_code::success;
    auto error = [&rc]() noexcept { return rc != result_code::success; };
    handler_t::awaitable event;
    while (true) {
      if (error()) {
        puts(std::format("setprice::txn_handler error: {}", (int)rc).c_str());
        // so maybe not all errors result in completing txn?
        // should we retry the whole txn?
        // more importantly if msg->msg_name is still setprice, completing doesn't
        // seem right, so what to do?
        co_await txn_complete(event.promise(), rc);
        rc = result_code::success;
      }

      puts(std::format("setprice::txn_handler before co_await").c_str());
      auto& promise = co_await event;
      // using price is optional here.  both price and state.price are in promise
      int price = get_price(promise);
      rc = validate_price(promise, price); // msg name, price contents vs. state
      //for (retry_t retry; error() && retry.allowed(); retry--) {
      if (error()) {
        //const auto& promise =
        co_yield input_price(promise, price);
        rc = validate_price(promise, price);
        if (error()) continue;
      }

      co_yield click_ok();
      rc = validate_complete(promise);  // sell_item should be "prev_msg_name" in state
      if (error()) continue;

      co_await txn_complete(promise);
    }
  }

  auto old_txn_handler() -> handler_t
  {
    handler_t::awaitable event;
    while (true) {
      puts(std::format("setprice::txn_handler before co_await").c_str());
      const auto& promise = co_await event;
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
              co_await dp::txn::complete_txn_awaitable{ promise.prev_handle().value(),
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
  using promise_type = handler_t::promise_type;
  using msg_t = DP::Message::Data_t;

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
    -> DP::msg_ptr_t
  {
    puts(std::format("sellitem::process_message: {}", msg.msg_name).c_str());
    auto row_data = get_matching_row(msg, state);

    // if message.name == setprice, return txn_start::setprice
    // BUT! we don't want to lose the cuurrent message either.
    // how do we send two messages? (txn_start, set_price::data_t)
    // seems we need promise.in_ to be a deque?
/*
    if (msg.msg_name == setprice::msg::name) {
      setprice::txn::state_t setprice_state(sellitem::txn::name, state.item_price);
      return std::make_unique<setprice::txn::start_t>(setprice::txn::name, setprice_state);
      return msg;
    }
*/
      return std::make_unique<DP::Message::Data_t>("sellitem-msg-result");
  }
  
  auto start_txn_setprice(handler_t::handle_t handle, promise_type& promise,
    const txn::state_t& sellitem_state)
  {
    auto setprice_state = std::make_unique<setprice::txn::state_t>(
      std::string(sellitem::msg::name), sellitem_state.item_price);
    return dp::txn::start_txn_awaitable<setprice::txn::state_t>{
      handle, std::move(promise.in_), std::move(setprice_state),
    };
  }

  auto txn_handler() -> handler_t
  {
    setprice::handler_t setprice_handler{ setprice::txn_handler() };
    handler_t::awaitable event;
    txn::state_t state{ "abc", 1 };

    while (true) {
      puts(std::format("sellitem::txn_handler, outer co_await").c_str());
      auto& promise = co_await event;
      auto& msg = *promise.in_.get();
      if (msg.msg_name == setprice::msg::name) {
        auto& txn_p = co_await start_txn_setprice(setprice_handler.handle(),
          promise, state);
        // move this to awaitable?
        puts(std::format("txn_result, msg_name: {}", txn_p.in_.get()->msg_name).c_str());
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

    auto m1 = std::make_unique<setprice::msg::data_t>(1);
    auto r1 = tx_sell.send_value(std::move(m1));

    auto m2 = std::make_unique<setprice::msg::data_t>(2);
    auto r2 = tx_sell.send_value(std::move(m2));

    auto m3 = std::make_unique<sellitem::msg::data_t>();
    auto r3 = tx_sell.send_value(std::move(m3));

    return 0;
}
