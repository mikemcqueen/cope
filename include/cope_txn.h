// cope_txn.h

#ifndef INCLUDE_COPE_TXN_H
#define INCLUDE_COPE_TXN_H

#include <coroutine>
#include <format>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>
#include "cope_msg.h"
#include "internal/cope_log.h"

namespace cope::txn {
  enum class id_t : int32_t {};

  namespace id {
    constexpr id_t kUndefined = static_cast<id_t>(-2);
  }

  enum class state : int {
    ready = 0,
    running,
    complete
  };

  /*
  struct data_t {
    data_t(txn::id_t txn_id, msg_t&& msg) noexcept :
      txn_id(txn_id), msg(std::move(msg)) {}

    id_t txn_id{ id::kUndefined };
    msg_t msg;
  };
  */

  template<typename stateT>
  struct start_t { // : data_t {
    static auto validate(const msg_t& msg, id_t txn_id) {
      if (!is_start_txn(msg)) {
        return result_code::e_unexpected_msg_name;
      }
      const auto txn = std::any_cast<start_t<stateT>>(&msg.payload);
      if (!txn) {
        return result_code::e_unexpected_msg_type;
      }
      if (txn->txn_id != txn_id) {
        return result_code::e_unexpected_txn_name; // TODO
      }
      return result_code::s_ok;
    }

/*
    constexpr start_t(id_t txn_id, msg_t&& msg, stateT&& state) noexcept :
      data_t(txn_id, std::move(msg)),
      state(std::move(state))
    {}
*/

    id_t txn_id{ id::kUndefined };
    msg_t msg{};
    stateT state{};
  };

  template<typename stateT>
  auto make_start_txn(id_t txn_id, msg_t&& msg, stateT&& state) {
    auto payload = std::make_any<start_t<stateT>>(txn_id, std::move(msg),
      std::forward<stateT>(state));
//    std::any x = payload;
    return msg::data_t(msg::id::kTxnStart, std::move(payload));
  }

#ifdef COPE_MSG_NAME
  struct data_t : msg::data_t {
    data_t(std::string_view msg_name, std::string_view txn_name) :
      msg::data_t(msg_name), txn_name(txn_name) {}

    std::string txn_name;
  };

  template<typename stateT>
  struct start_t : data_t {
    using state_ptr_t = std::unique_ptr<stateT>;
 
    static const msg_t& msg_from(const msg_t& txn) {
      auto& txn_start = dynamic_cast<const start_t<stateT>&>(txn);
      return *txn_start.msg_ptr.get();
    }

    // TODO getting state from a message usually precedes a move, therefore 
    // no const. maybe this should just be move_state(txn, stateT*)
    static stateT&& state_from(msg_t& txn) {
      start_t<stateT>& txn_start = dynamic_cast<start_t<stateT>&>(txn);
      return std::move(*txn_start.state.get());
    }

    static auto validate(const msg_t& msg, std::string_view txn_name) {
      if (!is_start_txn(msg)) {
        return result_code::e_unexpected_msg_name;
      }
      const auto* txn = dynamic_cast<const start_t<stateT>*>(&msg);
      if (!txn) {
        return result_code::e_unexpected_msg_type;
      }
      if (txn->txn_name != txn_name) {
        return result_code::e_unexpected_txn_name;
      }
      return result_code::s_ok;
    }

    constexpr start_t(std::string_view txn_name, msg_ptr_t msg_ptr,
        state_ptr_t state_ptr) :
      data_t(msg::name::kTxnStart, txn_name),
      msg_ptr(std::move(msg_ptr)), state_ptr(std::move(state_ptr)) {}

    msg_ptr_t msg_ptr;
    state_ptr_t state_ptr;
  };
#endif // COPE_MSG_NAME

  class handler_t final {
  public:
    struct promise_type;
    using handle_t = std::coroutine_handle<promise_type>;

    struct basic_awaitable {
      bool await_ready() const { return false; }
      void await_suspend(handle_t h) { promise_ = &h.promise(); }
      promise_type& await_resume() const { return *promise_; }

      const promise_type& promise() const { return *promise_; }
      promise_type& promise() { return *promise_; }

    private:
      promise_type* promise_ = nullptr;
    };

    struct initial_awaitable;
    struct promise_type {
      auto get_return_object() noexcept { return handler_t{ this }; }
      initial_awaitable initial_suspend() noexcept { return {}; }
      std::suspend_always final_suspend() noexcept { return {}; }
      void unhandled_exception() {
        log::info("*** unhandled_exception() in {}",
          (int)active_handle_.promise().txn_id());
        throw;
      }
      void return_void() { throw std::runtime_error("co_return not allowed"); }
      // TODO: return private awaitable w/o txn name?
      basic_awaitable yield_value(msg_t&& msg) {
        log::info("yielding msg_id:{} from txn_id:{}...",
          (int)msg.msg_id, (int)txn_id());
        root_handle_.promise().emplace_out(std::move(msg));
        msg.payload.reset();
        return {};
      }

      auto active_handle() const { return active_handle_; }
      void set_active_handle(handle_t h) { active_handle_ = h; }

      auto root_handle() const { return root_handle_; }
      void set_root_handle(handle_t h) { root_handle_ = h; }

      auto prev_handle() const { return prev_handle_; }
      void set_prev_handle(handle_t h) { prev_handle_.emplace(h); }

      // const auto&?
      auto& in() { return in_; }
      void emplace_in(msg_t&& in) { in_ = std::move(in); in.payload.reset();  }
//      [[nodiscard]] msg_ptr_t&& in_ptr() { return std::move(in_ptr_); }

      auto& out() { return out_; }
      void emplace_out(msg_t&& out) { out_ = std::move(out); out.payload.reset(); }
//      [[nodiscard]] msg_ptr_t&& out_ptr() { return std::move(out_ptr_); }

      id_t txn_id() const { return txn_id_; }
      void set_txn_id(id_t txn_id) { txn_id_ = txn_id; }

      operator result_t&() { return result_; }
      auto result() const { return result_; }
      result_t set_result(result_code rc) { result_.set(rc); return result_;  }

      auto txn_state() const { return txn_state_; }
      bool txn_ready() const { return txn_state_ == state::ready; }
      bool txn_running() const { return txn_state_ == state::running; }
      void set_txn_state(state txn_state) {
        txn_state_ = txn_state;
        log::info("txn_id:{}:set_txn_state({})", (int)txn_id_, txn_state_);
      }

    private:
      handle_t root_handle_;
      handle_t active_handle_;
      std::optional<handle_t> prev_handle_;
      id_t txn_id_;
      state txn_state_ = state::ready;
      msg_t in_;
      msg_t out_;
      result_t result_;
    }; // struct promise_type

    // TODO: private?
    struct initial_awaitable {
      bool await_ready() { return false; }
      bool await_suspend(handle_t h) {
        h.promise().set_root_handle(h);
        h.promise().set_active_handle(h);
        return false;
      }
      void await_resume() {}
    };

  public:
    explicit handler_t(promise_type* p) noexcept :
      coro_handle_(handle_t::from_promise(*p)) {}
    ~handler_t() { if (coro_handle_) coro_handle_.destroy(); }

    handler_t(handler_t&) = delete;
    handler_t operator=(handler_t&) = delete;

    auto handle() const { return coro_handle_; }
    auto& promise() const { return coro_handle_.promise(); }

    // TODO: this logic should be in an awaitable?
    /*[[nodiscard]]*/ msg_t send_msg(msg_t&& msg) {
      validate_send_msg(msg);
      auto& active_p = active_handle().promise();
      active_p.emplace_in(std::move(msg));
      log::info("sending msg_id:{} to txn_id:{}", (int)active_p.in().msg_id,
        (int)active_p.txn_id());
      active_handle().resume();
      // active_handle() may have changed at this point
      auto& root_p = root_handle().promise();
      log::info("received msg_id:{} from txn_id:{}", (int)root_p.out().msg_id,
        (int)active_handle().promise().txn_id());
      auto out = std::move(root_p.out());
      root_p.out().payload.reset();
      return out;
    }

  private:
    handle_t active_handle() const { return promise().active_handle(); }
    handle_t root_handle() const { return promise().root_handle(); }
    auto txn_id() const { return promise().txn_id(); }

    void validate_send_msg(const msg_t& msg) const {
      if (msg::is_start_txn(msg)) {
        if (handle().promise().txn_running()) {
          throw std::logic_error("send_msg(): txn already running");
        }
      } else if (!handle().promise().txn_running()) {
        throw std::logic_error("send_msg(): txn not running");
      }
    }

    handle_t coro_handle_;
  }; // class handler_t

  template<typename stateT>
  struct receive : handler_t::basic_awaitable {
    receive(id_t txn_id, stateT& state) :
      txn_id_(txn_id), state_(state) {}

    std::coroutine_handle<> await_suspend(handler_t::handle_t h) {
      handler_t::basic_awaitable::await_suspend(h);
      if (promise().txn_running()) {
        throw std::logic_error(std::format("txn::receive(txn_id:{}) cannot be"
          "used with txn in 'running' state", (int)promise().txn_id()));
      }
      promise().set_txn_id(txn_id_);
      if (promise().txn_state() == state::complete) {
        promise().set_txn_state(state::ready);
        if (promise().prev_handle().has_value()) {
          // txn complete, prev handle exists; symmetric xfer to prev_handle
          handler_t::handle_t dst_handle = promise().prev_handle().value();
          auto& dst_promise = dst_handle.promise();
          // move promise.in to dst_promise.in
          dst_promise.emplace_in(std::move(promise().in()));
          log::info("  returning a msg_id:{} to txn_id:{}",
            (int)dst_promise.in().msg_id, (int)dst_promise.txn_id());
          auto& root_promise = promise().root_handle().promise();
          root_promise.set_active_handle(dst_handle);
          // reset all handles of this completed coro to itself
          promise().set_root_handle(h);
          promise().set_active_handle(h);
          promise().prev_handle().reset();
          return dst_handle; // symmetric transfer to dst (prev) handle
        }
        else /*if (promise().in_ptr()) */ {
          // txn complete, no prev handle; root txn will return to send_msg()
          // move promise.in to root_promise.out
          // TODO: why move in -> out, we never use this?
          auto& root_promise = promise().root_handle().promise();
          root_promise.emplace_out(std::move(promise().in()));
          log::info("  returning a msg_id:{} to send_msg()",
            (int)root_promise.out().msg_id);
        }
/*
        else {
          // txn complete, no prev handle, no "in" value; not possible
          throw std::runtime_error("impossible state");
        }
*/
      }
      log::info("  txn_id:{} - returning noop_coroutine",
        (int)promise().txn_id());
      return std::noop_coroutine();
    }

    handler_t::promise_type& await_resume() {
      using start_t = start_t<stateT>;
      auto& msg = promise().in();
      // validate this is a start_txn message for the txn we are expecting.
      promise().set_result(start_t::validate(msg, txn_id_));
      if (promise().result().failed()) {
        log::info("ERROR resuming txn_id:{}, ({})", (int)txn_id_,
          promise().result());
        return promise();
      }
      auto txn_start = std::any_cast<start_t>(&msg.payload);
      if (!txn_start) throw std::bad_any_cast();
      // copy/move initial state into coroutine frame
      state_ = std::move(txn_start->state);
      // move msg contained in txn payload to promise().in()
      promise().emplace_in(std::move(txn_start->msg));
//      msg.payload.reset();
      promise().set_txn_state(state::running);
      return promise();
    }

  private:
    id_t txn_id_;
    stateT& state_;
  }; // struct receive

  template<typename stateT>
  struct start : handler_t::basic_awaitable {
    using handle_t = handler_t::handle_t;

    constexpr start(handle_t dst_handle, msg_t&& msg, stateT&& state)
        noexcept :
      dst_handle_(dst_handle), msg_(std::move(msg)),
      state_(std::move(state))
    {}

    auto await_suspend(handle_t h) {
      handler_t::basic_awaitable::await_suspend(h);
      log::info("start_txn_awaitable: Suspending txn_id:{}...",
        (int)promise().txn_id());
      auto& dst_promise = dst_handle_.promise();
      dst_promise.emplace_in(make_start_txn<stateT>(dst_promise.txn_id(),
        std::move(msg_), std::move(state_)));
      log::info("  sending a msg_id:{} to txn_id:{}", (int)dst_promise.in().msg_id,
        (int)dst_promise.txn_id());
      dst_promise.set_root_handle(promise().root_handle());
      dst_promise.set_prev_handle(promise().active_handle());
      auto& root_promise = promise().root_handle().promise();
      root_promise.set_active_handle(dst_promise.active_handle());
      return dst_handle_; // symmetric transfer to dst
    }

    auto& await_resume() {
      log::info("resuming txn_id:{}...", (int)promise().txn_id());
      return promise();
    }

  private:
    handle_t dst_handle_;
    msg_t msg_;
    stateT state_;
  }; // struct start_awaitable

  inline void complete(handler_t::promise_type& promise) {
    if (!promise.txn_running()) {
      throw std::logic_error(std::format("txn::complete(): txn txn_id:{} is"
        "not in running state", (int)promise.txn_id()));
    }
    promise.set_txn_state(state::complete);
  }
} // namespace cope::txn

template <>
struct std::formatter<cope::txn::state> : std::formatter<std::string> {
  auto format(cope::txn::state st, std::format_context& ctx) const {
    using cope::txn::state;
    static std::unordered_map<state, std::string> state_name_map = {
      { state::ready, "ready" },
      { state::running, "running" },
      { state::complete, "complete" }
    };
    return std::format_to(ctx.out(), "{}", state_name_map[st]);
  }
};

#endif // INCLUDE_COPE_TXN_H