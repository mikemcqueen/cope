#pragma once

#include "cope.h"
#include "txsellitem.h"
#include "txsetprice.h"
#include "internal/cope_log.h"


namespace visit {
  namespace log = cope::log;

struct type_name {
  void operator()(const sellitem::txn::type_bundle_t::start_txn_t&) const {
    log::info("sellitem::txn");
  }
  void operator()(const sellitem::msg::data_t&) const {
    log::info("sellitem::msg");
  }
  void operator()(const setprice::txn::type_bundle_t::start_txn_t&) const {
    log::info("setprice::txn");
  }
  void operator()(const setprice::msg::data_t&) const {
    log::info("setprice::msg");
  }
};
}

namespace app {
  using context_t = cope::txn::context_t<sellitem::txn::type_bundle_t, setprice::txn::type_bundle_t>;
  using task_t = cope::txn::task_t<context_t>;

  template<typename Variant>
  inline auto get_type_name(const Variant& var) {
    auto describe = [&var](auto&& arg) -> std::string {
      using T = std::decay_t<decltype(arg)>;
      if constexpr (std::is_same_v<T, sellitem::txn::type_bundle_t::start_txn_t>) {
        return "sellitem::txn";
      } else if constexpr (std::is_same_v<T, sellitem::msg::data_t>) {
        return "sellitem::msg";
        //auto& msg = std::get<sellitem::msg::data_t>(var);
        //return std::format("sellitem::msg {} ({})", msg.item_name, msg.price);
      } else if constexpr (std::is_same_v<T, setprice::txn::type_bundle_t::start_txn_t>) {
        return "setprice::txn";
      } else if constexpr (std::is_same_v<T, setprice::msg::data_t>) {
        auto& msg = std::get<setprice::msg::data_t>(var);
        return std::format("setprice::msg ({})", msg.price);
      } else {
        return "unknown";
      }
    };
    return std::visit(describe, var);
  }
}

namespace sellitem::txn {
  extern template auto handler<app::context_t>(app::context_t&, cope::txn::id_t) -> app::task_t;
}

namespace setprice::txn {
  extern template auto handler<app::context_t>(app::context_t&, cope::txn::id_t) -> app::task_t;
}
