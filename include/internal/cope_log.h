// cope_log.h

#ifndef INCLUDE_COPE_LOG_H
#define INCLUDE_COPE_LOG_H

#include <iostream>
#include <format>
#include <functional>
#include <string>
#include <utility>

namespace cope::log {
  using fn_t = std::function<void(const std::string& msg)>;

  inline bool enabled = false;
  inline fn_t func = [](const std::string& msg) { std::cout << msg << std::endl; };

  inline void enable(bool enable = true) { enabled = enable; }
  inline void set_logger(auto& func) { func = func; }

  template<typename... Args>
  void info(const char* fmt, Args&&... args) {
    if (enabled) {
      func(std::vformat(fmt, std::make_format_args(std::forward<Args>(args)...)));
    }
  };
} // namespace cope::log

#endif // INCLUDE_COPE_LOG_H
