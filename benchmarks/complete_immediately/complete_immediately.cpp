// complete_immediately.cpp

#include <chrono>
#include <iostream>
#include <string_view>
#include "cope.h"

auto new_empty_msg(std::string_view name) {
  return std::make_unique<cope::msg_t>(name);
}

namespace complete_immediately {
  constexpr std::string_view kTxnName = "tx_complete_immediately";

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
  }
}

double iters_per_ms(int iters, double ms) {
  return (double)iters / ms;
}

int main() {
  cope::txn::handler_t handler{ complete_immediately::txn::handler() };

  using namespace std::chrono;
  auto start = high_resolution_clock::now();
  // this is with no re-use of msg & state data
  int num_iter{ 1'000'000 };
  for (int iter{}; iter < num_iter; ++iter) {
    auto in_ptr = new_empty_msg("msg::in");
//    if (!handler.promise().txn_running()) {
    in_ptr = std::move(complete_immediately::txn::make_start_txn(
      std::move(in_ptr), iter));
//    }
    cope::msg_ptr_t out_ptr = handler.send_msg(std::move(in_ptr));
  }
  auto end = high_resolution_clock::now();
  auto elapsed =
    1e-6 * (double)duration_cast<nanoseconds>(end - start).count();
  std::cerr << "Elapsed: " << std::fixed << std::setprecision(5)
    << elapsed << "ms, (" << num_iter << " iters"
    << ", " << iters_per_ms(num_iter, elapsed) << " iters/ms)"
    << std::endl;
}