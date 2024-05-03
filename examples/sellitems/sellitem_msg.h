#pragma once

#include <optional>
#include <string>
#include <vector>
#include "cope_result.h"

namespace sellitem {
  namespace msg {
    struct row_data_t {
      std::string item_name;
      int item_price;
      bool item_listed;
      bool selected;
    };

    struct data_t {
      using row_vector = std::vector<row_data_t>;

      static std::optional<int> find_selected_row(const row_vector& rows) {
        for (size_t i{}; i < rows.size(); ++i) {
          if (rows[i].selected) return { (int)i };
        }
        return std::nullopt;
      }

      data_t() = default;
      data_t(row_vector&& rows) : rows(std::move(rows)) {}

      row_vector rows;
    }; // sellitem::msg::data_t

    template<typename Msg>
    inline auto validate(const Msg& msg) {
      // TODO: use cope::msg::validate<data_t>(msg)
      using namespace cope;
      return std::holds_alternative<data_t>(msg) ?
        result_code::s_ok : result_code::e_unexpected_msg_type;
    }
  } // namespace sellitem::msg
} // namespace sellitem
