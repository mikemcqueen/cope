// appcontext.h

#pragma once

#include "sellitem_types.h"
#include "setprice_types.h"
#include "cope_txn.h" // TODO: really just cope_context + cope_bundle

namespace app {
  struct get_type_name_t {
    auto operator()(const auto& msg) const -> std::string {
      auto ui_type_name = ui::msg::get_type_name(msg);
      if (ui_type_name.has_value()) {
        return ui_type_name.value();
      }
      using T = std::decay_t<decltype(msg)>;
      if constexpr (std::is_same_v<T, sellitem::msg::start_txn_t>) {
        return "sellitem::start_txn";
      } else if constexpr (std::is_same_v<T, sellitem::msg::data_t>) {
        //return "sellitem::msg";
        return std::format("sellitem::msg (rows: {})",  msg.rows.size());
      } else if constexpr (std::is_same_v<T, setprice::msg::start_txn_t>) {
        return "setprice::start_txn";
      } else if constexpr (std::is_same_v<T, setprice::msg::data_t>) {
        //return "setprice::msg";
        return std::format("setprice::msg ({})", msg.price);
      } else if constexpr (std::is_same_v<T, std::monostate>) {
        return "empty_msg";
      } else {
        return "unknown_msg";
      }
    }
  };

  using type_bundle_t =
      cope::msg::type_bundle_t<sellitem::msg::types, setprice::msg::types>;
  using context_t = cope::txn::context_t<type_bundle_t, get_type_name_t>;
}  // namespace app
