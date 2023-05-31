// txsetprice.h

#ifndef INCLUDE_TXSETPRICE_H
#define INCLUDE_TXSETPRICE_H

#include <string_view>
#include "txsellitem.h"
#include "cope.h"

namespace setprice {
  constexpr std::string_view kTxnName{ "txn::set_price" };
  constexpr std::string_view kMsgName{ "msg::set_price" };

  namespace msg {
    struct data_t : public cope::msg::data_t {
      data_t(int price) : cope::msg::data_t(kMsgName), price(price) {}

      int price;
    };

    inline auto validate(const cope::msg_t& msg) {
      return cope::msg::validate<data_t>(msg, kMsgName);
    }
  } // namespace msg

  namespace txn {
    struct state_t {
      std::string_view prev_msg_name; // i.e. "who called us"
      int price;
    };

    using start_t = cope::txn::start_t<state_t>;
    using handler_t = cope::txn::handler_t;

    inline auto start(handler_t::handle_t handle, handler_t::promise_type& promise,
      const sellitem::txn::state_t& sell_state)
    {
      auto setprice_state = std::make_unique<setprice::txn::state_t>(
        kMsgName, sell_state.item_price);
      return cope::txn::start_awaitable<setprice::txn::state_t>{
        handle, std::move(promise.in_ptr()), std::move(setprice_state)
      };
    }

    auto handler() -> cope::txn::handler_t;
  } // namespace msg
} // namespace setprice

#endif // INCLUDE_TXSETPRICE_H