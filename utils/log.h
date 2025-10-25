#ifndef __LOG_H__
#define __LOG_H__

#include <stdio.h>
#include <time.h>

// 日志级别
typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_FATAL
} log_level_t;

// 日志颜色
#define COLOR_DEBUG   "\033[36m"  // 青色
#define COLOR_INFO    "\033[32m"  // 绿色  
#define COLOR_WARN    "\033[33m"  // 黄色
#define COLOR_ERROR   "\033[31m"  // 红色
#define COLOR_FATAL   "\033[35m"  // 紫色
#define COLOR_RESET   "\033[0m"   // 重置

// 获取当前时间字符串
static inline void get_time_str(char *buf, int len) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  strftime(buf, len, "%Y-%m-%d %H:%M:%S", t);
}

// 日志宏定义
#define LOG(level, fmt, ...) do { \
  char time_str[32]; \
  get_time_str(time_str, sizeof(time_str)); \
  const char *color = ""; \
  const char *level_str = ""; \
  switch(level) { \
    case LOG_LEVEL_DEBUG: color = COLOR_DEBUG; level_str = "DEBUG"; break; \
    case LOG_LEVEL_INFO:  color = COLOR_INFO;  level_str = "INFO";  break; \
    case LOG_LEVEL_WARN:  color = COLOR_WARN;  level_str = "WARN";  break; \
    case LOG_LEVEL_ERROR: color = COLOR_ERROR; level_str = "ERROR"; break; \
    case LOG_LEVEL_FATAL: color = COLOR_FATAL; level_str = "FATAL"; break; \
  } \
  printf("%s[%s]%s %s[%s:%d %s]%s " fmt, \
         color, level_str, COLOR_RESET, \
         COLOR_DEBUG, __FILE__, __LINE__, __func__, COLOR_RESET, \
         ##__VA_ARGS__); \
} while(0)

// 便捷日志宏
#define LOG_DEBUG(fmt, ...) LOG(LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  LOG(LOG_LEVEL_INFO,  fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  LOG(LOG_LEVEL_WARN,  fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) LOG(LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...) LOG(LOG_LEVEL_FATAL, fmt, ##__VA_ARGS__)

#endif // __LOG_H__