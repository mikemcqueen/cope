// cope_msg.h

#ifndef INCLUDE_COPE_MSG_H
#define INCLUDE_COPE_MSG_H

#include <memory>
#include <string>
#include <string_view>
#include "cope_result.h"
#include "internal/cope_log.h"

using namespace std::literals;

namespace cope {
  namespace msg { struct data_t; }

  using msg_t = msg::data_t;
  using msg_ptr_t = std::unique_ptr<msg_t>;

  namespace msg {
    namespace name {
      constexpr std::string_view kTxnStart{ "msg::txn_start" };
      constexpr std::string_view kNoOp{ "msg::no_op" };
    }

    struct data_t {
      data_t(std::string_view name) : msg_name(name) {}
      data_t(const data_t&) = delete;
      data_t& operator=(const data_t&) = delete;
      virtual ~data_t() = default;

      template<typename T> const T& as() const {
        return dynamic_cast<const T&>(*this);
      }
      template<typename T> T& as() {
        return dynamic_cast<T&>(*this);
      }

      std::string msg_name;
    };

    struct noop_t : data_t {
      noop_t() : data_t(name::kNoOp) {}
    };

    inline auto make_noop() {
      return std::make_unique<noop_t>();
    }
      
    inline auto validate_name(const msg_t& msg, std::string_view msg_name) {
      result_code rc = result_code::s_ok;
      if (msg.msg_name != msg_name) {
        log::info("msg::validate_name() mismatch, expected({}), actual({})",
          msg_name, msg.msg_name);
        rc = result_code::e_unexpected_msg_name;
      }
      return rc;
    }

    template<typename msgT>
    auto validate(const msg_t& msg, std::string_view msg_name) {
      result_code rc = validate_name(msg, msg_name);
      if (succeeded(rc) && !dynamic_cast<const msgT*>(&msg)) {
        log::info("msg::validate() type mismatch, expected({}), actual({})",
          msg_name, msg.msg_name);
        rc = result_code::e_unexpected_msg_type;
      }
      return rc;
    }

    constexpr auto is_start_txn(const msg_t& msg) {
      return msg.msg_name == msg::name::kTxnStart;
    }
  } // namespace msg
} // namespace cope

#endif // INCLUDE_COPE_MSG_H