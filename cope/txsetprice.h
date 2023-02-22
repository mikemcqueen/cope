#pragma once

#include "dp.h"
#include <string>
#include <string_view>

namespace setprice {
  namespace msg {
    constexpr std::string_view name{"set_price"};

    struct data_t : public DP::Message::Data_t {
      data_t(int p) :
        DP::Message::Data_t(name),
        price(p)
      {}

      int price;
    };
  }

  namespace txn {
    constexpr std::string_view name{"set_price"};

    struct state_t {
      std::string prev_msg_name;
      int price;
    };

    using start_t = DP::txn::start_t<state_t>;
  }
}