// simple.cpp

#include <chrono>
#include <exception>
#include <iostream>
#include <optional>
#include <string_view>
#include "cope.h"

namespace simple {
  constexpr auto kTxnId{ cope::txn::make_id(100) };

  // simple::msg
  namespace msg {
    struct data_t {
      int value;
    };
  }

  // simple::types
  namespace txn {
    struct state_t {
      int value;
      std::optional<cope::operation> next_operation{};
    };
  }

  namespace msg {
    using start_txn_t = cope::msg::start_txn_t<data_t, txn::state_t>;

    struct types {
      using in_tuple_t = std::tuple<start_txn_t, data_t>;
      using out_tuple_t = std::tuple<std::monostate>;
    };
  }

  // app context
  namespace app {
    using type_bundle_t = cope::msg::type_bundle_t<msg::types>;
    using context_type = cope::txn::context_t<type_bundle_t>;
  }

  namespace txn {
    template <typename ContextT>
    using task_t = cope::txn::task_t<msg::data_t, state_t, ContextT>;

    template <cope::txn::Context ContextT>
    struct manager_t {
      using context_type = ContextT;
      using yield_msg_type = context_type::out_msg_type;
      using task_type = task_t<context_type>;
      // TODO
      using awaiter_types = std::tuple<std::monostate>;

      manager_t(context_type&) {}

      cope::result_t update_state(const context_type&, state_t& state) {
        state.next_operation = cope::operation::complete;
        return {};
      }

      // should never be called
      yield_msg_type get_yield_msg(const state_t&) {
        throw new std::runtime_error("get_yield_msg");
      }

      auto receive_start_txn(context_type&, state_t& state) {
        using namespace cope::txn;
        return receive_awaitable<task_type, msg::data_t, state_t>{state};
      }

      // not specialized, should never be called
      template <typename T>
      auto get_awaiter(context_type&, const state_t&, T&) {
        throw new std::runtime_error("get_awaiter");
      }
    };  // struct manager_t
  }  // namespace txn

  auto run(txn::task_t<app::context_type>& task, int num_iter) {
    using namespace std::chrono;
    auto start = high_resolution_clock::now();
    msg::data_t msg{.value{1}};
    for (int iter{}; iter < num_iter; ++iter) {
      auto txn_start =
          msg::start_txn_t{std::move(msg), txn::state_t{.value{iter}}};
      [[maybe_unused]] const auto& r = task.send_msg(std::move(txn_start));
    }
    auto end = high_resolution_clock::now();
    return (double)duration_cast<nanoseconds>(end - start).count();
  }

  double iters_per_ms(int iters, double ns) {
    return (double)iters / (ns * 1e-6);
  }

  double ns_per_iter(int iters, double ns) {
    return ns / (double)iters;
  }

  void log_result(std::string_view name, int iters, double ns) {
    std::cerr << name << ", elapsed: " << std::fixed << std::setprecision(0)
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
    app::context_type context{};
    auto task{cope::txn::handler<txn::task_t, txn::manager_t>(context, kTxnId)};
    elapsed = run(task, num_iter);
    log_result("simple", num_iter, elapsed);
  }
  catch (std::exception& e) {
    std::cout << "exception: " << e.what() << std::endl;
  }
}
