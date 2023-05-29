// ui_msg.h

#ifndef INCLUDE_UI_MSG_H
#define INCLUDE_UI_MSG_H

#include <string_view>
#include "cope_msg.h"

namespace ui::msg {
  namespace name {
    constexpr std::string_view click_widget{ "ui::msg::click_widget" };
    constexpr std::string_view click_point{ "ui::msg::click_point" };
    constexpr std::string_view click_table_row{ "ui::msg::click_table_row" };
    constexpr std::string_view send_chars{ "ui::msg::send_chars" };
    constexpr std::string_view set_widget_text{ "ui::msg::set_widget_text" };
  }
/*

  struct data_t : dp::msg_t {
    data_t(std::string_view msg_name, std::string_view window_name) :
      dp::msg_t(msg_name), window_name(window_name) {}

    std::string window_name;
  };
  */
} // namespace ui::msg

#endif // INCLUDE_UI_MSG_H
