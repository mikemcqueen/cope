// cope_txn.h

#ifndef INCLUDE_COPE_TXN_H
#define INCLUDE_COPE_TXN_H

#include <coroutine>
#include <format>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <variant>
#include "cope_msg.h"
#include "cope_result.h"
#include "internal/cope_log.h"

namespace cope::txn {
  enum class id_t : int {};

  inline constexpr auto make_id(int id) noexcept { return static_cast<id_t>(id); }

  enum class status : int { ready, running, complete };

  template <typename TypeBundleT, typename MsgNameFnT>
  // TODO: requires bundle declare: in_msg_type, out_msg_type, ??
  // typename -> concept TypeBundle
  struct context_t;

  namespace detail {
    template <typename ContextT>
    struct promise;

    template <typename PromiseT>
    // requires PromiseT is instance of promise
    class basic_awaiter {
    public:
      using promise_type = PromiseT;
      using handle_type = std::coroutine_handle<promise_type>;

      bool await_ready() const { return {}; }
      void await_suspend(handle_type h) { promise_ = &h.promise(); }
      promise_type& await_resume() const { return *promise_; }

      const promise_type& promise() const { return *promise_; }
      promise_type& promise() { return *promise_; }
      // const context_type& context() const { return promise().context(); }

    private:
      promise_type* promise_{};
    };  // struct basic_awaiter

    template <typename ContextT>
    // requires ContexT is instance of context_t
    struct initial_awaiter {
      using context_type = ContextT;
      using promise_type = promise<context_type>;
      using handle_type = std::coroutine_handle<promise_type>;

      bool await_ready() { return false; }
      bool await_suspend(handle_type h) {
        // TODO: should be a better way to do this. i believe this depends
        // on initialization order of task_t's.
        if (!h.promise().context().active_handle()) {
          h.promise().context().set_active_handle(h);
        }
        return false;
      }
      void await_resume() {}
    };  // initial_awaiter

    // promise
    template <typename ContextT>
    // requires ContexT is instance of context_t
    struct promise {
    private:
      using context_type = ContextT;
      using out_msg_type = context_type::out_msg_type;
      using initial_awaiter_type = initial_awaiter<context_type>;
      using promise_type = promise<context_type>;
      using handle_type = std::coroutine_handle<promise_type>;
      using yield_awaiter_type = detail::basic_awaiter<promise_type>;

    public:
      promise() = delete;
      promise(context_type& context, id_t task_id) noexcept :
        context_(context), txn_id_(task_id) {}

      auto get_return_object() noexcept { return this; }
      initial_awaiter_type initial_suspend() noexcept { return {}; }
      std::suspend_always final_suspend() noexcept { return {}; }
      void unhandled_exception() {
        log::info("task_id:{} *** unhandled_exception() ***", txn_id());
        // TODO: store for retrieval? store what? force suspend? set error?
        // something?
        throw;
      }
      void return_void() { throw std::runtime_error("co_return not allowed"); }
      
      yield_awaiter_type yield_value([[maybe_unused]] out_msg_type&& msg) {
        log::info("task_id:{} yielding {}", txn_id(), context().msg_name(msg));
        context().out() = std::move(msg);
        return {};
      }

      id_t txn_id() const { return txn_id_; }
      void set_txn_id(id_t txn_id) { txn_id_ = txn_id; }

      auto txn_status() const { return txn_status_; }
      bool txn_ready() const { return txn_status_ == status::ready; }
      bool txn_running() const { return txn_status_ == status::running; }
      void set_txn_status(status txn_status) {
        txn_status_ = txn_status;
        log::info("task_id:{} set_txn_status({})", txn_id_, txn_status_);
      }

      const auto& context() const { return context_; }
      auto& context() { return context_; }

      auto prev_handle() const { return prev_handle_; }
      void set_prev_handle(handle_type h) { prev_handle_.emplace(h); }

    private:
      ContextT& context_;
      id_t txn_id_;
      status txn_status_ = status::ready;
      std::optional<handle_type> prev_handle_;
    }; // promise_type
  }  // namespace detail

  // txn::task_t
  template<typename ContextT>
  // requires ContexT is instance of context_t
  struct task_t {
  public:
    using context_type = ContextT;
    using out_msg_type = context_type::out_msg_type;
    using promise_type = detail::promise<context_type>;
    using task_type = task_t<context_type>;
    using handle_type = std::coroutine_handle<promise_type>;

    task_t() = delete;
    task_t(task_t&) = delete;
    task_t& operator=(const task_t&) = delete;

    task_t(promise_type* p) noexcept :
      coro_handle_(handle_type::from_promise(*p)) {}
    ~task_t() { if (coro_handle_) coro_handle_.destroy(); }

    auto handle() const { return coro_handle_; }
    const auto& promise() const { return coro_handle_.promise(); }
    auto& promise() { return coro_handle_.promise(); }

    template<typename MsgT>
    // TODO: requires in_tuple_type contains Msg
    [[nodiscard]] decltype(auto) send_msg(MsgT&& msg) {
      //validate_send_msg<(msg);
      promise().context().in() = std::move(msg);
      log::info("sending {} to task_id:{}", 
        promise().context().msg_name(promise().context().in()),
        active_handle().promise().txn_id());
      active_handle().resume();
      //
      // NB: active_handle() may have changed at this point
      //
      log::info("received {} from task_id:{}",
        promise().context().msg_name(promise().context().out()),
        active_handle().promise().txn_id());
      return promise().context().out();
    }

    static void complete_txn(promise_type& promise) {
      if (!promise.txn_running()) {
        // TODO: nasty place to throw. need to figure out error handling.
        log::info("ERROR txn::complete(): txn is not running");
        throw std::logic_error("txn::complete(): txn is not running");
      }
      if (promise.context().result().failed()) {
        log::info("ERROR task_id:{} completing with code: {}", promise.txn_id(),
          promise.context().result().code);
      }
      promise.set_txn_status(status::complete);
    }

  private:
    handle_type active_handle() const { return promise().context().active_handle(); }
    auto txn_id() const { return promise().txn_id(); }

    handle_type coro_handle_;
  }; // txn::task_t

  inline constexpr auto default_msg_name_fn = [](const auto&)
    -> std::string {
    return "msg";
  };

  // txn::context_t
  template <typename TypeBundleT,
      typename MsgNameFnT = decltype(default_msg_name_fn)>
  // TODO: requires bundle declare: in_msg_type, out_msg_type, ??
  struct context_t {
  public:
    // NB: msg_type aliases MUST be declared BEFORE task_type
    using in_msg_type = TypeBundleT::in_msg_type;
    using out_msg_type = TypeBundleT::out_msg_type;
    using context_type = context_t<TypeBundleT, MsgNameFnT>;
    using promise_type = detail::promise<context_type>;
    using task_type = task_t<context_type>;
    using handle_type = std::coroutine_handle<promise_type>;

    context_t(MsgNameFnT& msg_name_fn = default_msg_name_fn)
      : msg_name_fn_(msg_name_fn) {}

    auto active_handle() const { return active_handle_; }
    void set_active_handle(handle_type h) {
      cope::log::info("task_id:{} set_active", h.promise().txn_id());
      active_handle_ = h;
    }

    operator result_t& () { return result_; }
    auto result() const { return result_; }
    auto set_result(result_code rc) { result_.code = rc; return result_; }

    const auto& in() const { return in_; }
    auto& in() { return in_; }
    const auto& out() const { return out_; }
    auto& out() { return out_; }

    template<typename Var>
    auto msg_name(const Var& arg) {
      return std::visit(msg_name_fn_, arg);
    }

  private:
    handle_type active_handle_{};
    result_t result_{result_code::s_ok};
    in_msg_type in_;
    out_msg_type out_;
    MsgNameFnT& msg_name_fn_;
  };  // txn::context_t

  // TODO: StartTxnT type instead of msg/state types?
  // txn::receive_awaitable
  template<typename TaskT, typename MsgT, typename StateT>
  struct receive_awaitable : public detail::basic_awaiter<typename TaskT::promise_type> {
  private:
    using base_type = detail::basic_awaiter<typename TaskT::promise_type>;
    using promise_type = TaskT::promise_type;
    using handle_type = TaskT::handle_type;
    using start_txn_t = msg::start_txn_t<MsgT, StateT>;

  public:
    receive_awaitable() = delete;
    explicit receive_awaitable(StateT& state) : state_(state) {}

    std::coroutine_handle<> await_suspend(handle_type h) {
      base_type::await_suspend(h);
      log::info("task_id:{} recieve_awaitable::await_suspend()",
        this->promise().txn_id());
      if (this->promise().txn_running()) {
        throw std::runtime_error(std::format("receive_awaitable::await_suspend(),"
          "task_id:{} cannot be running", this->promise().txn_id()));
      }
      if (this->promise().txn_status() == status::complete) {
        this->promise().set_txn_status(status::ready);
        if (this->promise().prev_handle().has_value()) {
          // txn complete + prev handle exists; symmetric xfer to prev_handle
          auto prev_handle = this->promise().prev_handle().value();
          log::info("  symmetric xfer to previous task_id:{}",
            prev_handle.promise().txn_id());
          this->promise().context().set_active_handle(prev_handle);
          this->promise().prev_handle().reset();
          return prev_handle; // symmetric transfer to prev handle
        }
        else {
          // txn complete + no prev handle; resume in send_msg()
          this->promise().context().out() = std::monostate{};
        }
      }
      log::info("  returning noop_coroutine", this->promise().txn_id());
      return std::noop_coroutine();
    }

    promise_type& await_resume() {
      log::info("task_id:{} receive_awaitable::await_resume()",
        this->promise().txn_id());
      auto& txn = this->promise().context().in();
      /* TODO: start_txn validation
      // validate this is a start_txn message for the expected txn_id
      if (this->promise().context().set_result(start_t::validate(txn, txn_id_)
        .failed()) {
        log::info("task_id:{} ERROR resuming: {}", this->promise().txn_id(),
          this->promise().context().result().code);
        return this->promise(); // TODO: error handling
      }
      */
      auto& txn_start = std::get<start_txn_t>(txn);
      // move initial state into coroutine frame
      state_ = std::move(txn_start.state);
      // move msg from incoming txn to context.in
      // this is a little awkward
      auto msg{ std::move(txn_start.msg) };
      this->promise().context().in() = std::move(msg);
      this->promise().set_txn_status(status::running);
      return this->promise();
    }

  private:
    StateT& state_;
  }; // txn::receive_awaitable

  // txn::start_awaitable
  template<typename TaskT, typename MsgT, typename StateT>
  struct start_awaitable : public detail::basic_awaiter<typename TaskT::promise_type> {
  private:
    using base_type = detail::basic_awaiter<typename TaskT::promise_type>;
    using handle_type = TaskT::handle_type;
    using start_txn_t = msg::start_txn_t<MsgT, StateT>;

  public:
    start_awaitable() = delete;
    // TODO: rvalue ref msg & state
    start_awaitable(handle_type dst_handle, MsgT msg, StateT state)
        : dst_handle_(dst_handle), msg_(std::move(msg)),
          state_(std::move(state)) {}

    auto await_suspend(handle_type h) {
      base_type::await_suspend(h);
      log::info("task_id:{} suspending...", this->promise().txn_id());
      auto& dst_promise = dst_handle_.promise();
      start_txn_t txn_start{std::move(msg_), std::move(state_)};
      dst_promise.context().in() = std::move(txn_start);
      log::info("  symmetric xfer to task_id:{}", dst_promise.txn_id());
      dst_promise.set_prev_handle(this->promise().context().active_handle());
      this->promise().context().set_active_handle(dst_handle_);
      return dst_handle_;  // symmetric transfer to dst
      }

      auto& await_resume() {
        log::info("task_id:{} resuming...", this->promise().txn_id());
        return this->promise();
      }

    private:
      handle_type dst_handle_;
      MsgT msg_;
      StateT state_;
    };  // txn::start_awaitable
} // namespace cope::txn

// clang-format off
template <>
struct std::formatter<cope::txn::id_t> : std::formatter<std::string> {
  auto format(cope::txn::id_t id, std::format_context& ctx) const {
    return std::format_to(ctx.out(), "{}", (int)id);
  }
};

template <>
struct std::formatter<cope::txn::status> : std::formatter<std::string> {
  auto format(cope::txn::status st, std::format_context& ctx) const {
    using cope::txn::status;
    static std::unordered_map<status, std::string> status_name_map = {
      { status::ready, "ready" },
      { status::running, "running" },
      { status::complete, "complete" }
    };
    return std::format_to(ctx.out(), "{}", status_name_map[st]);
  }
};
// clang-format on

#endif // INCLUDE_COPE_TXN_H
