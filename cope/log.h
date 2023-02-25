#pragma once

#include <string>

inline static bool logging_enabled = true;

void log(const char* msg);
inline void log(const std::string& msg) { return log(msg.c_str()); }

