#include "msvc_wall.h"
#include <optional>
#include "txsellitem.h"
#include "txsetprice.h"
#include "ui_msg.h"
#include "internal/cope_log.h"

namespace sellitem::txn {
  using namespace std::literals;

  using cope::result_code;
  using promise_type = handler_t::promise_type;
  using sellitem::msg::data_t;
  using sellitem::msg::row_data_t;
  namespace log = cope::log;

  auto get_row(const promise_type& promise, size_t row_index,
    const row_data_t** row)
  {
    const data_t* msg{ nullptr };
    if (cope::msg::is_start_txn<msg_base_t>(promise.in().get())) {
      const auto& txn = promise.in().get();
      const auto& txn_start = txn.template as<const start_t&>();
      msg = &txn_start.msg_proxy.get().as<data_t>();
      //this->promise().in().emplace(txn_start.msg_proxy.get_moveable());
      //const auto& txn = promise.in().get().as<start_t>();
      //msg = &start_t::msg_from(txn).as<data_t>();
    }
    else {
      msg = &promise.in().get().as<data_t>();
    }
    if (row_index >= msg->rows.size()) {
      return cope::result_code::e_fail;
    }
    *row = &msg->rows[row_index];
    return cope::result_code::s_ok;
  }

  auto is_candidate_row(const data_t& msg, const state_t& state,
    size_t row_index)
  {
    auto& row = msg.rows[row_index];
    if (row.item_name == state.item_name) {
      if ((row.item_price != state.item_price)
        || !row.item_listed)
      {
        return true;
      }
    }
    return false;
  }

  auto get_candidate_row(const data_t& msg, const state_t& state)
    -> std::optional<size_t>
  {
    for (size_t row{}; row < msg.rows.size(); ++row) {
      if (is_candidate_row(msg, state, row)) {
        log::info("get_candidate_row({})", row);
        return std::optional(row);
      }
    }
    log::info("get_candidate_row(none)");
    return std::nullopt;
  }

  auto get_candidate_row(const promise_type& promise, const txn::state_t& state)
    -> std::optional<size_t>
  {
    return get_candidate_row(promise.in().get().as<data_t>(), state);
  }

  struct validate_row_options {
    bool selected = false;
    bool price = false;
    bool listed = false;
  };

  auto validate_row(const promise_type& promise, size_t row_index,
    const state_t& state, const row_data_t** out_row,
    validate_row_options options)
  {
    result_code rc = msg::validate(promise.in().get());
    if (rc != result_code::s_ok) return rc;

    rc = get_row(promise, row_index, out_row);
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

  auto click_table_row(size_t /*row_index*/) {
    return std::make_unique<msg_base_t>(ui::msg::id::kClickTableRow);
  }

  auto click_setprice_button() {
    return std::make_unique<msg_base_t>(ui::msg::id::kClickWidget);
  }

  auto click_listitem_button() {
    return std::make_unique<msg_base_t>(ui::msg::id::kClickWidget);
  }

  auto handler() -> handler_t {
    /*setprice::txn::handler_t*/ auto setprice_handler{ setprice::txn::handler() };
    state_t state;

    while (true) {
      auto& promise = co_await txn::receive{ kTxnId, state };
      const auto& error = [&promise](result_code rc) { return promise.set_result(rc).failed(); };

      // TODO: better:
      while (!promise.result().unexpected()) {
        auto opt_row_index = get_candidate_row(promise, state);
        if (!opt_row_index.has_value()) break;
        auto row_index = opt_row_index.value();
        const row_data_t* row;
        if (error(get_row(promise, row_index, &row))) continue;

        if (!row->selected) {
          co_yield click_table_row(row_index);
          if (error(validate_row(promise, row_index, state, &row,
            { .selected{true} }))) continue;
        }

        if (row->item_price != state.item_price) {
          co_yield click_setprice_button();
          if (error(setprice::msg::validate(promise.in().get()))) continue;

          co_await setprice::txn::start(setprice_handler.handle(), promise, state);
          if (error(validate_row(promise, row_index, state, &row,
            { .selected{true}, .price{true} }))) continue;
        }

        if (!row->item_listed) {
          co_yield click_listitem_button();
          if (error(validate_row(promise, row_index, state, &row,
            { .selected{true}, .price{true}, .listed{true} }))) continue;
        }
      }
      cope::txn::complete<msg::proxy_t>(promise);
    }
  }
} // namespace sellitem::txn
