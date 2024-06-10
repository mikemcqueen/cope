// sellitem_coord.h: sellitem transaction coordinator

#pragma once

#include "handlers.h"
#include "txsellitem.h"
#include "ui_msg.h"

namespace sellitem {
  namespace txn {

    // would like to template this (entire struct) on ContextT and move to
    // txsetprice.h start awaiter
    template <typename ContextT>
    using setprice_start_awaiter =
        cope::txn::start_awaitable<setprice::txn::task_t<ContextT>,
            setprice::msg::data_t, setprice::txn::state_t>;

    template <typename ContextT>
    struct coordinator_t {
      using context_type = ContextT;
      using yield_msg_type = ContextT::out_msg_type;
      using task_type = task_t<context_type>;
      using receive_start_txn_awaiter =
          cope::txn::receive_awaitable<task_type, msg::data_t, state_t>;

      using awaiter_types = std::tuple<setprice_start_awaiter<context_type>>;

      coordinator_t(context_type& context)
          : setprice_task(setprice::txn::handler(context, setprice::kTxnId)) {}

      cope::result_t update_state(const msg::data_t& msg, state_t& state) {
        return sellitem::txn::update_state(msg, state);
      }

      yield_msg_type get_next_action_msg(const state_t& state) {
        return sellitem::txn::get_next_action_msg<yield_msg_type>(
            state);
      }

      auto receive_start_txn(context_type&, sellitem::txn::state_t& state) {
        return receive_start_txn_awaiter{state};
      }

      // setprice::txn start awaiter
      template <typename T>
      auto get_awaiter(context_type&, const sellitem::txn::state_t&, T&) {
        throw new std::runtime_error("baddd");
      }

      setprice::txn::task_t<context_type> setprice_task;
    };  // struct coordinator_t

  }  // namespace txn
}  // namespace sellitem
