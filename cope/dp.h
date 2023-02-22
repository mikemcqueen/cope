#pragma once

#include <string_view>

namespace DP {
  template<size_t N>
  struct string_literal {
    constexpr string_literal(const char(&str)[N]) {
      std::copy_n(str, N, value);
    }
    char value[N];
  };

  namespace Message {
    struct Data_t {
      // These go in something like "TranslateData"
      //std::string window_name;
      //std::string type_name;
      Data_t(std::string_view n) : msg_name(n) {}
      virtual ~Data_t() {}

      std::string msg_name;
    };
  }

  using msg_ptr_t = std::unique_ptr<Message::Data_t>;

  namespace txn {
    namespace name {
      constexpr std::string_view start = "txn_start";
      constexpr std::string_view complete = "txn_complete";
    }

    struct state_t {
      state_t(const state_t& state) = delete;
      state_t& operator=(const state_t& state) = delete;

      std::string prev_msg_name;
      int retries = 3;
    };

    // consider templatizing this (or start_t) with State, and specializing for void (no state)
    struct data_t : Message::Data_t {
      data_t(std::string_view msgName, std::string_view txnName) :
        Message::Data_t(msgName), txn_name(txnName) {}

      std::string txn_name;
    };

    template<typename State>
    struct start_t : data_t {
      using state_ptr_t = std::unique_ptr<State>;
      constexpr start_t(std::string_view txn_name, msg_ptr_t m, state_ptr_t s) :
        data_t(name::start, txn_name), msg(std::move(m)), state(std::move(s)) {}

      msg_ptr_t msg;
      state_ptr_t state;
    };

    enum class result_code {
      success = 0,
      error = 1 // error_retry/retriable_error, error_?? unrecoverable_error
    };

    // todo: T, -> reqires derives_from data_t ??
    struct complete_t : data_t {
      complete_t(std::string_view txn_name, result_code rc) :
        data_t(name::complete, txn_name), code(rc) {}

      result_code code;
    };

    /*struct success_result_t : result_t {
      constexpr success_result_t(std::string_view txn_name) :
        result_t(txn_name, result_code::success) {}
    };*/
  } // namespace txn

  constexpr auto is_txn_message(const Message::Data_t& msg) noexcept {
    return msg.msg_name.starts_with("txn");
  }

  constexpr auto is_start_txn(const Message::Data_t& msg) {
    return msg.msg_name == txn::name::start;
  }

  constexpr auto is_complete_txn(const Message::Data_t& msg) {
    return msg.msg_name == txn::name::complete;
  }

  constexpr auto get_txn_name(const Message::Data_t& msg) {
    // is this even safe? read up
    return dynamic_cast<const txn::data_t&>(msg).txn_name;
  }

} // namespace DP