#include <string.h>

#ifndef PA1_LOGGING_H
#define PA1_LOGGING_H

/**
 * Logs pipe event.
 *
 * @param fmt format of log message
 * @param ... message parameters
 */
void log_pipe(const char *fmt, ...);

/**
 * Logs event to stdout and log file.
 *
 * @param message a message that must be logged
 */
void log_event(const char *message);

#endif //PA1_LOGGING_H
