// ui_msg.h

#pragma once

#ifndef INCLUDE_UI_MSG_H
#define INCLUDE_UI_MSG_H

#include <format>
#include <optional>

namespace ui::msg {
  namespace click_widget {
    struct data_t {
      int widget_id;
    };
  }
  namespace click_point {
    struct data_t {
      int x;
      int y;
    };
  }
  namespace click_table_row {
    struct data_t {
      int row;
    };
  }
  namespace send_chars {
    struct data_t {
      int temp;
    };
  }

  namespace id {
    constexpr auto kFirst{          1000 };//cope::msg::make_id(1000) };

    constexpr auto kClickWidget{    kFirst };
    constexpr auto kClickPoint{     1001 }; // cope::msg::make_id(1001) };
    constexpr auto kClickTableRow{  1002 }; // cope::msg::make_id(1002) };
    constexpr auto kSendChars{      1003 }; // cope::msg::make_id(1003) }; 

    constexpr auto kLast{           1099 }; // cope::msg::make_id(1099) };
  }

  template<typename Variant>
#if 1
  inline auto get_id(const Variant& var) {
    auto id = [&var](auto&& arg) -> int {
      using T = std::decay_t<decltype(arg)>;
      if constexpr (std::is_same_v<T, click_widget::data_t>) {
        return id::kClickWidget;
      } else if constexpr (std::is_same_v<T, click_point::data_t>) {
        return id::kClickPoint;
      } else if constexpr (std::is_same_v<T, click_table_row::data_t>) {
        return id::kClickTableRow;
      } else if constexpr (std::is_same_v<T, send_chars::data_t>) {
        return id::kSendChars;
      } else {
        return -1;
      }
    };
    return std::visit(id, var);
#else
  inline auto get_id(const Variant&) {
    return -1;
#endif
  }

  template<typename T>
  inline constexpr auto get_type_name(const T&) -> std::optional<std::string> {
    return std::nullopt;
  }

  template<> inline constexpr auto get_type_name(const click_widget::data_t&)
      -> std::optional<std::string> {
    return "ui::msg::click_widget";
  }

  template<> inline constexpr auto get_type_name(const click_point::data_t&)
      -> std::optional<std::string> {
    return "ui::msg::click_point";
  }

  template<> inline constexpr auto get_type_name(const click_table_row::data_t&)
      -> std::optional<std::string> {
    return "ui::msg::click_table_row";
  }

  template<> inline constexpr auto get_type_name(const send_chars::data_t&)
      -> std::optional<std::string> {
    return "ui::msg::send_chars";
  }
} // namespace ui::msg

#endif // INCLUDE_UI_MSG_H
