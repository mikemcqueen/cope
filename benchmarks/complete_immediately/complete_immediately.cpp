// complete_immediately.cpp

#include <chrono>
#include <iostream>
#include <string_view>
#include "cope.h"

/*
auto new_empty_msg(std::string_view name) {
  return std::make_unique<cope::msg_t>(name);
}
*/

namespace complete_immediately {
  constexpr auto kTxnId = static_cast<cope::txn::id_t>(100);

  namespace txn {
    using state_t = int;
    using start_t = cope::txn::start_t<state_t>;

    inline auto make_start_txn(cope::msg_ptr_t msg_ptr, int value) {
      //auto state{ std::make_unique<txn::state_t>(value) };
      return cope::txn::make_start_txn<txn::state_t>(kTxnId,
        std::move(msg_ptr), std::make_unique<txn::state_t>(value));
    }

    auto handler() -> cope::txn::handler_t {
      state_t state;

      while (true) {
        auto& promise = co_await cope::txn::receive_awaitable{ kTxnId, state };
        if (state < 0) {
          cope::log::info("yielding!");
          co_yield cope::msg::make_noop();
        }
        cope::txn::complete(promise);
      }
    }
  } // namespace txn

  // this is with no re-use of msg & state data
  auto run_no_reuse(cope::txn::handler_t& handler, int num_iter) {
    using namespace std::chrono;
    auto start = high_resolution_clock::now();
    for (int iter{}; iter < num_iter; ++iter) {
      auto in_ptr = txn::make_start_txn(cope::msg::make_noop(), iter);
      cope::msg_ptr_t out_ptr = handler.send_msg(std::move(in_ptr));
    }
    auto end = high_resolution_clock::now();
    return (double)duration_cast<nanoseconds>(end - start).count();
  }

/*
  // this is with re-use of msg & state data
  auto run_with_reuse(cope::txn::handler_t& handler, int num_iter) {
    using namespace std::chrono;
    auto start = high_resolution_clock::now();
    auto start_txn = txn::make_start_txn(new_empty_msg("msg::in"), 0);
    for (int iter{}; iter < num_iter; ++iter) {
      auto out_ptr = std::move(handler.send_msg(std::move(start_txn)));
      //start_txn->msg_ptr = std::move(out_ptr);
    }
    auto end = high_resolution_clock::now();
    return 1e-6 * (double)duration_cast<nanoseconds>(end - start).count();
  }
*/  

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
} // namespace complete_immediately

int main() {
  using namespace complete_immediately;
  //cope::log::enable();
  cope::txn::handler_t handler{ txn::handler() };
  int num_iter{ 50'000'000 };

  auto elapsed = run_no_reuse(handler, num_iter);
  log_result("no_reuse", num_iter, elapsed);

/* TODO
  elapsed = run_with_reuse(handler, num_iter);
  log_result("with_reuse", num_iter, elapsed);
*/
}