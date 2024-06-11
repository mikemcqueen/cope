#pragma once

#include "txsellitem.h"
#include "txsetprice.h"
#include "sellitem_coord.h"
#include "internal/cope_log.h"
#include "cope.h"

/*
namespace visit {
  namespace log = cope::log;

  struct type_name {
    void operator()(const sellitem::msg::start_txn_t&) const {
      log::info("sellitem::txn");
    }
    void operator()(const sellitem::msg::data_t&) const {
      log::info("sellitem::msg");
    }
    void operator()(const setprice::msg::start_txn_t&) const {
      log::info("setprice::txn");
    }
    void operator()(const setprice::msg::data_t&) const {
      log::info("setprice::msg");
    }
  };
} // namespace visit
*/

namespace app {
  struct get_type_name_t {
    auto operator()(const auto& msg) const -> std::string {
      auto ui_type_name = ui::msg::get_type_name(msg);
      if (ui_type_name.has_value()) {
        return ui_type_name.value();
      }
      using T = std::decay_t<decltype(msg)>;
      if constexpr (std::is_same_v<T, sellitem::msg::start_txn_t>) {
        return "sellitem::start_txn";
      } else if constexpr (std::is_same_v<T, sellitem::msg::data_t>) {
        //return "sellitem::msg";
        return std::format("sellitem::msg (rows: {})",  msg.rows.size());
      } else if constexpr (std::is_same_v<T, setprice::msg::start_txn_t>) {
        return "setprice::start_txn";
      } else if constexpr (std::is_same_v<T, setprice::msg::data_t>) {
        //return "setprice::msg";
        return std::format("setprice::msg ({})", msg.price);
      } else if constexpr (std::is_same_v<T, std::monostate>) {
        return "empty_msg";
      } else {
        return "unknown_msg";
      }
    }
  };

  /*
  template<typename Variant>
  inline auto get_type_name(const Variant& var) {
    return std::visit(fn, var);
  }
  */

  using type_bundle_t =
      cope::msg::type_bundle_t<sellitem::msg::types, setprice::msg::types>;
  using context_t = cope::txn::context_t<type_bundle_t, get_type_name_t>;
}


namespace setprice::txn {
  extern template auto handler<app::context_t>(app::context_t&, cope::txn::id_t)
      -> task_t<app::context_t>;
}

namespace sellitem::txn {
  using task_type = no_context_task_t<app::context_t>;
  using coordinator_type = coordinator_t<app::context_t>;
  using start_setprice_txn = setprice::txn::start_awaiter<app::context_t>;

  // TODO: i think i actually need this specialization and can't just do
  // if constexpr (is_same_v<T, start_setprice_txn>()) because of
  // app::context_tt.. it's the context_t requirement that makes this
  //necessary (and messy)
  template <>
  template <>
  inline auto coordinator_type::get_awaiter(app::context_t& context,
      const sellitem::txn::state_t& state, start_setprice_txn& awaiter) {
    auto& setprice_msg = std::get<setprice::msg::data_t>(context.in());
    awaiter = std::move(setprice::txn::start(
        setprice_task, std::move(setprice_msg), state.item_price));
  }
}
