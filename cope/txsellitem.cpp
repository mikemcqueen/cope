#include "txsellitem.h"
#include "txsetprice.h"

namespace sellitem
{
  using dp::msg_t;
  using dp::txn::handler_t;
  using promise_type = handler_t::promise_type;

  auto get_matching_row(const dp::msg_t& msg, const txn::state_t& state) {
    msg; state;
    msg::row_data_t row_data{};
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
      using dp::txn::result_code;

      result_code rc{ result_code::success };
      const auto& error = [&rc]() noexcept { return rc != result_code::success; };
      state_t state{ "magic beans", 2 };
      dp::txn::handler_t setprice_handler{ setprice::txn::handler() };

      handler_t::awaitable initial{ txn::name };
#if 1
      while (true) {
#else
      bool first{ true };
      for (auto& promise = co_await initial; true;) {
        if (!first) {
          log(std::format("setprice::txn_handler before co_await").c_str());
          // could put this in for(;;here) loop
          co_await dp::txn::complete(promise, rc);
        }
        first = false;
        auto& txn = promise.in();
        rc = validate_txn_start(txn);
        if (error()) continue;
        state = std::move(txn::start_t::state_from(txn));

        const msg_t& msg = txn::start_t::msg_from(txn);
#endif
        error;
        log(std::format("sellitem::txn_handler, outer co_await").c_str());
        auto& promise = co_await initial;
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

