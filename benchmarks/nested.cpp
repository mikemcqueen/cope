// nested.cpp

#include <chrono>
#include <exception>
#include <tuple>
#include "cope.h"
#include "cope_handler/basic.h"
#include "log.h"

namespace nested {
  constexpr auto kOuterTxnId{ cope::txn::make_id(100) };
  constexpr auto kInnerTxnId{ cope::txn::make_id(200) };

  struct out_msg_t {
    int value;
  };

  namespace msg {
    struct data_t {
      int value;
    };
  }

  namespace txn {
    struct state_t {
      int value;
    };

    template <typename ContextT>
    using task_t = cope::txn::task_t<msg::data_t, state_t, ContextT>;
  }

  namespace inner::msg {
    using start_txn_t =
        cope::msg::start_txn_t<nested::msg::data_t, txn::state_t>;

    struct types {
      using in_tuple_t = std::tuple<start_txn_t, nested::msg::data_t>;
      using out_tuple_t = std::tuple<out_msg_t>;
    };
  }

  namespace outer::msg {
    using start_txn_t =
        cope::msg::start_txn_t<nested::msg::data_t, txn::state_t>;
    struct types {
      using in_tuple_t = std::tuple<start_txn_t, nested::msg::data_t>;
      using out_tuple_t = std::tuple<nested::out_msg_t>;
    };
  }

  using type_bundle_t = cope::msg::type_bundle_t<nested::inner::msg::types,
      nested::outer::msg::types>;
  using context_t = cope::txn::context_t<type_bundle_t>;

  namespace inner::txn {
    using nested::msg::data_t;
    using nested::txn::state_t;
    using task_type = nested::txn::task_t<nested::context_t>;
    using start_awaiter = cope::txn::start_awaitable<task_type>;

    inline auto start(task_type& task, data_t&& msg, int value) {
      state_t state{value};
      return start_awaiter{task.handle(), std::move(msg), std::move(state)};
    }

    template <typename ContextT>
    struct manager_t
        : cope::txn::basic_manager_t<nested::txn::state_t, ContextT> {
      using context_type = ContextT;
      using base = typename manager_t::basic_manager_t;
      using state_type = base::state_type;
      using yield_msg_type = base::yield_msg_type;

      manager_t(context_type&) {}

      cope::expected_operation update_state(
          const context_type&, state_type& state) {
        if (!state.value++) {
          return cope::operation::yield;
        } else {
          return cope::operation::complete;
        }
      }

      yield_msg_type get_yield_msg(const state_type&) { return out_msg_t{20}; }
    };  // struct manager_t
  }  // namespace inner::txn

  namespace outer::txn {
    template <cope::txn::Context ContextT>
    struct manager_t
        : cope::txn::basic_manager_t<nested::txn::state_t, ContextT> {
      using context_type = ContextT;
      using base = typename manager_t::basic_manager_t;
      using state_type = base::state_type;
      using yield_msg_type = base::yield_msg_type;
      using awaiter_types = std::tuple<inner::txn::start_awaiter>;

      manager_t(context_type& context)
          : inner_task_(cope::txn::basic_handler<nested::txn::task_t,
              inner::txn::manager_t, context_type>(context, kInnerTxnId)) {}

      cope::expected_operation update_state(
          const context_type&, state_type& state) {
        // state.value:
        //   0 : start_txn w/outer_msg -> yield out_msg_t
        //   1 : inner_msg -> await inner
        //   2 : outer_msg -> txn_complete
        switch (state.value++) {
        case 0: return cope::operation::yield;
        case 1: return cope::operation::await;
        case 2: return cope::operation::complete;
        default:
          cope::log::error("outer::update_state");
          throw new std::runtime_error("outer::update_state");
        }
      }

      yield_msg_type get_yield_msg(const state_type&) { return out_msg_t{10}; }

      template <typename T>
      cope::result_t get_awaiter(context_type&, const state_type&, T&) {
        using cope::result_code;
        return result_code::e_fail;
      }

    private:
      nested::txn::task_t<context_type> inner_task_;
    };  // struct manager_t

    template <>
    template <>
    inline cope::result_t
    manager_t<nested::context_t>::get_awaiter<inner::txn::start_awaiter>(
        nested::context_t& context, const state_type&,
        inner::txn::start_awaiter& awaiter) {
      auto& msg = std::get<nested::msg::data_t>(context.in());
      awaiter = std::move(inner::txn::start(inner_task_, std::move(msg), 0));
      return {};
    }

    auto run(auto& task, int num_iter) {
      using namespace std::chrono;
      using nested::msg::data_t;
      using nested::txn::state_t;
      data_t outer_msg{1};
      data_t inner_msg{2};
      auto start = high_resolution_clock::now();
      for (int iter{}; iter < num_iter; ++iter) {
        if (!task.promise().txn_running()) {
          // if no txn running, wrap msg in a start_t msg
          using start_txn_t = outer::msg::start_txn_t;
          auto txn_start = start_txn_t{std::move(outer_msg), state_t{0}};
          cope::log::info("sending outer start_txn msg, iter {}", iter);
          [[maybe_unused]] auto& out = task.send_msg(std::move(txn_start));
        } else if (!((iter + 1) % 2)) {
          cope::log::info("sending inner_msg, iter {}", iter);
          [[maybe_unused]] auto& out = task.send_msg(std::move(inner_msg));
        } else {
          cope::log::info("sending outer_msg, iter {}", iter);
          [[maybe_unused]] auto& out = task.send_msg(std::move(outer_msg));
        }
      }
      auto end = high_resolution_clock::now();
      return (double)duration_cast<nanoseconds>(end - start).count();
    }
  }  // namespace outer::txn
}  // namespace nested

int main() {
#ifndef NDEBUG
  cope::log::enable();
  int num_iter{ 10 };
#else
  int num_iter{ 50'000'000 };
#endif

  using namespace nested;
  double elapsed;
  context_t context{};
  auto task{
      cope::txn::basic_handler<txn::task_t, outer::txn::manager_t, context_t>(
          context, kOuterTxnId)};
  elapsed = outer::txn::run(task, num_iter);
  log("nested", num_iter, elapsed);
}
