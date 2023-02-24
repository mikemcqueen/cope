#include <chrono>
#include <iostream>
#include <format>
#include "cope.h"
#include "dp.h"
#include "txsetprice.h"
#include "txsellitem.h"

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

  const sellitem::msg::data_t::row_vector rows_page_1{
    { "magic beans", 1, false, false },
    /*
    { "magic beans", 2, false, false },
    { "magic beans", 2, false, true },
    { "magic beans", 2, false, true },
    { "magic beans", 3, false, false },
    { "magic beans", 5, false, false },
    { "magic balls", 7, false, false },
    { "magic balls", 9, false, false }
    */
  };

  dp::txn::handler_t tx_sell{ sellitem::txn::handler() };
  start_txn_sellitem(tx_sell, rows_page_1);

  int max = 1; 0'000;
  if (max > 1) logging_enabled = false;
  auto start = high_resolution_clock::now();
  for (auto i = 0; i < max; ++i) {
    log("---");

    sellitem::msg::data_t::row_vector rows_copy = rows_page_1;
    rows_copy[0].selected = true;
    auto m0 = std::make_unique<sellitem::msg::data_t>(std::move(rows_copy));
    auto r0 = tx_sell.send_value(std::move(m0));

    auto m1 = std::make_unique<setprice::msg::data_t>(1);
    auto r1 = tx_sell.send_value(std::move(m1));

    auto m2 = std::make_unique<setprice::msg::data_t>(2);
    auto r2 = tx_sell.send_value(std::move(m2));

  }
  auto end = std::chrono::high_resolution_clock::now();
  double elapsed = 1e-9 * duration_cast<nanoseconds>(end - start).count();

  std::cerr << "Elapsed: " << std::fixed << elapsed << std::setprecision(9)
    << " sec" << std::endl;

  return 0;
}