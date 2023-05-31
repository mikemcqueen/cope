// ui_msg.h

#ifndef INCLUDE_UI_MSG_H
#define INCLUDE_UI_MSG_H

#include <string_view>

namespace ui::msg {
  namespace name {
    constexpr std::string_view click_widget{ "ui::msg::click_widget" };
    constexpr std::string_view click_point{ "ui::msg::click_point" };
    constexpr std::string_view click_table_row{ "ui::msg::click_table_row" };
    constexpr std::string_view send_chars{ "ui::msg::send_chars" };
  }
} // namespace ui::msg

#endif // INCLUDE_UI_MSG_H
