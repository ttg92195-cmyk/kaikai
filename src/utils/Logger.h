#pragma once

#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <cstring>

namespace Kaikai {

// ---------------------------------------------------------------------------
// Internal formatting helpers
// ---------------------------------------------------------------------------

inline void logFormat(const char* level, const char* file, int line, const char* fmt, ...)
{
    // Timestamp
    std::time_t now = std::time(nullptr);
    char timeBuf[32];
    std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));

    // Extract just the filename from the full path (avoid noisy paths in log)
    const char* shortFile = std::strrchr(file, '/');
    shortFile = shortFile ? shortFile + 1 : file;

    // Print prefix: [TIME] [LEVEL] file:line:
    std::fprintf(stderr, "[%s] [%s] %s:%d: ", timeBuf, level, shortFile, line);

    // Print user message
    va_list args;
    va_start(args, fmt);
    std::vfprintf(stderr, fmt, args);
    va_end(args);

    std::fprintf(stderr, "\n");
    std::fflush(stderr);
}

}  // namespace Kaikai

// ---------------------------------------------------------------------------
// Public macros
// ---------------------------------------------------------------------------

#define LOG_INFO(fmt, ...)  ::Kaikai::logFormat("INFO",  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  ::Kaikai::logFormat("WARN",  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) ::Kaikai::logFormat("ERROR", __FILE__, __LINE__, fmt, ##__VA_ARGS__)
