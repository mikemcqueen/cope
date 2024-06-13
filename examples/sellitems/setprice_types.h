// setprice_types.h

#pragma once

#include <optional>
#include "cope.h"
#include "sellitem_msg.h"
#include "setprice_msg.h"
#include "ui_msg.h"

namespace setprice {
  constexpr auto kTxnId{ cope::txn::make_id(10) };

  enum class action : int {
    enter_price,
    click_ok
  };

  namespace txn {
    // state type
    struct state_t {
      //cope::msg::id_t prev_msg_id; // i.e. "who called us"
      int price;
      std::optional<action> next_action{std::nullopt};
      std::optional<cope::operation> next_operation{std::nullopt};
    };

    // task type (without context)
    template <typename ContextT>
    using task_t = cope::txn::task_t<msg::data_t, state_t, ContextT>;

    template <typename ContextT>
    using start_awaiter =
        cope::txn::start_awaitable<task_t<ContextT>, msg::data_t, state_t>;
  }  // namespace txn

  namespace msg {
    using start_txn_t = cope::msg::start_txn_t<msg::data_t, txn::state_t>;

    struct types {
      using in_tuple_t =
          std::tuple<start_txn_t, msg::data_t, sellitem::msg::data_t>;
      using out_tuple_t = std::tuple<ui::msg::click_widget::data_t,
          ui::msg::send_chars::data_t>;
    };  // types
  }  // namespace msg
} // namespace setprice

