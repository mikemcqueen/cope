// txsetprice.h

#ifndef INCLUDE_TXSETPRICE_H
#define INCLUDE_TXSETPRICE_H

#include <string_view>
#include "txsellitem.h"
#include "cope.h"

namespace setprice {
  constexpr auto kTxnId{ cope::txn::make_id(10) };
  constexpr auto kMsgId{ cope::msg::make_id(200) };

  using msg_base_t = cope::msg_t;

  namespace msg {
    struct data_t : public msg_base_t {
      data_t(int price) : msg_base_t{ kMsgId }, price(price) {}

      int price;
    };

    using proxy_t = cope::msg::proxy_t<cope::msg_ptr_t>;

    inline auto validate(const msg_base_t& msg) {
      return cope::msg::validate(msg, kMsgId);
    }
  } // namespace msg

  namespace txn {
    struct state_t {
      cope::msg::id_t prev_msg_id; // i.e. "who called us"
      int price;
    };

    using state_proxy_t = cope::proxy::unique_ptr_t<state_t>;
    using start_t = cope::txn::start_t<msg_base_t, msg::proxy_t, state_proxy_t>;
    using start_proxy_t = cope::msg::proxy_t<start_t>;
    using handler_t = cope::txn::handler_t<msg::proxy_t>;
    using receive = cope::txn::receive<msg_base_t, msg::proxy_t, state_proxy_t>;
    using start_awaitable = cope::txn::start<msg_base_t, msg::proxy_t, state_proxy_t>;

    inline auto start(handler_t::handle_t handle,
      handler_t::promise_type& promise,
      const sellitem::txn::state_t& sell_state)
    {
      auto setprice_state = std::make_unique<state_t>(
        sellitem::kMsgId, sell_state.item_price);
      return start_awaitable{
        handle, std::move(promise.in().get_moveable()), std::move(setprice_state)
      };
    }

    auto handler() -> handler_t;
  } // namespace msg
} // namespace setprice

#endif // INCLUDE_TXSETPRICE_H