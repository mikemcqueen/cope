// handlers/basic.h

#pragma once

#include "common.h"
#include "../cope_txn.h"
#include "../internal/cope_log.h"

namespace cope::txn {
  template <typename StateT, Context ContextT>
  struct basic_manager_t {
    using state_type = StateT;
    using context_type = ContextT;
    using yield_msg_type = context_type::out_msg_type;
    using awaiter_types = std::tuple<std::monostate>;

    basic_manager_t() = default;
    basic_manager_t(context_type&) {};

    expected_operation update_state(const context_type&, state_type&) {
      return cope::operation::complete;
    }

    yield_msg_type get_yield_msg(const state_type&) { return std::monostate{}; }

    template <typename T>
    cope::result_t get_awaiter(context_type&, const state_type&, T&) {
      cope::log::error("get_awaiter");
      throw new std::runtime_error("get_awaiter");
      // TODO:
      // return result_code::e_fail; // e_unhandled_awaiter_type
    }
  };  // struct manager_t

  template <template <typename> typename NoContextTaskT,
      template <typename> typename ManagerT, Context ContextT>
  // TODO: BasicManager concept requires:
  //   awaiter_types, update_state, get_yield_msg, get_awaiter
  //   only 1 element in awaiter_types
  auto basic_handler(ContextT& context, id_t) -> NoContextTaskT<ContextT> {
    using task_type = NoContextTaskT<ContextT>;
    using manager_type = ManagerT<ContextT>;
    using state_type = task_type::state_type;
    using receive_txn = receive_awaitable<task_type>;
    using awaiter_types = manager_type::awaiter_types;

    state_type state;
    manager_type mgr{context};

    while (true) {
      auto& promise = co_await receive_txn{state};
      auto& context = promise.context();
      /*
      const auto error = [&context](result_code rc) {
        return context.set_result(rc).failed();
      };
      */

      while (!context.result().unexpected()) {
        // TODO: ? see examples/txsellitems.cpp
        // if (error(msg::validate(context.in()))) break;
        auto result = mgr.update_state(context, state);
        if (!result.has_value()) {
          context.set_result(result.error());
          // TODO: .failed() ? result.error() : e_asdfasdf
          break;
        }
        if (*result == operation::yield) {
          co_yield mgr.get_yield_msg(state);
        } else if (*result == operation::await) {
          typename std::tuple_element<0, awaiter_types>::type awaiter;
          if constexpr (!std::is_same_v<decltype(awaiter), std::monostate>) {
            // TODO: error handling
            if (mgr.get_awaiter(context, state, awaiter).succeeded()) {
              co_await awaiter;
            }
          }
        } else {
          break;  // operation::complete
        }
      }
      task_type::complete_txn(promise);
    }
  }
}  // namespace cope::txn
