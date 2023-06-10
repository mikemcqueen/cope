#pragma once

#include <optional>
#include <string_view>
#include <vector>
#include "cope.h"

namespace sellitem {
  constexpr auto kTxnId{ cope::txn::make_id(1) };
  constexpr auto kMsgId{ cope::msg::make_id(100) };

  using msg_base_t = cope::msg_t;

  namespace msg {
    struct row_data_t {
      std::string item_name;
      int item_price;
      bool item_listed;
      bool selected;
    };

    struct data_t : msg_base_t {
      using row_vector = std::vector<row_data_t>;

      static std::optional<int> find_selected_row(const row_vector& rows) {
        for (size_t i{}; i < rows.size(); ++i) {
          if (rows[i].selected) return std::optional((int)i);
        }
        return std::nullopt;
      }

      data_t(row_vector&& rows) :
        msg_base_t(kMsgId), rows(std::move(rows)) {}

      std::vector<row_data_t> rows;
    };

    using proxy_t = cope::msg::proxy_t<cope::msg_ptr_t>;

    inline auto validate(const msg_base_t& msg) {
      return cope::msg::validate(msg, kMsgId);
    }
  } // namespace msg

  namespace txn {
    struct state_t {
      std::string item_name;
      int item_price;
    };

    using state_proxy_t = cope::proxy::unique_ptr_t<state_t>;
    using start_t = cope::txn::start_t<msg_base_t, msg::proxy_t, state_proxy_t>;
    using start_proxy_t = cope::msg::proxy_t<start_t>;
    using handler_t = cope::txn::handler_t<msg::proxy_t>;
    using receive = cope::txn::receive<msg_base_t, msg::proxy_t, state_proxy_t>;

    inline auto make_start_txn(cope::msg_ptr_t msg_ptr,
      const std::string& item_name, int price)
    {
      cope::log::info("starting txn::sell_item");
      auto state{ std::move(item_name), price };
      return std::make_unique<state_t>(kTxnId,
        std::move(msg_ptr), std::move(state));
    }

    auto handler() -> handler_t;
  } // namespace txn
} // namespace sellitem
