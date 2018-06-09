/*
  Utility for logging outputs
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include "bt_parse.h"
#include "log.h"

#define FILENAME_LEN 13

FILE *log_file;

void setup_logging(bt_config_t *config) {
    char filename[FILENAME_LEN];

    memset(filename, 0, FILENAME_LEN);
    sprintf(filename, "peer%d.log", config->identity);

    log_file = fopen(filename, "w+");

    if (log_file == NULL) {
        LOG("Fail to open log file.\n");
    } else {
        LOG("I am peer (%d) @ %u\n\n",
                   config->identity, config->myport);
    }
}

void log_printf(const char *func, const int line, const char *format, ...) {
    va_list args;
    FILE *stream;
    va_start(args, format);

    /* writes log to stderr if log_file is NULL, or else writes to log_file */
    if (log_file == NULL) {
        stream = stderr;
    } else {
        stream = log_file;
    }

    if (func != NULL) {
        fprintf(stream, "[%s (%d)]\t", func, line);
    }

    /* writes to log file with optional arguments */
    vfprintf(stream, format, args);

    fflush(stream);
    va_end(args);
}

void app_errno(char *msg) {
    LOG("(%d) %s\n", msg, errno, strerror(errno));
}
