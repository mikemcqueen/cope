// txsetprice.cpp

#include "msvc_wall.h"
#include "txsetprice.h"
#include "internal/cope_log.h"

namespace log = cope::log;

namespace {
  using namespace setprice;

  cope::result_t validate_price(const msg::data_t& msg, int price) {
    cope::result_t result{};
    if (msg.price != price) {
      log::info("setprice::validate_price, price mismatch: "
        "expected({}), actual({})", price, msg.price);
      result = cope::result_code::e_fail;  // price mismatch (this is expected)
    } else {
      log::info("setprice::validate_price, prices match({})", price);
    }
    return result;
  }
}  // namespace

namespace setprice::txn {
  cope::expected_operation update_state(
      const msg::data_t& msg, state_t& state) {
    if (validate_price(msg, state.price).failed()) {
      state.next_action = action::enter_price;
    } else {
      state.next_action = action::click_ok;
    }
    return cope::operation::yield;
  }
} // namespace setprice::txn
