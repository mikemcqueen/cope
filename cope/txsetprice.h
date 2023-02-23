#pragma once

#include "dp.h"
#include <string_view>

namespace setprice {
  namespace msg {
    constexpr std::string_view name{ "set_price" };

    struct data_t : public dp::msg::data_t {
      data_t(int p) : dp::msg::data_t(name), price(p) {}

      int price;
    };
  }

  namespace txn {
    constexpr std::string_view name{ "set_price" };

    struct state_t {
      std::string prev_msg_name; // i.e. "who called us"
      int price;
    };

    using start_t = dp::txn::start_t<state_t>;

    auto handler() -> dp::txn::handler_t;
  }
}