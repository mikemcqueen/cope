// txsetprice.cpp

#include "msvc_wall.h"
#include "txsetprice.h"
#include "ui_msg.h"
#include "internal/cope_log.h"

namespace setprice::txn {
  using cope::result_code;
//  using cope::txn::handler_t;
  using promise_type = handler_t::promise_type;
  using setprice::msg::data_t;
  namespace log = cope::log;

  auto validate_price(const msg_base_t& msg, int price) {
    result_code rc = msg::validate(msg);
    if (cope::succeeded(rc)) {
      const auto& setprice_msg = msg.as<const data_t&>();
      if (setprice_msg.price != price) {
        log::info("setprice::validate_price, price mismatch: "
          "expected({}), actual({})", price, setprice_msg.price);
        rc = result_code::e_fail; // price mismatch (this is expected)
      }
      else {
        log::info("setprice::validate_price, prices match({})", price);
      }
    }
    return rc;
  }

  auto validate_price(const promise_type& promise, int price) {
    return validate_price(promise.in().get(), price);
  }

  auto validate_complete(const promise_type& promise, cope::msg::id_t msg_id) {
    return cope::msg::validate_id(promise.in().get(), msg_id);
  }

  auto enter_price_text(int /*price*/) {
    return std::make_unique<msg_base_t>(ui::msg::id::kSendChars);
  }

  auto click_ok_button() {
    return std::make_unique<msg_base_t>(ui::msg::id::kClickWidget);
  }

  auto handler() -> handler_t {
    state_t state;

    while (true) {
      auto& promise = co_await txn::receive{ kTxnId, state };
      const auto& error = [&promise](result_code rc) {
        return promise.set_result(rc).failed();
      };
      while (!promise.result().unexpected()) {
        if (error(validate_price(promise, state.price))) {
          co_yield enter_price_text(state.price);
          if (error(validate_price(promise, state.price))) continue;
        }
        co_yield click_ok_button();
        error(validate_complete(promise, state.prev_msg_id));
        break;
      }
      cope::txn::complete<msg::proxy_t>(promise);
    }
  }
} // namespace setprice::txn