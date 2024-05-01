// txsetprice.cpp

#include "msvc_wall.h"
//#include "txsetprice.h"
#include "handlers.h"
#include "ui_msg.h"
#include "internal/cope_log.h"

namespace setprice::txn {
  using cope::result_code;
//  using cope::txn::handler_t;
  using promise_type = app::task_t::promise_type;
  using setprice::msg::data_t;
  namespace log = cope::log;

  auto validate_price(const msg::data_t& msg, int price) {
    result_code rc = result_code::s_ok; //msg::validate(msg);
    //    if (cope::succeeded(rc)) {
    //      const auto& setprice_msg = msg.as<const data_t&>();
      if (msg.price != price) {
        log::info("setprice::validate_price, price mismatch: "
          "expected({}), actual({})", price, msg.price);
        rc = result_code::e_fail; // price mismatch (this is expected)
      }
      else {
        log::info("setprice::validate_price, prices match({})", price);
      }
    //    }
    return rc;
  }

  auto validate_price(const promise_type& promise, int price) {
    auto& msg = promise.context().in();
    auto rc = setprice::msg::validate(msg);
    if (cope::succeeded(rc)) {
      rc = validate_price(std::get<data_t>(msg), price);
    }
    return rc;
  }

  auto validate_complete(const promise_type& promise, cope::msg::id_t /*msg_id*/) {
    return sellitem::msg::validate(promise.context().in());
  }

  auto enter_price_text(int /*price*/) {
    log::info("constructing enter_price_text");
    return ui::msg::send_chars::data_t{ 1 };
  }

  auto click_ok_button() {
    log::info("constructing click_ok_button");
    return ui::msg::click_widget::data_t{ 5 };
  }

  template<typename Context>
  auto handler(Context& /*context*/, cope::txn::id_t /*task_id*/)
    -> cope::txn::task_t<Context>
  {
    using task_t = cope::txn::task_t<Context>;
    using receive_start_txn = cope::txn::receive_awaitable<task_t, msg::data_t, state_t>;

    state_t state;

    while (true) {
      auto& promise = co_await receive_start_txn{ state }; // txn::receive{ kTxnId, state };
      const auto& error = [&promise](result_code rc) {
        return promise.context().set_result(rc).failed();
      };
      while (!promise.context().result().unexpected()) {
        if (error(validate_price(promise, state.price))) {
          co_yield enter_price_text(state.price);
          if (error(validate_price(promise, state.price))) continue;
        }
        co_yield click_ok_button();
        error(validate_complete(promise, 0/*state.prev_msg_id*/));
        break;
      }
      task_t::complete_txn(promise);
    }
  }

  template auto handler<app::context_t>(app::context_t&, cope::txn::id_t) -> app::task_t;
} // namespace setprice::txn
