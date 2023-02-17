#pragma once

#include "dp.h"
#include <string>
#include <string_view>

namespace SetPrice {
    namespace Window {
        constexpr std::string_view Name = "SetPrice";
    }

    namespace msg {
        struct data_t : public DP::Message::Data_t {
            std::string price_text;

            data_t(std::string_view n) : DP::Message::Data_t(n) {}
        };
    }

    namespace txn {

        struct state_t : DP::Transaction::state_t {
            int item_price;
        };
        /*
        struct Data_t : public DP::Transaction::Data_t {
            State_t state;

            Data_t(std::string_view n) : DP::Transaction::Data_t(n) {}
            Data_t(Data_t&&) = default;
        };
        */
    }
}