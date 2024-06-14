#pragma once

#include <iostream>
#include <string_view>

inline double iters_per_ms(int iters, double ns) {
  return (double)iters / (ns * 1e-6);
}

inline double ns_per_iter(int iters, double ns) {
  return ns / (double)iters;
}

inline void log(std::string_view name, int iters, double ns) {
  std::cerr << name << ", elapsed: " << std::fixed << std::setprecision(0)
            << ns * 1e-6 << "ms, (" << iters << " iters"
            << ", " << iters_per_ms(iters, ns) << " iters/ms"
            << ", " << ns_per_iter(iters, ns) << " ns/iter)" << std::endl;
}
