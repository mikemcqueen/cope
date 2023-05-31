// cope_result.h

#ifndef INCLUDE_COPE_RESULT_H
#define INCLUDE_COPE_RESULT_H

#include <format>
#include <functional>
#include <unordered_map>

namespace cope {
  enum class result_code : unsigned {
    s_ok = 0,
    s_false = 1,
    e_abort = 4,
    e_fail = 5,
    e_unexpected = 0xffff,
    e_unexpected_msg_name,
    e_unexpected_msg_type,
    e_unexpected_txn_name
  };

  inline auto succeeded(result_code rc) { return rc <= result_code::s_false; }
  inline auto failed(result_code rc) { return rc > result_code::s_false; }
  inline auto unexpected(result_code rc) {
    return rc >= result_code::e_unexpected;
  }

  struct result_t {
    result_code code = result_code::s_ok;

    void set(result_code rc) { code = rc; }

    auto succeeded() { return cope::succeeded(code); }
    auto failed() { return cope::failed(code); }
    auto unexpected() { return cope::unexpected(code); }
  };
} // namespace cope::result

template <>
struct std::formatter<cope::result_t> : std::formatter<std::string> {
  auto format(cope::result_t result, format_context& ctx) {
    using cope::result_code;
    static std::unordered_map<cope::result_code, std::string> rc_map = {
      { result_code::s_ok, "s_ok" },
      { result_code::s_false, "s_false" },
      { result_code::e_abort, "e_abort" },
      { result_code::e_fail, "e_fail" },
      { result_code::e_unexpected, "e_unexpected" },
      { result_code::e_unexpected_msg_name, "e_unexpected_msg_name" },
      { result_code::e_unexpected_msg_type, "e_unexpected_msg_type" },
      { result_code::e_unexpected_txn_name, "e_unexpected_txn_name" }
    };
    return std::format_to(ctx.out(), "{}", rc_map[result.code]);
/*
    if (rc_map.contains(rc)) {
      return std::formatter<string>::format(
        std::format("{}", rc_map[rc]));
    }
    else {
    }
  */
  }
};

#endif // INCLUDE_COPE_RESULT_H