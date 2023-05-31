#pragma once

#include <optional>
#include <string_view>
#include <vector>
#include "cope.h"

namespace sellitem {
  inline constexpr auto kTxnName{ "txn::sell_item" };
  inline constexpr auto kMsgName{ "msg::sell_item" };

  namespace msg {
    struct row_data_t {
      std::string item_name;
      int item_price;
      bool item_listed;
      bool selected;
    };

    struct data_t : cope::msg::data_t {
      using row_vector = std::vector<row_data_t>;

      static std::optional<int> find_selected_row(const row_vector& rows) {
        for (size_t i{}; i < rows.size(); ++i) {
          if (rows[i].selected) return std::optional((int)i);
        }
        return std::nullopt;
      }

      data_t(row_vector&& rows) :
        cope::msg::data_t(kMsgName), rows(std::move(rows)) {}

      std::vector<row_data_t> rows;
    };

    inline auto validate(const cope::msg_t& msg) {
      return cope::msg::validate<data_t>(msg, kMsgName);
    }
  } // namespace msg

  namespace txn {
    struct state_t {
      std::string item_name;
      int item_price;
    };

    using start_t = cope::txn::start_t<state_t>;

    inline auto make_start_txn(cope::msg_ptr_t msg_ptr, const std::string& item_name,
      int price)
    {
      cope::log::info("starting txn::sell_item");
      auto state{ std::make_unique<txn::state_t>(item_name, price) };
      return cope::txn::make_start_txn<txn::state_t>(kTxnName,
        std::move(msg_ptr), std::move(state));
    }

    auto handler() -> cope::txn::handler_t;
  } // namespace txn

} // namespace sellitem
