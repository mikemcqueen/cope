#include <format>
#include <memory>
#include "dp.h"
#include "log.h"

namespace dp::txn {
  complete_txn_awaitable complete(promise_type& promise, msg_ptr_t msg_ptr) {
    return complete_txn_awaitable{
      promise.txn_name(),
      promise.prev_handle().value(),
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
} // namespace dp::txn
