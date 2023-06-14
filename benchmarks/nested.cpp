// nested.cpp

#include <chrono>
#include <exception>
#include <iostream>
#include <string_view>
#include <tuple>
#include "cope.h"
#include "cope_proxy.h"

namespace nested {
  constexpr auto kOuterMsgId{ cope::msg::make_id(100) };
  constexpr auto kInnerMsgId{ cope::msg::make_id(101) };

  constexpr auto kOuterTxnId{ cope::txn::make_id(200) };
  constexpr auto kInnerTxnId{ cope::txn::make_id(201) };

#if 0
  // unique_ptr based messages, scalar state
  namespace uptr {
    using msg_base_t = cope::msg_t;
    using msg_proxy_t = cope::msg::proxy_t<cope::msg_ptr_t>;
    using handler_t = cope::txn::handler_t<msg_proxy_t>;

    namespace inner {
      namespace msg {
        struct data_t : msg_base_t {
          data_t(int value) : msg_base_t(kInnerMsgId), value(value) {}
          int value;
        };
      } // namespace msg

      namespace txn {
        using state_t = int;
        // TODO: or, proxy::scalar_t?
        using state_proxy_t = cope::proxy::raw_ptr_t<state_t>;
        using start_t = cope::txn::start_t<msg_base_t, msg_proxy_t, state_proxy_t>;
        using receive = cope::txn::receive_awaitable<msg_base_t, msg_proxy_t, state_proxy_t>;
        using start_awaitable = cope::txn::start_awaitable<msg_base_t, msg_proxy_t, state_proxy_t>;

        inline auto start(handler_t::handle_t handle,
          handler_t::promise_type& promise)
        {
          auto inner_state = state_t{ 1 };
          return start_awaitable{
            handle, msg_proxy_t{ std::move(promise.in().get_moveable()) },
              state_proxy_t{ inner_state }
          };
        }

        auto handler() -> handler_t {
          state_t state;

          while (true) {
            auto& promise = co_await receive{ kInnerTxnId, state };
            while (!promise.result().unexpected()) {
              co_yield cope::msg::make_noop();
              if (promise.in().get().msg_id == kOuterMsgId) {
                break;
              }
            }
            cope::txn::complete<msg_proxy_t>(promise);
          }
        }
      } // namespace txn

      inline auto make_msg(int value) {
        return std::make_unique<msg::data_t>(value);
      }
    } // namespace inner

    namespace outer {
      namespace msg {
        struct data_t : msg_base_t {
          data_t(int value) : msg_base_t(kOuterMsgId), value(value) {}
          int value;
        };
      } // namespace msg

      namespace txn {
        constexpr auto kStateValue = 123;

        using state_t = int;
        // TODO: or, proxy::scalar_t?
        using state_proxy_t = cope::proxy::raw_ptr_t<state_t>;
        using start_t = cope::txn::start_t<msg_base_t, msg_proxy_t, state_proxy_t>;
        using receive = cope::txn::receive_awaitable<msg_base_t, msg_proxy_t, state_proxy_t>;

        auto handler() -> handler_t {
          auto inner_handler{ inner::txn::handler() };
          state_t state;

          while (true) {
            auto& promise = co_await receive{ kOuterTxnId, state };
            while (!promise.result().unexpected()) {
              co_yield cope::msg::make_noop();
              if (promise.in().get().msg_id == kInnerMsgId) {
                co_await inner::txn::start(inner_handler.handle(), promise);
                break;
              }
            }
            cope::txn::complete<msg_proxy_t>(promise);
          }
        }
      } // namespace txn

      inline auto make_start_txn(cope::msg_ptr_t msg_ptr, txn::state_t value) {
        return std::make_unique<txn::start_t>(kOuterTxnId,
          msg_proxy_t{ std::move(msg_ptr) }, txn::state_proxy_t{ value });
      }

      inline auto make_msg(int value) {
        return std::make_unique<msg::data_t>(value);
      }
    } // namespace outer

    auto make_msg(handler_t& handler, int iter) {
      cope::msg_ptr_t msg;
      if (!handler.promise().txn_running() || !(iter % 2)) {
        // no txn running, or even iter: make outer
        msg = outer::make_msg(iter);
      }
      else {
        // else: txn running, and odd iter: make inner
        msg = inner::make_msg(iter);
      }
      // if no txn running, wrap msg in a start_t msg
      if (!handler.promise().txn_running()) {
        msg = outer::make_start_txn(std::move(msg), iter);
      }
      return msg_proxy_t{ std::move(msg) };
    }

    auto run(handler_t& handler, int num_iter) {
      using namespace std::chrono;
      auto start = high_resolution_clock::now();
      for (int iter{}; iter < num_iter; ++iter) {
        [[maybe_unused]] cope::msg_ptr_t out_ptr = handler.send_msg(make_msg(handler, iter));
      }
      auto end = high_resolution_clock::now();
      return (double)duration_cast<nanoseconds>(end - start).count();
    }
  } // namespace uptr

  namespace basic {
    using msg_base_t = cope::basic_msg_t;
    using msg_proxy_t = cope::msg::proxy_t<msg_base_t>;

    namespace outer::msg { struct data_t };

    namespace inner {
      namespace msg {
        struct data_t : msg_base_t {
          data_t(int value) : msg_base_t(kInnerMsgId), value(value) {}
          int value;
        };
      } // namespace msg

      namespace txn {
        using inbound_msg_types = std::tuple<inner::msg::data_t, outer::msg::data_t>;
        using outbound_msg_types = std::tuple<inner::msg::data_t, outer::msg::data_t>;

        using handler_t = cope::txn::handler_t<msg_proxy_t>;
        using state_t = int;
        using state_proxy_t = cope::proxy::raw_ptr_t<state_t>;
        using start_t = cope::txn::start_t<msg_base_t, msg_proxy_t, state_proxy_t>;
        using start_proxy_t = cope::msg::proxy_t<start_t>;
        using receive = cope::txn::receive_awaitable<msg_base_t, msg_proxy_t, state_proxy_t>;
        using start_awaitable = cope::txn::start_awaitable<msg_base_t, msg_proxy_t, state_proxy_t>;

        auto handler() -> handler_t {
          state_t state;

          while (true) {
            auto& promise = co_await receive{ kTxnId, state };
            cope::txn::complete<msg_proxy_t>(promise);
          }
        }
      } // namespace txn
    } // namespace inner

    namespace outer {
      namespace msg {
        struct data_t : msg_base_t {
          data_t(int value) : msg_base_t(kOuterMsgId), value(value) {}
          int value;
        };
      } // namespace msg

      namespace txn {
        constexpr auto kStateValue = 123;

        using state_t = int;
        // TODO: or, proxy::scalar_t?
        using state_proxy_t = cope::proxy::raw_ptr_t<state_t>;
        using start_t = cope::txn::start_t<msg_base_t, msg_proxy_t, state_proxy_t>;
        using receive = cope::txn::receive_awaitable<msg_base_t, msg_proxy_t, state_proxy_t>;

        auto handler() -> handler_t {
          auto inner_handler{ inner::txn::handler() };
          state_t state;

          while (true) {
            auto& promise = co_await receive{ kOuterTxnId, state };
            while (!promise.result().unexpected()) {
              co_yield cope::msg::make_noop();
              if (promise.in().get().msg_id == kInnerMsgId) {
                co_await inner::txn::start(inner_handler.handle(), promise);
                break;
              }
            }
            cope::txn::complete<msg_proxy_t>(promise);
          }
        }
      } // namespace txn

      inline auto make_start_txn(cope::msg_ptr_t msg_ptr, txn::state_t value) {
        return std::make_unique<txn::start_t>(kOuterTxnId,
          msg_proxy_t{ std::move(msg_ptr) }, txn::state_proxy_t{ value });
      }

      inline auto make_msg(int value) {
        return std::make_unique<msg::data_t>(value);
      }
    } // namespace outer

    auto make_msg(handler_t& handler, int iter) {
      cope::msg_ptr_t msg;
      if (!handler.promise().txn_running() || !(iter % 2)) {
        // no txn running, or even iter: make outer
        msg = outer::make_msg(iter);
      }
      else {
        // else: txn running, and odd iter: make inner
        msg = inner::make_msg(iter);
      }
      // if no txn running, wrap msg in a start_t msg
      if (!handler.promise().txn_running()) {
        msg = outer::make_start_txn(std::move(msg), iter);
      }
      return msg_proxy_t{ std::move(msg) };
    }

    auto run(handler_t& handler, int num_iter) {
      using namespace std::chrono;
      auto start = high_resolution_clock::now();
      for (int iter{}; iter < num_iter; ++iter) {
        [[maybe_unused]] cope::msg_ptr_t out_ptr = handler.send_msg(make_msg(handler, iter));
      }
      auto end = high_resolution_clock::now();
      return (double)duration_cast<nanoseconds>(end - start).count();
    }
  } // namespace basic
#endif

  // std::variant<> messages (non-virtual) & scalar state
  namespace variant {
    namespace inner {
      struct in_msg_t {
        int value;
      };
      struct out_msg_t {
        int value;
      };
    } // namespace inner

    namespace outer {
      struct in_msg_t {
        int value;
      };
      struct out_msg_t {
        int value;
      };
    } // namespace outer

    namespace inner {
      using state_t = int;
      //using start_txn_t = cope::txn::start_t<in_msg_t, state_t>;
      //using start_proxy_t = cope::msg::proxy_t<start_t>;
      using in_tuple_t = std::tuple<in_msg_t, outer::in_msg_t>;
      using out_tuple_t = std::tuple<out_msg_t>;

      /*
      //using msg_proxy_t = cope::msg::proxy_t<msg_t>;
      // TODO: or, proxy::scalar_t? or, "trivial_t", "aggregrate_t" ?
      using state_proxy_t = cope::proxy::raw_ptr_t<state_t>;
      */

      using start_msg_t = in_msg_t;
      using handler_t = cope::txn::handler_t<start_msg_t, state_t, in_tuple_t, out_tuple_t>;
      using receive_start_txn = cope::txn::receive_awaitable<handler_t, state_t>;

      auto handler() -> handler_t {
        state_t state;

        while (true) {
          auto& promise = co_await receive_start_txn{ kInnerTxnId, state };
          while (!promise.result().unexpected()) {
            co_yield out_msg_t{ 3 };
            if (std::holds_alternative<outer::in_msg_t>(promise.in())) {
              break;
            }
          }
          handler_t::complete_txn(promise);
        }
      }
    } // namespace inner;

    namespace outer {
      /*
      struct in_msg_t {
        int value;
      };
      struct out_msg_t {
        int value;
      };
      */

      using state_t = int;
      //using start_txn_t = cope::txn::start_t<in_msg_t, state_t>;
      //using start_proxy_t = cope::msg::proxy_t<start_t>;
      using in_tuple_t = std::tuple<in_msg_t, inner::in_msg_t>;
      using out_tuple_t = std::tuple<out_msg_t, inner::out_msg_t>;

      /*
      //using msg_proxy_t = cope::msg::proxy_t<msg_t>;
      // TODO: or, proxy::scalar_t? or, "trivial_t", "aggregrate_t" ?
      using state_proxy_t = cope::proxy::raw_ptr_t<state_t>;
      */

      using handler_t = cope::txn::handler_t<in_msg_t, state_t, in_tuple_t, out_tuple_t>;
      using receive_start_txn = cope::txn::receive_awaitable<handler_t, state_t>;
      using start_inner_awaitable = cope::txn::start_awaitable<handler_t, inner::handler_t, state_t>;

      inline auto start_inner(const inner::handler_t& handler, inner::start_msg_t&& msg) {
        auto inner_state = inner::state_t{ 1 };
        return start_inner_awaitable{ handler.handle(), std::move(msg), inner_state };
      }

      auto handler() -> handler_t {
        auto inner_handler{ inner::handler() };
        state_t state;

        while (true) {
          auto& promise = co_await receive_start_txn{ kOuterTxnId, state };
          while (!promise.result().unexpected()) {
            co_yield out_msg_t{ 2 };
            if (std::holds_alternative<inner::start_msg_t>(promise.in())) {
              co_await start_inner(inner_handler, std::move(
                std::get<inner::start_msg_t>(promise.in())));
              break;
            }
          }
          handler_t::complete_txn(promise);
        }
      }

      auto run(outer::handler_t& handler, int num_iter) {
        using namespace std::chrono;
        auto start = high_resolution_clock::now();
        in_msg_t msg{ 1 };
        for (int iter{}; iter < num_iter; ++iter) {
          // both msg and txn **must** be lvalues to avoid dangling pointers
          // (or have the whole nested expression in send_msg())
          auto txn_start = handler_t::start_txn_t{ std::move(msg), state_t{iter} };
          //auto proxy = start_proxy_t{ txn };
          [[maybe_unused]] const auto& r = handler.send_msg(std::move(txn_start));
        }
        auto end = high_resolution_clock::now();
        return (double)duration_cast<nanoseconds>(end - start).count();
      }
    } // namespace outer
  } // namespace variant
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
  //cope::log::enable();
  int num_iter{ 10'000'000 };

  try {
    double elapsed;
#if 0
    uptr::handler_t uptr_outer_handler{ uptr::outer::txn::handler() };
    elapsed = uptr::run(uptr_outer_handler, num_iter);
    log_result("uptr", num_iter, elapsed);

    basic::handler_t basic_outer_handler{ basic::outer::txn::handler() };
    elapsed = basic::run(basic_outer_handler, num_iter);
    log_result("basic", num_iter, elapsed);
#endif
    variant::outer::handler_t variant_handler{ variant::outer::handler() };
    elapsed = variant::outer::run(variant_handler, num_iter);
    log_result("variant", num_iter, elapsed);
  }
  catch (std::exception& e) {
    std::cout << "exception: " << e.what() << std::endl;
  }
}
