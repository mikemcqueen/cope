// simple.cpp

#include <chrono>
#include "cope.h"
#include "cope_handler/basic.h"
#include "log.h"

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

    template <typename ContextT>
    using manager_t = cope::txn::basic_manager_t<txn::state_t, ContextT>;
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
} // namespace simple

int main() {
#ifndef NDEBUG
  cope::log::enable();
  int num_iter{ 10 };
#else
  int num_iter{ 50'000'000 }; 
#endif

  using namespace simple;
  double elapsed{};
  app::context_type context{};
  auto task{
      cope::txn::basic_handler<txn::task_t, txn::manager_t>(context, kTxnId)};
  elapsed = run(task, num_iter);
  log("simple", num_iter, elapsed);
}
