// sellitems.cpp

#include "msvc_wall.h"
#include <cassert>
#include <chrono>
#include <format>
#include <iostream>
#include <string>
#include <variant>
#include "cope.h"
#include "txsellitem.h"
#include "txsetprice.h"
#include "ui_msg.h"
#include "internal/cope_log.h"

using namespace std::literals;

namespace ui {
  auto dispatch(const sellitem::msg_base_t& msg) {
    using namespace cope;
    using namespace ui::msg;
    if ((msg.msg_id < id::kFirst) || (msg.msg_id > id::kLast)) {
      log::info("dispatch(): unsupported message id, {}", msg.msg_id);
      return result_code::e_unexpected_msg_id;
    }
    // actual dispatch of message (e.g. to click a button) would go here...
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

  auto get_data(cope::msg::id_t& out_msg, std::string& out_extra) {
    using namespace sellitem::msg;
    using namespace state;

    bool xtralog = false;

    out_msg = cope::msg::id::kUndefined;
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
          out_msg = ui::msg::id::kClickTableRow;
          if (xtralog) log::info("****1 ");
          break;
        }
        row.selected = true;
      }

      if (row.item_price != 2 && !setprice_clicked) {
        out_msg = ui::msg::id::kClickWidget; // click setprice_button
        setprice_clicked = true;
        if (xtralog) log::info("****2 ");
        break;
      }

      if (row.item_price != 2) {
        if (!in_setprice) {
          in_setprice = true;
          out_msg = ui::msg::id::kSendChars; // enter price_text
          if (xtralog) log::info("****3  price({})", row.item_price);
          break;
        }
        out_msg = ui::msg::id::kClickWidget; // click ok_button
        if (xtralog) log::info("****4  price({}) row({})", row.item_price,
          row_index);
        row.item_price = 2;
        break;
      }
      in_setprice = false;
      if (!row.item_listed) {
        if (!listed_clicked) {
          listed_clicked = true;
          out_msg = ui::msg::id::kClickWidget; // click list_item_button
          if (xtralog) log::info("****5 ");
          break;
        }
        row.item_listed = true;
      }
    }
    first = false;

    std::variant<std::monostate, data_t::row_vector, int> result{};
    if (row_index == rows_page_1.size()) {
      if (final_message_sent) {
        final_message_sent = false;
        // result = std::monostate;
      }
      else {
        final_message_sent = true;
      }
    }
    else if (in_setprice) {
      result = rows_page_1[row_index].item_price;
    }
    else {
      // unnecessary. pass & copy directly below w/no move
      data_t::row_vector rows_copy = rows_page_1;
      result = rows_copy;
    }
    return result;
  } // namespace (anonymous)

  auto run(sellitem::txn::handler_t& handler) {
    assert(handler.promise().txn_ready());
    state::reset();
    int frame_count{};
    while (true) {
      cope::msg::id_t actual_out_msg_id;
      cope::msg::id_t expected_out_msg_id;
      std::string extra;
      auto var = get_data(expected_out_msg_id, extra);
      using namespace sellitem;
      // TODO: get_data can do this
      std::variant<txn::msg_proxy_t, setprice::msg::proxy_t> v2;
      if (std::holds_alternative<msg::data_t::row_vector>(var)) {
        auto& rows = std::get<msg::data_t::row_vector>(var);
        auto msg = msg::data_t{ std::move(rows) };
        auto proxy = txn::msg_proxy_t{ std::move(msg) };
        v2 = std::move(proxy);
      }
      else {
        assert(std::holds_alternative<int>(var));
        auto msg = setprice::msg::data_t{ std::get<int>(var) };
        auto proxy = setprice::txn::msg_proxy_t{ msg };
        v2 = std::move(proxy);
      }
      if (!handler.promise().txn_running()) {
        // msg_ptr = std::move(txn::make_start_txn(std::move(in_ptr),
        //   "magic beans", 2));
        auto txn = txn::start_t{ kTxnId, 
          std::holds_alternative<>(v2) ?
          std::get<>(v2) : std::get<setprice::msg_proxy_t&>(v2),
          txn::state_proxy_t{ "magic beans", 2} };
        const auto& out_msg = handler.send_msg(txn::start_proxy_t{ txn });
        actual_out_msg_id = out_msg.msg_id;
      }
      else {
        const auto& out_msg = handler.send_msg(std::holds_alternative<msg_proxy_t>(v2) ?
          std::get<msg_proxy_t&>(v2) : std::get<setprice::msg_proxy_t&>(v2));
        actual_out_msg_id = out_msg.msg_id;
      }
      ++frame_count;
      if (!handler.promise().txn_running()) {
        assert(expected_out_msg_id == cope::msg::id::kUndefined);
        break;
      }
      assert(actual_out_msg_id == expected_out_msg_id);
      //ui::dispatch(out_msg);
    }
    return frame_count;
  }
} // namespace (anon)

int main() {
  cope::log::enable();
  sellitem::txn::handler_t tx_sell{ sellitem::txn::handler() };
  using namespace std::chrono;
  auto start = high_resolution_clock::now();
  auto total_frames{ 0 };
  auto num_iter{ 1 };
  for (int iter{}; iter < num_iter; ++iter) {
    auto frame_count = run(tx_sell);
    total_frames += frame_count;
  }
  auto end = high_resolution_clock::now();
  auto elapsed = 1e-6 * (double)duration_cast<nanoseconds>(end - start).count();
  std::cerr << "Elapsed: " << std::fixed << elapsed << std::setprecision(9)
    << "ms, (" << num_iter << " iters, " << total_frames << " frames)"
    << std::endl;
  return 0;
}