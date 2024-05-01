#pragma once

#include "cope_result.h"

namespace setprice {
  namespace msg {
    struct data_t {
      int price;
    }; // setprice::msg::data_t

    template<typename Msg>
    inline auto validate(const Msg& msg) {
      using namespace cope;
      // TODO: use cope::msg::validate<data_t>(msg)
      return std::holds_alternative<data_t>(msg) ?
        result_code::s_ok : result_code::e_unexpected_msg_type;
    }
  } // namespace msg
} // namespace setprice
