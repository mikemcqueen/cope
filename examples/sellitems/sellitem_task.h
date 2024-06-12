// sellitem_task.h: hard to explain

#pragma once

#include "app.h"

namespace sellitem::txn {
  using task_type = no_context_task_t<app::context_t>;
  using coordinator_type = coordinator_t<app::context_t>;
  using start_setprice_txn = setprice::txn::start_awaiter<app::context_t>;

  // TODO: i think i actually need this specialization and can't just do
  // if constexpr (is_same_v<T, start_setprice_txn>) because of
  // app::context_t.. it's the context_t requirement that makes this
  // necessary (and messy)
  template <>
  template <>
  inline auto coordinator_type::get_awaiter(app::context_t& context,
      const state_t& state, start_setprice_txn& awaiter) {
    auto& setprice_msg = std::get<setprice::msg::data_t>(context.in());
    awaiter = std::move(setprice::txn::start(
        setprice_task, std::move(setprice_msg), state.item_price));
  }
}  // namespace sellitem::txn
