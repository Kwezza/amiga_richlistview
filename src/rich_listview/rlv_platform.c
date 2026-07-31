/**
 * Minimal platform allocation shim for RichListview.
 *
 * Backend:
 *   - Host / non-Amiga builds: C library malloc / free
 *   - Amiga builds: same (VBCC provides ANSI malloc backed by AllocMem)
 *
 * Semantics (matched to WorkbenchFoundation platform.h non-tracking path):
 *   - rlv_platform_malloc(0): returns whatever malloc(0) returns (implementation-defined)
 *   - rlv_platform_free(NULL): no-op (free(NULL) is safe)
 *   - rlv_platform_strdup(NULL): returns NULL
 *   - rlv_platform_strdup failure: returns NULL (no partial copy)
 */

#include "rlv_platform_internal.h"
#include "rlv_bench_internal.h"

#include <stdlib.h>
#include <string.h>

void *rlv_platform_malloc(size_t size)
{
    RLV_BENCH_COUNT(RLV_BENCH_COUNTER_ALLOCATIONS);
    RLV_BENCH_ADD(RLV_BENCH_COUNTER_ALLOC_BYTES_REQUESTED, (ULONG)size);
    return malloc(size);
}

void rlv_platform_free(void *ptr)
{
    if (ptr != 0) {
        RLV_BENCH_COUNT(RLV_BENCH_COUNTER_FREES);
    }
    free(ptr);
}

char *rlv_platform_strdup(const char *str)
{
    char *copy;
    size_t len;

    if (str == 0) {
        return 0;
    }

    len = strlen(str) + 1;
    copy = (char *)malloc(len);
    if (copy != 0) {
        memcpy(copy, str, len);
    }

    return copy;
}
