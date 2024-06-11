// sellitem_coord.h: sellitem transaction coordinator

#pragma once

#include "handlers.h"
#include "txsellitem.h"
#include "txsetprice.h"
#include "ui_msg.h"

namespace sellitem {
  namespace txn {

    template <typename ContextT>
    struct coordinator_t {
      using context_type = ContextT;
      using yield_msg_type = ContextT::out_msg_type;
      using task_type = task_t<context_type>;

      using awaiter_types = std::tuple<setprice::txn::start_awaiter<context_type>>;

      coordinator_t(context_type& context)
          : setprice_task(setprice::txn::handler(context, setprice::kTxnId)) {}

      /*
      cope::result_t update_state(const msg::data_t& msg, state_t& state) {
        return sellitem::txn::update_state(msg, state);
      }
      */
      cope::result_t update_state(const context_type& context, state_t& state) {
        return sellitem::txn::update_state(context, state);
      }

      yield_msg_type get_next_action_msg(const state_t& state) {
        return sellitem::txn::get_next_action_msg<yield_msg_type>(state);
      }

      auto receive_start_txn(context_type&, state_t& state) {
        using namespace cope::txn;
        return receive_awaitable<task_type, msg::data_t, state_t>{state};
      }

      // specialized "elsewhere" (TODO: make it somewhere sane)
      template <typename T>
      auto get_awaiter(context_type&, const state_t&, T&) {
        throw new std::runtime_error("baddd");
      }

      setprice::txn::task_t<context_type> setprice_task;
    };  // struct coordinator_t

  }  // namespace txn
}  // namespace sellitem
