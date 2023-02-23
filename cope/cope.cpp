#include <chrono>
#include <coroutine>
#include <format>
#include <optional>
//#include <unordered_map>
#include "cope.h"
#include "dp.h"
#include "txsetprice.h"
#include "txsellitem.h"

bool logging_enabled = true;
void log(const char* msg) {
  if (logging_enabled) puts(msg);
}

namespace dp::txn {
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

    struct promise_type {
      handler_t::initial_awaitable initial_suspend() noexcept { return {}; }
      std::suspend_always final_suspend() noexcept { return {}; }
      auto /*handler_t*/ get_return_object() noexcept { return handler_t{this}; }
      void unhandled_exception() { throw; }
      void return_void() { throw std::runtime_error("co_return not allowed"); }
      awaitable yield_value(dp::msg_ptr_t value) {
        log(std::format("yield_value, value: {}", value.get()->msg_name).c_str());
        root_handle_.promise().emplace_out(std::move(value));
        //log(std::format("yield_value, out_: {}", out_.get() ? out_.get()->msg_name : "empty").c_str());
        return {};
      }

      auto active_handle() const { return active_handle_; }
      void set_active_handle(handle_t h) { active_handle_ = h; }

      auto root_handle() const { return root_handle_; }
      void set_root_handle(handle_t h) { root_handle_ = h; }

      auto prev_handle() const { return prev_handle_; }
      void set_prev_handle(handle_t h) { prev_handle_.emplace(h); }

      //auto& txn_handler() const { return *txn_handler_;  }
      //void set_txn_handler(handler_t* handler) { txn_handler_ = handler; }

      auto& in() const { return *in_.get(); }
      dp::msg_ptr_t&& in_ptr() { return std::move(in_); }
      void emplace_in(dp::msg_ptr_t value) { in_ = std::move(value); }

      auto& out() const { return *out_.get(); }
      dp::msg_ptr_t&& out_ptr() { return std::move(out_); }
      void emplace_out(dp::msg_ptr_t value) { out_ = std::move(value); }

    private:
      handle_t active_handle_;
      handle_t root_handle_;
      std::optional<handle_t> prev_handle_; // TODO should be non-optional noop_coroutine<>();
      dp::msg_ptr_t in_;
      dp::msg_ptr_t out_;
      //handler_t* txn_handler_;
    };

    struct initial_awaitable {
      bool await_ready() { return false; }
      bool await_suspend(handle_t h) {
        log("initial_awaitable::await_suspend()");
        h.promise().set_active_handle(h);
        h.promise().set_root_handle(h);
        return false;
      }
      void await_resume() { /* will never be called */ }
    };

  public:
    handler_t(handler_t& other) = delete;
    handler_t operator=(handler_t& other) = delete;

    explicit handler_t(promise_type* p) noexcept :
      coro_handle_(handle_t::from_promise(*p)) { /*p->set_txn_handler(this);*/ }
    ~handler_t() { if (coro_handle_) coro_handle_.destroy(); }

    handle_t handle() const noexcept { return coro_handle_; }

    // TODO: this logic should be in an awaitable?
    [[nodiscard]] dp::msg_ptr_t&& send_value(dp::msg_ptr_t value)
    {
      auto& ap = active_handle().promise();
      ap.emplace_in(std::move(value));
      log(std::format("send_value, emplace_in({})", ap.in().msg_name).c_str());
      active_handle().resume();
      auto& rp = root_handle().promise();
      if (rp.out_ptr()) {
        log(std::format("send_value, out({})", rp.out().msg_name).c_str());
      } else {
        log(std::format("send_value, out empty").c_str());
      }
      return std::move(rp.out_ptr());
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

  struct transfer_txn_awaitable {
    using handle_t = handler_t::handle_t;

    transfer_txn_awaitable(std::string_view txn_name, handle_t handle,
      dp::msg_ptr_t msg) :
      dst_handle_(handle), txn_name_(txn_name), msg_(std::move(msg)) {}

    auto await_ready() { return false; }
    auto& await_resume() {
      log(std::format("transfer_txn_awaitable({})::await_resume()", txn_name_).c_str());
      return *src_promise_;
    }
  protected:
    handle_t dst_handle_;
    std::string txn_name_;
    dp::msg_ptr_t msg_;
    handler_t::promise_type* src_promise_ = nullptr;
  };

  template<typename State>
  struct start_txn_awaitable : transfer_txn_awaitable {
    using start_t = dp::txn::start_t<State>;
    using state_ptr_t = start_t::state_ptr_t;

    start_txn_awaitable(std::string_view txn_name, handle_t& handle,
      dp::msg_ptr_t msg, state_ptr_t state) :
      transfer_txn_awaitable(txn_name, handle, std::move(msg)),
      state_(std::move(state)) {}

    auto await_suspend(handle_t h) {
      log(std::format("start_txn_awaitable({})::await_suspend()", txn_name_).c_str());
      src_promise_ = &h.promise();
      auto& dst_p = dst_handle_.promise();
      dst_p.emplace_in(make_start(std::move(msg_), std::move(state_)));
      log(std::format("  dst_p.emplace_in({})", dst_p.in().msg_name).c_str());

      dst_p.set_root_handle(src_promise_->root_handle());
      dst_p.set_prev_handle(src_promise_->active_handle());
      auto & root_p = src_promise_->root_handle().promise();
      root_p.set_active_handle(dst_p.active_handle());

      return dst_handle_; // symmetric transfer to dst
    }

  private:
    auto make_start(dp::msg_ptr_t msg, state_ptr_t state) {
      return std::make_unique<start_t>(txn_name_, std::move(msg), std::move(state));
    }

    state_ptr_t state_;
  };

  struct complete_txn_awaitable : transfer_txn_awaitable {
    complete_txn_awaitable(std::string_view txn_name, handle_t handle, dp::msg_ptr_t msg) :
      transfer_txn_awaitable(txn_name, handle, std::move(msg)) {}

    auto await_suspend(handle_t h) {
      log(std::format("complete_txn_awaitable({})::await_suspend()", txn_name_).c_str());
      src_promise_ = &h.promise();
      auto & dst_p = dst_handle_.promise();
      dst_p.emplace_in(std::move(msg_));
      log(std::format("  dst_p.emplace_in({})", dst_p.in().msg_name).c_str());
      // todo: fix this comment. we just use src->prev because:
      // note there is some funny business here. because we are not storing
      // the "actual handle" in a promise, we infer that active_handle is the
      // "actual handle", which is true for all cases except root handle,
      // where, the root_handle the "actual". we can tell when we're at the
      // root because there is no prev_handle.
      auto& root_p = src_promise_->root_handle().promise();
      root_p.set_active_handle(src_promise_->prev_handle().value());
      src_promise_->prev_handle().reset();

      return dst_handle_; // symmetric transfer to dst
    }
  };
} // namespace txn

namespace setprice {
  using dp::txn::result_code;
  using dp::txn::handler_t;
  using dp::msg_t;
  using promise_type = handler_t::promise_type;

  // txn_complete are candidates for moving to dp::txn namespace
  auto txn_complete(promise_type& promise, dp::msg_ptr_t msg_ptr) {
    return dp::txn::complete_txn_awaitable{
      setprice::txn::name,
      promise.prev_handle().value(),
      std::move(msg_ptr)
    };
  }

  auto txn_complete(promise_type& promise) {
    return txn_complete(promise, promise.in_ptr());
  }

  auto txn_complete(promise_type& promise, result_code rc) {
    if (rc != result_code::success) {
      log(std::format("setprice::txn_complete error: {}", (int)rc).c_str());
      return txn_complete(promise, std::move(
        std::make_unique<dp::txn::complete_t>(txn::name, rc)));
    } else {
      return txn_complete(promise, std::move(promise.in_ptr()));
    }
  }

  auto validate_message_name(const msg_t& msg, std::string_view msg_name) {
    result_code rc = result_code::success;
    if (msg.msg_name != msg_name) {
      log(std::format("validate_message_name: name mismatch, expected({}), actual({})",
        msg_name, msg.msg_name).c_str());
      rc = result_code::unexpected_error;
    }
    return rc;
  }
    
  template<typename T>
  auto validate_message(const msg_t& msg, std::string_view msg_name) {
    result_code rc = validate_message_name(msg, msg_name);
    if (rc == result_code::success) {
      if (!dynamic_cast<const T*>(&msg)) {
        log("validate_message: message type mismatch");
        rc = result_code::unexpected_error;
      }
    }
    return rc;
  }

  auto validate_txn_start(const msg_t& txn) {
    result_code rc = validate_message<txn::start_t>(txn, dp::txn::name::start);
    if (rc == result_code::success) {
      // validate that the msg contained within is of type setprice::msg::data_t
      rc = validate_message<msg::data_t>(txn::start_t::msg_from(txn), msg::name);
    }
    return rc;
  }

  auto validate_price(const msg_t& msg, int price) {
    result_code rc = validate_message<msg::data_t>(msg, msg::name);
    if (rc == result_code::success) {
      const msg::data_t& spmsg = static_cast<const msg::data_t&>(msg);
      if (spmsg.price != price) {
        log(std::format("setprice::validate_price: price mismatch, expected({}), actual({})",
          price, spmsg.price).c_str());
        rc = result_code::expected_error; // price mismatch? (this is expected)
      } else {
        log(std::format("setprice::validate_price: prices match({})", price).c_str());
      }
    }
    return rc;
  }

  auto validate_price(const promise_type& promise, int price) {
    return validate_price(promise.in(), price);
  }

  auto validate_complete(const promise_type& promise, std::string_view msg_name) {
    return validate_message_name(promise.in(), msg_name);
  }

  auto input_price(const promise_type& promise, int) {
    promise;
    return std::make_unique<msg_t>("input_price");
  }

  auto click_ok() {
    return std::make_unique<msg_t>("click_ok");
  }

  auto txn_handler() -> handler_t
  {
    txn::state_t state;
    result_code rc{ result_code::success };
    const auto& error = [&rc]() noexcept { return rc != result_code::success; };

    handler_t::awaitable event;
    bool first{ true };
    for (auto& promise = co_await event; true;) {
      if (!first) {
        log(std::format("setprice::txn_handler before co_await").c_str());
        // could put this in for(;;here) loop
        co_await txn_complete(promise, rc);
      }
      first = false;
      auto& txn = promise.in();
      rc = validate_txn_start(txn);
      if (error()) continue;
      state = std::move(txn::start_t::state_from(txn));

      const msg_t& msg = txn::start_t::msg_from(txn);
      rc = validate_price(msg, state.price);
      if (rc == result_code::unexpected_error) continue;
      //for (retry_t retry; error() && retry.allowed(); retry--) {
      if (error()) {
        co_yield input_price(promise, state.price);
        rc = validate_price(promise, state.price);
        if (error()) continue;
      }

      co_yield click_ok();
      rc = validate_complete(promise, state.prev_msg_name);
      //if (error()) continue;
    }
  }
} // namespace setprice

namespace sellitem
{
  using dp::txn::handler_t;
  using dp::msg_t;
  using promise_type = handler_t::promise_type;

  struct row_data_t {
    int index;
    bool price_matches;
    bool selected;
    bool listed;
  };

  auto get_matching_row(const dp::msg_t& msg, const txn::state_t& state) {
    msg; state;
    row_data_t row_data{ -1 };
    return row_data;
  }

  auto start_txn_setprice(handler_t::handle_t handle, promise_type& promise,
    const txn::state_t& sellitem_state)
  {
    auto setprice_state = std::make_unique<setprice::txn::state_t>(
      std::string(sellitem::msg::name), sellitem_state.item_price);
    return dp::txn::start_txn_awaitable<setprice::txn::state_t>{
      setprice::txn::name,
      handle,
      std::move(promise.in_ptr()),
      std::move(setprice_state)
    };
  }

  auto txn_handler() -> handler_t
  {
    setprice::handler_t setprice_handler{ setprice::txn_handler() };
    handler_t::awaitable event;
    txn::state_t state{ "abc", 2 };

    while (true) {
      log(std::format("sellitem::txn_handler, outer co_await").c_str());
      auto& promise = co_await event;
      auto& msg = promise.in();
      if (msg.msg_name == setprice::msg::name) {
        auto& txn_p = co_await start_txn_setprice(setprice_handler.handle(),
          promise, state);
        // move this to awaitable?
        log(std::format("txn_result, msg_name: {}", txn_p.in().msg_name).c_str());
      }
    }
  }
} // namespace sellitem

int main()
{
  using namespace std::chrono;

  dp::txn::handler_t tx_sell{ sellitem::txn_handler() };

/* TODO
  sellitem::txn::state_t s1{"some item", 1};
  auto m1 = std::make_unique<sellitem::txn::start_t>(sellitem::txn::name, s1);
  // resume_with_value
  auto r1 = tx_sell.send_value(std::move(m1));
*/
  int max = 1; 0'000;
  if (max > 1) logging_enabled = false;
  auto start = high_resolution_clock::now();
  for (auto i = 0; i < max; ++i) {
    log("---");
    auto m1 = std::make_unique<setprice::msg::data_t>(1);
    auto r1 = tx_sell.send_value(std::move(m1));

    auto m2 = std::make_unique<setprice::msg::data_t>(2);
    auto r2 = tx_sell.send_value(std::move(m2));

    auto m3 = std::make_unique<sellitem::msg::data_t>();
    auto r3 = tx_sell.send_value(std::move(m3));
  }
  auto end = std::chrono::high_resolution_clock::now();
  double elapsed = 1e-9 * duration_cast<nanoseconds>(end - start).count();

  std::cerr << "Elapsed: " << std::fixed << elapsed << std::setprecision(9)
    << " sec" << std::endl;

  return 0;
}
