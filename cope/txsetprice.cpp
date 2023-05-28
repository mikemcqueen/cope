// txsetprice.cpp

#include "msvc_wall.h"
#include "txsetprice.h"
#include "ui_msg.h"

namespace setprice::txn {
  using dp::result_code;
  using dp::txn::handler_t;
  using promise_type = handler_t::promise_type;
  using setprice::msg::data_t;
  using Data_t = setprice::msg::data_t;

  auto validate_price(const dp::msg_t& msg, int price) {
//    using namespace Translate;
    result_code rc = msg::validate(msg);
    if (rc == result_code::s_ok) {
      const auto& spmsg = msg.as<Data_t>();
      if (spmsg.price.GetPlat() != price) {
        LogError(L"setprice::validate_price, price mismatch: "
          "expected(%d), actual(%d)", price, spmsg.price.GetPlat());
        rc = result_code::e_fail; // price mismatch? (this is expected)
      }
      else {
        LogInfo(L"setprice::validate_price, prices match(%d)", price);
      }
    }
    return rc;
  }

  auto validate_price(const promise_type& promise, int price) {
    return validate_price(promise.in(), price);
  }

  auto validate_complete(const promise_type& promise, std::string_view msg_name) {
    return dp::msg::validate_name(promise.in(), msg_name);
  }

  auto enter_price_text(int price) {
    return std::make_unique<ui::msg::send_chars::data_t>("setprice", 1,
      std::to_string(price));
  }

  auto click_ok_button() {
    return std::make_unique<ui::msg::click::data_t>("setprice", 2);
  }

  auto handler() -> handler_t {
    state_t state;

    while (true) {
      auto& promise = co_await dp::txn::receive_awaitable{ kTxnName, state };
      const auto& error = [&promise](result_code rc) { return promise.set_result(rc).failed(); };

      if (error(validate_price(promise, state.price))) {
        co_yield enter_price_text(state.price);
        if (error(validate_price(promise, state.price))) continue;
      }

      co_yield click_ok_button();
      error(validate_complete(promise, state.prev_msg_name));

      dp::txn::complete(promise);
    }
  }
} // namespace Broker::SetPrice::Txn
