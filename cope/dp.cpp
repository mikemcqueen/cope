#include <format>
#include <memory>
#include "dp.h"
#include "log.h"

namespace dp {
  namespace msg {
    //using txn::result_code;

    auto validate_name(const msg_t& msg, std::string_view name) -> result_code {
      result_code rc = result_code::success;
      if (msg.msg_name != name) {
        log(std::format("msg::validate_name: name mismatch, expected({}), actual({})",
          name, msg.msg_name).c_str());
        rc = result_code::unexpected_error;
      }
      return rc;
    }

/*
    template<typename T>
    result_code validate(const msg_t& msg, std::string_view name) {
*/
  } // namespace msg

  namespace txn {
    complete_txn_awaitable complete(promise_type& promise, msg_ptr_t msg_ptr) {
      return complete_txn_awaitable{
        promise.txn_name(),
        promise.prev_handle(),
        std::move(msg_ptr)
      };
    }

    complete_txn_awaitable complete(promise_type& promise) {
      return complete(promise, promise.in_ptr());
    }

    complete_txn_awaitable complete(promise_type& promise, result_code rc) {
      if (rc != result_code::success) {
        log(std::format("{}::complete() error: {}", promise.txn_name(), (int)rc).c_str());
        return complete(promise, std::move(
          std::make_unique<dp::txn::complete_t>(promise.txn_name(), rc)));
      }
      else {
        return complete(promise);
      }
    }
  } // namespace txn
} // namespace dp
