// Log.h

#if _MSC_VER > 1000
#pragma once
#endif

#ifndef Include_LOG_H
#define Include_LOG_H

void LogAlways (const wchar_t* pszFormat, ...);
void LogError(const wchar_t* pszFormat, ...);
void LogWarning(const wchar_t* pszFormat, ...);
void LogInfo(const wchar_t* pszFormat, ...);

#endif // Include_LOG_H

