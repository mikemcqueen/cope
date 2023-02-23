#include "txsellitem.h"
#include "txsetprice.h"

namespace sellitem
{
  using dp::msg_t;
  using dp::txn::handler_t;
  using promise_type = handler_t::promise_type;

  struct row_data_t {
    int index;
    bool price_matches;
    bool selected;
    bool listed;
  };

  auto get_matching_row(const dp::msg_t& msg, const txn::state_t& state) {
    msg; state;
    row_data_t row_data{ -1 };
    return row_data;
  }

  auto start_txn_setprice(handler_t::handle_t handle, promise_type& promise,
    const txn::state_t& sellitem_state)
  {
    auto setprice_state = std::make_unique<setprice::txn::state_t>(
      std::string(sellitem::msg::name), sellitem_state.item_price);
    return dp::txn::start_txn_awaitable<setprice::txn::state_t>{
      setprice::txn::name,
        handle,
        std::move(promise.in_ptr()),
        std::move(setprice_state)
    };
  }

  namespace txn {
    auto handler() -> handler_t {
      handler_t setprice_handler{ setprice::txn::handler() };
      handler_t::awaitable event;
      txn::state_t state{ "abc", 2 };

      while (true) {
        log(std::format("sellitem::txn_handler, outer co_await").c_str());
        auto& promise = co_await event;
        auto& msg = promise.in();
        if (msg.msg_name == setprice::msg::name) {
          auto& txn_p = co_await start_txn_setprice(setprice_handler.handle(),
            promise, state);
          // move this to awaitable?
          log(std::format("txn_result, msg_name: {}", txn_p.in().msg_name).c_str());
        }
      }
    }
  } // namespace txn
} // namespace sellitem

