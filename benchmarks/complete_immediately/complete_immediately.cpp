// complete_immediately.cpp

#include <chrono>
#include <exception>
#include <iostream>
#include <string_view>
#include "cope.h"
#include "cope_proxy.h"

namespace complete_immediately {
  constexpr auto kTxnId{ static_cast<cope::txn::id_t>(100) };

  namespace txn {
    namespace uptr { // unique_ptr<> based messages
      using msg_t = cope::msg_t;
      using msg_proxy_t = cope::msg::proxy_t<cope::msg_ptr_t>;
      using state_t = int;
      // TODO: or, proxy::scalar_t?
      using state_proxy_t = cope::proxy::raw_ptr_t<state_t>;
      using start_t = cope::txn::start_t<msg_t, msg_proxy_t, state_proxy_t>;
      using handler_t = cope::txn::handler_t<msg_proxy_t>;
      using receive = cope::txn::receive_awaitable<msg_t, msg_proxy_t, state_proxy_t>;

      inline auto make_start_txn(cope::msg_ptr_t msg_ptr, state_t value) {
        return std::make_unique<start_t>(kTxnId,
          msg_proxy_t{ std::move(msg_ptr) },
          state_proxy_t{ value });
      }

      auto handler() -> handler_t {
        state_t state;

        while (true) {
          auto& promise = co_await receive{ kTxnId, state };
          if (state < 0) {
            cope::log::info("yielding!");
            co_yield cope::msg::make_noop();
          }
          cope::txn::complete<msg_proxy_t>(promise);
        }
      }

      auto run_no_reuse(handler_t& handler, int num_iter) {
        using namespace std::chrono;
        auto start = high_resolution_clock::now();
        for (int iter{}; iter < num_iter; ++iter) {
          auto txn = make_start_txn(cope::msg::make_noop(), iter);
          auto proxy = msg_proxy_t{ std::move(txn) };
          cope::msg_ptr_t out_ptr = handler.send_msg(proxy);
        }
        auto end = high_resolution_clock::now();
        return (double)duration_cast<nanoseconds>(end - start).count();
      }
    } // namespace uptr

    namespace scalar { // int messages & state
      struct msg_t {
        template<typename T> T& as() { return static_cast<T&>(*this); }

        cope::msg::id_t msg_id;
      };
      using msg_proxy_t = cope::msg::proxy_t<msg_t>;
      using state_t = int;
      // TODO: or, proxy::scalar_t?
      using state_proxy_t = cope::proxy::raw_ptr_t<state_t>;
      using start_t = cope::txn::start_t<msg_t, msg_proxy_t, state_proxy_t>;
      using start_proxy_t = cope::msg::proxy_t<start_t>;
      using handler_t = cope::txn::handler_t<msg_proxy_t>;
      using receive = cope::txn::receive_awaitable<msg_t, msg_proxy_t, state_proxy_t>;

      auto handler() -> handler_t {
        state_t state;

        while (true) {
          auto& promise = co_await receive{ kTxnId, state };
          cope::log::info("  state: {}", state);
          if (state < 0) {
            cope::log::info("  yielding!");
            co_yield msg_t{ cope::msg::id::kNoOp };
          }
          cope::txn::complete<msg_proxy_t>(promise);
        }
      }

      auto run_no_reuse(handler_t& handler, int num_iter) {
        using namespace std::chrono;
        auto start = high_resolution_clock::now();
        msg_t msg{ cope::msg::id::kNoOp };
        // TODO: once state is no longer unique_ptr
        for (int iter{}; iter < num_iter; ++iter) {
          // both msg and txn **must** be lvalues to avoid dangling pointers
          // (or have the whole nested expression in send_msg())
          auto txn = start_t{ kTxnId, msg, state_proxy_t{iter} };
          auto proxy = start_proxy_t{ txn };
          [[maybe_unused]] const auto& r = handler.send_msg(proxy);
        }
        auto end = high_resolution_clock::now();
        return (double)duration_cast<nanoseconds>(end - start).count();
      }
    } // namespace scalar
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
} // namespace complete_immediately

int main() {
  using namespace complete_immediately;
  //cope::log::enable();
  int num_iter{ 50'000'000};

  try {
    txn::uptr::handler_t uptr_handler{ txn::uptr::handler() };
    auto elapsed = txn::uptr::run_no_reuse(uptr_handler, num_iter);
    log_result("uptr no_reuse", num_iter, elapsed);

    txn::scalar::handler_t scalar_handler{ txn::scalar::handler() };
    elapsed = txn::scalar::run_no_reuse(scalar_handler, num_iter);
    log_result("scalar no_reuse", num_iter, elapsed);
  }
  catch (std::exception& e) {
    std::cout << "exception: " << e.what() << std::endl;
  }
}