// txsetprice.cpp

#include "txsetprice.h"

namespace setprice {
  using dp::result_code;
  using dp::txn::handler_t;
  using dp::msg_t;
  using promise_type = handler_t::promise_type;

  auto validate_price(const msg::data_t& msg, int price) {
    result_code rc = dp::msg::validate<msg::data_t>(msg, msg::name);
    if (rc == result_code::success) {
      const msg::data_t& spmsg = dynamic_cast<const msg::data_t&>(msg);
      if (spmsg.price != price) {
        log(std::format("setprice::validate_price, price mismatch: "
          "expected({}), actual({})", price, spmsg.price));
        rc = result_code::expected_error; // price mismatch? (this is expected)
      }
      else {
        log(std::format("setprice::validate_price, prices match({})", price));
      }
    }
    return rc;
  }

  auto validate_price(const promise_type& promise, int price) {
    return validate_price(promise.in_as<msg::data_t>(), price);
  }

  auto validate_complete(const promise_type& promise, std::string_view msg_name) {
    return dp::msg::validate_name(promise.in(), msg_name);
  }

  auto input_price(int price) {
    return std::make_unique<msg_t>(std::format("input_price({})", price));
  }

  auto click_ok() {
    return std::make_unique<msg_t>("click_ok");
  }

  namespace txn {
    auto handler() -> handler_t {
      result_code rc{ result_code::success };
      const auto& error = [&rc](result_code new_rc) noexcept {
        rc = new_rc;
        const auto error = (rc != result_code::success);
        if (error) log(std::format("  txn::setprice error({})", (int)rc));
        return error;
      };
      state_t state;

      for (auto& promise = co_await handler_t::awaitable{ txn::name }; true;
        co_await dp::txn::complete(promise, rc))
      {
        auto& txn = promise.in();
        if (error(validate_start(txn))) continue;
        state = std::move(start_t::state_from(txn));

        const msg::data_t& msg = dynamic_cast<const msg::data_t&>(start_t::msg_from(txn));
        // TODO: I don't like this. validation errors should always continue?
        // can't we access the price? have it returned via &price param?
        if (error(validate_price(msg, state.price))) {
          if (rc == result_code::unexpected_error) continue;
          co_yield input_price(state.price);
          if (error(validate_price(promise, state.price))) continue;
        }

        co_yield click_ok();
        error(validate_complete(promise, state.prev_msg_name));
      }
    }
  } // namespace txn
} // namespace setprice
