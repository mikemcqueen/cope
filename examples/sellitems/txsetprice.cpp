// txsetprice.cpp

#include "msvc_wall.h"
#include "app.h"
#include "ui_msg.h"
#include "internal/cope_log.h"

namespace setprice::txn {
  using context_type = app::context_t;
  using cope::result_code;
  using namespace setprice::msg;
  namespace log = cope::log;

  cope::result_t validate_price(const msg::data_t& msg, int price) {
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

  /*
  auto validate_price(const context_type& context, int price) {
    auto& msg = context.in();
    auto rc = setprice::msg::validate(msg);
    if (cope::succeeded(rc)) {
      rc = validate_price(std::get<data_t>(msg), price);
    }
    return rc;
  }
  */

  cope::result_t update_state(const data_t& msg, state_t& state) {
    if (validate_price(msg, state.price).failed()) {
      // TODO: it's not clear this next_action business is necessary.
      // couldn't we just store the out_msg in the state?
      state.next_action = action::enter_price;
    } else {
      state.next_action = action::click_ok;
    }
    state.next_operation = cope::operation::yield;
    return result_code::s_ok;
  }

  template <>
  cope::result_t update_state(const context_type& context, state_t& state) {
    const auto& msg = context.in();
    cope::result_t result = setprice::msg::validate(msg);
    if (result.succeeded()) {
      // setprice:msg -> yield next_action_msg
      return update_state(std::get<data_t>(msg), state);
    }
    result = sellitem::msg::validate(msg);
    if (result.succeeded()) {
      // sellitem::msg -> txn::complete
      state.next_operation = cope::operation::complete;
    }
    return result;
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

  template <>
  app::context_t::out_msg_type get_yield_msg(const state_t& state) {
    switch (state.next_action.value()) {
    case action::enter_price: return enter_price_text(state.price);
    case action::click_ok: return click_ok_button();
    default: throw new std::runtime_error("invalid action");
    }
  }

  /*
  template<typename ContextT>
  auto handler(ContextT&, cope::txn::id_t)
    -> no_context_task_t<ContextT>
  {
    using task_type = no_context_task_t<ContextT>;
    using receive_start_txn = cope::txn::receive_awaitable<task_type, msg::data_t, state_t>;

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
        error(validate_complete(context, 0)); // state.prev_msg_id
        break;
      }
      task_type::complete_txn(promise);
    }
  }

  template auto handler<app::context_t>(app::context_t&, cope::txn::id_t)
      -> no_context_task_t<app::context_t>;
  */
} // namespace setprice::txn
