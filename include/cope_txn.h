// cope_txn.h

#ifndef INCLUDE_COPE_TXN_H
#define INCLUDE_COPE_TXN_H

#include <coroutine>
#include <format>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>
//#include "cope_proxy.h"
#include "cope_msg.h"
#include "cope_result.h"
#include "internal/cope_log.h"
#include "tuple.h"

namespace cope::txn {
  enum class id_t : int32_t {};

  inline constexpr auto make_id(int id) { return static_cast<id_t>(id); }

  enum class state : int {
    ready = 0,
    running,
    complete
  };

#if 0
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
  }; // struct start_t
#endif

  // this can move back into task_t and not be templatized
  template<typename TaskT>
  struct basic_awaitable {
    using handle_t = TaskT::handle_t;
    using promise_type = TaskT::promise_type;

    bool await_ready() const { return false; }
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
    using handle_t = std::coroutine_handle<promise_type>;
    using in_msg_t = typename ContextT::in_msg_t;
    using out_msg_t = typename ContextT::out_msg_t;

    // TODO: private?
    struct initial_awaitable {
      bool await_ready() { return false; }
      bool await_suspend(handle_t h) {
        h.promise().context().set_active_handle(h);
        return false;
      }
      void await_resume() {}
    }; // initial_awaitable

    // promise_type
    struct promise_type {
      promise_type() = delete;
      promise_type(ContextT& context) : context_(context) {}

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
      // TODO: return private awaitable w/o txn name?
      basic_awaitable<this_t> yield_value([[maybe_unused]] out_msg_t&& msg) {
        log::info("yielding msg from txn_id:{}...",
          /*(int)msg.msg_id,*/ (int)txn_id());
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
      void set_prev_handle(handle_t h) { prev_handle_.emplace(h); }

    private:
      id_t txn_id_;
      state txn_state_ = state::ready;
      ContextT& context_;
      std::optional<handle_t> prev_handle_;
    }; // promise_type

    task_t() = delete;
    explicit task_t(promise_type* p) noexcept :
      coro_handle_(handle_t::from_promise(*p)) {}
    ~task_t() { if (coro_handle_) coro_handle_.destroy(); }

    task_t(task_t&) = delete;
    task_t& operator=(const task_t&) = delete;

    auto handle() const { return coro_handle_; }
    auto& promise() const { return coro_handle_.promise(); }

    /*
    //template<typename msg_proxyT>
    [[nodiscard]] decltype(auto) start_txn(in_msg_t&& msg) {

    }
    */

    template<typename MsgT>
    // TODO: requires in_tuple_t contains MsgT
    [[nodiscard]] decltype(auto) send_msg(MsgT&& msg) {
      //validate_send_msg<(msg);
      promise().context().in() = std::move(msg);
      auto& active_p = active_handle().promise();
      log::info("sending msg to txn_id:{}",
        /*(int)active_p.in().get().msg_id,*/ (int)active_p.txn_id());
      active_handle().resume();
      // NB: active_handle() may have changed at this point
      //auto& root_p = root_handle().promise();
      /* TODO: has_out()? 
      log::info("received msg_id:{} from txn_id:{}", root_p.out_ptr() ?
        (int)root_p.out().msg_id : 0,
        (int)active_handle().promise().txn_id());
      */
      return promise().context().out();
    }

    static void complete_txn(promise_type& promise) {
      if (!promise.txn_running()) {
        // TODO: nasty place to throw. need to figure out error handling.
        log::info("ERROR txn::complete(): txn is not in running state");
        throw std::logic_error("txn::complete(): txn is not in running state");
      }
      promise.set_txn_state(state::complete);
    }

  private:
    handle_t active_handle() const { return promise().context().active_handle(); }
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

    handle_t coro_handle_;
  }; // txn::task_t

  // txn::context_t
  template<typename... TypeBundleTs>
  // TODO: requires each bundle declare: in_tuple_t, out_tuple_t
  class context_t {
  public:

    using in_concat_t = tuple::concat_t<typename TypeBundleTs::in_tuple_t...>;
    using in_tuple_t = tuple::distinct_t<in_concat_t>;
    using in_msg_t = tuple::to_variant_t<in_tuple_t>;

    using out_concat_t = tuple::concat_t<std::tuple<std::monostate>, typename TypeBundleTs::out_tuple_t...>;
    using out_tuple_t = tuple::distinct_t<out_concat_t>;
    using out_msg_t = tuple::to_variant_t<out_tuple_t>;

    using this_t = context_t<TypeBundleTs...>;
    using task_t = task_t<this_t>;
    using promise_t = task_t::promise_type;
    using handle_t = std::coroutine_handle<promise_t>;

    /*
    using handle_concat_t = std::tuple<std::monostate, typename TypeBundleTs::handle_t...>;
    using handle_tuple_t = tuple::distinct_t<handle_concat_t>;
    using handle_t = tuple::to_variant_t<handle_tuple_t>;
    */

    auto active_handle() const { return active_handle_; }
    void set_active_handle(handle_t h) { active_handle_ = h; }

    //auto root_handle() const { return root_handle_; }
    //void set_root_handle(handle_t h) { root_handle_ = h; }

    operator result_t& () { return result_; }
    auto result() const { return result_; }
    auto set_result(result_code rc) { result_.set(rc); return result_; }

    const auto& in() const { return in_; }
    auto& in() { return in_; }
    const auto& out() const { return out_; }
    auto& out() { return out_; }

  private:
    handle_t active_handle_;
    //std::optional<handle_t> prev_handle_;
    result_t result_;
    in_msg_t in_;
    out_msg_t out_;
  }; // txn::context_t 

  // TODO: StartTxnT type instead of msg/state types?
  // txn::receive_awaitable
  template<typename TaskT, typename MsgT, typename StateT>
  struct receive_awaitable : basic_awaitable<TaskT> {
    using task_t = TaskT;
    using start_txn_t = msg::start_txn_t<MsgT, StateT>;

    receive_awaitable(id_t txn_id, StateT& state) :
      txn_id_(txn_id), state_(state) {}

    std::coroutine_handle<> await_suspend(task_t::handle_t h) {
      basic_awaitable<task_t>::await_suspend(h);
      if (this->promise().txn_running()) {
        throw std::logic_error(std::format("receive_awaitable(), txn_id:{}"
          " cannot be in 'running' state", (int)this->promise().txn_id()));
      }
      this->promise().set_txn_id(txn_id_);
      if (this->promise().txn_state() == state::complete) {
        this->promise().set_txn_state(state::ready);
        if (this->promise().prev_handle().has_value()) {
          // txn complete, prev handle exists; symmetric xfer to prev_handle
          typename task_t::handle_t dst_handle =
            this->promise().prev_handle().value();
          /*
          auto& dst_promise = dst_handle.promise();
          // move promise.in to dst_promise.in
          dst_promise.in().emplace(this->promise().in().get_moveable());
          log::info("  returning a msg to txn_id:{}",
            //(int)dst_promise.in().get().msg_id,
            (int)dst_promise.txn_id());
          */
          //auto& root_promise = this->promise().root_handle().promise();
          //root_promise.set_active_handle(dst_handle);
          // set context active handle, and clear our prev handle
          //this->promise().set_root_handle(h);
          this->promise().context().set_active_handle(h);
          this->promise().prev_handle().reset();
          return dst_handle; // symmetric transfer to dst (prev) handle
        }
        else {/*if (this->promise().in_ptr()) {*/
          // txn complete, no prev handle; root txn will return to send_msg()
          // move promise.in to root_promise.out
          //auto& root_promise = this->promise().root_handle().promise();
          // root_promise.out().emplace(this->promise().in().get_moveable());
          this->promise().context().out() = std::monostate{}; // std::move(this->promise().in());
          log::info("  returning a msg to send_msg()");
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

    task_t::promise_type& await_resume() {
      auto& txn = this->promise().context().in();
      // validate this is a start_txn message for the expected txn_id
      //this->promise().set_result(start_t::validate(txn, txn_id_));
      if (this->promise().context().result().failed()) {
        log::info("ERROR resuming txn_id:{}, ({})", (int)txn_id_,
          this->promise().context().result());
        // TODO: error handling
        return this->promise();
      }
      auto& txn_start = std::get<start_txn_t>(txn);
      // move initial state into coroutine frame
      state_ = std::move(txn_start.state);
      // move msg from incoming txn to this->promise().in()
      //this->promise().in().emplace(txn_start.msg_proxy.get_moveable());
      this->promise().context().in() = std::move(txn_start.msg);
      this->promise().set_txn_state(state::running);
      return this->promise();
    }

  private:
    id_t txn_id_;
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
  template<typename TaskT, typename MsgT, typename StateT>
  struct start_awaitable : basic_awaitable<TaskT> {
    using start_txn_t = msg::start_txn_t<MsgT, StateT>;

    // TODO: rvalue ref msg & state
    start_awaitable(TaskT::handle_t dst_handle, MsgT msg, StateT state) :
      dst_handle_(dst_handle), msg_(std::move(msg)), state_(std::move(state))
    {}

    auto await_suspend(TaskT::handle_t h) {
      basic_awaitable<TaskT>::await_suspend(h);
      log::info("start_txn_awaitable: suspending txn_id:{}...",
        (int)this->promise().txn_id());
      auto& dst_promise = dst_handle_.promise();
      /*
      auto start_txn = make_start_txn<txn_baseT, msg_proxyT, state_proxyT>(
        dst_promise.txn_id(), std::move(msg_).get_moveable(),
        std::move(state_).get_moveable());
      */
      start_txn_t txn_start{ std::move(msg_), std::move(state_) };
      dst_promise.context().in() = std::move(txn_start);
      log::info("  sending a msg to txn_id:{}",
        /*(int)dst_promise.in().get().msg_id, */(int)dst_promise.txn_id());
      //dst_promise.set_root_handle(this->promise().root_handle());
      dst_promise.set_prev_handle(this->promise().context().active_handle());
      //auto& root_promise = this->promise().root_handle().promise();
      this->promise().context().set_active_handle(dst_handle_); // dst_promise.active_handle());
      return dst_handle_; // symmetric transfer to dst
    }

    auto& await_resume() {
      log::info("resuming txn_id:{}...", (int)this->promise().txn_id());
      return this->promise();
    }

  private:
    TaskT::handle_t dst_handle_;
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