#ifndef CLV_PLATFORM_INTERNAL_H
#define CLV_PLATFORM_INTERNAL_H

/*
 * Custom ListView internal implementation header.
 * Do not include from client application code.
 *
 * Allocation shim (clv_platform_malloc / free / strdup).
 * Distinct from public clv_platform.h (Amiga target policy).
 */
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *clv_platform_malloc(size_t size);
void clv_platform_free(void *ptr);
char *clv_platform_strdup(const char *str);

#ifdef __cplusplus
}
#endif

#endif /* CLV_PLATFORM_INTERNAL_H */

