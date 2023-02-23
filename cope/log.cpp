#include <cstdio>
#include "log.h"

void log(const char* msg) {
  if (logging_enabled) puts(msg);
}
