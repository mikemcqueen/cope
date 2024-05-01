// txsetprice.h

#ifndef INCLUDE_TXSETPRICE_H
#define INCLUDE_TXSETPRICE_H

#include <string_view>
#include "cope.h"
#include "sellitem_msg.h"
#include "setprice_msg.h"
#include "ui_msg.h"

namespace setprice {
  constexpr auto kTxnId{ cope::txn::make_id(10) };

  namespace txn {
    using out_msg_types = std::tuple<ui::msg::click_widget::data_t, ui::msg::send_chars::data_t>;

    struct state_t {
      //cope::msg::id_t prev_msg_id; // i.e. "who called us"
      int price;
    };

    struct type_bundle_t {
      using start_txn_t = cope::msg::start_txn_t<msg::data_t, state_t>;
      using in_tuple_t = std::tuple<start_txn_t, msg::data_t, sellitem::msg::data_t>;
      using out_tuple_t = out_msg_types;
    };

    template<typename ToTaskT>
    using start_awaitable = cope::txn::start_awaitable<ToTaskT, msg::data_t, state_t>;

    template<typename ToTaskT>
    inline auto start(const ToTaskT& task, setprice::msg::data_t&& msg, int price) {
      state_t state{ price };
      return start_awaitable<ToTaskT>{ task.handle(), std::move(msg), std::move(state) };
    }

    template<typename Context>
    auto handler(Context&, cope::txn::id_t) -> cope::txn::task_t<Context>;
  } // namespace txn
} // namespace setprice

#endif // INCLUDE_TXSETPRICE_H
