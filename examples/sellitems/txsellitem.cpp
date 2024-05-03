#include "msvc_wall.h"
#include <optional>
#include "handlers.h"
#include "ui_msg.h"
#include "internal/cope_log.h"

namespace sellitem::txn {
  using namespace std::literals;
  namespace log = cope::log;
  using cope::result_code;
  using sellitem::msg::data_t;
  using sellitem::msg::row_data_t;

  //using type_bundle_t = cope::msg::type_bundle_t<sellitem::msg::types, setprice::msg::types>;
  //using context_type = cope::txn::context_t<type_bundle_t>;
  using context_type = app::context_t;
  //  using task_t = app::task_t; // = cope::txn::task_t<context_type>;

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

  auto get_candidate_row(const data_t& msg, const state_t& state)
    -> std::optional<size_t> {
    for (size_t row_idx{}; row_idx < msg.rows.size(); ++row_idx) {
      const auto& row = msg.rows.at(row_idx);
      if (is_candidate_row(row, state)) {
        log::info(" row: {}, actual: {}:{}, expected: {}:{}", row_idx,
          row.item_name, row.item_price, state.item_name, state.item_price);
        return std::optional(row_idx);
      }
    }
    log::info(" row: none");
    return std::nullopt;
  }

  auto get_candidate_row(const context_type& context,
      const txn::state_t& state) {
    return get_candidate_row(std::get<data_t>(context.in()), state);
  }

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
      -> cope::txn::task_t<Context> {
    using receive_start_txn =
      cope::txn::receive_awaitable<app::task_t, data_t, state_t>;

    auto setprice_task{ setprice::txn::handler(context, setprice::kTxnId) };
    state_t state;

    while (true) {
      auto& promise = co_await receive_start_txn{ state };
      auto& context = promise.context();
      const auto& error = [&context](result_code rc) {
        return context.set_result(rc).failed();
      };

      while (!context.result().unexpected()) {
        if (error(msg::validate(context.in()))) break;
        auto opt_row_index = get_candidate_row(context, state);
        if (!opt_row_index.has_value()) break;
        auto row_index = opt_row_index.value();
        const row_data_t* row;
        if (error(get_row(context, row_index, &row))) continue;

        if (!row->selected) {
          co_yield click_table_row(row_index);
          if (error(validate_row(context, row_index, state, &row,
            { .selected{true} }))) continue;
        }

        if (row->item_price != state.item_price) {
          co_yield click_setprice_button();
          if (error(setprice::msg::validate(context.in()))) continue;

          auto& setprice_msg = std::get<setprice::msg::data_t>(context.in());
          co_await setprice::txn::start(setprice_task, std::move(setprice_msg),
            state.item_price);
          if (error(validate_row(context, row_index, state, &row,
            { .selected{true}, .price{true} }))) continue;
        }

        if (!row->item_listed) {
          co_yield click_listitem_button();
          if (error(validate_row(context, row_index, state, &row,
            { .selected{true}, .price{true}, .listed{true} }))) continue;
        }
      }
      app::task_t::complete_txn(promise);
    }
  }

  template auto handler<context_type>(context_type&, cope::txn::id_t) -> app::task_t;
} // namespace sellitem::txn
