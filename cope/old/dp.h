#pragma once

#include <string_view>

namespace DP {
    namespace Message {
        struct Data_t {
            // These go in something like "TranslateData"
            //std::string window_name;
            //std::string type_name;
            std::string msg_name;

            Data_t(std::string_view n) : msg_name(n) {}
            virtual ~Data_t() {}
        };
    }
    
    template<typename T = Message::Data_t>
    constexpr auto is_txn_message(const T& msg) noexcept -> bool
    {
        if constexpr (requires{ msg.txn_name; })
            return true;
        else
            return false;
    }

    namespace Transaction {
        namespace name {
            constexpr std::string_view start = "start_txn";
        }

        constexpr bool is_start_txn(const Message::Data_t& d) {
            return d.msg_name == name::start;
        }

        struct state_t {
            int retry_count = 0;
            int retry_max = 3;
        };

        // consider templatizing this (or start_t) with State, and specializing for void (no state)
        struct Data_t : Message::Data_t {
            Data_t(std::string_view msgName, std::string_view txnName) :
              Message::Data_t(msgName), txn_name(txnName) {}

            std::string txn_name;
        };

        struct start_t : Data_t {
          start_t(std::string_view txnName) :
             Data_t(name::start, txnName) {}
        };
    }
}