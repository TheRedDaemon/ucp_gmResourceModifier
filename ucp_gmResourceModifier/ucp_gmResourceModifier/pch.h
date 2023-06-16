// pch.h: Precompiled header file

#ifndef PCH_H
#define PCH_H

#include <ucp3.h>

#include "framework.h"

// normal enum, to allow easier transform to int
enum LogLevel : int
{
  LOG_NONE = 99, // for control stuff in the dll
  LOG_FATAL = ucp_NamedVerbosity::Verbosity_FATAL,
  LOG_ERROR = ucp_NamedVerbosity::Verbosity_ERROR,
  LOG_WARNING = ucp_NamedVerbosity::Verbosity_WARNING,
  LOG_INFO = ucp_NamedVerbosity::Verbosity_INFO,
  LOG_DEBUG = ucp_NamedVerbosity::Verbosity_1,
};


inline void Log(LogLevel level, const char* message)
{
  ucp_log(static_cast<ucp_NamedVerbosity>(level), message);
}

#endif //PCH_H