#pragma once

#include <optional>
#include <string_view>
#include <vector>
#include "cope.h"
#include "Price_t.h"

namespace sellitem {
  inline constexpr auto kTxnName{ "txn::sell_item" };
  inline constexpr auto kMsgName{ "msg::sell_item" };

  namespace msg {
    struct row_data_t {
      std::string item_name;
      Price_t item_price;
      bool item_listed;
      bool selected;
    };

    struct data_t : dp::msg::data_t {
      using row_vector = std::vector<row_data_t>;

      static std::optional<int> find_selected_row(const row_vector& rows) {
        for (auto i = 0u; i < rows.size(); ++i) {
          if (rows[i].selected) return std::optional((int)i);
        }
        return std::nullopt;
      }

      data_t(row_vector&& rows) :
        dp::msg::data_t(kMsgName), rows(std::move(rows)) {}

      std::vector<row_data_t> rows;
    };

    inline auto validate(const dp::msg_t& msg) {
      return dp::msg::validate<data_t>(msg, kMsgName);
    }
  } // namespace msg

  namespace txn {
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
