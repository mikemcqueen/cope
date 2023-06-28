// simple.cpp

#include <chrono>
#include <exception>
#include <iostream>
#include <string_view>
#include "cope.h"
#include "cope_proxy.h"

namespace simple {
  constexpr auto kTxnId{ static_cast<cope::txn::id_t>(100) };

  namespace txn {
    struct in_msg_t {
      int value;
    };
    struct out_msg_t {
      int value;
    };

    using state_t = int;

    //using start_txn_t = cope::txn::start_t<in_msg_t, state_t>;
    //using start_proxy_t = cope::msg::proxy_t<start_t>;
    struct type_bundle_t {
      // todo: templated type bundle that takes startMsgT, stateT, in=startMsgT, out=monostate
      using start_txn_t = cope::msg::start_txn_t<in_msg_t, state_t>;
      using in_tuple_t = std::tuple<start_txn_t, in_msg_t>;
      using out_tuple_t = std::tuple<out_msg_t>;
    };

    /*
    //using msg_proxy_t = cope::msg::proxy_t<msg_t>;
    // TODO: or, proxy::scalar_t? or, "trivial_t", "aggregrate_t" ?
    using state_proxy_t = cope::proxy::raw_ptr_t<state_t>;
    */

    //using handler_t = cope::txn::handler_t<in_msg_t, state_t>;
    //using receive_start_txn = cope::txn::receive_awaitable<handler_t, state_t>;

    template<typename ContextT>
    auto handler(ContextT& context) -> cope::txn::task_t<ContextT> {
      using task_t = cope::txn::task_t<ContextT>;
      using receive_start_txn = cope::txn::receive_awaitable<task_t, in_msg_t, state_t>;

      state_t state;

      while (true) {
        auto& promise = co_await receive_start_txn{ kTxnId, state };
        cope::log::info("  started txn with state: {}", state);
        task_t::complete_txn(promise);
      }
    }

    template<typename ContextT>
    auto run(cope::txn::task_t<ContextT>& task, int num_iter) {
      using namespace std::chrono;
      auto start = high_resolution_clock::now();
      // in_msg_t msg{ 1 };
      in_msg_t msg; msg.value = 1;
      // TODO: once state is no longer unique_ptr
      for (int iter{}; iter < num_iter; ++iter) {
        // both msg and txn **must** be lvalues to avoid dangling pointers
        // (or have the whole nested expression in send_msg())
        using start_txn_t = simple::txn::type_bundle_t::start_txn_t;
        auto txn_start = start_txn_t{ std::move(msg), state_t{iter} };
        [[maybe_unused]] const auto& r = task.send_msg(std::move(txn_start));
      }
      auto end = high_resolution_clock::now();
      return (double)duration_cast<nanoseconds>(end - start).count();
    }
  } // namespace txn

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
} // namespace simple

int main() {
  using namespace simple;
#ifndef NDEBUG
  cope::log::enable();
  int num_iter{ 10 };
#else
  int num_iter{ 50'000'000 }; 
#endif

  try {
    double elapsed{};
    using context_t = cope::txn::context_t<txn::type_bundle_t>;
    using task_t = cope::txn::task_t<context_t>;

    context_t txn_context{};
    task_t task{ txn::handler(txn_context) };
    elapsed = txn::run(task, num_iter);
    log_result("simple", num_iter, elapsed);
  }
  catch (std::exception& e) {
    std::cout << "exception: " << e.what() << std::endl;
  }
}