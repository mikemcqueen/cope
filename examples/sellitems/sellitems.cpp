// sellitems.cpp

#include "msvc_wall.h"
#include <cassert>
#include <chrono>
#include <format>
#include <iostream>
#include <string>
#include "cope.h"
#include "txsellitem.h"
#include "txsetprice.h"
#include "ui_msg.h"
#include "internal/cope_log.h"

using namespace std::literals;
namespace dp = cope;

namespace cope {
  auto dispatch(const msg_t& msg) {
    if (!msg.msg_name.starts_with("ui::msg")) {
      log::info("dispatch(): unsupported message name, {}", msg.msg_name);
      return result_code::e_unexpected_msg_name;
    }
    // actual dispatch of message would go here...
    return result_code::s_ok;
  }
}

namespace sellitems {

  namespace log = cope::log;

  auto start_txn_sellitem(dp::msg_ptr_t msg_ptr) {
    using namespace sellitem;
    log::info("starting txn::sell_item");
    auto state = std::make_unique<txn::state_t>("magic beans"s, 2);
    return dp::txn::make_start_txn<txn::state_t>(kTxnName,
      std::move(msg_ptr), std::move(state));
  }

  auto screenshot() {
    return std::make_unique<dp::msg_t>("msg::screenshot");
  }

  namespace state {
    using namespace sellitem::msg;

    static const data_t::row_vector const_rows_page_1{
      { "magic balls", 7, false, false },
      { "magic beans", 1, false, false },
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

      if (first) log::info("---ROW {}--- selected: {}, listed: {}",
        row_index, row.selected, row.item_listed);

      if (!row.selected) {
        if (first) {
          out_msg.assign(ui::msg::name::click_table_row);
          if (xtralog) log::info("****1 ");
          break;
        }
        row.selected = true;
      }

      if (row.item_price != 2 && !setprice_clicked) {
        out_msg.assign(ui::msg::name::click_widget); // click set_price_button
        //out_extra.assign(std::to_string(eq2::broker::sell_tab::widget::id::SetPriceButton));
        setprice_clicked = true;
        if (xtralog) log::info("****2 ");
        break;
      }

      if (row.item_price != 2) {
        if (!in_setprice) {
          in_setprice = true;
          out_msg.assign(ui::msg::name::send_chars); // enter price_text
          if (xtralog) log::info("****3  price({})", row.item_price);
          break;
        }
        out_msg.assign(ui::msg::name::click_widget); // click ok_button
        //out_extra.assign(std::to_string(eq2::broker::set_price_popup::widget::id::OkButton));
        if (xtralog) log::info("****4  price({}) row({})", row.item_price,
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
          if (xtralog) log::info("****5 ");
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
      }
      else {
        final_message_sent = true;
      }
    }
    else if (in_setprice) {
      return std::make_unique<setprice::msg::data_t>(rows_page_1[row_index]
        .item_price);
    }
    //unnecessary. pass & copy directly below/ (no move)
    data_t::row_vector rows_copy = rows_page_1;
    return std::make_unique<data_t>(std::move(rows_copy));
  }
} // namespace sellitems

int main()
{
  cope::log::enable();

  using namespace sellitems;
  using namespace std::chrono;

  dp::txn::handler_t tx_sell{ sellitem::txn::handler() };

  auto start = high_resolution_clock::now();

  auto total_frames{ 0 };
  auto max_iter{ 1 }; // 0'000};
  auto iter{ 0 };
  for (; iter < max_iter; ++iter) {
    state::reset();
    int frame{};
    for (; true; frame++) {
      std::string expected_out_msg_name;
      std::string extra;

      dp::msg_ptr_t in_ptr = translate(screenshot(), expected_out_msg_name,
        extra);
      if (in_ptr->msg_name == "done") break;
      if (!tx_sell.promise().txn_started()) {
        in_ptr = std::move(start_txn_sellitem(std::move(in_ptr)));
      }
      dp::msg_ptr_t out_ptr = tx_sell.send_value(std::move(in_ptr));
      if (out_ptr && expected_out_msg_name.starts_with("ui::")) {
        assert(out_ptr->msg_name == expected_out_msg_name);
        dp::dispatch(*out_ptr.get());
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