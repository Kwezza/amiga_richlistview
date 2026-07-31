#ifndef RLV_LOG_H
#define RLV_LOG_H

/*
 * Optional crash-safe diagnostic logger for RichListview.
 * Internal / demo-variant use only — not part of the public API.
 *
 * Enabled only when RLV_ENABLE_LOGGING is defined at compile time.
 * The implementation object (rlv_log.c) must be linked only then.
 */

#include <exec/types.h>

#ifdef RLV_ENABLE_LOGGING

BOOL rlv_log_init(void);
VOID rlv_log_shutdown(void);
VOID rlv_log_write(CONST_STRPTR message);
VOID rlv_log_printf(CONST_STRPTR format, ...);

#define RLV_LOG(message) \
    rlv_log_write((message))

#define RLV_LOGF(...) \
    rlv_log_printf(__VA_ARGS__)

#else /* !RLV_ENABLE_LOGGING */

/* do/while(0) — not ((void)0) — so VBCC does not emit warning 153. */
#define rlv_log_init() \
    do { } while (0)

#define rlv_log_shutdown() \
    do { } while (0)

#define RLV_LOG(message) \
    do { } while (0)

#define RLV_LOGF(...) \
    do { } while (0)

#endif /* RLV_ENABLE_LOGGING */

#endif /* RLV_LOG_H */
