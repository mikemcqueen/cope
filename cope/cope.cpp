#include <chrono>
#include <iostream>
#include <format>
#include "cope.h"
#include "dp.h"
#include "txsetprice.h"
#include "txsellitem.h"

int main()
{
  using namespace std::chrono;

  dp::txn::handler_t tx_sell{ sellitem::txn::handler() };

/* TODO
  sellitem::txn::state_t s1{"some item", 1};
  auto m1 = std::make_unique<sellitem::txn::start_t>(sellitem::txn::name, s1);
  // resume_with_value
  auto r1 = tx_sell.send_value(std::move(m1));
*/
  int max = 1; 0'000;
  if (max > 1) logging_enabled = false;
  auto start = high_resolution_clock::now();
  for (auto i = 0; i < max; ++i) {
    log("---");
    auto m1 = std::make_unique<setprice::msg::data_t>(1);
    auto r1 = tx_sell.send_value(std::move(m1));

    auto m2 = std::make_unique<setprice::msg::data_t>(2);
    auto r2 = tx_sell.send_value(std::move(m2));

    auto m3 = std::make_unique<sellitem::msg::data_t>();
    auto r3 = tx_sell.send_value(std::move(m3));
  }
  auto end = std::chrono::high_resolution_clock::now();
  double elapsed = 1e-9 * duration_cast<nanoseconds>(end - start).count();

  std::cerr << "Elapsed: " << std::fixed << elapsed << std::setprecision(9)
    << " sec" << std::endl;

  return 0;
}