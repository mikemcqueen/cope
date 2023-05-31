// txsetprice.cpp

#include "msvc_wall.h"
#include "txsetprice.h"
#include "ui_msg.h"
#include "internal/cope_log.h"

namespace setprice::txn {
  using cope::result_code;
  using cope::txn::handler_t;
  using promise_type = handler_t::promise_type;
  using setprice::msg::data_t;
  namespace log = cope::log;

  auto validate_price(const cope::msg_t& msg, int price) {
    result_code rc = msg::validate(msg);
    if (cope::succeeded(rc)) {
      const auto& spmsg = msg.as<data_t>();
      if (spmsg.price != price) {
        log::info("setprice::validate_price, price mismatch: "
          "expected({}), actual({})", price, spmsg.price);
        rc = result_code::e_fail; // price mismatch (this is expected)
      }
      else {
        log::info("setprice::validate_price, prices match({})", price);
      }
    }
    return rc;
  }

  auto validate_price(const promise_type& promise, int price) {
    return validate_price(promise.in(), price);
  }

  auto validate_complete(const promise_type& promise,
    std::string_view msg_name)
  {
    return cope::msg::validate_name(promise.in(), msg_name);
  }

  auto enter_price_text(int /*price*/) {
    return std::make_unique<cope::msg_t>(ui::msg::name::send_chars);
  }

  auto click_ok_button() {
    return std::make_unique<cope::msg_t>(ui::msg::name::click_widget);
  }

  auto handler() -> handler_t {
    state_t state;

    while (true) {
      auto& promise = co_await cope::txn::receive_awaitable{ kTxnName, state };
      const auto& error = [&promise](result_code rc) {
        return promise.set_result(rc).failed();
      };
      while (!promise.result().unexpected()) {
        if (error(validate_price(promise, state.price))) {
          co_yield enter_price_text(state.price);
          if (error(validate_price(promise, state.price))) continue;
        }
        co_yield click_ok_button();
        error(validate_complete(promise, state.prev_msg_name));
        break;
      }
      cope::txn::complete(promise);
    }
  }
} // namespace setprice::txn