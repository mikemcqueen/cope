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
    struct state_t {
      //cope::msg::id_t prev_msg_id; // i.e. "who called us"
      int price;
    };

    template<typename TaskT>
    using start_awaitable = cope::txn::start_awaitable<TaskT, msg::data_t, state_t>;

    template<typename TaskT>
    inline auto start(const TaskT& task, setprice::msg::data_t&& msg, int price) {
      state_t state{ price };
      return start_awaitable<TaskT>{ task.handle(), std::move(msg), std::move(state) };
    }

    template<typename ContextT>
    auto handler(ContextT&, cope::txn::id_t) -> cope::txn::task_t<ContextT>;
  } // namespace txn

  namespace msg {
    using in_types = std::tuple<setprice::msg::data_t, sellitem::msg::data_t>;
    using out_types = std::tuple<ui::msg::click_widget::data_t, ui::msg::send_chars::data_t>;

    struct types {
      using start_txn_t = cope::msg::start_txn_t<setprice::msg::data_t, setprice::txn::state_t>;
      // TODO: tuple::concat_t<start, in_types>
      using in_tuple_t = std::tuple<start_txn_t, setprice::msg::data_t, sellitem::msg::data_t>;
      using out_tuple_t = setprice::msg::out_types;
    }; // setprice::msg::types
  } // namespace msg
} // namespace setprice

#endif // INCLUDE_TXSETPRICE_H
