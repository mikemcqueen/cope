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

namespace ui {
  auto dispatch(const cope::msg_t& msg) {
    using namespace cope;
    if (!msg.msg_name.starts_with("ui::msg")) {
      log::info("dispatch(): unsupported message name, {}", msg.msg_name);
      return result_code::e_unexpected_msg_name;
    }
    // actual dispatch of message (e.g. click a button) would go here...
    return result_code::s_ok;
  }
}

namespace {
  namespace log = cope::log;

  namespace state {
    using namespace sellitem::msg;

    const data_t::row_vector const_rows_page_1{
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
    data_t::row_vector rows_page_1;

    bool first{ true };
    bool in_setprice{};
    bool setprice_clicked{};
    bool listed_clicked{};
    bool final_message_sent{ false };
    auto row_index{ 0u };

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

  cope::msg_ptr_t get_data(std::string& out_msg,
    std::string& out_extra)
  {
    using namespace sellitem::msg;
    using namespace state;

    bool xtralog = false;

    out_msg.clear();
    out_extra.clear();
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
          if (xtralog) log::info("****5 ");
          break;
        }
        row.item_listed = true;
      }
    }
    first = false;

    if (row_index == rows_page_1.size()) {
      if (final_message_sent) {
        final_message_sent = false;
        return std::make_unique<cope::msg_t>("done");
      }
      else {
        final_message_sent = true;
      }
    }
    else if (in_setprice) {
      return std::make_unique<setprice::msg::data_t>(rows_page_1[row_index]
        .item_price);
    }
    // unnecessary. pass & copy directly below w/no move
    data_t::row_vector rows_copy = rows_page_1;
    return std::make_unique<data_t>(std::move(rows_copy));
  }

  auto run(cope::txn::handler_t& handler) {
    assert(handler.promise().txn_ready());
    state::reset();
    int frame_count{};
    while (true) {
      std::string expected_out_msg_name;
      std::string extra;

      cope::msg_ptr_t in_ptr = get_data(expected_out_msg_name, extra);
      if (!handler.promise().txn_running()) {
        in_ptr = std::move(sellitem::txn::make_start_txn(std::move(in_ptr),
          "magic beans", 2));
      }
      cope::msg_ptr_t out_ptr = handler.send_msg(std::move(in_ptr));
      ++frame_count;
      if (!handler.promise().txn_running()) {
        assert(expected_out_msg_name.empty());
        break;
      }
      assert(out_ptr->msg_name == expected_out_msg_name);
      ui::dispatch(*out_ptr.get());
    }
    return frame_count;
  }
} // namespace (anon)

int main() {
  using namespace std::chrono;
  cope::log::enable();
  cope::txn::handler_t tx_sell{ sellitem::txn::handler() };
  auto total_frames{ 0 };
  auto start = high_resolution_clock::now();
  auto num_iter{ 1 }; // 0'000};
  for (int iter{}; iter < num_iter; ++iter) {
    auto frame_count = run(tx_sell);
    total_frames += frame_count;
  }
  auto end = std::chrono::high_resolution_clock::now();
  auto elapsed = 1e-6 * (double)duration_cast<nanoseconds>(end - start).count();
  std::cerr << "Elapsed: " << std::fixed << elapsed << std::setprecision(9)
    << "ms, (" << num_iter << " iters, " << total_frames << " frames)"
    << std::endl;
  return 0;
}