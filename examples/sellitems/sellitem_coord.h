// sellitem_coord.h: sellitem transaction coordinator

#include "handlers.h"
#include "ui_msg.h"

namespace sellitem {

  namespace txn {

    // HACK having this here
    app::context_t::out_msg_type get_next_action_msg(const state_t& state);

    // would like to template this (entire struct) on ContextT and move to
    // txsetprice.h start awaiter
    using setprice_start_awaiter =
        cope::txn::start_awaitable<setprice::txn::task_t<app::context_t>,
            setprice::msg::data_t, setprice::txn::state_t>;

    struct coordinator_t {

      using task_type = task_t<app::context_t>;
      using receive_start_txn_awaiter =
          cope::txn::receive_awaitable<task_type, msg::data_t, state_t>;

      using awaiter_types = std::tuple<setprice_start_awaiter>;

      coordinator_t(app::context_t& context)
          : setprice_task(setprice::txn::handler(context, setprice::kTxnId)) {}

      cope::result_t update_state(const msg::data_t& msg, state_t& state) {
        return sellitem::txn::update_state(msg, state);
      }

      app::context_t::out_msg_type get_next_action_msg(const state_t& state) {
        return sellitem::txn::get_next_action_msg(state);
      }

      auto receive_start_txn(app::context_t&, sellitem::txn::state_t& state) {
        return receive_start_txn_awaiter{state};
      }

      // setprice::txn start awaiter
      /*setprice_start_awaiter*/
      template <typename T>
      auto get_awaiter(app::context_t& /*context*/,
          const sellitem::txn::state_t& /*state*/, T& /* awaiter*/) {
        throw new std::runtime_error("baddd");
      }

      setprice::txn::task_t<app::context_t>
          setprice_task; /*{setprice::txn::handler(context,
                            setprice::kTxnId)};*/
    };  // struct coordinator_t

    template <>
    auto coordinator_t::get_awaiter<setprice_start_awaiter>(
        app::context_t& context, const sellitem::txn::state_t& state,
        setprice_start_awaiter& awaiter) {
      auto& setprice_msg = std::get<setprice::msg::data_t>(context.in());
      awaiter = std::move(setprice::txn::start(
          setprice_task, std::move(setprice_msg), state.item_price));
    }

  }  // namespace txn
}  // namespace sellitem
