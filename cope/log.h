// log.h

#ifndef INCLUDE_LOG_H
#define INCLUDE_LOG_H

enum class LogLevel {
  none,
  error,
  info,
  warning
};

void LogAlways (const wchar_t* pszFormat, ...);
void LogError(const wchar_t* pszFormat, ...);
void LogWarning(const wchar_t* pszFormat, ...);
void LogInfo(const wchar_t* pszFormat, ...);

void SetLogLevel(LogLevel level);

#endif // INCLUDE_LOG_H

