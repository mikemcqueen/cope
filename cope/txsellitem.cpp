#include <optional>
#include "txsellitem.h"
#include "txsetprice.h"
#include "ui_msg.h"
#include "Eq2UiIds.h"

namespace sellitem
{
  using namespace std::literals;

  using dp::msg_t;
  using dp::result_code;
  using dp::txn::handler_t;
  using promise_type = handler_t::promise_type;

  auto get_row(const promise_type& promise, int row_index, const msg::row_data_t** row) {
    const msg::data_t* msg{ nullptr };
    if (dp::is_start_txn(promise.in())) {
      const auto& txn = promise.in().as<txn::start_t>();
      msg = &txn::start_t::msg_from(txn).as<msg::data_t>();
      //msg = dynamic_cast<const msg::data_t*>(&txn::start_t::msg_from(txn));
    } else {
      msg = &promise.in().as<msg::data_t>();
    }
    if (row_index < 0 || row_index >= msg->rows.size()) {
      return dp::result_code::expected_error; // TODO test this
    }
    *row = &msg->rows[row_index];
    return dp::result_code::success;
  }

  auto is_candidate_row(const msg::data_t& msg, const txn::state_t& state,
    int row_index)
  {
    auto& row = msg.rows[row_index];
    if (row.item_name == state.item_name) {
      if ((row.price != state.item_price) || !row.listed) return true;
    }
    return false;
  }

  auto get_candidate_row(const msg::data_t& msg, const txn::state_t& state)
    -> std::optional<int>
  {
    using result_t = std::optional<int>;
    for (auto row = 0; row < msg.rows.size(); ++row) {
      if (is_candidate_row(msg, state, row)) {
        log(std::format("get_candidate_row({})", row));
        return result_t(row);
      }
    }
    log("get_candidate_row(none)");
    return std::nullopt;
  }

  auto get_candidate_row(const promise_type& promise, const txn::state_t& state)
    -> std::optional<int>
  {
    return get_candidate_row(promise.in().as<msg::data_t>(), state);
  }

  struct validate_row_options {
    bool selected = false;
    bool price = false;
    bool listed = false;
  };

  auto validate_row(const promise_type& promise, int row_index,
    const txn::state_t& state, const msg::row_data_t** out_row,
    validate_row_options options)
  {
    result_code rc = msg::validate(promise.in());
    if (rc != result_code::success) return rc;

    rc = get_row(promise, row_index, out_row);
    if (rc != result_code::success) return rc;
    const msg::row_data_t& row = **out_row;

    if ((options.selected && !row.selected)
      || (options.listed && !row.listed)
      || (options.price && (row.price != state.item_price)))
    {
      log(std::format("sellitem::validate_row failed, options: "
        "selected({}), listed({}), price({})",
        options.selected && !row.selected, options.listed && !row.listed,
        options.price && (row.price != state.item_price)));
      rc = result_code::expected_error;
    }
    return rc;
  }

  auto start_txn_setprice(handler_t::handle_t handle, promise_type& promise,
    const txn::state_t& sellitem_state)
  {
    auto setprice_state = std::make_unique<setprice::txn::state_t>(
      std::string(sellitem::msg::name), sellitem_state.item_price);
    return dp::txn::start_txn_awaitable<setprice::txn::state_t>{
      handle, std::move(promise.in_ptr()), std::move(setprice_state)
    };
  }

  auto click_table_row(int row_index) {
    // maybe this should be a separate ui::msg, so we don't need to muck
    // with window stuff here. we could dynamic_cast current window to 
    // TableWindow, and call GetRowRect (or ClickRow directly). click_table_row maybe
    row_index;
    return std::make_unique<msg_t>(ui::msg::name::click_table_row);
  }

  auto click_setprice_button() {
    using namespace eq2::broker::sell_tab;
    return std::make_unique<ui::msg::click::data_t>(window::id,
      widget::id::SetPriceButton);
  }

  auto click_listitem_button() {
    using namespace eq2::broker::sell_tab;
    return std::make_unique<ui::msg::click::data_t>(window::id,
      widget::id::ListItemButton);
  }

  namespace txn {
    auto handler() -> handler_t {

      using dp::result_code;

      result_code rc{ result_code::success };
      const auto& error = [&rc](result_code new_rc) noexcept {
        rc = new_rc;
        const auto error = (rc != result_code::success);
        if (error) log(std::format("  txn::sellitem::onepage_handler error({})", (int)rc));
        return error;
      };
      dp::txn::handler_t setprice_handler{ setprice::txn::handler() };
      state_t state;

      for (auto& promise = co_await handler_t::awaitable{ txn::name }; true;
        co_await dp::txn::complete(promise, rc))
      {
        dp::msg_t& txn = promise.in();
        if (error(txn::validate_start(txn))) continue;
        state = start_t::state_from(txn);
        const auto& msg = start_t::msg_from(txn).as<msg::data_t>();

        for(auto opt_row = get_candidate_row(msg, state);
          (rc != result_code::unexpected_error) && opt_row.has_value();
          opt_row = get_candidate_row(promise, state))
        {
          auto row_index = opt_row.value();
          const msg::row_data_t* row;
          if (error(get_row(promise, row_index, &row))) continue;

          if (!row->selected) {
            co_yield click_table_row(row_index);
            if (error(validate_row(promise, row_index, state, &row,
              { .selected{true} }))) continue;
          }

          if (row->price != state.item_price) {
            co_yield click_setprice_button();
            if (error(setprice::msg::validate(promise.in()))) continue;

            co_await start_txn_setprice(setprice_handler.handle(), promise, state);
            if (error(validate_row(promise, row_index, state, &row,
              { .selected{true}, .price{true} }))) continue;
          }

          if (!row->listed) {
            co_yield click_listitem_button();
            if (error(validate_row(promise, row_index, state, &row,
              { .selected{true}, .price{true}, .listed{true} }))) continue;
          }
        }
      }
    }
  } // namespace txn
} // namespace sellitem

