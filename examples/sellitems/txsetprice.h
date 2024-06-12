// txsetprice.h

#pragma once

#ifndef INCLUDE_TXSETPRICE_H
#define INCLUDE_TXSETPRICE_H

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
    using no_context_task_t = cope::txn::task_t<msg::data_t, state_t, ContextT>;

    template <typename ContextT>
    using start_awaiter =
        cope::txn::start_awaitable<no_context_task_t<ContextT>, msg::data_t,
            state_t>;

    template <typename TaskT>
    inline auto start(
        const TaskT& task, setprice::msg::data_t&& msg, int price) {
      state_t state{price};
      return start_awaiter<typename TaskT::context_type>{
          task.handle(), std::move(msg), std::move(state)};
    }

    template <typename ContextT>
    cope::result_t update_state(const ContextT& context, state_t& state);

    template <typename T>
    T get_yield_msg(const state_t& state);
  }  // namespace txn

  namespace msg {
    using in_types = std::tuple<setprice::msg::data_t, sellitem::msg::data_t>;
    using out_types =
        std::tuple<ui::msg::click_widget::data_t, ui::msg::send_chars::data_t>;
    using start_txn_t =
        cope::msg::start_txn_t<setprice::msg::data_t, setprice::txn::state_t>;

    struct types {
      // TODO: tuple::concat_t<start, in_types>
      using in_tuple_t = std::tuple<start_txn_t, setprice::msg::data_t,
        sellitem::msg::data_t>;
      using out_tuple_t = setprice::msg::out_types;
    }; // setprice::msg::types
  }  // namespace msg
} // namespace setprice

#endif // INCLUDE_TXSETPRICE_H
