#include "msvc_wall.h"
#include <optional>
#include "handlers.h"
#include "ui_msg.h"
#include "internal/cope_log.h"

namespace sellitem::txn {
  using namespace std::literals;

  using cope::result_code;
  using promise_type = app::task_t::promise_type;
  using sellitem::msg::data_t;
  using sellitem::msg::row_data_t;
  namespace log = cope::log;

  auto get_row(const promise_type& promise, size_t row_index,
    const row_data_t** row)
  {
    using start_txn_t = sellitem::txn::type_bundle_t::start_txn_t;
    const data_t* msg{};
    if (std::holds_alternative<start_txn_t>(promise.context().in())) {
      const auto& txn = std::get<start_txn_t>(promise.context().in());
      msg = &txn.msg;
    } else {
      msg = &std::get<data_t>(promise.context().in());
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
    auto& row = msg.rows.at(row_index);
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
    log::info("get_candidate_row({})", msg.rows.size());
    for (size_t row{}; row < msg.rows.size(); ++row) {
      if (is_candidate_row(msg, state, row)) {
        const auto& rd = msg.rows.at(row);
        log::info(" row: {}, actual: {}:{}, expected: {}:{}", row,
          rd.item_name, rd.item_price, state.item_name, state.item_price);
        return std::optional(row);
      }
    }
    log::info(" row: none");
    return std::nullopt;
  }

  auto get_candidate_row(const promise_type& promise, const txn::state_t& state)
    -> std::optional<size_t>
  {
    return get_candidate_row(std::get<data_t>(promise.context().in()), state);
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
    result_code rc = msg::validate(promise.context().in());
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

  template<typename Context>
  auto handler(Context& context, cope::txn::id_t /*task_id*/)
    -> cope::txn::task_t<Context>
  {
    using receive_start_txn = cope::txn::receive_awaitable<app::task_t, msg::data_t, state_t>;

    auto setprice_task{ setprice::txn::handler(context, setprice::kTxnId) };
    state_t state;

    while (true) {
      auto& promise = co_await receive_start_txn{ state };
      const auto& error = [&promise](result_code rc) {
        const auto& result = promise.context().set_result(rc);
        if (result.failed()) {
          log::info("  failed: {}", rc);
        } else {
          log::info("  success");
        }
        return result.failed();
      };

      // TODO: better:
      while (!promise.context().result().unexpected()) {
    log::info("1 sellitem::handler received {}", app::get_type_name(promise.context().in()));

        if (error(msg::validate(promise.context().in()))) break;
        auto opt_row_index = get_candidate_row(promise, state);
        if (!opt_row_index.has_value()) break;
        auto row_index = opt_row_index.value();
        const row_data_t* row;
        if (error(get_row(promise, row_index, &row))) continue;

        if (!row->selected) {
          co_yield click_table_row(row_index);
    log::info("2 sellitem::handler received {}", app::get_type_name(promise.context().in()));
          if (error(validate_row(promise, row_index, state, &row,
            { .selected{true} }))) continue;
        }

        if (row->item_price != state.item_price) {
          co_yield click_setprice_button();
    log::info("3 sellitem::handler received {}", app::get_type_name(promise.context().in()));
          if (error(setprice::msg::validate(promise.context().in()))) continue;

          auto& setprice_msg = std::get<setprice::msg::data_t>(promise.context().in());
          co_await setprice::txn::start(setprice_task, std::move(setprice_msg),
            state.item_price);
    log::info("4 sellitem::handler received {}", app::get_type_name(promise.context().in()));
          if (error(validate_row(promise, row_index, state, &row,
            { .selected{true}, .price{true} }))) continue;
        }

        if (!row->item_listed) {
          co_yield click_listitem_button();
    log::info("5 sellitem::handler received {}", app::get_type_name(promise.context().in()));
          if (error(validate_row(promise, row_index, state, &row,
            { .selected{true}, .price{true}, .listed{true} }))) continue;
        }
      }
      app::task_t::complete_txn(promise);
    }
  }

  template auto handler<app::context_t>(app::context_t&, cope::txn::id_t) -> app::task_t;
} // namespace sellitem::txn
