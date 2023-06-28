// cope_msg.h

#ifndef INCLUDE_COPE_MSG_H
#define INCLUDE_COPE_MSG_H

//#include <memory>
//#include "cope_proxy.h"
//#include "cope_result.h"
//#include "internal/cope_log.h"

using namespace std::literals;

namespace cope {
  namespace msg {
    template<typename MsgT, typename StateT>
    struct start_txn_t {
      MsgT msg;
      StateT state;
    };
  } // namespace msg

#if 0
  namespace msg {
    struct data_t;
    enum class id_t;
  }

  struct basic_msg_t {
    template<typename T> T& as() { return static_cast<T&>(*this); }
    template<typename T> const T& as() const { return static_cast<T&>(*this); }

    cope::msg::id_t msg_id;
  };

  // TODO: polymorphic_msg_t
  using msg_t = msg::data_t;
  using msg_ptr_t = std::unique_ptr<msg_t>;

  namespace msg {
    enum class id_t : int32_t {};

    inline constexpr auto make_id(int id) { return static_cast<id_t>(id); }

    namespace id {
      constexpr auto kUndefined{ make_id(0) };
      constexpr auto kNoOp{      make_id(1) };
      constexpr auto kTxnStart{  make_id(2) };
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

    template<typename T>
    //  requires requires (T x) { x.msg_id; }
    struct proxy_t : cope::proxy::raw_ptr_t<T> {
      using base_t = cope::proxy::raw_ptr_t<T>;

      proxy_t() = default;
      proxy_t(const T& msg) : base_t(msg) {}
      proxy_t(T&& msg) = delete;
    };

    template<>
    struct proxy_t<msg_ptr_t> : cope::proxy::unique_ptr_t<msg_t> {
      using base_t = cope::proxy::unique_ptr_t<msg_t>;

      proxy_t() = default;
      proxy_t(const msg_ptr_t& msg_ptr) = delete;
      proxy_t(msg_ptr_t&& msg_ptr) : base_t(std::move(msg_ptr)) {}
    };

    inline auto make_noop() {
      return std::make_unique<noop_t>();
    }
      
    template<typename msgT>
    inline auto validate_id(const msgT& msg, id_t msg_id) {
      result_code rc = result_code::s_ok;
      if (msg.msg_id != msg_id) {
        log::info("msg::validate_id() mismatch, expected({}), actual({})",
          (int)msg_id, (int)msg.msg_id);
        rc = result_code::e_unexpected_msg_id;
      }
      return rc;
    }

    template<typename msgT>
    auto validate(const msgT& msg, id_t msg_id) {
      result_code rc = validate_id(msg, msg_id);
      if (succeeded(rc) && !dynamic_cast<const msgT*>(&msg)) {
        log::info("msg::validate() type mismatch, expected({}), actual({})",
          (int)msg_id, (int)msg.msg_id);
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
#endif
} // namespace cope

#endif // INCLUDE_COPE_MSG_H