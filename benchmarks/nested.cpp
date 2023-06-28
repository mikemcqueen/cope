// nested.cpp

#include <chrono>
#include <exception>
#include <iostream>
#include <string_view>
#include <tuple>
#include "cope.h"
#include "cope_proxy.h"

namespace nested {
  constexpr auto kOuterTxnId{ cope::txn::make_id(100) };
  constexpr auto kInnerTxnId{ cope::txn::make_id(200) };

  namespace inner {
    struct in_msg_t { int value; };
    struct out_msg_t { int value; };
  } // namespace inner

  namespace outer {
    struct in_msg_t { int value; };
    struct out_msg_t { int value; };
  } // namespace outer

  namespace inner {
    using state_t = int;
    struct type_bundle_t {
      using start_txn_t = cope::msg::start_txn_t<in_msg_t, state_t>;
      using in_tuple_t = std::tuple<start_txn_t, in_msg_t, outer::in_msg_t>;
      using out_tuple_t = std::tuple<out_msg_t>;
    };

    template<typename TaskT>
    using start_awaitable = cope::txn::start_awaitable<TaskT, in_msg_t, state_t>;

    template<typename ContextT>
    auto handler([[maybe_unused]] ContextT& context,
      [[maybe_unused]] cope::txn::id_t task_id)
      -> cope::txn::task_t<ContextT>
    {
      using task_t = cope::txn::task_t<ContextT>;
      using receive_start_txn = cope::txn::receive_awaitable<task_t, in_msg_t, state_t>;

      state_t state;

      while (true) {
        auto& promise = co_await receive_start_txn{ state };
        cope::log::info("  inner: received start_txn");
        while (!promise.context().result().unexpected()) {
          co_yield out_msg_t{ 3 };
          if (std::holds_alternative<outer::in_msg_t>(promise.context().in())) {
            break;
          }
        }
        if (!promise.context().result().succeeded()) {
          cope::log::info("  inner: completing with error {}", promise.context().result());
        }
        else {
          cope::log::info("  inner: completing");
        }
        task_t::complete_txn(promise);
      }
    }
  } // namespace inner;

  namespace outer {
    using state_t = int;
    struct type_bundle_t {
      using start_txn_t = cope::msg::start_txn_t<in_msg_t, state_t>;
      using in_tuple_t = std::tuple<start_txn_t, in_msg_t, inner::in_msg_t>;
      using out_tuple_t = std::tuple<out_msg_t>;
    };

    template<typename TaskT>
    inline auto start_inner(const TaskT& task, inner::in_msg_t&& msg) {
      auto inner_state = inner::state_t{ 1 };
      return inner::start_awaitable<TaskT>{ task.handle(), std::move(msg), inner_state };
    }

    template<typename ContextT>
    auto handler(ContextT& context, [[maybe_unused]] cope::txn::id_t task_id)
      -> cope::txn::task_t<ContextT>
    {
      using task_t = cope::txn::task_t<ContextT>;
      using receive_start_txn = cope::txn::receive_awaitable<task_t, in_msg_t, state_t>;

      auto inner_handler{ inner::handler(context, kInnerTxnId) };
      state_t state;

      while (true) {
        cope::log::info("  outer: waiting for start_txn");
        auto& promise = co_await receive_start_txn{ state };
        cope::log::info("  outer: received start_txn");
        while (!promise.context().result().unexpected()) {
          cope::log::info("  outer: yielding...");
          co_yield out_msg_t{ 2 };
          cope::log::info("  outer: resumed");
          if (std::holds_alternative<inner::in_msg_t>(promise.context().in())) {
            cope::log::info("  outer: inner txn starting...");
            co_await start_inner(inner_handler, std::move(
              std::get<inner::in_msg_t>(promise.context().in())));
            cope::log::info("  outer: inner txn complete");
            break;
          }
        }
        if (!promise.context().result().succeeded()) {
          cope::log::info("  outer: completing with error {}", promise.context().result());
        } else {
          cope::log::info("  outer: completing");
        }
        task_t::complete_txn(promise);
      }
    }

    // template<typename ContextT>
    auto run(/*cope::txn::task_t<ContextT>*/ auto& task, int num_iter) {
      using namespace std::chrono;

      auto start = high_resolution_clock::now();
      outer::in_msg_t outer_msg{ 1 };
      inner::in_msg_t inner_msg{ 2 };
      for (int iter{}; iter < num_iter; ++iter) {
        // if no txn running, wrap msg in a start_t msg
        if (!task.promise().txn_running()) {
          using start_txn_t = outer::type_bundle_t::start_txn_t;
          auto txn_start = start_txn_t{ std::move(outer_msg), state_t{iter} };
          cope::log::info("sending outer start_txn msg, iter {}", iter);
          [[maybe_unused]] auto& out = task.send_msg(std::move(txn_start));
        }
        else if (!((iter + 1) % 2)) {
          cope::log::info("sending inner_msg, iter {}", iter);
          [[maybe_unused]] auto& out = task.send_msg(std::move(inner_msg));
        }
        else {
          cope::log::info("sending outer_msg, iter {}", iter);
          [[maybe_unused]] auto& out = task.send_msg(std::move(outer_msg));
        }
      }
      auto end = high_resolution_clock::now();
      return (double)duration_cast<nanoseconds>(end - start).count();
    }
  } // namespace outer
} // namespace nested

double iters_per_ms(int iters, double ns) {
  return (double)iters / (ns * 1e-6);
}

double ns_per_iter(int iters, double ns) {
  return ns / (double)iters;
}

void log_result(std::string_view name, int iters, double ns) {
  std::cerr << name << ", elapsed: " << std::fixed << std::setprecision(5)
    << ns * 1e-6 << "ms, (" << iters << " iters"
    << ", " << iters_per_ms(iters, ns) << " iters/ms"
    << ", " << ns_per_iter(iters, ns) << " ns/iter)"
    << std::endl;
}

int main() {
  using namespace nested;
#ifndef NDEBUG
  cope::log::enable();
  int num_iter{ 10 };
#else
  int num_iter{ 50'000'000 };
#endif

  try {
    double elapsed;
    using context_t = cope::txn::context_t<inner::type_bundle_t, outer::type_bundle_t>;
    using task_t = cope::txn::task_t<context_t>;

    context_t txn_context{};
    task_t task{ outer::handler(txn_context, kOuterTxnId) };
    elapsed = outer::run(task, num_iter);
    log_result("variant", num_iter, elapsed);
  }
  catch (std::exception& e) {
    std::cout << "exception: " << e.what() << std::endl;
  }
}
