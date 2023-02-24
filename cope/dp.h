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
  enum class result_code {
    success = 0,
    expected_error,
    unexpected_error
  };

  namespace msg { struct data_t; }

  using msg_t = msg::data_t;
  using msg_ptr_t = std::unique_ptr<msg_t>;

  namespace msg {
    namespace name {
      constexpr std::string_view txn_start = "txn::start";
      constexpr std::string_view txn_complete = "txn::complete";
    }

    struct data_t {
      data_t(std::string_view name) : msg_name(name) {}
      data_t(const data_t&) = delete;
      data_t& operator=(const data_t&) = delete;
      virtual ~data_t() {}

      std::string msg_name;
    };

    auto validate_name(const msg_t& msg, std::string_view msg_name) -> result_code;

    template<typename msgT>
    auto validate(const msg_t& msg, std::string_view msg_name) {
      result_code rc = validate_name(msg, msg_name);
      if (rc == result_code::success) {
        if (!dynamic_cast<const msgT*>(&msg)) {
          log("msg::validate: type mismatch");
          rc = result_code::unexpected_error;
        }
      }
      return rc;
    }

    // TODO: this should take std::string_view txn_name, for completeness (and slowness)
    template<typename startT, typename msgT>
    auto validate_txn_start(const msg_t& txn, std::string_view txn_name,
        std::string_view msg_name) {
      // would call validate_txn() here
      txn_name;
      result_code rc = validate<startT>(txn, name::txn_start);
      if (rc == result_code::success) {
        // validate that the msg contained within is of supplied type and name
        rc = validate<msgT>(startT::msg_from(txn), msg_name);
      }
      return rc;
    }
  } // namespace msg
} // namespace dp

namespace dp::txn {
  /*
  struct state_t {
    state_t(const state_t& state) = delete;
    state_t& operator=(const state_t& state) = delete;

    std::string prev_msg_name;
    int retries = 3;
  };
  */

  struct data_t : msg::data_t {
    data_t(std::string_view msgName, std::string_view txnName) :
      msg::data_t(msgName), txn_name(txnName) {}

    std::string txn_name;
  };

  template<typename stateT>
  struct start_t : data_t {
    using state_ptr_t = std::unique_ptr<stateT>;

    static const msg_t& msg_from(const msg_t& txn) {
      /*const start_t<stateT>&*/auto& txn_start = dynamic_cast<const start_t<stateT>&>(txn);
      return *txn_start.msg.get();
    }
 
    // getting state from a message usually precedes a move, therefore no const.
    // maybe i should just have a move method
    static stateT& state_from(msg_t& txn) {
      start_t<stateT>& txn_start = dynamic_cast<start_t<stateT>&>(txn);
      return *txn_start.state.get();
    }

    constexpr start_t(std::string_view txn_name, msg_ptr_t m, state_ptr_t s) :
      data_t(msg::name::txn_start, txn_name), msg(std::move(m)), state(std::move(s)) {}

    msg_ptr_t msg;
    state_ptr_t state;
  };

  // todo: T, -> reqires derives_from data_t ??
  struct complete_t : data_t {
    complete_t(std::string_view txn_name, result_code rc) :
      data_t(msg::name::txn_complete, txn_name), code(rc) {}

    result_code code;
  };

  class handler_t {
  public:
    struct initial_awaitable;
    struct promise_type;
    using handle_t = std::coroutine_handle<promise_type>;

    struct awaitable {
      awaitable(std::string_view txn_name) : txn_name_(txn_name) {}

      bool await_ready() const { return false; }
      void await_suspend(handle_t h) {
        promise_ = &h.promise();
        promise_->set_txn_name(txn_name_);
      }
      promise_type& await_resume() const { return *promise_; }

      const promise_type& promise() const { return *promise_; }
      promise_type& promise() { return *promise_; }
    private:
      promise_type* promise_ = nullptr;
      std::string txn_name_;
    };

    struct promise_type {
      initial_awaitable initial_suspend() noexcept { return {}; }
      std::suspend_always final_suspend() noexcept { return {}; }
      auto get_return_object() noexcept { return handler_t{ this }; }
      void unhandled_exception() { throw; }
      void return_void() { throw std::runtime_error("co_return not allowed"); }
      // TODO: return private awaitable w/o txn name?
      awaitable yield_value(dp::msg_ptr_t value) {
        log(std::format("{}::yield_value({})", txn_name(),
          value.get()->msg_name).c_str());
        root_handle_.promise().emplace_out(std::move(value));
        //log(std::format("yield_value, out_: {}",
        //  out_ptr() ? out().msg_name : "empty").c_str());
        return { std::string_view(txn_name()) };
      }

      const std::string& txn_name() { return txn_name_; }
      void set_txn_name(const std::string& txn_name) { txn_name_ = txn_name; }

      auto active_handle() const { return active_handle_; }
      void set_active_handle(handle_t h) { active_handle_ = h; }

      auto root_handle() const { return root_handle_; }
      void set_root_handle(handle_t h) { root_handle_ = h; }

      auto prev_handle() const { return prev_handle_; }
      void set_prev_handle(handle_t h) { prev_handle_.emplace(h); }

      //auto& txn_handler() const { return *txn_handler_;  }
      //void set_txn_handler(handler_t* handler) { txn_handler_ = handler; }

      auto& in() const { return *in_.get(); }
      template<typename T> const T& in_as() const {
        // dynamic? i mean, i know when it's safe, but read up perf vs. best practice downcast
        return *static_cast<const T*>(in_.get());
      }
      dp::msg_ptr_t&& in_ptr() { return std::move(in_); }
      void emplace_in(dp::msg_ptr_t value) { in_ = std::move(value); }

      auto& out() const { return *out_.get(); }
      dp::msg_ptr_t&& out_ptr() { return std::move(out_); }
      void emplace_out(dp::msg_ptr_t value) { out_ = std::move(value); }

    private:
      handle_t root_handle_;
      handle_t active_handle_;
      std::optional<handle_t> prev_handle_;
      std::string txn_name_;
      dp::msg_ptr_t in_;
      dp::msg_ptr_t out_;
    };

    struct initial_awaitable {
      bool await_ready() { return false; }
      bool await_suspend(handle_t h) {
        log("initial_awaitable::await_suspend()");
        h.promise().set_root_handle(h);
        h.promise().set_active_handle(h);
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

  private:
    auto active_handle() const noexcept { return coro_handle_.promise().active_handle(); }
    auto root_handle() const noexcept { return coro_handle_.promise().root_handle(); }
    auto& txn_name() const noexcept { return coro_handle_.promise().txn_name(); }

  public:
    // TODO: this logic should be in an awaitable?
    [[nodiscard]] dp::msg_ptr_t&& send_value(dp::msg_ptr_t value)
    {
      auto& ap = active_handle().promise();
      ap.emplace_in(std::move(value));
      log(std::format("++{}::send_value, emplace_in({})", txn_name(),
        ap.in().msg_name).c_str());
      active_handle().resume();
      auto& rp = root_handle().promise();
      if (rp.out_ptr()) {
        log(std::format("--{}::send_value, out({})", txn_name(),
          rp.out().msg_name).c_str());
      } else {
        log(std::format("--{}::send_value, out empty", txn_name()).c_str());
      }
      return std::move(rp.out_ptr());
    }
  private:
    handle_t coro_handle_;
  }; // struct handler_t

  using promise_type = handler_t::promise_type;

  struct transfer_txn_awaitable {
    using handle_t = handler_t::handle_t;

    transfer_txn_awaitable(std::string_view txn_name, std::optional<handle_t> dst_handle,
        dp::msg_ptr_t msg) :
      dst_handle_(dst_handle), txn_name_(txn_name), msg_(std::move(msg)) {}

    transfer_txn_awaitable(std::string_view txn_name, handle_t dst_handle,
        dp::msg_ptr_t msg) :
      transfer_txn_awaitable(txn_name, std::optional(dst_handle), std::move(msg)) {}

    auto await_ready() { return false; }
    auto& await_resume() {
      log(std::format("transfer_txn_awaitable({})::await_resume()", txn_name_).c_str());
      return *src_promise_;
    }
  protected:
    handler_t::promise_type* src_promise_ = nullptr;
    std::optional<handle_t> dst_handle_;
    std::string txn_name_; // TODO: do i still need this here? or in src_promise_?
    dp::msg_ptr_t msg_;
  };

  template<typename stateT>
  struct start_txn_awaitable;
    
  template<typename stateT>
  auto make_start_txn(std::string_view txn_name, dp::msg_ptr_t msg,
    typename start_t<stateT>::state_ptr_t state)
  {
    return std::make_unique<start_t<stateT>>(txn_name, std::move(msg), std::move(state));
  }

  template<typename stateT>
  struct start_txn_awaitable : transfer_txn_awaitable {
    using start_t = dp::txn::start_t<stateT>;
    using state_ptr_t = start_t::state_ptr_t;

    start_txn_awaitable(std::string_view txn_name, handle_t handle,
        dp::msg_ptr_t msg, state_ptr_t state) :
      transfer_txn_awaitable(txn_name, handle, std::move(msg)),
      state_(std::move(state)) {}

    auto await_suspend(handle_t h) {
      log(std::format("start_txn_awaitable({})::await_suspend()", txn_name_).c_str());
      src_promise_ = &h.promise();
      auto& dst_p = dst_handle_->promise();
      dst_p.emplace_in(make_start_txn<stateT>(txn_name_, std::move(msg_), std::move(state_)));
      log(std::format("  dst_p.emplace_in({})", dst_p.in().msg_name).c_str());
      dst_p.set_root_handle(src_promise_->root_handle());
      dst_p.set_prev_handle(src_promise_->active_handle());
      auto& root_p = src_promise_->root_handle().promise();
      root_p.set_active_handle(dst_p.active_handle());
      return dst_handle_.value(); // symmetric transfer to dst
    }
  private:
    state_ptr_t state_;
  };

  struct complete_txn_awaitable : transfer_txn_awaitable {
    complete_txn_awaitable(std::string_view txn_name,
        std::optional<handle_t> handle, dp::msg_ptr_t msg) :
      transfer_txn_awaitable(txn_name, handle, std::move(msg)) {}

    std::coroutine_handle<> await_suspend(handle_t h) {
      log(std::format("complete_txn_awaitable({})::await_suspend()", txn_name_).c_str());
      src_promise_ = &h.promise();
      if (dst_handle_.has_value()) {
        auto& dst_p = dst_handle_->promise();
        dst_p.emplace_in(std::move(msg_));
        log(std::format("  dst_p.emplace_in({})", dst_p.in().msg_name).c_str());
        auto& root_p = src_promise_->root_handle().promise();
        root_p.set_active_handle(dst_handle_.value());
        src_promise_->prev_handle().reset();
        return dst_handle_.value(); // symmetric transfer to dst
      } else {
        log("  return noop_coroutine()");
        return std::noop_coroutine();
      }
    }
  };

  complete_txn_awaitable complete(promise_type& promise, msg_ptr_t msg_ptr);
  complete_txn_awaitable complete(promise_type& promise);
  complete_txn_awaitable complete(promise_type& promise, result_code rc);
} // namespace dp::txn

// TODO: probably want to try to relocate this in above dp{} block
namespace dp {
  constexpr auto is_start_txn(const msg_t& msg) {
    return msg.msg_name == msg::name::txn_start;
  }
  /*
  constexpr auto is_complete_txn(const msg_t& msg) {
    return msg.msg_name == msg::name::txn_complete;
  }

  constexpr auto get_txn_name(const msg_t& msg) {
    dynamic_cast<const txn::data_t&>(msg).txn_name;
  }
  */
} // namespace dp::txn