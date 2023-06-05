// complete_immediately.cpp

#include <chrono>
#include <iostream>
#include <string_view>
#include "cope.h"

auto new_empty_msg(std::string_view name) {
  return std::make_unique<cope::msg_t>(name);
}

namespace complete_immediately {
  constexpr std::string_view kTxnName = "txn::complete_immediately";

  namespace txn {
    using state_t = int;
    using start_t = cope::txn::start_t<state_t>;

    inline auto make_start_txn(cope::msg_ptr_t msg_ptr, int value) {
      auto state{ std::make_unique<txn::state_t>(value) };
      return cope::txn::make_start_txn<txn::state_t>(kTxnName,
        std::move(msg_ptr), std::move(state));
    }

    auto handler() -> cope::txn::handler_t {
      state_t state;

      while (true) {
        auto& promise = co_await cope::txn::receive_awaitable{ kTxnName, state };
        if (state < 0) {
          co_yield new_empty_msg("msg::out");
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
      auto in_ptr = new_empty_msg("msg::in");
      in_ptr = std::move(txn::make_start_txn(std::move(in_ptr), iter));
      cope::msg_ptr_t out_ptr = handler.send_msg(std::move(in_ptr));
    }
    auto end = high_resolution_clock::now();
    return 1e-6 * (double)duration_cast<nanoseconds>(end - start).count();
  }

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

  double iters_per_ms(int iters, double ms) {
    return (double)iters / ms;
  }

  void log_result(std::string_view name, int iters, double ms) {
    std::cerr << name << ", elapsed: " << std::fixed << std::setprecision(5)
      << ms << "ms, (" << iters << " iters"
      << ", " << iters_per_ms(iters, ms) << " iters/ms)"
      << std::endl;
  }
} // namespace complete_immediately

int main() {
  using namespace complete_immediately;
  cope::txn::handler_t handler{ txn::handler() };
  int num_iter{ 1'000'000 };

  auto elapsed = run_no_reuse(handler, num_iter);
  log_result("no_reuse", num_iter, elapsed);

/* TODO
  elapsed = run_with_reuse(handler, num_iter);
  log_result("with_reuse", num_iter, elapsed);
*/
}