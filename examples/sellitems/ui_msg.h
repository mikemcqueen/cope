// ui_msg.h

#ifndef INCLUDE_UI_MSG_H
#define INCLUDE_UI_MSG_H

#include <string_view>

namespace ui::msg {
  namespace id {
    constexpr auto kFirst{          cope::msg::make_id(1000) };

    constexpr auto kClickWidget{    kFirst };
    constexpr auto kClickPoint{     cope::msg::make_id(1001) };
    constexpr auto kClickTableRow{  cope::msg::make_id(1002) };
    constexpr auto kSendChars{      cope::msg::make_id(1003) }; 

    constexpr auto kLast{           cope::msg::make_id(1099) };
  }
} // namespace ui::msg

#endif // INCLUDE_UI_MSG_H
