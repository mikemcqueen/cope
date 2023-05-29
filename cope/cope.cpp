#include "msvc_wall.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include <format>
#include <string>
#include "cope.h"
#include "log.h"
#include "txsetprice.h"
#include "txsellitem.h"
#include "ui_msg.h"

using namespace std::literals;

namespace dp {
  result_code dispatch(const msg_t& msg) {
    if (!msg.msg_name.starts_with("ui::msg")) {
      LogInfo(L"dispatch(): unsupported message name, %S",
        msg.msg_name.c_str());
      return result_code::e_unexpected_msg_name;
    }
    return result_code::s_ok;
  }
}

dp::msg_ptr_t start_txn_sellitem(dp::msg_ptr_t msg_ptr) {
  using namespace sellitem;
  LogInfo(L"starting txn::sell_item");
  auto state = std::make_unique<txn::state_t>("magic beans"s, 2);
  return std::move(dp::txn::make_start_txn<txn::state_t>(kTxnName,
    std::move(msg_ptr), std::move(state)));
}

auto screenshot() {
  return std::make_unique<dp::msg_t>("dp::msg::screenshot");
}

namespace state {
  using namespace sellitem::msg;

  static const data_t::row_vector const_rows_page_1{
    //{ "magic balls", 7, false, false },
    { "magic beans", 1, false, false },
#if 1
    { "magic beans", 1, false, false },
    { "magic beans", 1, true, false },
    { "magic beans", 1, false, true },
    { "magic beans", 1, true, true },

    { "magic balls", 7, false, false },

    { "magic beans", 2, false, false },
    { "magic beans", 2, true, false },
    { "magic beans", 2, false, true },
    { "magic beans", 2, true, true },

    { "magic balls", 8, false, false },
    { "magic beans", 3, false, true },
    { "magic beans", 5, true, false },
    { "magic balls", 9, false, false }
#endif
  };
  static data_t::row_vector rows_page_1;

  static bool first{ true };
  static bool in_setprice{};
  static bool setprice_clicked{};
  static bool listed_clicked{};
  static bool final_message_sent{ false };
  static auto row_index{ 0u };

  void reset() {
    first = true;
    in_setprice = false;
    setprice_clicked = false;
    listed_clicked = false;
    final_message_sent = false;
    row_index = 0u;
    rows_page_1 = const_rows_page_1;
  }
}

dp::msg_ptr_t translate(dp::msg_ptr_t /*msg_ptr*/, std::string& out_msg,
  std::string& out_extra)
{
  using namespace sellitem::msg;

  bool xtralog = true;
 
  using namespace state;

  out_msg.clear();
  out_extra.clear();
  //row_data_t prev_row = rows_page_1[row_index];
  for (; row_index < rows_page_1.size(); ++row_index,
    first = true,
    setprice_clicked = false,
    in_setprice = false,
    listed_clicked = false)
  {
    auto& row = rows_page_1[row_index];

    if (row.item_name != "magic beans") continue;
    if (row.item_price == 2 && row.item_listed) continue;

    if (first) LogInfo(L"---ROW %d--- selected: %d, listed: %d",
      row_index, row.selected, row.item_listed);

    if (!row.selected) {
      if (first) {
        out_msg.assign(ui::msg::name::click_table_row);
        if (xtralog) LogInfo(L"****1 ");
        break;
      }
      row.selected = true;
    }

    if (row.item_price != 2 && !setprice_clicked) {
      out_msg.assign(ui::msg::name::click_widget); // click set_price_button
      //out_extra.assign(std::to_string(eq2::broker::sell_tab::widget::id::SetPriceButton));
      setprice_clicked = true;
      if (xtralog) LogInfo(L"****2 ");
      break;
    }

    if (row.item_price != 2) {
      if (!in_setprice) {
        in_setprice = true;
        out_msg.assign(ui::msg::name::send_chars); // enter price_text
        if (xtralog) LogInfo(L"****3  price(%d)", row.item_price);
        break;
      }
      out_msg.assign(ui::msg::name::click_widget); // click ok_button
      //out_extra.assign(std::to_string(eq2::broker::set_price_popup::widget::id::OkButton));
      if (xtralog) LogInfo(L"****4  price(%d) row(%d)", row.item_price,
        row_index);
      row.item_price = 2;
      break;
    }
    in_setprice = false;
    if (!row.item_listed) {
      if (!listed_clicked) {
        listed_clicked = true;
        out_msg.assign(ui::msg::name::click_widget); // click list_item_button
        //out_extra.assign(std::to_string(eq2::broker::sell_tab::widget::id::ListItemButton));
        if (xtralog) LogInfo(L"****5 ");
        break;
      }
      row.item_listed = true;
      //if (xtralog) log("****6 ");
      //out_msg.assign("skip");
      //break;
    }
  }
  first = false;

  if (row_index == rows_page_1.size()) {
    if (final_message_sent) {
      final_message_sent = false;
      return std::make_unique<dp::msg_t>("done");
    } else {
      final_message_sent = true;
    }
  } else if (in_setprice) {
    return std::make_unique<setprice::msg::data_t>(rows_page_1[row_index]
      .item_price);
  }
  //unnecessary. pass & copy directly below/ (no move)
  data_t::row_vector rows_copy = rows_page_1;
  return std::make_unique<data_t>(std::move(rows_copy));
}

int main()
{
  SetLogLevel(LogLevel::none);
  using namespace std::chrono;

  dp::txn::handler_t tx_sell{ sellitem::txn::handler() };
  dp::msg_ptr_t out{};

//  int max = 1; 0'000;
//  if (max > 1) logging_enabled = false;
  auto start = high_resolution_clock::now();

  auto total_frames{ 0 };
  auto max_iter{ 10'000 };
  auto iter{ 0 };
  for (; iter < max_iter; ++iter) {
    state::reset();
    int frame{};
    for (; true; frame++) {
      std::string expected_out_msg_name;
      std::string extra;

      dp::msg_ptr_t out_ptr = translate(screenshot(), expected_out_msg_name, extra);
      if (out_ptr->msg_name == "done") break;
      if (!tx_sell.promise().txn_started()) {
        out_ptr = std::move(start_txn_sellitem(std::move(out_ptr)));
      }
      out = tx_sell.send_value(std::move(out_ptr));
      if (!expected_out_msg_name.empty()) {
        LogInfo(L"expected out msg_name: %S %S", expected_out_msg_name.c_str(),
          extra.c_str());
      }
      if (out && expected_out_msg_name.starts_with("ui::")) {
        assert(out->msg_name == expected_out_msg_name);
        dp::dispatch(*out.get());
      }
      else {
        assert(expected_out_msg_name.empty());
      }
    }
    total_frames += frame;
  }
  auto end = std::chrono::high_resolution_clock::now();
  auto elapsed = 1e-6 * (double)duration_cast<nanoseconds>(end - start).count();

  std::cerr << "Elapsed: " << std::fixed << elapsed << std::setprecision(9)
    << "ms, (" << iter << " iters, " << total_frames << " frames)" << std::endl;

  return 0;
}

