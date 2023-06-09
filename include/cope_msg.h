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

    enum class id_t : int32_t {};

    namespace id {
      constexpr auto kNoOp{ static_cast<id_t>(1) };
      constexpr auto kTxnStart{ static_cast<id_t>(2) };
    }

    struct data_t {
      data_t(id_t id) : msg_id(id) {}
      data_t(const data_t&) = delete;
      data_t& operator=(const data_t&) = delete;
      virtual ~data_t() = default;

      template<typename T> const T& as() const {
        return dynamic_cast<const T&>(*this);
      }
      template<typename T> T& as() {
        return dynamic_cast<T&>(*this);
      }

      id_t msg_id;
    };

    struct noop_t : data_t {
      noop_t() : data_t(id::kNoOp) {}
    };

    inline auto make_noop() {
      return std::make_unique<noop_t>();
    }
      
    inline auto validate_id(const msg_t& msg, id_t msg_id) {
      result_code rc = result_code::s_ok;
      if (msg.msg_id != msg_id) {
        log::info("msg::validate_id() mismatch, expected({}), actual({})",
          (int)msg_id, (int)msg.msg_id);
        rc = result_code::e_unexpected_msg_name;
      }
      return rc;
    }

    template<typename msgT>
    auto validate(const msg_t& msg, id_t msg_id) {
      result_code rc = validate_id(msg, msg_id);
      if (succeeded(rc) && !dynamic_cast<const msgT*>(&msg)) {
        log::info("msg::validate() type mismatch, expected({}), actual({})",
          msg_id, msg.msg_id);
        rc = result_code::e_unexpected_msg_type;
      }
      return rc;
    }

    template<typename msgT> 
      requires requires (const msgT& msg) { msg.msg_id; }
    constexpr auto is_start_txn(const msgT& msg) {
      return msg.msg_id == id::kTxnStart;
    }
  } // namespace msg
} // namespace cope

#endif // INCLUDE_COPE_MSG_H