// txsellitem.cpp

#include "msvc_wall.h"
#include "txsellitem.h"
#include "internal/cope_log.h"

namespace log = cope::log;

namespace {
  using namespace sellitem;
  /*
  struct validate_row_options {
    bool selected{};
    bool price{};
    bool listed{};
  };

  const auto& get_row(const context_type& context, const state_t& state) {
    const data_t& msg = std::get<data_t>(context.in());
    return msg.rows.at(state.row_idx.value());
  }

  auto get_row(
      const context_type& context, size_t row_index, const row_data_t** row) {
    const data_t& msg = std::get<data_t>(context.in());
    if (row_index >= msg.rows.size()) { return result_code::e_fail; }
    *row = &msg.rows.at(row_index);
    return result_code::s_ok;
  }

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
        || (options.price && (row.item_price != state.item_price))) {
      log::info("sellitem::validate_row failed, options: "
                "selected({}), listed({}), price({})",
          options.selected && !row.selected,
          options.listed && !row.item_listed,
          options.price && (row.item_price != state.item_price));
      rc = result_code::e_fail;
    }
    return rc;
  }
  */

  auto is_candidate_row(const msg::row_data_t& row, const txn::state_t& state) {
    return (row.item_name == state.item_name)
           && ((row.item_price != state.item_price) || !row.item_listed);
  }

  auto get_next_action(const msg::row_data_t& row, const txn::state_t& state) {
    // if (row.item_name != state.item_name) return
    // row_state::item_name_mismatch;
    if (!row.selected) return action::select_row;
    if (row.item_price != state.item_price) return action::set_price;
    if (!row.item_listed) return action::list_item;
    throw new std::runtime_error("sellitem::txn::get_next_action()");
  }
}  // namespace

namespace sellitem::txn {
  cope::operation update_state(
      const msg::data_t& msg, state_t& state) {
    for (size_t row_idx{}; row_idx < msg.rows.size(); ++row_idx) {
      const auto& row = msg.rows.at(row_idx);
      if (is_candidate_row(row, state)) {
        log::info(" row: {}, actual: {}:{}, expected: {}:{}", row_idx,
            row.item_name, row.item_price, state.item_name, state.item_price);
        state.row_idx = row_idx;
        // TODO: it's not clear this next_action business is necessary.
        // couldn't we just store the out_msg in the state?
        state.next_action = get_next_action(row, state);
        return cope::operation::yield;
      }
    }
    log::info("row: none");
    return cope::operation::complete;
  }
} // namespace sellitem::txn
