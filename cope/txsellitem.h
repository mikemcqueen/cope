#pragma once

#include "dp.h"
#include <string_view>

namespace sellitem {
    namespace window {
      inline constexpr std::string_view name = "BrokerSellTab";
    }

    namespace msg {
      inline constexpr std::string_view name{ "sell_item" };

      struct data_t : DP::Message::Data_t
      {
        data_t() :
          DP::Message::Data_t(name)
        {}
      };
    }

  namespace txn {
    inline constexpr std::string_view name{ "sell_item" };

    struct state_t {
//      state_t(const state_t& state) = delete;
//      state_t& operator=(const state_t& state) = delete;

      std::string item_name;
      int item_price;
    };

    using start_t = DP::txn::start_t<state_t>;
  }
}