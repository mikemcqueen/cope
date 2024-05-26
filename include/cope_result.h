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
    e_unexpected_msg_id,
    e_unexpected_msg_type,
    e_unexpected_txn_id
  };

  inline auto succeeded(result_code rc) { return rc <= result_code::s_false; }
  inline auto failed(result_code rc) { return rc > result_code::s_false; }
  inline auto unexpected(result_code rc) {
    return rc >= result_code::e_unexpected;
  }

  struct result_t {
    auto succeeded() const { return cope::succeeded(code); }
    auto failed() const { return cope::failed(code); }
    auto unexpected() const { return cope::unexpected(code); }

    result_t() = delete;
    result_t(result_code rc) : code(rc) {}

    operator result_code() const { return code; }

    result_code code = result_code::s_ok;
  };
} // namespace cope::result

template <>
struct std::formatter<cope::result_code> : std::formatter<std::string> {
  auto format(cope::result_code result, format_context& ctx) const {
    using cope::result_code;
    static std::unordered_map<cope::result_code, std::string> rc_map = {
      { result_code::s_ok, "s_ok" },
      { result_code::s_false, "s_false" },
      { result_code::e_abort, "e_abort" },
      { result_code::e_fail, "e_fail" },
      { result_code::e_unexpected, "e_unexpected" },
      { result_code::e_unexpected_msg_id, "e_unexpected_msg_id" },
      { result_code::e_unexpected_msg_type, "e_unexpected_msg_type" },
      { result_code::e_unexpected_txn_id, "e_unexpected_txn_id" }
    };
    return std::format_to(ctx.out(), "{}", rc_map[result/*.code*/]);
  }
};

#endif // INCLUDE_COPE_RESULT_H
