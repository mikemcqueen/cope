#pragma once

#include <optional>
#include <string_view>
#include <tuple>
#include "cope.h"
#include "sellitem_msg.h"
#include "setprice_msg.h"
#include "ui_msg.h"

namespace sellitem {
  constexpr auto kTxnId{ cope::txn::make_id(1) };

  enum class action : int {
    select_row,
    set_price,
    list_item
  };

  namespace txn {
    struct state_t {
      std::string item_name;
      int item_price;

      std::optional<int> row_idx;
      std::optional<action> next_action;
      std::optional<cope::operation> next_operation;
    }; // sellitem::txn::state_t

    inline constexpr auto make_state(
        const std::string& item_name, int item_price) {
      return state_t{
          item_name, item_price, std::nullopt, std::nullopt, std::nullopt};
    }

    template <typename ContextT>
    using task_t = cope::txn::task_t<msg::data_t, state_t, ContextT>;

    template <typename ContextT>
    auto handler(ContextT&, cope::txn::id_t) -> task_t<ContextT>;
  } // namespace txn

  namespace msg {
    /*
    using in_types = std::tuple<sellitem::msg::data_t, setprice::msg::data_t>;
    using out_types = std::tuple<ui::msg::click_widget::data_t,
        ui::msg::click_table_row::data_t>;
    */
    using start_txn_t = cope::msg::start_txn_t<data_t, txn::state_t>;

    struct types {
      using in_tuple_t = std::tuple<start_txn_t, data_t, setprice::msg::data_t>;
      using out_tuple_t = std::tuple<ui::msg::click_widget::data_t,
          ui::msg::click_table_row::data_t>;
    }; // types
  } // namespace msg
} // namespace sellitem
