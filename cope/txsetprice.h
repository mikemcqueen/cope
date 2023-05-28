#pragma once

#include <string_view>
#include "cope.h"
#include "Price_t.h"

namespace setprice {
  constexpr auto kTxnName{ "txn::set_price" };
  constexpr std::string_view kMsgName{ "msg::set_price" };

  namespace msg {
    struct data_t : public dp::msg::data_t {
      data_t(int p) : dp::msg::data_t(kMsgName), price(p) {}

      Price_t price;
    };

    inline auto validate(const dp::msg_t& msg) {
      return dp::msg::validate<data_t>(msg, kMsgName);
    }
  }

  namespace txn {
    struct state_t {
      std::string prev_msg_name; // i.e. "who called us"
      int price;
    };

    using start_t = dp::txn::start_t<state_t>;

    auto handler() -> dp::txn::handler_t;
  }
}