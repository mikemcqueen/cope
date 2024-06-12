// setprice_coord.h: setprice transaction coordinator

#pragma once

#include "txsetprice.h"

namespace setprice::txn {

  template <typename ContextT>
  struct coordinator_t {
    using context_type = ContextT;
    using yield_msg_type = ContextT::out_msg_type;
    using task_type = no_context_task_t<context_type>;
    // TODO
    using awaiter_types = std::tuple<std::monostate>;

    coordinator_t(context_type&) {}

    cope::result_t update_state(const context_type& context, state_t& state) {
      return setprice::txn::update_state(context, state);
    }

    yield_msg_type get_yield_msg(const state_t& state) {
      return setprice::txn::get_yield_msg<yield_msg_type>(state);
    }

    auto receive_start_txn(context_type&, state_t& state) {
      using namespace cope::txn;
      return receive_awaitable<task_type, msg::data_t, state_t>{state};
    }

    // not specialized, should never be called
    template <typename T>
    auto get_awaiter(context_type&, const state_t&, T&) {
      throw new std::runtime_error("baddd");
    }
  };  // struct coordinator_t

}  // namespace setprice::txn
