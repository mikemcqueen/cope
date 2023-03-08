#pragma once

#include <string_view>
#include "dp.h"
#include "Price_t.h"

namespace setprice {
  constexpr auto kMsgName{ "msg::set_price_popup" };

  namespace msg {
    constexpr std::string_view name{ "msg::set_price_popup" };

    struct data_t : public dp::msg::Data_t {
      data_t(int p) : dp::msg::Data_t(name), price(p) {}

      Price_t price;
    };

    inline auto validate(const dp::msg_t& msg) {
      return dp::msg::validate<data_t>(msg, name);
    }
  }

  namespace txn {
    constexpr auto kTxnName{ "txn::set_price" };
    constexpr std::string_view name{ "txn::set_price" };

    struct state_t {
      std::string prev_msg_name; // i.e. "who called us"
      int price;
    };

    using start_t = dp::txn::start_t<state_t>;

    auto handler() -> dp::txn::handler_t;

#if 0
    inline auto validate_start(const dp::msg_t& txn) {
      return dp::txn::validate_start<start_t, msg::data_t>(txn, name, msg::name);
    }
#endif
  }
}