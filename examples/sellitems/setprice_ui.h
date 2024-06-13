#pragma once

#include "ui_msg.h"
#include "internal/cope_log.h"

namespace setprice::ui {
  namespace log = cope::log;
  using namespace ::ui::msg;

  inline auto enter_price_text(int price) {
    log::info("constructing enter_price_text");
    return send_chars::data_t{ price };
  }

  inline auto click_ok_button() {
    log::info("constructing click_ok_button");
    return click_widget::data_t{ 5 };
  }
}  // namespace setprice::ui
