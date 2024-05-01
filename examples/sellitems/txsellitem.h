#pragma once

#include <tuple>
#include "cope.h"
#include "sellitem_msg.h"
#include "setprice_msg.h"
//#include "txsetprice.h"
#include "ui_msg.h"

namespace sellitem {
  constexpr auto kTxnId{ cope::txn::make_id(1) };

  namespace txn {
    using in_msg_types = std::tuple<sellitem::msg::data_t, setprice::msg::data_t>;
    using out_msg_types = std::tuple<ui::msg::click_widget::data_t, ui::msg::click_table_row::data_t>;

    struct state_t {
      std::string item_name;
      int item_price;
    }; // sellitem::txn::state_t

    struct type_bundle_t {
      using start_txn_t = cope::msg::start_txn_t<sellitem::msg::data_t, state_t>;
      using in_tuple_t = std::tuple<start_txn_t, sellitem::msg::data_t, setprice::msg::data_t>;
      using out_tuple_t = out_msg_types;
    }; // sellitem::txn::type_bundle_t

    template<typename Context>
    auto handler(Context& /*context*/, cope::txn::id_t /*task_id*/)
      -> cope::txn::task_t<Context>;
  } // namespace txn
} // namespace sellitem
