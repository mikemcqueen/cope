#pragma once

#include <expected>
#include "../cope_result.h"

namespace cope {
  enum class operation : int {
    yield,
    await,
    complete
  };

  using expected_operation = std::expected<operation, result_t>;
}  // namespace cope
