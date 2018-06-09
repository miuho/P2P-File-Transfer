/*
  Utility for logging outputs
*/
#ifndef LOG_H
#define LOG_H

#include "bt_parse.h"

#ifdef ENABLE_LOG
#define LOG(...) \
    do { log_printf(__func__, __LINE__, __VA_ARGS__); } while(0)
#else
#define LOG(...)
#endif

#define GRAPH(...) \
    do { log_printf(NULL, 0, __VA_ARGS__); } while(0)

/*
  Open the log file for reading
*/
void setup_logging(bt_config_t *config);

/*
  Output to log file similar to printf family of functions
*/
void log_printf(const char *func, const int line, const char *format, ...);

/* help print errno code and message text */
void app_errno(char *msg);

#endif
