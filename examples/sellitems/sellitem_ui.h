#pragma once

#include "ui_msg.h"
#include "internal/cope_log.h"

namespace sellitem::ui {
  namespace log = cope::log;
  using namespace ::ui::msg;

  inline auto click_table_row(int row_index) {
    log::info("constructing click_table_row ({})", row_index);
    return click_table_row::data_t{ row_index };
  }

  inline auto click_setprice_button() {
    log::info("constructing click_setprice_button");
    return click_widget::data_t{ 1 };
  }

  inline auto click_listitem_button() {
    log::info("constructing click_listitem_button");
    return click_widget::data_t{ 2 };
  }
}  // namespace sellitem::ui
