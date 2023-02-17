#pragma once

#include "dp.h"
#include <string_view>

namespace sellitem {
    namespace window {
      inline constexpr std::string_view name = "BrokerSellTab";
    }

    namespace msg {
      struct data_t : DP::Message::Data_t
      {
        data_t() : DP::Message::Data_t("TranslateData")
        {}
      };
    }

  namespace txn {
    inline constexpr std::string_view name{ "sell_item" };
    //inline constexpr DP::string_literal name{ "sell_item" };

    struct state_t : DP::txn::state_t {
      state_t() = default;
      state_t(const std::string_view name, int price) :
        item_name(name),
        item_price(price)
      {}
      state_t(const state_t& state) = default;

      std::string item_name;
      int item_price;
    };

    using start_t = DP::txn::start_t<state_t>;
    /*
    {
      start_t(const state_t& state) :
        DP::Transaction::start_t("SellItem"),
        txn_state(state) // std::move
      {}

      state_t txn_state;
    };
    */
  }
}