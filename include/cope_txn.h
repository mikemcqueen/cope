// cope_txn.h

#ifndef INCLUDE_COPE_TXN_H
#define INCLUDE_COPE_TXN_H

#include <coroutine>
#include <format>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>
#include "cope_proxy.h"
#include "cope_msg.h"
#include "internal/cope_log.h"

namespace cope::txn {
  enum class id_t : int32_t {};

  inline constexpr auto make_id(int id) { return static_cast<id_t>(id); }

  enum class state : int {
    ready = 0,
    running,
    complete
  };

  template<typename msg_baseT>
  struct data_t : msg_baseT {
    data_t(msg::id_t msg_id, txn::id_t txn_id) :
      msg_baseT(msg_id), txn_id(txn_id) {}

    id_t txn_id;
  };

  template<typename txn_baseT, typename msg_proxyT, typename state_proxyT>
  struct start_t : data_t<txn_baseT> {
    //  using state_ptr_t = std::unique_ptr<stateT>;
 
    /*
    static const msg_t& msg_from(const msg_t& txn) {
      auto& txn_start = dynamic_cast<const start_t<stateT>&>(txn);
      return *txn_start.msg_ptr.get();
    }
    */

    /*
    // TODO getting state from a message usually precedes a move, therefore 
    // no const. maybe this should just be move_state(txn, stateT*)
    [[nodiscard]] static stateT&& state_from(msg_t& txn) {
      start_t<stateT>& txn_start = dynamic_cast<start_t<stateT>&>(txn);
      return std::move(*txn_start.state.get());
    }
    */

    // TODO: template<...> ?
    static auto validate(const txn_baseT& msg, id_t /*txn_id*/) {
      if (!msg::is_start_txn<txn_baseT>(msg)) {
        return result_code::e_unexpected_msg_id;
      }
      /*
      const auto* txn = dynamic_cast<const start_t<stateT>*>(&msg);
      if (!txn) {
        return result_code::e_unexpected_msg_type;
      }
      if (txn->txn_id != txn_id) {
        return result_code::e_unexpected_txn_id;
      }
      */
      return result_code::s_ok;
    }

    constexpr start_t(id_t txn_id, msg_proxyT&& msg_proxy,
        state_proxyT&& state_proxy) noexcept :
      data_t<txn_baseT>(msg::id::kTxnStart, txn_id),
      msg_proxy(std::move(msg_proxy)),
      state_proxy(std::move(state_proxy)) {}

    msg_proxyT msg_proxy;
    state_proxyT state_proxy;
  };

  template<typename msg_proxyT>
  class handler_t {
  public:
    struct promise_type;
    using handle_t = std::coroutine_handle<promise_type>;
    using msg_type = typename msg_proxyT::type;

    struct basic_awaitable {
      bool await_ready() const { return false; }
      void await_suspend(handle_t h) { promise_ = &h.promise(); }
      promise_type& await_resume() const { return *promise_; }

      const promise_type& promise() const { return *promise_; }
      promise_type& promise() { return *promise_; }

    private:
      promise_type* promise_ = nullptr;
    }; // struct basic_awaitable

    struct initial_awaitable;
    struct promise_type {//: io_proxy_t {
      auto get_return_object() noexcept { return handler_t{ this }; }
      initial_awaitable initial_suspend() noexcept { return {}; }
      std::suspend_always final_suspend() noexcept { return {}; }
      void unhandled_exception() {
        log::info("*** unhandled_exception() in txn_id:{}",
          (int)active_handle_.promise().txn_id());
        // TODO: store for retrieval? store what? force suspend? set error?
        // something?
        throw;
      }
      void return_void() { throw std::runtime_error("co_return not allowed"); }
      // TODO: return private awaitable w/o txn name?
      // TODO: r-value reference?
      template<typename msgT>
      // TODO: should be proxy_t<msgT> or just msg_proxyT
      basic_awaitable yield_value(msgT&& msg) {
        log::info("yielding msg_id:not-generic-fixit from txn_id:{}...",
          /*(int)msg.msg_id,*/ (int)txn_id());
        root_handle_.promise().out().emplace(std::move(msg));
        return {};
      }

      auto active_handle() const { return active_handle_; }
      void set_active_handle(handle_t h) { active_handle_ = h; }

      auto root_handle() const { return root_handle_; }
      void set_root_handle(handle_t h) { root_handle_ = h; }

      auto prev_handle() const { return prev_handle_; }
      void set_prev_handle(handle_t h) { prev_handle_.emplace(h); }

      id_t txn_id() const { return txn_id_; }
      void set_txn_id(id_t txn_id) { txn_id_ = txn_id; }

      operator result_t& () { return result_; }
      auto result() const { return result_; }
      auto set_result(result_code rc) { result_.set(rc); return result_; }

      auto txn_state() const { return txn_state_; }
      bool txn_ready() const { return txn_state_ == state::ready; }
      bool txn_running() const { return txn_state_ == state::running; }
      void set_txn_state(state txn_state) {
        txn_state_ = txn_state;
        log::info("txn_id:{}:set_txn_state({})", (int)txn_id_, txn_state_);
      }

      const auto& in() const { return in_; }
      auto& in() { return in_; }
      const auto& out() const { return out_; }
      auto& out() { return out_; }

    private:
      handle_t root_handle_;
      handle_t active_handle_;
      std::optional<handle_t> prev_handle_;
      id_t txn_id_;
      state txn_state_ = state::ready;
      result_t result_;
      msg_proxyT in_;
      msg_proxyT out_;
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
    }; // struct initial_awaitable

    explicit handler_t(promise_type* p) noexcept :
      coro_handle_(handle_t::from_promise(*p)) {}
    ~handler_t() { if (coro_handle_) coro_handle_.destroy(); }

    handler_t(handler_t&) = delete;
    handler_t operator=(handler_t&) = delete;

    auto handle() const { return coro_handle_; }
    auto& promise() const { return coro_handle_.promise(); }

    template<typename msg_proxyT>
    [[nodiscard]]
    decltype(auto) send_msg(msg_proxyT& msg_proxy) {
      validate_send_msg(msg_proxy.get());
      auto& active_p = active_handle().promise();
      active_p.in().emplace(msg_proxy.get_moveable());
      log::info("sending msg_id:{} to txn_id:{}",
        (int)active_p.in().get().msg_id, (int)active_p.txn_id());
      active_handle().resume();
      // active_handle() may have changed at this point
      auto& root_p = root_handle().promise();
      /* TODO: has_out()? 
      log::info("received msg_id:{} from txn_id:{}", root_p.out_ptr() ?
        (int)root_p.out().msg_id : 0,
        (int)active_handle().promise().txn_id());
      */
      return root_p.out().get_moveable();
    }

  private:
    handle_t active_handle() const { return promise().active_handle(); }
    handle_t root_handle() const { return promise().root_handle(); }
    auto txn_id() const { return promise().txn_id(); }

    template<typename msgT>
    void validate_send_msg(const msgT& msg) const {
      if (msg::is_start_txn(msg)) {
        if (handle().promise().txn_running()) {
          throw std::logic_error(std::format("send_msg(): txn already running"
            " ({})", (int)msg.msg_id));
        }
      } else if (!handle().promise().txn_running()) {
        throw std::logic_error("send_msg(): txn not running");
      }
    }

    handle_t coro_handle_;
  }; // class handler_t

  // receive
  template<typename txn_baseT, typename msg_proxyT, typename state_proxyT>
  struct receive_awaitable : handler_t<msg_proxyT>::basic_awaitable {
    using handler_t = txn::handler_t<msg_proxyT>;
    using state_t = typename state_proxyT::type;

    receive_awaitable(id_t txn_id, state_t& state) :
      txn_id_(txn_id), state_(state) {}

    std::coroutine_handle<> await_suspend(handler_t::handle_t h) {
      handler_t::basic_awaitable::await_suspend(h);
      if (this->promise().txn_running()) {
        throw std::logic_error(std::format("receive_awaitable(), txn_id:{}"
          " cannot be in 'running' state", (int)this->promise().txn_id()));
      }
      this->promise().set_txn_id(txn_id_);
      if (this->promise().txn_state() == state::complete) {
        this->promise().set_txn_state(state::ready);
        if (this->promise().prev_handle().has_value()) {
          // txn complete, prev handle exists; symmetric xfer to prev_handle
          typename handler_t::handle_t dst_handle = this->promise().prev_handle().value();
          auto& dst_promise = dst_handle.promise();
          // move promise.in to dst_promise.in
          dst_promise.in().emplace(this->promise().in().get_moveable());
          log::info("  returning a msg_id:{} to txn_id:{}",
            (int)dst_promise.in().get().msg_id, (int)dst_promise.txn_id());
          auto& root_promise = this->promise().root_handle().promise();
          root_promise.set_active_handle(dst_handle);
          // reset all handles of this completed coro to itself
          this->promise().set_root_handle(h);
          this->promise().set_active_handle(h);
          this->promise().prev_handle().reset();
          return dst_handle; // symmetric transfer to dst (prev) handle
        }
        else {/*if (this->promise().in_ptr()) {*/
          // txn complete, no prev handle; root txn will return to send_msg()
          // move promise.in to root_promise.out
          auto& root_promise = this->promise().root_handle().promise();
          root_promise.out().emplace(this->promise().in().get_moveable());
          log::info("  returning a msg_id:{} to send_msg()",
            (int)root_promise.out().get().msg_id);
        }
        /*
        else {
          log::info("terminating");
          // txn complete, no prev handle, no "in" value; not possible
          throw std::runtime_error("impossible state");
        }
        */
      }
      log::info("txn_id:{} - returning noop_coroutine", (int)this->promise().txn_id());
      return std::noop_coroutine();
    }

    handler_t::promise_type& await_resume() {
      using start_t = start_t<txn_baseT, msg_proxyT, state_proxyT>;
      auto& txn = this->promise().in().get();
      // validate this is a start_txn message for the expected txn_id
      this->promise().set_result(start_t::validate(txn, txn_id_));
      if (this->promise().result().failed()) {
        log::info("ERROR resuming txn_id:{}, ({})", (int)txn_id_,
          this->promise().result());
        // TODO: error handling
        return this->promise();
      }
      auto& txn_start = txn.template as<start_t&>();
      // move initial state into coroutine frame
      state_ = std::move(txn_start.state_proxy.get());
      // move msg from incoming txn to this->promise().in()
      this->promise().in().emplace(txn_start.msg_proxy.get_moveable());
      this->promise().set_txn_state(state::running);
      return this->promise();
    }

  private:
    id_t txn_id_;
    state_t& state_;
  }; // struct receive_awaitable

  // maybe belongs elsewhere. maybe belongs here. maybe nowhere.
  // needs more template arguments and some thinkin.
  template<typename msg_proxyT, typename state_proxyT>
  auto make_start_txn(id_t txn_id, msg_proxyT&& msg_proxy,
    state_proxyT&& state_proxy)
  {
    using start_t = start_t<msg_proxyT, state_proxyT>;
    return std::make_unique<start_t>(txn_id, std::move(msg_proxy),
      std::move(state_proxy));
  }

  // start
  // TODO!
  template<typename txn_baseT, typename msg_proxyT, typename state_proxyT>
  struct start_awaitable : handler_t<msg_proxyT>::basic_awaitable {
    using base_t = handler_t<msg_proxyT>;
    using handle_t = base_t::handle_t;
    //using state_ptr_t = start_t<txn_baseT, msg_proxyT, stateT>::state_ptr_t;

    start_awaitable(handle_t dst_handle, msg_proxyT msg_proxy,
        state_proxyT state_proxy) :
      dst_handle_(dst_handle), msg_proxy_(std::move(msg_proxy)),
      state_proxy_(std::move(state_proxy))
    {}

    auto await_suspend(handle_t h) {
      base_t::basic_awaitable::await_suspend(h);
      log::info("start_txn_awaitable: suspending txn_id:{}...",
        (int)this->promise().txn_id());
      auto& dst_promise = dst_handle_.promise();
      dst_promise.in().emplace(make_start_txn(dst_promise.txn_id(),
        msg_proxy_.get_moveable(), state_proxy_.get_moveable()));
      log::info("  sending a msg_id:{} to txn_id:{}",
        (int)dst_promise.in().msg_id, (int)dst_promise.txn_id());
      dst_promise.set_root_handle(this->promise().root_handle());
      dst_promise.set_prev_handle(this->promise().active_handle());
      auto& root_promise = this->promise().root_handle().promise();
      root_promise.set_active_handle(dst_promise.active_handle());
      return dst_handle_; // symmetric transfer to dst
    }

    auto& await_resume() {
      log::info("resuming txn_id:{}...", (int)this->promise().txn_id());
      return this->promise();
    }

  private:
    handle_t dst_handle_;
    msg_proxyT msg_proxy_;
    state_proxyT state_proxy_;
  }; // struct start_awaitable

  template<typename msg_proxyT>
  inline void complete(typename handler_t<msg_proxyT>::promise_type& promise) {
    if (!promise.txn_running()) {
      throw std::logic_error("txn::complete(): txn is not in running state");
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