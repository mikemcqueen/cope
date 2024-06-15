// txsetprice.h

#pragma once

#ifndef INCLUDE_TXSETPRICE_H
#define INCLUDE_TXSETPRICE_H

#include "appcontext.h"
#include "setprice_types.h"
#include "setprice_ui.h"
#include "cope_handler/basic.h"

namespace cope::txn {
  // explicit instantiation
  template class cope::txn::task_t<setprice::msg::data_t,
      setprice::txn::state_t, app::context_t>;
}  // namespace cope::txn

namespace setprice::txn {
  using task_type = task_t<app::context_t>;
  using start_awaiter = cope::txn::start_awaitable<task_type>;

  inline auto start(task_type& task, msg::data_t&& msg, int price) {
    state_t state{price};
    return start_awaiter{task.handle(), std::move(msg), std::move(state)};
  }

  cope::expected_operation update_state(const msg::data_t& msg, state_t& state);

  template <cope::txn::Context ContextT>
  struct manager_t : cope::txn::basic_manager_t<state_t, ContextT> {
    using context_type = ContextT;
    using base = typename manager_t::basic_manager_t;
    using state_type = base::state_type;
    using yield_msg_type = base::yield_msg_type;

    manager_t(context_type&) {}

    // TODO: can we change this return value to auto?
    cope::expected_operation update_state(
        const context_type& context, state_type& state) {
      const auto& msg = context.in();
      if (std::holds_alternative<msg::data_t>(msg)) {
        // setprice:msg -> yield next_action_msg
        return setprice::txn::update_state(std::get<msg::data_t>(msg), state);
      } else if (std::holds_alternative<sellitem::msg::data_t>(msg)) {
        // sellitem::msg -> txn::complete
        return cope::operation::complete;
      }
      return std::unexpected(cope::result_code::e_unexpected_msg_type);
    }

    yield_msg_type get_yield_msg(const state_type& state) {
      using namespace setprice::ui;
      switch (state.next_action.value()) {
      case action::enter_price: return enter_price_text(state.price);
      case action::click_ok: return click_ok_button();
      default: throw new std::runtime_error("invalid action");
      }
    }
  };  // struct manager_t
}  // namespace setprice::txn

#endif // INCLUDE_TXSETPRICE_H
