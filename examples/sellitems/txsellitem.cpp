#include "msvc_wall.h"
#include "app.h"
#include "ui_msg.h"
#include "sellitem_coord.h"
#include "internal/cope_log.h"

namespace sellitem::txn {
  namespace log = cope::log;
  using cope::result_code;
  using sellitem::msg::data_t;
  using sellitem::msg::row_data_t;
  using context_type = app::context_t;

  const auto& get_row(const context_type& context, const state_t& state) {
    const data_t& msg = std::get<data_t>(context.in());
    return msg.rows.at(state.row_idx.value());
  }

  auto get_row(const context_type& context, size_t row_index,
      const row_data_t** row) {
    const data_t& msg = std::get<data_t>(context.in());
    if (row_index >= msg.rows.size()) {
      return result_code::e_fail;
    }
    *row = &msg.rows.at(row_index);
    return result_code::s_ok;
  }

  auto is_candidate_row(const row_data_t& row, const state_t& state) {
    return (row.item_name == state.item_name) &&
      ((row.item_price != state.item_price) || !row.item_listed);
  }

  auto get_next_action(const row_data_t& row, const state_t& state) {
    //if (row.item_name != state.item_name) return row_state::item_name_mismatch;
    if (!row.selected) return action::select_row;
    if (row.item_price != state.item_price) return action::set_price;
    if (!row.item_listed) return action::list_item;
    throw new std::runtime_error("get_row_state()");
  }

  cope::result_t update_state(const data_t& msg, state_t& state) {
    for (size_t row_idx{}; row_idx < msg.rows.size(); ++row_idx) {
      const auto& row = msg.rows.at(row_idx);
      if (is_candidate_row(row, state)) {
        log::info(" row: {}, actual: {}:{}, expected: {}:{}", row_idx,
          row.item_name, row.item_price, state.item_name, state.item_price);
        state.row_idx = row_idx;
        // TODO: it's not clear this next_action business is necessary.
        // couldn't we just store the out_msg in the state?
        state.next_action = get_next_action(row, state);
        state.next_operation = cope::operation::yield;
        return result_code::s_ok;
      }
    }
    log::info("row: none");
    state.next_operation = cope::operation::complete;
    return result_code::s_ok;
  }

  template <>
  cope::result_t update_state(const context_type& context, state_t& state) {
    // sellitem::msg -> yield next_action_msg
    if (std::holds_alternative<sellitem::msg::data_t>(context.in())) {
      return update_state(std::get<data_t>(context.in()), state);
    }
    // setprice::msg -> await setprice::start_txn
    state.next_operation = cope::operation::await;
    return result_code::s_ok;
  }

  // maybe: namespace ui
  auto click_table_row(int row_index) {
    log::info("constructing click_table_row ({})", row_index);
    return ui::msg::click_table_row::data_t{ row_index };
  }

  auto click_setprice_button() {
    log::info("constructing click_setprice_button");
    return ui::msg::click_widget::data_t{ 1 };
  }

  auto click_listitem_button() {
    log::info("constructing click_listitem_button");
    return ui::msg::click_widget::data_t{ 2 };
  }

  template <>
  app::context_t::out_msg_type get_yield_msg(const state_t& state) {
    switch (state.next_action.value()) {
    case action::select_row: return click_table_row(state.row_idx.value());
    case action::set_price: return click_setprice_button();
    case action::list_item: return click_listitem_button();
    default: throw new std::runtime_error("invalid action");
    }
  }

  /*
  struct validate_row_options {
    bool selected{};
    bool price{};
    bool listed{};
  };

  auto validate_row(const context_type& context, size_t row_index,
      const state_t& state, const row_data_t** out_row,
      const validate_row_options& options) {
    result_code rc = msg::validate(context.in());
    if (rc != result_code::s_ok) return rc;

    rc = get_row(context, row_index, out_row);
    if (rc != result_code::s_ok) return rc;
    const row_data_t& row = **out_row;

    if ((options.selected && !row.selected)
      || (options.listed && !row.item_listed)
      || (options.price && (row.item_price != state.item_price)))
    {
      log::info("sellitem::validate_row failed, options: "
        "selected({}), listed({}), price({})",
        options.selected && !row.selected, options.listed && !row.item_listed,
        options.price && (row.item_price != state.item_price));
      rc = result_code::e_fail;
    }
    return rc;
  }
  */

  /*
  template <typename ContextT, typename CoordinatorT>
  auto handler(ContextT& context, cope::txn::id_t)
      -> task_t<ContextT> {
    using task_type = task_t<ContextT>;

    state_t state;
    CoordinatorT test(context);

    while (true) {
      auto& promise = co_await test.receive_start_txn(context, state);
      auto& context = promise.context();
      const auto error = [&context](result_code rc) {
        return context.set_result(rc).failed();
      };

      while (!context.result().unexpected()) {
        // TODO: ?
        // if (error(msg::validate(context.in()))) break;
        if (error(test.update_state(context, state))) break;
        using cope::operation;
        if (state.next_operation == operation::yield) {
          co_yield test.get_next_action_msg(state);
        } else if (state.next_operation == operation::await) {
          typename std::tuple_element<0,
              typename CoordinatorT::awaiter_types>::type awaiter;
          test.get_awaiter(context, state, awaiter);
          co_await awaiter;
        } else {
          break;  // operation::complete
        }
      }
      task_type::complete_txn(promise);
    }
  }

  template auto handler<context_type, coordinator_type>(context_type&, cope::txn::id_t)
    -> task_type;
  */
} // namespace sellitem::txn
