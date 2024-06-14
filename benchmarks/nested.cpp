// nested.cpp

#include <chrono>
#include <exception>
#include <tuple>
#include "cope.h"
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
      std::optional<cope::operation> next_operation{};
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
    using task_type = nested::txn::task_t<nested::context_t>;
    using nested::msg::data_t;
    using nested::txn::state_t;

    using start_awaiter =
        cope::txn::start_awaitable<task_type, data_t, state_t>;

    inline auto start(task_type& task, data_t&& msg, int value) {
      state_t state{value};
      return start_awaiter{task.handle(), std::move(msg), std::move(state)};
    }

    template <cope::txn::Context ContextT>
    struct manager_t {
      using context_type = ContextT;
      using yield_msg_type = context_type::out_msg_type;
      using task_type = nested::txn::task_t<context_type>;
      // TODO
      using awaiter_types = std::tuple<std::monostate>;

      manager_t(context_type&) {}

      cope::result_t update_state(const context_type&, state_t& state) {
        if (!state.value++) {
          state.next_operation = cope::operation::yield;
        } else {
          state.next_operation = cope::operation::complete;
        }
        return {};
      }

      yield_msg_type get_yield_msg(const state_t&) { return out_msg_t{20}; }

      auto receive_start_txn(context_type&, state_t& state) {
        using namespace cope::txn;
        return receive_awaitable<task_type, data_t, state_t>{state};
      }

      // not specialized, should never be called
      template <typename T>
      auto get_awaiter(context_type&, const state_t&, T&) {
        cope::log::error("inner::get_awaiter");
        throw new std::runtime_error("inner::get_awaiter");
      }
    };  // struct manager_t
  }  // namespace inner::txn

  namespace outer::txn {
    using nested::msg::data_t;
    using nested::txn::state_t;

    template <cope::txn::Context ContextT>
    struct manager_t {
      using context_type = ContextT;
      using yield_msg_type = context_type::out_msg_type;
      using task_type = nested::txn::task_t<context_type>;
      // TODO
      using awaiter_types = std::tuple<std::monostate>;

      manager_t(context_type& context)
          : inner_task_(cope::txn::handler<nested::txn::task_t,
              inner::txn::manager_t, context_type>(context, kInnerTxnId)) {}

      cope::result_t update_state(const context_type&, state_t& state) {
        // state.value:
        //   0 : start_txn w/ outer_msg -> yield out_msg_t
        //   1 : inner_msg -> await inner
        //   2 : outer_msg -> txn_complete
        switch (state.value++) {
        case 0: state.next_operation = cope::operation::yield; break;
        case 1: state.next_operation = cope::operation::await; break;
        case 2: state.next_operation = cope::operation::complete; break;
        default:
          cope::log::error("outer::update_state");
          throw new std::runtime_error("outer::update_state");
        }
        return {};
      }

      yield_msg_type get_yield_msg(const state_t&) { return out_msg_t{10}; }

      auto receive_start_txn(context_type&, state_t& state) {
        using namespace cope::txn;
        return receive_awaitable<task_type, data_t, state_t>{state};
      }

      // specialized below
      template <typename T>
      auto get_awaiter(context_type&, const state_t&, T&) {
        cope::log::error("outer::get_awaiter");
        throw new std::runtime_error("outer::get_awaiter");
      }

    private:
      nested::txn::task_t<context_type> inner_task_;
    };  // struct manager_t

    template <>
    template <>
    inline auto manager_t<nested::context_t>::get_awaiter(
        nested::context_t& context, const state_t&,
        inner::txn::start_awaiter& awaiter) {
      auto& inner_msg = std::get<data_t>(context.in());
      awaiter =
          std::move(inner::txn::start(inner_task_, std::move(inner_msg), 0));
    }

    auto run(auto& task, int num_iter) {
      using namespace std::chrono;

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
  using namespace nested;
#ifndef NDEBUG
  cope::log::enable();
  int num_iter{ 10 };
#else
  int num_iter{ 50'000'000 };
#endif

  using namespace nested;
  double elapsed;
  context_t context{};
  auto task{cope::txn::handler<txn::task_t, outer::txn::manager_t,
      context_t>(context, kOuterTxnId)};
  elapsed = outer::txn::run(task, num_iter);
  log("nested", num_iter, elapsed);
}
