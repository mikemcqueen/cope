// complete_immediately.cpp

#include <chrono>
#include <iostream>
#include <string_view>
#include "cope.h"
#include "cope_proxy.h"

/*
auto new_empty_msg(std::string_view name) {
  return std::make_unique<cope::msg_t>(name);
}
*/

namespace complete_immediately {
  constexpr auto kTxnId{ static_cast<cope::txn::id_t>(100) };

  namespace txn {
    namespace uptr { // unique_ptr<> based messages
      using txn_base_t = cope::msg_t;
      using msg_data_t = cope::msg_ptr_t;
      using msg_proxy_t = cope::msg::proxy_t<msg_data_t>;
      using state_t = int;
      using start_t = cope::txn::start_t<txn_base_t, msg_proxy_t, state_t>;
      using handler_t = cope::txn::handler_t<msg_proxy_t>;
      using receive = cope::txn::receive<txn_base_t, msg_proxy_t, state_t>;

      inline auto make_start_txn(cope::msg_ptr_t msg_ptr, int value) {
        //auto state{ std::make_unique<txn::state_t>(value) };
        return std::make_unique<start_t>(kTxnId, msg_proxy_t{ std::move(msg_ptr) },
          std::make_unique<state_t>(value));
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

    namespace scalar { // int messages
      // todo: maybe this could be teh base of txn::msg::data_t; msg::base_t ?
      struct msg_t {
        template<typename T> const T& as() const {
          return static_cast<const T&>(*this);
        }
        template<typename T> T& as() { return static_cast<T&>(*this); }

        cope::msg::id_t msg_id;
      };
      //using msg_data_t = int;
      using msg_proxy_t = cope::msg::proxy_t<msg_t>;
      using state_t = int;
      using start_t = cope::txn::start_t<msg_t, msg_proxy_t, state_t>;
      using start_proxy_t = cope::msg::proxy_t<start_t>; //cope::txn::start_t<msg_t, msg_proxy_t, state_t>;
      using handler_t = cope::txn::handler_t<msg_proxy_t>;
      using receive = cope::txn::receive<msg_t, msg_proxy_t, state_t>;

      /*
      inline auto make_start_txn(cope::msg_ptr_t msg_ptr, int value) {
        //auto state{ std::make_unique<txn::state_t>(value) };
        return std::make_unique<start_t>(kTxnId, msg_proxy_t{ std::move(msg_ptr) },
          std::make_unique<state_t>(value));
      }
      */

      auto handler() -> handler_t {
        state_t state;

        while (true) {
          auto& promise = co_await receive{ kTxnId, state };
          if (state < 0) {
            cope::log::info("yielding!");
            co_yield msg_t{ cope::msg::id::kNoOp };
          }
          cope::txn::complete<msg_proxy_t>(promise);
        }
      }

      auto run_no_reuse(handler_t& handler, int num_iter) {
        using namespace std::chrono;
        auto start = high_resolution_clock::now();
        // TODO: once state is no longer unique_ptr
        //auto txn = start_t{ kTxnId, msg_proxy_t{1}, 0 };
        for (int iter{}; iter < num_iter; ++iter) {
          auto msg_id = static_cast<cope::msg::id_t>(iter);
          auto txn = start_t{ kTxnId, msg_t{ msg_id },
            std::make_unique<state_t>(iter) };
          auto proxy = start_proxy_t{ std::move(txn) };
          auto r = handler.send_msg(proxy);
          if (r.msg_id != msg_id) r.msg_id = cope::msg::id::kNoOp;
        }
        auto end = high_resolution_clock::now();
        return (double)duration_cast<nanoseconds>(end - start).count();
      }
    } // namespace scalar
  } // namespace txn

/*
  // this is with re-use of msg & state data
  auto run_with_reuse(txn::handler_t& handler, int num_iter) {
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
  int num_iter{ 10'000'000 };

  txn::uptr::handler_t uptr_handler{ txn::uptr::handler() };
  auto elapsed = txn::uptr::run_no_reuse(uptr_handler, num_iter);
  log_result("uptr no_reuse", num_iter, elapsed);

  txn::scalar::handler_t scalar_handler{ txn::scalar::handler() };
  elapsed = txn::scalar::run_no_reuse(scalar_handler, num_iter);
  log_result("scalar no_reuse", num_iter, elapsed);

/* TODO
  elapsed = run_with_reuse(handler, num_iter);
  log_result("with_reuse", num_iter, elapsed);
*/
}