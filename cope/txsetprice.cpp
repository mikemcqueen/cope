// txsetprice.cpp

#include "txsetprice.h"

namespace setprice {
  using dp::txn::result_code;
  using dp::txn::handler_t;
  using dp::msg_t;
  using promise_type = handler_t::promise_type;

  // txn_complete are candidates for moving to dp::txn namespace
  auto txn_complete(promise_type& promise, dp::msg_ptr_t msg_ptr) {
    return dp::txn::complete_txn_awaitable{
      setprice::txn::name,
      promise.prev_handle().value(),
      std::move(msg_ptr)
    };
  }

  auto txn_complete(promise_type& promise) {
    return txn_complete(promise, promise.in_ptr());
  }

  auto txn_complete(promise_type& promise, result_code rc) {
    if (rc != result_code::success) {
      log(std::format("setprice::txn_complete error: {}", (int)rc).c_str());
      return txn_complete(promise, std::move(
        std::make_unique<dp::txn::complete_t>(txn::name, rc)));
    }
    else {
      return txn_complete(promise, std::move(promise.in_ptr()));
    }
  }

  auto validate_message_name(const msg_t& msg, std::string_view msg_name) {
    result_code rc = result_code::success;
    if (msg.msg_name != msg_name) {
      log(std::format("validate_message_name: name mismatch, expected({}), actual({})",
        msg_name, msg.msg_name).c_str());
      rc = result_code::unexpected_error;
    }
    return rc;
  }

  template<typename T>
  auto validate_message(const msg_t& msg, std::string_view msg_name) {
    result_code rc = validate_message_name(msg, msg_name);
    if (rc == result_code::success) {
      if (!dynamic_cast<const T*>(&msg)) {
        log("validate_message: message type mismatch");
        rc = result_code::unexpected_error;
      }
    }
    return rc;
  }

  auto validate_txn_start(const msg_t& txn) {
    result_code rc = validate_message<txn::start_t>(txn, dp::txn::name::start);
    if (rc == result_code::success) {
      // validate that the msg contained within is of type setprice::msg::data_t
      rc = validate_message<msg::data_t>(txn::start_t::msg_from(txn), msg::name);
    }
    return rc;
  }

  auto validate_price(const msg_t& msg, int price) {
    result_code rc = validate_message<msg::data_t>(msg, msg::name);
    if (rc == result_code::success) {
      const msg::data_t& spmsg = static_cast<const msg::data_t&>(msg);
      if (spmsg.price != price) {
        log(std::format("setprice::validate_price: price mismatch, expected({}), actual({})",
          price, spmsg.price).c_str());
        rc = result_code::expected_error; // price mismatch? (this is expected)
      }
      else {
        log(std::format("setprice::validate_price: prices match({})", price).c_str());
      }
    }
    return rc;
  }

  auto validate_price(const promise_type& promise, int price) {
    return validate_price(promise.in(), price);
  }

  auto validate_complete(const promise_type& promise, std::string_view msg_name) {
    return validate_message_name(promise.in(), msg_name);
  }

  auto input_price(const promise_type& promise, int) {
    promise;
    return std::make_unique<msg_t>("input_price");
  }

  auto click_ok() {
    return std::make_unique<msg_t>("click_ok");
  }

  namespace txn {
    auto handler() -> handler_t {
      txn::state_t state;
      result_code rc{ result_code::success };
      const auto& error = [&rc]() noexcept { return rc != result_code::success; };

      handler_t::awaitable event;
      bool first{ true };
      for (auto& promise = co_await event; true;) {
        if (!first) {
          log(std::format("setprice::txn_handler before co_await").c_str());
          // could put this in for(;;here) loop
          co_await txn_complete(promise, rc);
        }
        first = false;
        auto& txn = promise.in();
        rc = validate_txn_start(txn);
        if (error()) continue;
        state = std::move(txn::start_t::state_from(txn));

        const msg_t& msg = txn::start_t::msg_from(txn);
        rc = validate_price(msg, state.price);
        if (rc == result_code::unexpected_error) continue;
        //for (retry_t retry; error() && retry.allowed(); retry--) {
        if (error()) {
          co_yield input_price(promise, state.price);
          rc = validate_price(promise, state.price);
          if (error()) continue;
        }

        co_yield click_ok();
        rc = validate_complete(promise, state.prev_msg_name);
        //if (error()) continue;
      }
    }
  } // namespace txn
} // namespace setprice
