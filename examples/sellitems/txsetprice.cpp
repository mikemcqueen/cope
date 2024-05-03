// txsetprice.cpp

#include "msvc_wall.h"
#include "handlers.h"
#include "ui_msg.h"
#include "internal/cope_log.h"

namespace setprice::txn {
  using context_type = app::context_t;
  using cope::result_code;
  using setprice::msg::data_t;
  namespace log = cope::log;

  auto validate_price(const msg::data_t& msg, int price) {
    result_code rc = result_code::s_ok;
    if (msg.price != price) {
      log::info("setprice::validate_price, price mismatch: "
        "expected({}), actual({})", price, msg.price);
      rc = result_code::e_fail; // price mismatch (this is expected)
    } else {
      log::info("setprice::validate_price, prices match({})", price);
    }
    return rc;
  }

  auto validate_price(const context_type& context, int price) {
    auto& msg = context.in();
    auto rc = setprice::msg::validate(msg);
    if (cope::succeeded(rc)) {
      rc = validate_price(std::get<data_t>(msg), price);
    }
    return rc;
  }

  auto validate_complete(const context_type& context, cope::msg::id_t /*msg_id*/) {
    return sellitem::msg::validate(context.in());
  }

  auto enter_price_text(int price) {
    log::info("constructing enter_price_text");
    return ui::msg::send_chars::data_t{ price };
  }

  auto click_ok_button() {
    log::info("constructing click_ok_button");
    return ui::msg::click_widget::data_t{ 5 };
  }

  template<typename ContextT>
  auto handler(ContextT& /*context*/, cope::txn::id_t /*task_id*/)
    -> cope::txn::task_t<ContextT>
  {
    using task_t = cope::txn::task_t<ContextT>;
    using receive_start_txn = cope::txn::receive_awaitable<task_t, msg::data_t, state_t>;

    state_t state;

    while (true) {
      auto& promise = co_await receive_start_txn{ state };
      auto& context = promise.context();
      const auto& error = [&context](result_code rc) {
        return context.set_result(rc).failed();
      };
      while (!context.result().unexpected()) {
        if (error(validate_price(context, state.price))) {
          co_yield enter_price_text(state.price);
          if (error(validate_price(context, state.price))) continue;
        }
        co_yield click_ok_button();
        error(validate_complete(context, 0/*state.prev_msg_id*/));
        break;
      }
      task_t::complete_txn(promise);
    }
  }

  template auto handler<app::context_t>(app::context_t&, cope::txn::id_t) -> app::task_t;
} // namespace setprice::txn
