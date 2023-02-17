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
    }

    struct state_t {
      int retry_count = 0;
      int retry_max = 3;
    };

    // consider templatizing this (or start_t) with State, and specializing for void (no state)
    struct data_t : Message::Data_t {
      data_t(std::string_view msgName, std::string_view txnName) :
        Message::Data_t(msgName), txn_name(txnName) {}

      std::string txn_name;
    };

    template<typename State>
    struct start_t : data_t {
      constexpr start_t(std::string_view txn_name, State st) :
        data_t(name::start, txn_name),
        state(st) {}

      // should probably be a unique ptr and std::move it around
      State state;
    };
  }

  constexpr auto is_txn_message(const Message::Data_t& msg) noexcept {
    return msg.msg_name.starts_with("txn");
  }

  constexpr auto is_start_txn(const Message::Data_t& msg) {
    return msg.msg_name == txn::name::start;
  }

  constexpr auto get_txn_name(const Message::Data_t& msg) {
    // is this even safe? read up
    return dynamic_cast<const txn::data_t&>(msg).txn_name;
  }

}