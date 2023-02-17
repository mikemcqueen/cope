#pragma once

#include "dp.h"
#include <string>
#include <string_view>

namespace setprice {
  namespace msg {
    constexpr std::string_view name{ "set_price" };

    struct data_t : public DP::Message::Data_t {
      data_t(int p) :
        DP::Message::Data_t(name),
        price(p)
      {}

      int price;
    };
  }

  namespace txn {
    inline constexpr std::string_view name{ "set_price" };

    struct state_t : DP::txn::state_t {
      state_t() = default;
      state_t(int price) :
        item_price(price)
      {}
      state_t(const state_t& state) = default;

      int item_price;
    };

    using start_t = DP::txn::start_t<state_t>;
  }
}