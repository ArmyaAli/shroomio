#include "logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

static LogLevel g_min_level = LOG_LEVEL_INFO;
static int g_use_color = 0;
static atomic_flag g_log_lock = ATOMIC_FLAG_INIT;

#define COLOR_RESET "\033[0m"
#define COLOR_DEBUG "\033[36m"
#define COLOR_INFO "\033[32m"
#define COLOR_WARN "\033[33m"
#define COLOR_ERROR "\033[31m"
#define COLOR_FATAL "\033[35m"

static const char* GetLevelString(LogLevel level) {
  switch (level) {
  case LOG_LEVEL_DEBUG:
    return "DEBUG";
  case LOG_LEVEL_INFO:
    return "INFO ";
  case LOG_LEVEL_WARN:
    return "WARN ";
  case LOG_LEVEL_ERROR:
    return "ERROR";
  case LOG_LEVEL_FATAL:
    return "FATAL";
  default:
    return "?????";
  }
}

static const char* GetLevelColor(LogLevel level) {
  switch (level) {
  case LOG_LEVEL_DEBUG:
    return COLOR_DEBUG;
  case LOG_LEVEL_INFO:
    return COLOR_INFO;
  case LOG_LEVEL_WARN:
    return COLOR_WARN;
  case LOG_LEVEL_ERROR:
    return COLOR_ERROR;
  case LOG_LEVEL_FATAL:
    return COLOR_FATAL;
  default:
    return COLOR_RESET;
  }
}

static void GetTimestamp(char* buffer, size_t size) {
#ifdef _WIN32
  SYSTEMTIME time_info;

  GetLocalTime(&time_info);
  snprintf(buffer, size, "%04u-%02u-%02u %02u:%02u:%02u.%03u", (unsigned)time_info.wYear,
           (unsigned)time_info.wMonth, (unsigned)time_info.wDay, (unsigned)time_info.wHour,
           (unsigned)time_info.wMinute, (unsigned)time_info.wSecond,
           (unsigned)time_info.wMilliseconds);
#else
  struct timespec ts;
  struct tm tm_info;

  clock_gettime(CLOCK_REALTIME, &ts);
  localtime_r(&ts.tv_sec, &tm_info);

  snprintf(buffer, size, "%04d-%02d-%02d %02d:%02d:%02d.%03ld", tm_info.tm_year + 1900,
           tm_info.tm_mon + 1, tm_info.tm_mday, tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec,
           ts.tv_nsec / 1000000L);
#endif
}

static const char* ExtractFilename(const char* path) {
  const char* slash = strrchr(path, '/');
  return slash ? slash + 1 : path;
}

void LoggerInit(LogLevel min_level, int use_color) {
  g_min_level = min_level;
  g_use_color = use_color;
  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);
}

void LoggerSetLevel(LogLevel level) { g_min_level = level; }

void LoggerLog(LogLevel level, const char* file, int line, const char* fmt, ...) {
  char timestamp[64];
  va_list args;

  if (level < g_min_level) {
    return;
  }

  while (atomic_flag_test_and_set(&g_log_lock)) {
  }

  GetTimestamp(timestamp, sizeof(timestamp));

  if (g_use_color) {
    fprintf(stderr, "%s [%s%s%s] %s:%d: ", timestamp, GetLevelColor(level), GetLevelString(level),
            COLOR_RESET, ExtractFilename(file), line);
  } else {
    fprintf(stderr, "%s [%s] %s:%d: ", timestamp, GetLevelString(level), ExtractFilename(file),
            line);
  }

  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);

  fprintf(stderr, "\n");

  atomic_flag_clear(&g_log_lock);

  if (level == LOG_LEVEL_FATAL) {
    abort();
  }
}
