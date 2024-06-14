// txsetprice.h

#pragma once

#ifndef INCLUDE_TXSETPRICE_H
#define INCLUDE_TXSETPRICE_H

#include "appcontext.h"
#include "setprice_types.h"
#include "setprice_ui.h"

namespace cope::txn {
  // explicit instantiation
  template class cope::txn::task_t<setprice::msg::data_t,
      setprice::txn::state_t, app::context_t>;
}  // namespace cope::txn

namespace setprice::txn {
  using task_type = task_t<app::context_t>;

  using start_awaiter =
      cope::txn::start_awaitable<task_type, msg::data_t, state_t>;

  inline auto start(task_type& task, msg::data_t&& msg, int price) {
    state_t state{price};
    return start_awaiter{task.handle(), std::move(msg), std::move(state)};
  }

  cope::result_t update_state(const msg::data_t& msg, state_t& state);

  template <cope::txn::Context ContextT>
  struct manager_t {
    using context_type = ContextT;
    using yield_msg_type = context_type::out_msg_type;
    using task_type = task_t<context_type>;
    // TODO
    using awaiter_types = std::tuple<std::monostate>;

    manager_t(context_type&) {}

    cope::result_t update_state(const context_type& context, state_t& state) {
      const auto& msg = context.in();
      cope::result_t result{};
      if (result = msg::validate(msg); result.succeeded()) {
        // setprice:msg -> yield next_action_msg
        using setprice::txn::update_state;
        return update_state(std::get<msg::data_t>(msg), state);
      }
      if (result = sellitem::msg::validate(msg); result.succeeded()) {
        // sellitem::msg -> txn::complete
        state.next_operation = cope::operation::complete;
      }
      return result;
    }

    yield_msg_type get_yield_msg(const state_t& state) {
      using namespace setprice::ui;
      switch (state.next_action.value()) {
      case action::enter_price: return enter_price_text(state.price);
      case action::click_ok: return click_ok_button();
      default: throw new std::runtime_error("invalid action");
      }
    }

    auto receive_start_txn(context_type&, state_t& state) {
      using namespace cope::txn;
      return receive_awaitable<task_type, msg::data_t, state_t>{state};
    }

    // not specialized, should never be called
    template <typename T>
    auto get_awaiter(context_type&, const state_t&, T&) {
      throw new std::runtime_error("baddd");
    }
  };  // struct manager_t
}  // namespace setprice::txn

#endif // INCLUDE_TXSETPRICE_H
