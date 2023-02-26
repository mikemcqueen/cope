#include <cassert>
#include <chrono>
#include <iostream>
#include <format>
#include <string>
#include "cope.h"
#include "dp.h"
#include "log.h"
#include "txsetprice.h"
#include "txsellitem.h"
#include "ui_msg.h" // dispatch, or "register_dispatch"

namespace dp {
  result_code dispatch(const msg_t& msg) {
    result_code rc{ result_code::success };
    if (!msg.msg_name.starts_with("ui::msg")) {
      log(std::format("dispatch(): unsupported message name, {}", msg.msg_name));
      rc = result_code::unexpected_error;
    } else {
      rc = ui::msg::dispatch(msg);
    }
    return rc;
  }
}

dp::msg_ptr_t start_txn_sellitem(dp::txn::handler_t& tx_sell,
  const sellitem::msg::data_t::row_vector& rows)
{
  using namespace sellitem;

  msg::data_t::row_vector rows_copy{ rows };
  auto msg = std::make_unique<msg::data_t>(std::move(rows_copy));
  // TODO: would like to allow this and build a unique_ptr from it
  // sellitem::txn::state_t state{ "some item", 1 };
  auto state = std::make_unique<txn::state_t>("magic beans", 2);
  auto txn = dp::txn::make_start_txn<txn::state_t>(txn::name,
    std::move(msg), std::move(state));
  return std::move(tx_sell.send_value(std::move(txn)));
}

int main()
{
  using namespace std::chrono;

  sellitem::msg::data_t::row_vector rows_page_1{
//    { "magic balls", 7, false, false },

    { "magic beans", 1, false, false },
#if 1
    { "magic beans", 1, true, false },
    { "magic beans", 1, false, true },
    { "magic beans", 1, true, true },

    { "magic balls", 7, false, false },

    { "magic beans", 2, false, false },
    { "magic beans", 2, true, false},
    { "magic beans", 2, false, true },
    { "magic beans", 2, true, true },

    { "magic balls", 8, false, false },

    { "magic beans", 3, false, true },
    { "magic beans", 5, true, false },

    { "magic balls", 9, false, false }
#endif
  };

  dp::txn::handler_t tx_sell{ sellitem::txn::handler() };
  dp::msg_ptr_t out = start_txn_sellitem(tx_sell, rows_page_1);

  int max = 1; 0'000;
  if (max > 1) logging_enabled = false;
  auto start = high_resolution_clock::now();
  for (auto row_index = 0; row_index < rows_page_1.size(); ++row_index) {
    log(std::format("---ROW {}---", row_index));

    auto& row = rows_page_1[row_index];
    if (row.item_name != "magic beans") continue;
    if (row.price == 2 && row.listed) continue;

    sellitem::msg::data_t::row_vector rows_copy = rows_page_1;

    if (!row.selected) {
      assert(out && out->msg_name == std::format("click_row({})", row_index));
      //log(std::format("CLICK_ROW({})", row_index));
      dp::dispatch(*out.get());

      row.selected = true;
      rows_copy = rows_page_1;
      auto in = std::make_unique<sellitem::msg::data_t>(std::move(rows_copy));
      out = tx_sell.send_value(std::move(in));
    }

    if (row.price != 2) {
      assert(out && out->msg_name == "click_setprice");
      //log("CLICK_SETPRICE");
      dp::dispatch(*out.get());

      auto m1 = std::make_unique<setprice::msg::data_t>(row.price);
      out = tx_sell.send_value(std::move(m1));
      assert(out && out->msg_name == "ui::msg::send_chars"); // input_price
      //log("SET_WIDGET_TEXT(PriceText, 2)");
      dp::dispatch(*out.get());

      row.price = 2;
      auto m2 = std::make_unique<setprice::msg::data_t>(row.price);
      out = tx_sell.send_value(std::move(m2));

      assert(out && out->msg_name == "ui::msg::click_widget"); // click_ok
      //log("CLICK_WIDGET(OK)");
      dp::dispatch(*out.get());

      rows_copy = rows_page_1;
      auto m3 = std::make_unique<sellitem::msg::data_t>(std::move(rows_copy));
      out = tx_sell.send_value(std::move(m3));
    }

    if (!row.listed) {
      assert(out && out->msg_name == "click_listitem");
      //log("CLICK_LISTITEM");
      dp::dispatch(*out.get());

      row.listed = true;
      rows_copy = rows_page_1;
      auto in = std::make_unique<sellitem::msg::data_t>(std::move(rows_copy));
      out = tx_sell.send_value(std::move(in));
    }
  }
  auto end = std::chrono::high_resolution_clock::now();
  double elapsed = 1e-6 * duration_cast<nanoseconds>(end - start).count();

  std::cerr << "Elapsed: " << std::fixed << elapsed << std::setprecision(9)
    << "ms" << std::endl;

  return 0;
}