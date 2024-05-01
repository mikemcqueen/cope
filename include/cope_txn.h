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
#include "cope_result.h"
#include "internal/cope_log.h"
#include "tuple.h"

namespace cope::txn {
  enum class id_t : int {};

  inline constexpr auto make_id(int id) { return static_cast<id_t>(id); }

  enum class state : int {
    ready = 0,
    running,
    complete
  };

  // this can move back into task_t and not be templatized
  template<typename TaskT>
  struct basic_awaitable {
    using handle_t = TaskT::handle_type;
    using promise_type = TaskT::promise_type;

    bool await_ready() const { return {}; }
    void await_suspend(handle_t h) { promise_ = &h.promise(); }
    promise_type& await_resume() const { return *promise_; }

    const promise_type& promise() const { return *promise_; }
    promise_type& promise() { return *promise_; }

    // context() ? can i auto& it ?
    //auto& context() { return promise().context(); }

  private:
    promise_type* promise_ = nullptr;
  }; // struct basic_awaitable

  // txn::task_t
  template<typename ContextT>
  class task_t {
  public:
    using this_t = task_t<ContextT>;
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;
    using in_msg_type = typename ContextT::in_msg_type;
    using out_msg_type = typename ContextT::out_msg_type;

    // TODO: private?
    struct initial_awaitable {
      bool await_ready() { return false; }
      bool await_suspend([[maybe_unused]] handle_type h) {
        if (!h.promise().context().active_handle()) {
          h.promise().context().set_active_handle(h);
        }
        return false;
      }
      void await_resume() {}
    }; // initial_awaitable

    // promise_type
    struct promise_type {
      promise_type() = delete;
      promise_type(ContextT& context, id_t task_id) noexcept :
        context_(context), txn_id_(task_id) {}

      auto get_return_object() noexcept { return task_t{ this }; }
      initial_awaitable initial_suspend() noexcept { return {}; }
      std::suspend_always final_suspend() noexcept { return {}; }
      void unhandled_exception() {
        log::info("*** unhandled_exception() in txn_id:{}",
          (int)/*active_handle_.promise().*/txn_id());
        // TODO: store for retrieval? store what? force suspend? set error?
        // something?
        throw;
      }
      void return_void() { throw std::runtime_error("co_return not allowed"); }
      basic_awaitable<this_t> yield_value([[maybe_unused]] out_msg_type&& msg) {
        log::info("yielding msg from txn_id:{}...", (int)txn_id());
        ////root_handle_.promise().out() = std::move(msg);
        return {};
      }

      id_t txn_id() const { return txn_id_; }
      void set_txn_id(id_t txn_id) { txn_id_ = txn_id; }

      auto txn_state() const { return txn_state_; }
      bool txn_ready() const { return txn_state_ == state::ready; }
      bool txn_running() const { return txn_state_ == state::running; }
      void set_txn_state(state txn_state) {
        txn_state_ = txn_state;
        log::info("txn_id:{}:set_txn_state({})", (int)txn_id_, txn_state_);
      }

      const auto& context() const { return context_; }
      auto& context() { return context_; }

      auto prev_handle() const { return prev_handle_; }
      void set_prev_handle(handle_type h) { prev_handle_.emplace(h); }

    private:
      ContextT& context_;
      id_t txn_id_;
      state txn_state_ = state::ready;
      std::optional<handle_type> prev_handle_;
    }; // promise_type

    task_t() = delete;
    explicit task_t(promise_type* p) noexcept :
      coro_handle_(handle_type::from_promise(*p)) {}
    ~task_t() { if (coro_handle_) coro_handle_.destroy(); }

    task_t(task_t&) = delete;
    task_t& operator=(const task_t&) = delete;

    auto handle() const { return coro_handle_; }
    auto& promise() const { return coro_handle_.promise(); }

    template<typename Msg>
    // TODO: requires in_tuple_type contains Msg
    [[nodiscard]] decltype(auto) send_msg(Msg&& msg) {
      //validate_send_msg<(msg);
      promise().context().set_in(std::move(msg));
      log::info("sending msg_idx:{} to task_id:{}", 
        promise().context().in().index(), active_handle().promise().txn_id());
      active_handle().resume();
      //
      // NB: active_handle() may have changed at this point
      //
      log::info("received msg_idx:{} from txn_id:{}",
        promise().context().out().index(), active_handle().promise().txn_id());
      return promise().context().out();
    }

    static void complete_txn(promise_type& promise) {
      if (!promise.txn_running()) {
        // TODO: nasty place to throw. need to figure out error handling.
        log::info("ERROR txn::complete(): txn is not in running state");
        throw std::logic_error("txn::complete(): txn is not in running state");
      }
      if (promise.context().result().failed()) {
        log::info("ERROR txn_id:{} completing with code: {}", promise.txn_id(),
          promise.context().result().code);
      }
      promise.set_txn_state(state::complete);
    }

  private:
    handle_type active_handle() const { return promise().context().active_handle(); }
    auto txn_id() const { return promise().txn_id(); }

#if 0 //TODO template MsgT, StateT maybe
    template<typename msgT>
    void validate_send_msg(const msgT& msg) const {
      if (msg::is_start_txn(msg)) {
        if (promise().txn_running()) {
          throw std::logic_error("send_msg(): cannot send a start_txn msg"
            " to a running txn");
        }
      } else if (!promise().txn_running()) {
        throw std::logic_error("send_msg(): can only send a start_txn msg"
          " to a txn that is not running");
      }
    }
#endif 

    handle_type coro_handle_;
  }; // txn::task_t

  // txn::context_t
  template<typename... TypeBundles>
  // TODO: requires each bundle declare: in_tuple_type, out_tuple_type, ??
  class context_t {
  public:

    using in_concat_type = tuple::concat_t<typename TypeBundles::in_tuple_t...>;
    using in_tuple_type = tuple::distinct_t<in_concat_type>;
    using in_msg_type = tuple::to_variant_t<in_tuple_type>;

    using out_concat_type = tuple::concat_t<std::tuple<std::monostate>, typename TypeBundles::out_tuple_t...>;
    using out_tuple_type = tuple::distinct_t<out_concat_type>;
    using out_msg_type = tuple::to_variant_t<out_tuple_type>;

    using context_type = context_t<TypeBundles...>;
    using task_type = task_t<context_type>;
    using promise_type = task_type::promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    auto active_handle() const { return active_handle_; }
    void set_active_handle(handle_type h) {
      cope::log::info("set_active: task_id:{}", (int)h.promise().txn_id());
      active_handle_ = h;
    }

    operator result_t& () { return result_; }
    auto result() const { return result_; }
    auto set_result(result_code rc) { result_.code = rc; return result_; }

    const auto& in() const { return in_; }
    // TODO: use set_in, set_out only
    auto& in() { return in_; }
    const auto& out() const { return out_; }
    auto& out() { return out_; }

    //friend class task_t;
    //friend class receive_awaitable<task, start, state>
    template<typename T>
    void set_in(T&& value) { in_ = std::move(value); }

  private:
    handle_type active_handle_ = nullptr;
    result_t result_;
    in_msg_type in_;
    out_msg_type out_;
  }; // txn::context_t 

  // TODO: StartTxnT type instead of msg/state types?
  // txn::receive_awaitable
  template<typename TaskT, typename MsgT, typename StateT>
  struct receive_awaitable : basic_awaitable<TaskT> {
    using task_type = TaskT;
    using start_txn_t = msg::start_txn_t<MsgT, StateT>;

    receive_awaitable() = delete;
    explicit receive_awaitable(StateT& state) : state_(state) {}

    std::coroutine_handle<> await_suspend(task_type::handle_type h) {
      basic_awaitable<task_type>::await_suspend(h);
      log::info("recieve_awaitable::await_suspend() task_id:{}",
        this->promise().txn_id());
      if (this->promise().txn_running()) {
        throw std::logic_error(std::format("receive_awaitable::await_suspend(),"
          "txn_id:{} cannot be in 'running' state", this->promise().txn_id()));
      }
      if (this->promise().txn_state() == state::complete) {
        this->promise().set_txn_state(state::ready);
        if (this->promise().prev_handle().has_value()) {
          // txn complete, prev handle exists; symmetric xfer to prev_handle
          auto prev_handle = this->promise().prev_handle().value();
          auto& prev_promise = prev_handle.promise();
          // move promise.in to dst_promise.in
          log::info("symmetric xfer to txn_id:{}", (int)prev_promise.txn_id());
          this->promise().context().set_active_handle(prev_handle);
          this->promise().prev_handle().reset();
          return prev_handle; // symmetric transfer to dst (prev) handle
        }
        else {
          // txn complete, no prev handle; "root" txn will return to send_msg()
          // TODO: set_out
          this->promise().context().out() = std::monostate{};
          log::info("  returning a msg to send_msg()");
        }
      }
      log::info("txn_id:{} - returning noop_coroutine", (int)this->promise().txn_id());
      return std::noop_coroutine();
    }

    task_type::promise_type& await_resume() {
      log::info("receive_awaitable::await_resume() task_id:{}",
        (int)this->promise().txn_id());
      auto& txn = this->promise().context().in();
      // validate this is a start_txn message for the expected txn_id
      //this->promise().set_result(start_t::validate(txn, txn_id_));
      if (this->promise().context().result().failed()) {
        log::info("ERROR resuming txn_id:{}, ({})", (int)this->promise().txn_id(),
          this->promise().context().result().code);
        // TODO: error handling
        return this->promise();
      }
      auto& txn_start = std::get<start_txn_t>(txn);
      // move initial state into coroutine frame
      state_ = std::move(txn_start.state);
      // move msg from incoming txn to context.in
      auto msg{ std::move(txn_start.msg) };
      this->promise().context().set_in(std::move(msg));
      this->promise().set_txn_state(state::running);
      return this->promise();
    }

  private:
    StateT& state_;
  }; // txn::receive_awaitable

#if 0
  // maybe belongs elsewhere. maybe belongs here. maybe nowhere.
  // needs more template arguments and some thinkin.
  template<typename msg_baseT, typename msg_proxyT, typename state_proxyT>
  auto make_start_txn(id_t txn_id, msg_proxyT&& msg_proxy,
    state_proxyT&& state_proxy)
  {
    using start_t = start_t<msg_baseT, msg_proxyT, state_proxyT>;
    return std::make_unique<start_t>(txn_id, std::move(msg_proxy),
      std::move(state_proxy));
  }
#endif

  // txn::start_awaitable
  template</*typename FromTaskT, */typename ToTaskT, typename MsgT, typename StateT>
  struct start_awaitable : basic_awaitable<ToTaskT> {
    using start_txn_t = msg::start_txn_t<MsgT, StateT>;

    start_awaitable() = delete;
    // TODO: rvalue ref msg & state
    start_awaitable(ToTaskT::handle_type dst_handle, MsgT msg, StateT state) :
      dst_handle_(dst_handle), msg_(std::move(msg)), state_(std::move(state)) {}

    //    auto await_suspend(task_type::handle_type h) {
    auto await_suspend(ToTaskT::handle_type h) {
      basic_awaitable<ToTaskT>::await_suspend(h);
      log::info("start_awaitable:await_suspend() suspending txn_id:{}...",
        (int)this->promise().txn_id());
      auto& dst_promise = dst_handle_.promise();
      start_txn_t txn_start{ std::move(msg_), std::move(state_) };
      // TODO: set_in
      dst_promise.context().in() = std::move(txn_start);
      log::info("  symmetric xfer to txn_id:{}", (int)dst_promise.txn_id());
      dst_promise.set_prev_handle(this->promise().context().active_handle());
      this->promise().context().set_active_handle(dst_handle_);
      return dst_handle_; // symmetric transfer to dst
    }

    auto& await_resume() {
      log::info("resuming txn_id:{}...", (int)this->promise().txn_id());
      return this->promise();
    }

  private:
    ToTaskT::handle_type dst_handle_;
    MsgT msg_;
    StateT state_;
  }; // txn::start_awaitable

  /*
  template<typename msg_proxyT>
  inline void complete(typename task_t<msg_proxyT>::promise_type& promise) {
    if (!promise.txn_running()) {
      throw std::logic_error("txn::complete(): txn is not in running state");
    }
    promise.set_txn_state(state::complete);
  }
  */
} // namespace cope::txn

template <>
struct std::formatter<cope::txn::id_t> : std::formatter<std::string> {
  auto format(cope::txn::id_t id, std::format_context& ctx) const {
    return std::format_to(ctx.out(), "{}", (int)id);
  }
};

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
