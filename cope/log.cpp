// Log.cpp

#include "msvc_wall.h"
#include <cstdarg>
#include <cstdio>
#include <stdexcept>
#include "Log.h"

LogLevel log_level = LogLevel::none;

void SetLogLevel(LogLevel level) { 
  log_level = level;
}

void LogString(const wchar_t* pszBuf) {
  wprintf(L"%ls", pszBuf);
#if 0
  if (Log::Output::Debug & m_output) {
    OutputDebugString(pszBuf);
  }
#endif
}

int InitLogBuffer(
  wchar_t* pszBuf,
  size_t   CchBuffer,
  const wchar_t* pszType)
{
  int Count = _snwprintf_s(pszBuf, CchBuffer, _TRUNCATE, L"%ls: ",
    const_cast<wchar_t*>(pszType));
  if (Count < 0) {
    throw std::runtime_error("InitLogBuffer");
  }
  return Count;
}

void AppendNewline(wchar_t* Buf, size_t Count) {
    size_t Last = Count - 2;
    size_t Pos = 0;
    for (; Pos < Last; ++Pos)
    {
        if (L'\0' == Buf[Pos])
            break;
    }
    Buf[Pos++] = L'\n';
    Buf[Pos]   = L'\0';
}

void LogAlways(const wchar_t* pszFormat, ...) {
  wchar_t szBuf[256];
  int Len = InitLogBuffer(szBuf, _countof(szBuf), L"LOG");
  if (0 == Len) Len = 1;
  va_list marker;
  va_start(marker, pszFormat);
  _vsnwprintf_s(&szBuf[Len], _countof(szBuf) - Len, _TRUNCATE, pszFormat, marker);
  va_end(marker);
  AppendNewline(szBuf, _countof(szBuf));
  LogString(szBuf);
}

void LogInfo(const wchar_t* pszFormat, ...) {
  if (log_level < LogLevel::info) return;
  wchar_t szBuf[256];
  int Len = InitLogBuffer(szBuf, _countof(szBuf), L"INF");
  va_list marker;
  va_start(marker, pszFormat);
  _vsnwprintf_s(&szBuf[Len], _countof(szBuf) - Len, _TRUNCATE, pszFormat, marker);
  va_end(marker);
  AppendNewline(szBuf, _countof(szBuf));
  LogString(szBuf);
}

void LogWarning(const wchar_t* pszFormat, ...) {
  if (log_level < LogLevel::warning) return;
  wchar_t szBuf[256];
  int Len = InitLogBuffer(szBuf, _countof(szBuf), L"WRN");
  va_list marker;
  va_start(marker, pszFormat);
  _vsnwprintf_s(&szBuf[Len], _countof(szBuf) - Len, _TRUNCATE, pszFormat, marker);
  va_end(marker);
  AppendNewline(szBuf, _countof(szBuf));
  LogString(szBuf);
}

void LogError(const wchar_t* pszFormat, ...) {
  if (log_level < LogLevel::error) return;
  wchar_t szBuf[256];
  int Len = InitLogBuffer(szBuf, _countof(szBuf), L"ERR");
  va_list marker;
  va_start(marker, pszFormat);
  _vsnwprintf_s(&szBuf[Len], _countof(szBuf) - Len, _TRUNCATE, pszFormat, marker);
  va_end(marker);
  AppendNewline(szBuf, _countof(szBuf));
  LogString(szBuf);
}

