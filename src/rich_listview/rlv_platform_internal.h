#ifndef RLV_PLATFORM_INTERNAL_H
#define RLV_PLATFORM_INTERNAL_H

/*
 * RichListview internal allocation shim.
 * Do not include from client application code.
 *
 * Distinct from rlv_platform.h (Amiga target-policy assert).
 */
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *rlv_platform_malloc(size_t size);
void rlv_platform_free(void *ptr);
char *rlv_platform_strdup(const char *str);

#ifdef __cplusplus
}
#endif

#endif /* RLV_PLATFORM_INTERNAL_H */

