#pragma once

#include <optional>
#include <string_view>
#include <vector>
#include "dp.h"
#include "Price_t.h"

namespace sellitem {
  inline constexpr auto kMsgName{ "msg::broker_sell_tab" };

  namespace msg {
    inline constexpr std::string_view name{ "msg::broker_sell_tab" };

    struct row_data_t {
      std::string item_name;
      Price_t item_price;
      bool item_listed;
      bool selected;
    };

    struct data_t : dp::msg::Data_t {
      using row_vector = std::vector<row_data_t>;

      static std::optional<int> find_selected_row(const row_vector& rows) {
        for (auto i = 0; i < rows.size(); ++i) {
          if (rows[i].selected) return std::optional(i);
        }
        return std::nullopt;
      }

      data_t(row_vector rv) : dp::msg::Data_t(name), rows(std::move(rv)) {}

      std::vector<row_data_t> rows;
    };

    inline auto validate(const dp::msg_t& msg) {
      return dp::msg::validate<data_t>(msg, name);
    }
  } // namespace msg

  namespace txn {
    inline constexpr auto kTxnName{ "txn::sell_item" };
    inline constexpr std::string_view name{ "txn::sell_item" };

    struct state_t {
      std::string item_name;
      int item_price;
    };

    using start_t = dp::txn::start_t<state_t>;

    auto handler() -> dp::txn::handler_t;

#if 0
    inline auto validate_start(const dp::msg_t& txn) {
      return dp::txn::validate_start<start_t, msg::data_t>(txn, name, msg::name);
    }
#endif
  } // namespace txn
} // namespace sellitem
