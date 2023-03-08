// txsetprice.cpp

#include "stdafx.h"
#include "txsetprice.h"
#include "ui_msg.h"
//#include "DcrSetPrice.h" // Types.h"

namespace setprice::txn {
  using dp::result_code;
  using dp::txn::handler_t;
  using promise_type = handler_t::promise_type;
  using setprice::msg::data_t;
  using Data_t = setprice::msg::data_t;

  auto validate_price(const dp::msg_t& msg, int price) {
//    using namespace Translate;
    result_code rc = msg::validate(msg);
    if (rc == result_code::success) {
      const auto& spmsg = msg.as<Data_t>();
      if (spmsg.price.GetPlat() != price) {
        LogError(L"setprice::validate_price, price mismatch: "
          "expected(%d), actual(%d)", price, spmsg.price.GetPlat());
        rc = result_code::expected_error; // price mismatch? (this is expected)
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
//    using namespace Broker::SetPrice;
    price;
    return std::make_unique<ui::msg::send_chars::data_t>("setprice", 1, "1"); //  kWindowName,
//      Widget::Id::PriceText, std::to_string(price));
  }

  auto click_ok_button() {
//    using namespace Broker::SetPrice;
    return std::make_unique<ui::msg::click::data_t>("setprice", 2); // kWindowName,
//      Widget::Id::OkButton);
  }

  auto handler() -> handler_t {
    result_code rc{ result_code::success };
    const auto& error = [&rc](result_code new_rc) noexcept {
      rc = new_rc;
      const auto is_error = (rc != result_code::success);
      if (is_error) {
        LogError(L"  txn::setprice error(%d)", (int)rc);
      }
      return is_error;
    };
    state_t state;

    while (true) {
      auto& promise = co_await dp::txn::receive_txn_awaitable{ kTxnName, state };
#if 0
      auto& txn = promise.in();
      if (error(dp::txn::validate_start(txn, kTxnName))) continue;
      state = start_t::state_from(txn);
      const auto& msg = start_t::msg_from(txn).as<Translate::Data_t>();
#endif
      // TODO: I don't like this. validation errors should always continue?
      // can't we access the price? have it returned via &price param?
      // result_t, succeeded, s_false
      if (error(validate_price(promise, state.price))) {
        if (rc == result_code::unexpected_error) continue;
        co_yield enter_price_text(state.price);
        if (error(validate_price(promise, state.price))) continue;
      }

      co_yield click_ok_button();
      error(validate_complete(promise, state.prev_msg_name));

      dp::txn::complete(promise, rc);
    }
  }
} // namespace Broker::SetPrice::Txn
