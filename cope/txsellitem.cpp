#include <optional>
#include "txsellitem.h"
#include "txsetprice.h"

namespace sellitem
{
  using dp::msg_t;
  using dp::result_code;
  using dp::txn::handler_t;
  using promise_type = handler_t::promise_type;

//  template<typename T>
  auto& get_row(const promise_type& promise, int row_index) {
    if (dp::is_start_txn(promise.in())) {
      const auto& txn = promise.in_as<txn::start_t>();
      const auto& msg = static_cast<const msg::data_t&>(txn::start_t::msg_from(txn));
      if (row_index < 0 || row_index >= msg.rows.size()) {
        // oops! TODO
        row_index = 0;
      }
      return msg.rows.at(row_index);
    } else {
      const auto& msg = promise.in_as<const msg::data_t>();
      if (row_index < 0 || row_index >= msg.rows.size()) {
        // oops! TODO
        row_index = 0;
      }
      return msg.rows.at(row_index);
    }
   }

  auto is_candidate_row(const msg::data_t& msg, const txn::state_t& state,
    int row_index)
  {
    auto& row = msg.rows[row_index];
    if (row.item_name == state.item_name) {
      if ((row.price != state.item_price) || !row.listed) return true;
    }
    return false;
  }

  auto get_candidate_row(const msg::data_t& msg, const txn::state_t& state)
    -> std::optional<int>
  {
    using result_t = std::optional<int>;
    // check selected row first, less work to do
    int selected_row = -1;
    if (msg.selected_row.has_value()) {
      selected_row = msg.selected_row.value();
      if (is_candidate_row(msg, state, selected_row)) {
        log(std::format("candidate selected row ({})", selected_row).c_str());
        return result_t(selected_row);
      }
    }
    for (auto row = 0; row < msg.rows.size(); ++row) {
      if (row == selected_row) continue;
      if (is_candidate_row(msg, state, row)) {
        log(std::format("candidate row ({})", row).c_str());
        return result_t(row);
      }
    }
    log("no (more) candidate rows");
    return std::nullopt;
  }

  auto get_candidate_row(const promise_type& promise, const txn::state_t& state)
    ->std::optional<int>
  {
      return get_candidate_row(promise.in_as<msg::data_t>(), state);
  }

  auto start_txn_setprice(handler_t::handle_t handle, promise_type& promise,
    const txn::state_t& sellitem_state)
  {
    auto setprice_state = std::make_unique<setprice::txn::state_t>(
      std::string(sellitem::msg::name), sellitem_state.item_price);
    return dp::txn::start_txn_awaitable<setprice::txn::state_t>{
      setprice::txn::name,
        handle,
        std::move(promise.in_ptr()),
        std::move(setprice_state)
    };
  }

  auto click_row(const promise_type& promise, int row_index) {
    promise;
    return std::make_unique<msg_t>(std::format("click_row({})", row_index));
  }

  auto click_setprice(const promise_type& promise, int row_index) {
    promise;
    return std::make_unique<msg_t>(std::format("click_setprice({})", row_index));
  }

  auto click_listed(const promise_type& promise, int row_index) {
    promise;
    return std::make_unique<msg_t>(std::format("click_listed({})", row_index));
  }

  namespace txn {
    auto handler() -> handler_t {
      using dp::result_code;

      result_code rc{ result_code::success };
      const auto& error = [&rc]() noexcept {
        const auto error = (rc != result_code::success);
        if (error) log(std::format("  txn::sellitem error({})", (int)rc).c_str());
        return error;
      };
      state_t state{ "magic beans", 2 };
      dp::txn::handler_t setprice_handler{ setprice::txn::handler() };

      handler_t::awaitable initial{ txn::name };
#if 0
      while (true) {
        log(std::format("sellitem::txn_handler, outer co_await").c_str());
        auto& promise = co_await initial;
        auto& msg = promise.in();
        if (msg.msg_name == setprice::msg::name) {
          auto& txn_p = co_await start_txn_setprice(setprice_handler.handle(),
            promise, state);
          // move this to awaitable?
          log(std::format("txn_result, msg_name: {}", txn_p.in().msg_name).c_str());
        }
#else
      bool first{ true };
      for (auto& promise = co_await initial; true;) {
        if (!first) {
          log(std::format("sellitem::txn_handler before txn::complete, "
            "error({})", (int)rc).c_str());
          // could put this in for(;;here) loop
          co_await dp::txn::complete(promise, rc);
        }
        first = false;

        auto& txn = promise.in();
        rc = dp::msg::validate_txn_start<txn::start_t, msg::data_t>(txn, txn::name, msg::name);
        if (error()) continue;
        state = std::move(txn::start_t::state_from(txn));
        auto& msg = static_cast<const msg::data_t&>(txn::start_t::msg_from(txn));
        auto first2 = true;
        for(auto opt_row = get_candidate_row(msg, state);
          (rc != result_code::unexpected_error) && opt_row.has_value();)
        {
          if (!first2) opt_row = get_candidate_row(promise, state);
          first2 = false;

         auto row_index = opt_row.value();
          if (auto& row = get_row(promise, row_index);  !row.selected) {
            // NOTE: promise here **may** not contain a msg::data_t, (msg does)
            co_yield click_row(promise, row_index);
            //rc = validate_row(promise, row_index, { .selected{true} });
            rc = dp::msg::validate<msg::data_t>(promise.in(), msg::name);
            if (error()) continue;
            // msg = promise.in_as<msg::data_t>();
            // row = msg->rows.at(row_index)
          }
          if (auto& row = get_row(promise, row_index); row.price != state.item_price) {
            co_yield click_setprice(promise, row_index);
            //rc = validate_row(promise, row_index, { .selected{true} });
            rc = dp::msg::validate<setprice::msg::data_t>(promise.in(), setprice::msg::name);
            if (error()) continue;

            auto& txn_p = co_await start_txn_setprice(setprice_handler.handle(),
              promise, state);
            // move this to awaitable?
            log(std::format("txn_result, msg_name: {}", txn_p.in().msg_name).c_str());
            rc = dp::msg::validate<msg::data_t>(promise.in(), msg::name);
            if (error()) continue;
          }
          if (auto& row = get_row(promise, row_index); !row.listed) {
            // co_await click_listed(promise, row_index);
            // rc = validate_row(promise, state, row_index, { .price{true}, .listed{true} );
            // if (error()) continue;
          }
        }
#endif
      }
    }
  } // namespace txn
} // namespace sellitem

