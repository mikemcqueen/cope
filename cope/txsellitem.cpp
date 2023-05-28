#include "msvc_wall.h"
#include <optional>
#include "txsellitem.h"
#include "txsetprice.h"
#include "ui_msg.h"
//#include "BrokerSellTypes.h"

namespace sellitem::txn {//Broker::Sell::txn {
  using namespace std::literals;

  using dp::msg_t;
  using dp::result_code;
  using dp::txn::handler_t;
  using promise_type = handler_t::promise_type;
  using sellitem::msg::data_t;
  using Data_t = sellitem::msg::data_t;
  using RowData_t = msg::row_data_t;


  auto get_row(const promise_type& promise, size_t row_index,
    const RowData_t** row)
  {
    const Data_t* msg{ nullptr };
    if (dp::msg::is_start_txn(promise.in())) {
      const auto& txn = promise.in().as<start_t>();
      msg = &start_t::msg_from(txn).as<Data_t>();
      //msg = dynamic_cast<const msg::data_t*>(&txn::start_t::msg_from(txn));
    }
    else {
      msg = &promise.in().as<data_t>();
    }
    if (row_index >= msg->rows.size()) {
      return dp::result_code::e_fail; // TODO test this
    }
    *row = &msg->rows[row_index];
    return dp::result_code::s_ok;
  }

  auto is_candidate_row(const Data_t& msg, const state_t& state,
    size_t row_index)
  {
    auto& row = msg.rows[row_index];
    if (row.item_name == state.item_name) {
      if ((row.item_price.GetPlat() != state.item_price)
        || !row.item_listed)
      {
        return true;
      }
    }
    return false;
  }

  auto get_candidate_row(const Data_t& msg, const state_t& state)
    -> std::optional<size_t>
  {
    using result_t = std::optional<size_t>;
    for (size_t row{}; row < msg.rows.size(); ++row) {
      if (is_candidate_row(msg, state, row)) {
        LogInfo(L"get_candidate_row(%d)", row);
        return result_t(row);
      }
    }
    LogInfo(L"get_candidate_row(none)");
    return std::nullopt;
  }

  auto get_candidate_row(const promise_type& promise, const txn::state_t& state)
    -> std::optional<size_t>
  {
    return get_candidate_row(promise.in().as<Data_t>(), state);
  }

  struct validate_row_options {
    bool selected = false;
    bool price = false;
    bool listed = false;
  };

  auto validate_row(const promise_type& promise, size_t row_index,
    const state_t& state, const RowData_t** out_row,
    validate_row_options options)
  {
    result_code rc = msg::validate(promise.in());
    if (rc != result_code::s_ok) return rc;

    rc = get_row(promise, row_index, out_row);
    if (rc != result_code::s_ok) return rc;
    const RowData_t& row = **out_row;

    if ((options.selected && !row.selected)
      || (options.listed && !row.item_listed)
      || (options.price && (row.item_price.GetPlat() != state.item_price)))
    {
      LogInfo(L"sellitem::validate_row failed, options: "
        L"selected(%d), listed(%d), price(%d)",
        options.selected && !row.selected, options.listed && !row.item_listed,
        options.price && (row.item_price.GetPlat() != state.item_price));
      rc = result_code::e_fail;
    }
    return rc;
  }

  auto start_txn_setprice(handler_t::handle_t handle, promise_type& promise,
    const state_t& sell_state)
  {
    // build a SetPrice txn state that contains a Broker::Sell::Translate msg
    auto setprice_state = std::make_unique<setprice::txn::state_t>(
      kMsgName, sell_state.item_price);
    return dp::txn::start_awaitable<setprice::txn::state_t>{
      handle, std::move(promise.in_ptr()), std::move(setprice_state)
    };
  }

  auto click_table_row(size_t /*row_index*/) {
    return std::make_unique<msg_t>(ui::msg::name::click_table_row);
  }

  auto click_setprice_button() {
//    return std::make_unique<ui::msg::click::data_t>("brokersell", 1);
    return std::make_unique<dp::msg_t>(ui::msg::name::click_widget);//"brokersell", 1);
  }

  auto click_listitem_button() {
    return std::make_unique<dp::msg_t>(ui::msg::name::click_widget);//"brokersell", 1);
//    return std::make_unique<ui::msg::click::data_t>("brokersell", 2);
  }

  auto handler() -> handler_t {

    dp::txn::handler_t setprice_handler{ setprice::txn::handler() };
    state_t state;

    while (true) {
      auto& promise = co_await dp::txn::receive_awaitable{ kTxnName, state };
      const auto& error = [&promise](result_code rc) { return promise.set_result(rc).failed(); };

      // TODO: better:
      while (!promise.result().unexpected()) {
        auto opt_row_index = get_candidate_row(promise, state);
        if (!opt_row_index.has_value()) break;
        auto row_index = opt_row_index.value();
        const RowData_t* row;
        if (error(get_row(promise, row_index, &row))) continue;

        if (!row->selected) {
          co_yield click_table_row(row_index);
          if (error(validate_row(promise, row_index, state, &row,
            { .selected{true} }))) continue;
        }

        if (row->item_price.GetPlat() != state.item_price) {
          co_yield click_setprice_button();
          if (error(setprice::msg::validate(promise.in()))) continue;

          co_await start_txn_setprice(setprice_handler.handle(), promise, state);
          if (error(validate_row(promise, row_index, state, &row,
            { .selected{true}, .price{true} }))) continue;
        }

        if (!row->item_listed) {
          co_yield click_listitem_button();
          if (error(validate_row(promise, row_index, state, &row,
            { .selected{true}, .price{true}, .listed{true} }))) continue;
        }
      }
      dp::txn::complete(promise);
    }
  }
} // namespace Broker::Sell:txn
