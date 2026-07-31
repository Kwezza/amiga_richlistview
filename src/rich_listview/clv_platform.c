/**
 * Minimal platform allocation shim for amiga_custom_listview Stage 1.
 *
 * Backend:
 *   - Host / non-Amiga builds: C library malloc / free
 *   - Amiga builds: same (VBCC provides ANSI malloc backed by AllocMem)
 *
 * Semantics (matched to WorkbenchFoundation platform.h non-tracking path):
 *   - clv_platform_malloc(0): returns whatever malloc(0) returns (implementation-defined)
 *   - clv_platform_free(NULL): no-op (free(NULL) is safe)
 *   - clv_platform_strdup(NULL): returns NULL
 *   - clv_platform_strdup failure: returns NULL (no partial copy)
 */

#include "clv_platform_internal.h"
#include "clv_bench_internal.h"

#include <stdlib.h>
#include <string.h>

void *clv_platform_malloc(size_t size)
{
    CLV_BENCH_COUNT(CLV_BENCH_COUNTER_ALLOCATIONS);
    CLV_BENCH_ADD(CLV_BENCH_COUNTER_ALLOC_BYTES_REQUESTED, (ULONG)size);
    return malloc(size);
}

void clv_platform_free(void *ptr)
{
    if (ptr != 0) {
        CLV_BENCH_COUNT(CLV_BENCH_COUNTER_FREES);
    }
    free(ptr);
}

char *clv_platform_strdup(const char *str)
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
