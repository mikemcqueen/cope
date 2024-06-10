// sellitems.cpp

#include "msvc_wall.h"
#include <cassert>
#include <chrono>
#include <format>
#include <iostream>
#include <string>
#include <variant>
#include "cope.h"
#include "handlers.h"
#include "sellitem_coord.h"
#include "ui_msg.h"
#include "internal/cope_log.h"

using namespace std::literals;

namespace {
  namespace log = cope::log;

  namespace state {
    using namespace sellitem::msg;

    const data_t::row_vector const_rows_page_1{
      { "magic balls", 7, false, false }, // 0
      { "magic beans", 1, false, false },
      { "magic beans", 1, false, false },
      { "magic beans", 1, true, false },
      { "magic beans", 1, false, true },
      { "magic beans", 1, true, true },   // 5
      { "magic balls", 7, false, false },
      { "magic beans", 2, false, false },
      { "magic beans", 2, true, false },
      { "magic beans", 2, false, true },
      { "magic beans", 2, true, true },   // 10
      { "magic balls", 8, false, false },
      { "magic beans", 3, false, true },
      { "magic beans", 5, true, false },
      { "magic balls", 9, false, false }  // 14
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

    bool xtralog{};

    out_msg = -1; //cope::msg::id::kUndefined;
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

      if (first) {
        log::info("---ROW {}--- selected: {}, listed: {}", row_index, row.selected,
          row.item_listed);
      }

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
    if (in_setprice) {
      result = rows_page_1[row_index].item_price;
    } else {
      if (row_index < rows_page_1.size() || !final_message_sent) {
        // unnecessary. pass & copy directly below w/no move
        data_t::row_vector rows_copy = rows_page_1;
        result = rows_copy;
      }
      if (row_index == rows_page_1.size()) {
        final_message_sent = !final_message_sent;
      }
    }
    return result;
  }

  constexpr inline auto dispatch = [](const auto& msg) {
    using namespace cope;
    using namespace ui::msg;
    auto msg_name = get_type_name(msg);
    if (!msg_name.has_value()) {
      log::info("dispatch: unsupported message");
      return result_code::e_unexpected_msg_type;
    }
    log::info("dispatching {}...", msg_name.value());
    return result_code::s_ok;
  };

  auto run(sellitem::txn::task_t<app::context_t>& task) {
    assert(task.promise().txn_ready());
    state::reset();
    int frame_count{};
    while (true) {
      cope::msg::id_t expected_out_msg_id;
      std::string extra;
      auto var = get_data(expected_out_msg_id, extra);
      // TODO get_data can do this
      std::variant<sellitem::msg::data_t, setprice::msg::data_t,
          sellitem::msg::start_txn_t> v2;
      using namespace sellitem;
      if (std::holds_alternative<msg::data_t::row_vector>(var)) {
        auto& rows = std::get<msg::data_t::row_vector>(var);
        auto msg = msg::data_t{ std::move(rows) };
        if (!task.promise().txn_running()) {
          // todo: 2-param constructor?  check c++ is trivial cppnow jason turner 2024
          auto state = txn::make_state("magic beans"s, 2);
          v2 = msg::start_txn_t{ std::move(msg), std::move(state) };
        } else {
          v2 = msg;
        }
      } else {
        assert(std::holds_alternative<int>(var));
        v2 = setprice::msg::data_t{ std::get<int>(var) };
      }
      //log::info("constructed {}", app::get_type_name(v2));
      int out_msg_id{};
      std::visit([&task, &out_msg_id](auto&& msg) {
        [[maybe_unused]] const auto& var = task.send_msg(std::move(msg));
        if (task.promise().txn_running()) {
          std::visit(dispatch, var);
        }
        out_msg_id = ui::msg::get_id(var);
      }, v2);
      //assert(out_msg_id == expected_out_msg_id);
      ++frame_count;
      if (!task.promise().txn_running()) {
        assert(expected_out_msg_id == -1);
        break;
      }
    }
    return frame_count;
  }
} // namespace (anon)

int main() {
  using namespace std::chrono;
#ifndef NDEBUG
  cope::log::enable();
  int num_iter{ 1 };
#else
  int num_iter{ 100000 };
#endif
  app::get_type_name_t get_type_name{};
  app::context_t context{ get_type_name };
  //  sellitem::txn::task_t<app::context_t> task{
  sellitem::txn::task_type task{
      sellitem::txn::handler<app::context_t, sellitem::txn::coordinator_type>(
          context, sellitem::kTxnId)};
  int total_frames{};
  [[maybe_unused]] auto start = high_resolution_clock::now();
  for (int iter{}; iter < num_iter; ++iter) {
    auto frame_count = run(task);
    total_frames += frame_count;
  }
#ifdef NDEBUG
  auto end = high_resolution_clock::now();
  auto elapsed = (double)duration_cast<nanoseconds>(end - start).count();
#endif
  std::cerr << num_iter << " iters, " << total_frames << " frames"
#ifdef NDEBUG
            << std::fixed << std::setprecision(2)
            << ", Elapsed: " << elapsed * 1e-6 << "ms "
            << std::setprecision(0)
            << "(" << elapsed / total_frames << "ns/frame)"
#endif
            << std::endl;
  return 0;
}
