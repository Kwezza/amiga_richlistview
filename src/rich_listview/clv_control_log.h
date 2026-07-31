#ifndef CLV_CONTROL_LOG_H
#define CLV_CONTROL_LOG_H

/*
 * Optional crash-safe diagnostic logger for the experimental CLV control.
 * Internal use only — not part of the public clv_control API.
 *
 * Enabled only when CLV_ENABLE_LOGGING is defined at compile time.
 * The implementation object (clv_control_log.c) must be linked only then.
 */

#include <exec/types.h>

#ifdef CLV_ENABLE_LOGGING

BOOL clv_log_init(void);
VOID clv_log_shutdown(void);
VOID clv_log_write(CONST_STRPTR message);
VOID clv_log_printf(CONST_STRPTR format, ...);

#define CLV_LOG(message) \
    clv_log_write((message))

#define CLV_LOGF(...) \
    clv_log_printf(__VA_ARGS__)

#else /* !CLV_ENABLE_LOGGING */

/* do/while(0) — not ((void)0) — so VBCC does not emit warning 153. */
#define clv_log_init() \
    do { } while (0)

#define clv_log_shutdown() \
    do { } while (0)

#define CLV_LOG(message) \
    do { } while (0)

#define CLV_LOGF(...) \
    do { } while (0)

#endif /* CLV_ENABLE_LOGGING */

#endif /* CLV_CONTROL_LOG_H */
