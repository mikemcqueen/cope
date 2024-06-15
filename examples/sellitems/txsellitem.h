#pragma once

#include "appcontext.h"
#include "sellitem_types.h"
#include "sellitem_ui.h"
#include "txsetprice.h"
#include "cope_handler/basic.h"
#include "internal/cope_log.h"

namespace cope::txn {
  // explicit instantiation
  template class cope::txn::task_t<sellitem::msg::data_t,
      sellitem::txn::state_t, app::context_t>;
}

namespace sellitem::txn {
  using task_type = task_t<app::context_t>;

  cope::operation update_state(const msg::data_t& msg, state_t& state);

  template <cope::txn::Context ContextT>
  struct manager_t : cope::txn::basic_manager_t<state_t, ContextT> {
    using context_type = ContextT;
    using base = typename manager_t::basic_manager_t;
    using state_type = base::state_type;
    using yield_msg_type = base::yield_msg_type;
    using awaiter_types = std::tuple<setprice::txn::start_awaiter>;

    manager_t(context_type& context)
        : setprice_task_(cope::txn::basic_handler<setprice::txn::task_t,
            setprice::txn::manager_t, context_type>(
            context, setprice::kTxnId)) {}

    cope::expected_operation update_state(
        const context_type& context, state_t& state) {
      // sellitem::msg -> yield next_action_msg
      if (std::holds_alternative<msg::data_t>(context.in())) {
        using sellitem::txn::update_state;
        return update_state(std::get<msg::data_t>(context.in()), state);
      }
      // setprice::msg -> await setprice::start_txn
      return cope::operation::await;
    }

    yield_msg_type get_yield_msg(const state_t& state) {
      using namespace sellitem::ui;
      switch (state.next_action.value()) {
      case action::select_row: return click_table_row(state.row_idx.value());
      case action::set_price: return click_setprice_button();
      case action::list_item: return click_listitem_button();
      default: throw new std::runtime_error("invalid action");
      }
    }

    // specialized below
    template <typename T>
    cope::result_t get_awaiter(context_type&, const state_type&, T&) {
      return cope::result_code::e_fail;
    }

  private:
    setprice::txn::task_t<context_type> setprice_task_;
  };  // struct manager_t

  template <>
  template <>
  inline cope::result_t manager_t<app::context_t>::get_awaiter(
      app::context_t& context, const state_t& state,
      setprice::txn::start_awaiter& awaiter) {
    auto& setprice_msg = std::get<setprice::msg::data_t>(context.in());
    awaiter = std::move(setprice::txn::start(
        setprice_task_, std::move(setprice_msg), state.item_price));
    return {};
  }
}  // namespace sellitem::txn
