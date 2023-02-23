#pragma once

#include <cassert>
#include <coroutine>
#include <format>
#include <memory>
#include <optional> // TODO: remove
#include <stdexcept>
#include <string_view>
#include "log.h"

namespace dp {
  namespace msg {
    struct data_t {
      // These go in something like "TranslateData"
      //std::string window_name;
      //std::string type_name;
      data_t(std::string_view n) : msg_name(n) {}
      virtual ~data_t() {}

      std::string msg_name;
    };
  }

  using msg_t = msg::data_t;
  using msg_ptr_t = std::unique_ptr<msg_t>;
}

namespace dp::txn {
  namespace name {
    constexpr std::string_view start = "txn_start";
    constexpr std::string_view complete = "txn_complete";
  }

  struct state_t {
    state_t(const state_t& state) = delete;
    state_t& operator=(const state_t& state) = delete;

    std::string prev_msg_name;
    int retries = 3;
  };

  // consider templatizing this (or start_t) with State, and specializing for void (no state)
  struct data_t : msg::data_t {
    data_t(std::string_view msgName, std::string_view txnName) :
      msg::data_t(msgName), txn_name(txnName) {}

    std::string txn_name;
  };

  template<typename State>
  struct start_t : data_t {
    using state_ptr_t = std::unique_ptr<State>;

    static const msg_t& msg_from(const msg_t& txn) {
      const start_t<State>& txn_start = dynamic_cast<const start_t<State>&>(txn);
      return *txn_start.msg.get();
    }
 
    // getting state from a message usually precedes a move, therefore no const.
    // maybe i should just have a move method
    static State& state_from(msg_t& txn) {
      start_t<State>& txn_start = dynamic_cast<start_t<State>&>(txn);
      return *txn_start.state.get();
    }

    constexpr start_t(std::string_view txn_name, msg_ptr_t m, state_ptr_t s) :
      data_t(name::start, txn_name), msg(std::move(m)), state(std::move(s)) {}

    msg_ptr_t msg;
    state_ptr_t state;
  };

  enum class result_code {
    success = 0,
    expected_error,
    unexpected_error
  };

  // todo: T, -> reqires derives_from data_t ??
  struct complete_t : data_t {
    complete_t(std::string_view txn_name, result_code rc) :
      data_t(name::complete, txn_name), code(rc) {}

    result_code code;
  };

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
      auto /*handler_t*/ get_return_object() noexcept { return handler_t{ this }; }
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
      coro_handle_(handle_t::from_promise(*p)) { /*p->set_txn_handler(this);*/
    }
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
      }
      else {
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
      auto& root_p = src_promise_->root_handle().promise();
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
      auto& dst_p = dst_handle_.promise();
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
} // namespace dp::txn

// TODO: probably want to try to relocate this in above dp{} block
namespace dp {
  constexpr auto is_txn_message(const msg_t& msg) noexcept {
    return msg.msg_name.starts_with("txn");
  }

  constexpr auto is_start_txn(const msg_t& msg) {
    return msg.msg_name == txn::name::start;
  }

  constexpr auto is_complete_txn(const msg_t& msg) {
    return msg.msg_name == txn::name::complete;
  }

  constexpr auto get_txn_name(const msg_t& msg) {
    const txn::data_t* txn = dynamic_cast<const txn::data_t*>(&msg);
    assert(txn);
    return txn->txn_name;
  }
} // namespace dp::txn