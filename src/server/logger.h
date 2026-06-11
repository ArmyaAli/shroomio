#ifndef SHROOM_LOGGER_H
#define SHROOM_LOGGER_H

#include <stdarg.h>

typedef enum LogLevel {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_FATAL
} LogLevel;

void LoggerInit(LogLevel min_level, int use_color);
void LoggerSetLevel(LogLevel level);
void LoggerLog(LogLevel level, const char *file, int line, const char *fmt, ...);

#define LOG_DEBUG(...) LoggerLog(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...) LoggerLog(LOG_LEVEL_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...) LoggerLog(LOG_LEVEL_WARN, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) LoggerLog(LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_FATAL(...) LoggerLog(LOG_LEVEL_FATAL, __FILE__, __LINE__, __VA_ARGS__)

#endif
