#include "clv_bench_internal.h"

#ifdef CLV_ENABLE_BENCHMARKS

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/timer.h>

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

struct Device *TimerBase = 0;

typedef struct CLV_BenchU64
{
    ULONG hi;
    ULONG lo;
} CLV_BenchU64;

typedef struct CLV_BenchTimingStat
{
    ULONG calls;
    CLV_BenchU64 total_ticks;
    CLV_BenchU64 min_ticks;
    CLV_BenchU64 max_ticks;
    BOOL have_minmax;
} CLV_BenchTimingStat;

typedef struct CLV_BenchTestRecord
{
    UWORD active;
    UWORD ran;
    ULONG steps_completed;
    struct EClockVal start_tick;
    struct EClockVal end_tick;
    ULONG counters_begin[CLV_BENCH_COUNTER_COUNT];
    ULONG counters_end[CLV_BENCH_COUNTER_COUNT];
} CLV_BenchTestRecord;

typedef struct CLV_BenchState
{
    BOOL initialized;
    BOOL count_only;
    ULONG timer_freq;
    ULONG timer_overhead_ticks;
    char target_name[CLV_BENCH_MAX_TEXT];
    char profile_name[CLV_BENCH_MAX_TEXT];
    char environment_note[CLV_BENCH_MAX_TEXT];
    char timer_source[CLV_BENCH_MAX_TEXT];
    ULONG feature_flags;
    UWORD column_count;
    ULONG logical_rows;
    ULONG physical_rows;
    ULONG visible_rows;
    WORD width_pixels;
    WORD height_pixels;
    UWORD screen_depth;
    UWORD font_x;
    UWORD font_y;
    char font_name[CLV_BENCH_MAX_TEXT];
    struct MsgPort *timer_port;
    struct timerequest *timer_req;
    BOOL timer_open;
    UWORD current_test_id;
    BOOL current_test_active;
    ULONG counters[CLV_BENCH_COUNTER_COUNT];
    CLV_BenchTimingStat timings[CLV_BENCH_TIMING_COUNT];
    CLV_BenchTestRecord test_selection_only_down;
    CLV_BenchTestRecord test_selection_only_up;
    CLV_BenchTestRecord test_steady_scroll_down;
    CLV_BenchTestRecord test_steady_scroll_up;
    CLV_BenchTestRecord test_large_movement;
    CLV_BenchTestRecord test_end_to_end;
    CLV_BenchTestRecord test_redraw_baseline;
    CLV_BenchTestRecord test_prepare_baseline;
} CLV_BenchState;

static CLV_BenchState g_clv_bench;

/*
 * Keep per-test metadata and storage wholly inside this translation unit.
 * The explicit switch also avoids array-of-struct address arithmetic in the
 * vbcc/68000 path that previously read every record two bytes late.
 */
static CLV_BenchTestRecord *clv_bench_test_record(UWORD id)
{
    switch (id) {
        case CLV_BENCH_TEST_SELECTION_ONLY_DOWN:
            return &g_clv_bench.test_selection_only_down;
        case CLV_BENCH_TEST_SELECTION_ONLY_UP:
            return &g_clv_bench.test_selection_only_up;
        case CLV_BENCH_TEST_STEADY_SCROLL_DOWN:
            return &g_clv_bench.test_steady_scroll_down;
        case CLV_BENCH_TEST_STEADY_SCROLL_UP:
            return &g_clv_bench.test_steady_scroll_up;
        case CLV_BENCH_TEST_LARGE_MOVEMENT:
            return &g_clv_bench.test_large_movement;
        case CLV_BENCH_TEST_END_TO_END:
            return &g_clv_bench.test_end_to_end;
        case CLV_BENCH_TEST_REDRAW_BASELINE:
            return &g_clv_bench.test_redraw_baseline;
        case CLV_BENCH_TEST_PREPARE_BASELINE:
            return &g_clv_bench.test_prepare_baseline;
        default:
            return 0;
    }
}

static CONST_STRPTR clv_bench_test_label(UWORD id)
{
    switch (id) {
        case CLV_BENCH_TEST_SELECTION_ONLY_DOWN:
            return "selection_only_down";
        case CLV_BENCH_TEST_SELECTION_ONLY_UP:
            return "selection_only_up";
        case CLV_BENCH_TEST_STEADY_SCROLL_DOWN:
            return "steady_scroll_down";
        case CLV_BENCH_TEST_STEADY_SCROLL_UP:
            return "steady_scroll_up";
        case CLV_BENCH_TEST_LARGE_MOVEMENT:
            return "large_movement";
        case CLV_BENCH_TEST_END_TO_END:
            return "end_to_end_traversal";
        case CLV_BENCH_TEST_REDRAW_BASELINE:
            return "redraw_baseline";
        case CLV_BENCH_TEST_PREPARE_BASELINE:
            return "prepare_baseline";
        default:
            return "unknown";
    }
}

static ULONG clv_bench_test_steps_requested(UWORD id)
{
    switch (id) {
        case CLV_BENCH_TEST_SELECTION_ONLY_DOWN:
        case CLV_BENCH_TEST_SELECTION_ONLY_UP:
            return 6UL;
        case CLV_BENCH_TEST_STEADY_SCROLL_DOWN:
        case CLV_BENCH_TEST_STEADY_SCROLL_UP:
            return 50UL;
        case CLV_BENCH_TEST_LARGE_MOVEMENT:
            return 10UL;
        case CLV_BENCH_TEST_END_TO_END:
            return 2UL;
        case CLV_BENCH_TEST_REDRAW_BASELINE:
            return 1UL;
        case CLV_BENCH_TEST_PREPARE_BASELINE:
            return 3UL;
        default:
            return 0;
    }
}

static VOID clv_bench_debug_log(CONST_STRPTR format, ...)
{
    FILE *fp;
    va_list args;

    fp = fopen("PROGDIR:clv_benchmark_debug.log", "a");
    if (fp == 0) {
        return;
    }
    va_start(args, format);
    vfprintf(fp, (const char *)format, args);
    fprintf(fp, "\n");
    va_end(args);
    fclose(fp);
}

VOID clv_bench_debug_mark(CONST_STRPTR message)
{
    if (message == 0) {
        message = "null";
    }
    clv_bench_debug_log("bench: mark %s", message);
}

static const char *g_clv_bench_timing_names[CLV_BENCH_TIMING_COUNT] =
{
    "TOTAL_PREPARE",
    "TOTAL_CREATE",
    "TOTAL_INITIAL_RENDER",
    "TOTAL_NAVIGATION_RUN",
    "TOTAL_SHUTDOWN",
    "ASCII_FORMAT",
    "COLUMN_WIDTH_CALC",
    "COLUMN_LAYOUT",
    "SORT",
    "PATH_SHORTEN",
    "CHAR_WRAP",
    "PIXEL_WRAP",
    "TEXT_MEASURE",
    "ROW_HEIGHT_CALC",
    "DISPLAY_MAP_BUILD",
    "PREPARED_LIST_BUILD",
    "RENDERER_CREATE",
    "RENDERER_SETUP",
    "DETAILS_BUILD",
    "LABEL_ATTACH",
    "KEY_EVENT_TOTAL",
    "NAV_SELECTION_UPDATE",
    "NAV_MAKE_VISIBLE",
    "SCROLLER_UPDATE",
    "SELECTION_REDRAW",
    "VIEWPORT_SCROLL",
    "FULL_REDRAW",
    "PARTIAL_REDRAW",
    "DRAW_HOOK_TOTAL",
    "DRAW_ROW_TOTAL",
    "DRAW_CELL_TOTAL",
    "TIMER_READ"
};

static const char *g_clv_bench_counter_names[CLV_BENCH_COUNTER_COUNT] =
{
    "logical_rows",
    "physical_rows",
    "wrapped_continuation_rows",
    "selectable_rows",
    "nonselectable_rows",
    "columns_processed",
    "cells_prepared",
    "cells_drawn",
    "rows_drawn",
    "draw_hook_calls",
    "full_redraws",
    "partial_redraws",
    "selection_only_redraws",
    "viewport_scrolls",
    "scroll_copy_attempts",
    "scroll_copy_successes",
    "scroll_copy_fallbacks",
    "newly_exposed_rows",
    "text_measure_calls",
    "text_fit_calls",
    "text_length_calls",
    "wrap_decisions",
    "row_height_calcs",
    "display_map_growths",
    "display_map_entries",
    "allocations",
    "reallocations",
    "frees",
    "alloc_bytes_requested",
    "icon_draws",
    "horizontal_lines",
    "vertical_lines",
    "background_fills",
    "highlight_fills",
    "scrollbar_updates",
    "keyboard_events",
    "nav_moves_accepted",
    "nav_moves_rejected",
    "nonselectable_skips",
    "boundary_hits",
    "repeated_key_events",
    "prepare_rebuilds_during_nav"
};

static VOID clv_bench_copy_text(char *dst, CONST_STRPTR src, UWORD size)
{
    if (dst == 0 || size == 0) {
        return;
    }
    dst[0] = '\0';
    if (src == 0) {
        return;
    }
    strncpy(dst, src, (size_t)(size - 1));
    dst[size - 1] = '\0';
}

static CLV_BenchU64 clv_bench_u64_zero(VOID)
{
    CLV_BenchU64 v;
    v.hi = 0;
    v.lo = 0;
    return v;
}

static CLV_BenchU64 clv_bench_u64_from_eclock(const struct EClockVal *v)
{
    CLV_BenchU64 out;
    out.hi = 0;
    out.lo = 0;
    if (v != 0) {
        out.hi = v->ev_hi;
        out.lo = v->ev_lo;
    }
    return out;
}

static int clv_bench_u64_compare(CLV_BenchU64 a, CLV_BenchU64 b)
{
    if (a.hi < b.hi) {
        return -1;
    }
    if (a.hi > b.hi) {
        return 1;
    }
    if (a.lo < b.lo) {
        return -1;
    }
    if (a.lo > b.lo) {
        return 1;
    }
    return 0;
}

static CLV_BenchU64 clv_bench_u64_add(CLV_BenchU64 a, CLV_BenchU64 b)
{
    CLV_BenchU64 out;
    out.lo = a.lo + b.lo;
    out.hi = a.hi + b.hi;
    if (out.lo < a.lo) {
        out.hi++;
    }
    return out;
}

static CLV_BenchU64 clv_bench_u64_sub(CLV_BenchU64 a, CLV_BenchU64 b)
{
    CLV_BenchU64 out;
    out.hi = a.hi - b.hi;
    out.lo = a.lo - b.lo;
    if (a.lo < b.lo) {
        out.hi--;
    }
    return out;
}

static CLV_BenchU64 clv_bench_u64_mul_u32(ULONG a, ULONG b)
{
    ULONG a0, a1, b0, b1;
    ULONG p0, p1, p2, p3;
    ULONG mid_lo, mid_hi;
    CLV_BenchU64 out;

    a0 = a & 0xFFFFUL;
    a1 = a >> 16;
    b0 = b & 0xFFFFUL;
    b1 = b >> 16;

    p0 = a0 * b0;
    p1 = a0 * b1;
    p2 = a1 * b0;
    p3 = a1 * b1;

    mid_lo = (p1 & 0xFFFFUL) + (p2 & 0xFFFFUL) + (p0 >> 16);
    mid_hi = (p1 >> 16) + (p2 >> 16) + (mid_lo >> 16);

    out.lo = (p0 & 0xFFFFUL) | ((mid_lo & 0xFFFFUL) << 16);
    out.hi = p3 + mid_hi;
    return out;
}

static VOID clv_bench_u64_div_u32(CLV_BenchU64 num,
                                  ULONG den,
                                  ULONG *quot_out,
                                  ULONG *rem_out)
{
    ULONG q;
    ULONG r;
    int bit;

    if (quot_out != 0) {
        *quot_out = 0;
    }
    if (rem_out != 0) {
        *rem_out = 0;
    }
    if (den == 0) {
        return;
    }

    q = 0;
    r = 0;
    for (bit = 63; bit >= 0; bit--) {
        ULONG incoming;
        if (bit >= 32) {
            incoming = (num.hi >> (bit - 32)) & 1UL;
        } else {
            incoming = (num.lo >> bit) & 1UL;
        }
        r = (r << 1) | incoming;
        if (r >= den) {
            r -= den;
            if (bit < 32) {
                q |= (1UL << bit);
            }
        }
    }
    if (quot_out != 0) {
        *quot_out = q;
    }
    if (rem_out != 0) {
        *rem_out = r;
    }
}

static ULONG clv_bench_ticks_to_us(CLV_BenchU64 ticks, ULONG freq)
{
    ULONG secs;
    ULONG rem;
    CLV_BenchU64 frac_num;
    ULONG frac_us;

    if (freq == 0) {
        return 0;
    }

    clv_bench_u64_div_u32(ticks, freq, &secs, &rem);
    if (secs > (ULONG)(~0UL / 1000000UL)) {
        return ~0UL;
    }

    frac_num = clv_bench_u64_mul_u32(rem, 1000000UL);
    clv_bench_u64_div_u32(frac_num, freq, &frac_us, 0);
    return (secs * 1000000UL) + frac_us;
}

static ULONG clv_bench_ticks_to_ms(CLV_BenchU64 ticks, ULONG freq)
{
    ULONG secs;
    ULONG rem;
    CLV_BenchU64 frac_num;
    ULONG frac_ms;

    if (freq == 0) {
        return 0;
    }

    clv_bench_u64_div_u32(ticks, freq, &secs, &rem);
    if (secs > (ULONG)(~0UL / 1000UL)) {
        return ~0UL;
    }
    frac_num = clv_bench_u64_mul_u32(rem, 1000UL);
    clv_bench_u64_div_u32(frac_num, freq, &frac_ms, 0);
    return (secs * 1000UL) + frac_ms;
}

static VOID clv_bench_format_u64_hex(CLV_BenchU64 value,
                                     char *buffer,
                                     UWORD buffer_size)
{
    if (buffer == 0 || buffer_size == 0) {
        return;
    }
    snprintf(buffer, (size_t)buffer_size, "%08lx:%08lx",
             (unsigned long)value.hi,
             (unsigned long)value.lo);
}

static BOOL clv_bench_read_tick(struct EClockVal *tick)
{
    ULONG freq;

    if (tick == 0) {
        return FALSE;
    }
    if (!g_clv_bench.timer_open) {
        return FALSE;
    }
    freq = ReadEClock(tick);
    if (g_clv_bench.timer_freq == 0) {
        g_clv_bench.timer_freq = freq;
    }
    return (freq != 0) ? TRUE : FALSE;
}

static BOOL clv_bench_timer_open(VOID)
{
    LONG error;

    if (g_clv_bench.timer_open) {
        return TRUE;
    }

    clv_bench_debug_log("bench: timer open begin");
    g_clv_bench.timer_port = CreateMsgPort();
    if (g_clv_bench.timer_port == 0) {
        clv_bench_debug_log("bench: CreateMsgPort failed");
        return FALSE;
    }

    g_clv_bench.timer_req = (struct timerequest *)CreateIORequest(
        g_clv_bench.timer_port, sizeof(struct timerequest));
    if (g_clv_bench.timer_req == 0) {
        clv_bench_debug_log("bench: CreateIORequest failed");
        DeleteMsgPort(g_clv_bench.timer_port);
        g_clv_bench.timer_port = 0;
        return FALSE;
    }

    error = OpenDevice((CONST_STRPTR)TIMERNAME,
                       UNIT_ECLOCK,
                       (struct IORequest *)g_clv_bench.timer_req,
                       0);
    if (error != 0) {
        clv_bench_debug_log("bench: OpenDevice UNIT_ECLOCK failed error=%ld",
                            (long)error);
        DeleteIORequest((APTR)g_clv_bench.timer_req);
        DeleteMsgPort(g_clv_bench.timer_port);
        g_clv_bench.timer_req = 0;
        g_clv_bench.timer_port = 0;
        return FALSE;
    }

    TimerBase = g_clv_bench.timer_req->tr_node.io_Device;
    g_clv_bench.timer_open = TRUE;
    clv_bench_copy_text(g_clv_bench.timer_source,
                        "ReadEClock(timer.device)",
                        CLV_BENCH_MAX_TEXT);
    clv_bench_debug_log("bench: timer open success TimerBase=%p",
                        (void *)TimerBase);
    return TRUE;
}

static VOID clv_bench_timer_close(VOID)
{
    clv_bench_debug_log("bench: timer close begin open=%u",
                        g_clv_bench.timer_open ? 1U : 0U);
    if (g_clv_bench.timer_open && g_clv_bench.timer_req != 0) {
        CloseDevice((struct IORequest *)g_clv_bench.timer_req);
    }
    g_clv_bench.timer_open = FALSE;
    TimerBase = 0;
    if (g_clv_bench.timer_req != 0) {
        DeleteIORequest((APTR)g_clv_bench.timer_req);
        g_clv_bench.timer_req = 0;
    }
    if (g_clv_bench.timer_port != 0) {
        DeleteMsgPort(g_clv_bench.timer_port);
        g_clv_bench.timer_port = 0;
    }
    clv_bench_debug_log("bench: timer close end");
}

VOID clv_bench_init(VOID)
{
    struct EClockVal start_tick;
    struct EClockVal end_tick;
    CLV_BenchU64 delta;
    int i;

    memset(&g_clv_bench, 0, sizeof(g_clv_bench));
    g_clv_bench.initialized = TRUE;
    clv_bench_copy_text(g_clv_bench.target_name, "unknown", CLV_BENCH_MAX_TEXT);
    clv_bench_copy_text(g_clv_bench.profile_name, "unknown", CLV_BENCH_MAX_TEXT);
    clv_bench_copy_text(g_clv_bench.timer_source,
                        "ReadEClock(pending-open)",
                        CLV_BENCH_MAX_TEXT);
    clv_bench_debug_log("bench: init begin");

    if (!clv_bench_timer_open()
        || !clv_bench_read_tick(&start_tick)
        || !clv_bench_read_tick(&end_tick)
        || g_clv_bench.timer_freq == 0) {
        g_clv_bench.count_only = TRUE;
        clv_bench_copy_text(g_clv_bench.timer_source, "count-only",
                            CLV_BENCH_MAX_TEXT);
        clv_bench_debug_log("bench: init fallback count-only freq=%lu",
                            (unsigned long)g_clv_bench.timer_freq);
        return;
    }

    delta = clv_bench_u64_sub(clv_bench_u64_from_eclock(&end_tick),
                              clv_bench_u64_from_eclock(&start_tick));
    g_clv_bench.timer_overhead_ticks = delta.lo;

    for (i = 0; i < 32; i++) {
        CLV_BenchStamp stamp;
        clv_bench_begin(CLV_BENCH_TIMER_READ, &stamp);
        clv_bench_end(CLV_BENCH_TIMER_READ, &stamp);
    }
    clv_bench_debug_log("bench: init complete freq=%lu overhead=%lu",
                        (unsigned long)g_clv_bench.timer_freq,
                        (unsigned long)g_clv_bench.timer_overhead_ticks);
}

VOID clv_bench_shutdown(VOID)
{
    clv_bench_debug_log("bench: shutdown begin");
    clv_bench_timer_close();
    g_clv_bench.initialized = FALSE;
    clv_bench_debug_log("bench: shutdown end");
}

VOID clv_bench_reset_all(VOID)
{
    ULONG freq;
    ULONG overhead;
    BOOL count_only;
    BOOL timer_open;
    struct MsgPort *timer_port;
    struct timerequest *timer_req;
    char target_name[CLV_BENCH_MAX_TEXT];
    char profile_name[CLV_BENCH_MAX_TEXT];
    char note[CLV_BENCH_MAX_TEXT];
    char timer_source[CLV_BENCH_MAX_TEXT];
    char font_name[CLV_BENCH_MAX_TEXT];

    freq = g_clv_bench.timer_freq;
    overhead = g_clv_bench.timer_overhead_ticks;
    count_only = g_clv_bench.count_only;
    timer_open = g_clv_bench.timer_open;
    timer_port = g_clv_bench.timer_port;
    timer_req = g_clv_bench.timer_req;
    clv_bench_copy_text(target_name, g_clv_bench.target_name, CLV_BENCH_MAX_TEXT);
    clv_bench_copy_text(profile_name, g_clv_bench.profile_name, CLV_BENCH_MAX_TEXT);
    clv_bench_copy_text(note, g_clv_bench.environment_note, CLV_BENCH_MAX_TEXT);
    clv_bench_copy_text(timer_source, g_clv_bench.timer_source, CLV_BENCH_MAX_TEXT);
    clv_bench_copy_text(font_name, g_clv_bench.font_name, CLV_BENCH_MAX_TEXT);

    memset(&g_clv_bench, 0, sizeof(g_clv_bench));
    g_clv_bench.initialized = TRUE;
    g_clv_bench.timer_freq = freq;
    g_clv_bench.timer_overhead_ticks = overhead;
    g_clv_bench.count_only = count_only;
    g_clv_bench.timer_port = timer_port;
    g_clv_bench.timer_req = timer_req;
    g_clv_bench.timer_open = timer_open;
    clv_bench_copy_text(g_clv_bench.target_name, target_name, CLV_BENCH_MAX_TEXT);
    clv_bench_copy_text(g_clv_bench.profile_name, profile_name, CLV_BENCH_MAX_TEXT);
    clv_bench_copy_text(g_clv_bench.environment_note, note, CLV_BENCH_MAX_TEXT);
    clv_bench_copy_text(g_clv_bench.timer_source, timer_source, CLV_BENCH_MAX_TEXT);
    clv_bench_copy_text(g_clv_bench.font_name, font_name, CLV_BENCH_MAX_TEXT);
}

VOID clv_bench_reset_counters(VOID)
{
    memset(g_clv_bench.counters, 0, sizeof(g_clv_bench.counters));
}

VOID clv_bench_begin(UWORD id, CLV_BenchStamp *stamp)
{
    if (!g_clv_bench.initialized || g_clv_bench.count_only || stamp == 0) {
        if (stamp != 0) {
            stamp->valid = FALSE;
        }
        return;
    }
    if ((ULONG)id >= (ULONG)CLV_BENCH_TIMING_COUNT) {
        stamp->valid = FALSE;
        return;
    }
    stamp->valid = clv_bench_read_tick(&stamp->start);
}

VOID clv_bench_end(UWORD id, CLV_BenchStamp *stamp)
{
    struct EClockVal end_tick;
    CLV_BenchU64 delta;
    CLV_BenchTimingStat *stat;
    CLV_BenchU64 overhead;

    if (!g_clv_bench.initialized || g_clv_bench.count_only || stamp == 0
        || !stamp->valid) {
        return;
    }
    if ((ULONG)id >= (ULONG)CLV_BENCH_TIMING_COUNT) {
        return;
    }
    if (!clv_bench_read_tick(&end_tick)) {
        return;
    }

    delta = clv_bench_u64_sub(clv_bench_u64_from_eclock(&end_tick),
                              clv_bench_u64_from_eclock(&stamp->start));
    overhead = clv_bench_u64_zero();
    overhead.lo = g_clv_bench.timer_overhead_ticks;
    if (clv_bench_u64_compare(delta, overhead) > 0) {
        delta = clv_bench_u64_sub(delta, overhead);
    }

    stat = &g_clv_bench.timings[id];
    stat->calls++;
    stat->total_ticks = clv_bench_u64_add(stat->total_ticks, delta);
    if (!stat->have_minmax || clv_bench_u64_compare(delta, stat->min_ticks) < 0) {
        stat->min_ticks = delta;
    }
    if (!stat->have_minmax || clv_bench_u64_compare(delta, stat->max_ticks) > 0) {
        stat->max_ticks = delta;
    }
    stat->have_minmax = TRUE;
}

VOID clv_bench_count(UWORD id)
{
    if (!g_clv_bench.initialized) {
        return;
    }
    if ((ULONG)id >= (ULONG)CLV_BENCH_COUNTER_COUNT) {
        return;
    }
    g_clv_bench.counters[id]++;
}

VOID clv_bench_add(UWORD id, ULONG amount)
{
    if (!g_clv_bench.initialized) {
        return;
    }
    if ((ULONG)id >= (ULONG)CLV_BENCH_COUNTER_COUNT) {
        return;
    }
    g_clv_bench.counters[id] += amount;
}

ULONG clv_bench_get_counter(UWORD id)
{
    if ((ULONG)id >= (ULONG)CLV_BENCH_COUNTER_COUNT) {
        return 0;
    }
    return g_clv_bench.counters[id];
}

VOID clv_bench_set_profile(CONST_STRPTR profile_name)
{
    clv_bench_copy_text(g_clv_bench.profile_name, profile_name, CLV_BENCH_MAX_TEXT);
}

VOID clv_bench_set_target(CONST_STRPTR target_name)
{
    clv_bench_copy_text(g_clv_bench.target_name, target_name, CLV_BENCH_MAX_TEXT);
}

VOID clv_bench_set_environment_note(CONST_STRPTR note)
{
    clv_bench_copy_text(g_clv_bench.environment_note, note, CLV_BENCH_MAX_TEXT);
}

VOID clv_bench_set_timer_source(CONST_STRPTR timer_source)
{
    clv_bench_copy_text(g_clv_bench.timer_source, timer_source, CLV_BENCH_MAX_TEXT);
}

VOID clv_bench_set_feature_flags(ULONG feature_flags)
{
    g_clv_bench.feature_flags = feature_flags;
}

VOID clv_bench_set_dimensions(UWORD columns,
                              ULONG logical_rows,
                              ULONG physical_rows,
                              ULONG visible_rows,
                              WORD width_pixels,
                              WORD height_pixels)
{
    g_clv_bench.column_count = columns;
    g_clv_bench.logical_rows = logical_rows;
    g_clv_bench.physical_rows = physical_rows;
    g_clv_bench.visible_rows = visible_rows;
    g_clv_bench.width_pixels = width_pixels;
    g_clv_bench.height_pixels = height_pixels;
}

VOID clv_bench_set_screen_info(UWORD depth,
                               CONST_STRPTR font_name,
                               UWORD font_y,
                               UWORD font_x)
{
    g_clv_bench.screen_depth = depth;
    g_clv_bench.font_y = font_y;
    g_clv_bench.font_x = font_x;
    clv_bench_copy_text(g_clv_bench.font_name, font_name, CLV_BENCH_MAX_TEXT);
}

VOID clv_bench_note_prepare_rebuild(VOID)
{
    clv_bench_count(CLV_BENCH_COUNTER_PREPARE_REBUILDS_DURING_NAV);
}

VOID clv_bench_test_begin(UWORD id)
{
    CLV_BenchTestRecord *test;

    if ((ULONG)id >= (ULONG)CLV_BENCH_TEST_COUNT) {
        return;
    }
    test = clv_bench_test_record(id);
    if (test == 0) {
        return;
    }
    memset(test, 0, sizeof(*test));
    test->active = TRUE;
    test->ran = TRUE;
    g_clv_bench.current_test_id = id;
    g_clv_bench.current_test_active = TRUE;
    memcpy(test->counters_begin,
           g_clv_bench.counters,
           sizeof(test->counters_begin));
    if (!g_clv_bench.count_only) {
        (VOID)clv_bench_read_tick(&test->start_tick);
    }
}

VOID clv_bench_test_note_steps(ULONG steps_completed)
{
    CLV_BenchTestRecord *test;
    UWORD id;

    if (!g_clv_bench.current_test_active) {
        return;
    }
    id = g_clv_bench.current_test_id;
    if ((ULONG)id >= (ULONG)CLV_BENCH_TEST_COUNT) {
        return;
    }
    test = clv_bench_test_record(id);
    if (test == 0) {
        return;
    }
    test->steps_completed += steps_completed;
}

VOID clv_bench_test_end(VOID)
{
    CLV_BenchTestRecord *test;
    UWORD id;

    if (!g_clv_bench.current_test_active) {
        return;
    }
    id = g_clv_bench.current_test_id;
    if ((ULONG)id >= (ULONG)CLV_BENCH_TEST_COUNT) {
        g_clv_bench.current_test_active = FALSE;
        return;
    }
    test = clv_bench_test_record(id);
    if (test == 0) {
        g_clv_bench.current_test_active = FALSE;
        return;
    }
    test->active = FALSE;
    g_clv_bench.current_test_active = FALSE;
    memcpy(test->counters_end,
           g_clv_bench.counters,
           sizeof(test->counters_end));
    if (!g_clv_bench.count_only) {
        (VOID)clv_bench_read_tick(&test->end_tick);
    }
}

static VOID clv_bench_print_header(FILE *fp)
{
    fprintf(fp, "[HEADER]\n");
    fprintf(fp, "format_version=%u\n", (unsigned)CLV_BENCH_FORMAT_VERSION);
    fprintf(fp, "build_date=%s %s\n", __DATE__, __TIME__);
    fprintf(fp, "target=%s\n", g_clv_bench.target_name);
    fprintf(fp, "profile=%s\n", g_clv_bench.profile_name);
    fprintf(fp, "benchmark_enabled=1\n");
    fprintf(fp, "timer_source=%s\n", g_clv_bench.timer_source);
    fprintf(fp, "timer_frequency=%lu\n", (unsigned long)g_clv_bench.timer_freq);
    fprintf(fp, "timer_overhead_ticks=%lu\n",
            (unsigned long)g_clv_bench.timer_overhead_ticks);
    fprintf(fp, "count_only=%u\n", g_clv_bench.count_only ? 1U : 0U);
    fprintf(fp, "screen_depth=%u\n", (unsigned)g_clv_bench.screen_depth);
    fprintf(fp, "font_name=%s\n", g_clv_bench.font_name);
    fprintf(fp, "font_width=%u\n", (unsigned)g_clv_bench.font_x);
    fprintf(fp, "font_height=%u\n", (unsigned)g_clv_bench.font_y);
    fprintf(fp, "listview_width=%d\n", (int)g_clv_bench.width_pixels);
    fprintf(fp, "listview_height=%d\n", (int)g_clv_bench.height_pixels);
    fprintf(fp, "visible_physical_rows=%lu\n",
            (unsigned long)g_clv_bench.visible_rows);
    fprintf(fp, "logical_rows=%lu\n", (unsigned long)g_clv_bench.logical_rows);
    fprintf(fp, "physical_rows=%lu\n", (unsigned long)g_clv_bench.physical_rows);
    fprintf(fp, "column_count=%u\n", (unsigned)g_clv_bench.column_count);
    fprintf(fp, "wrap_enabled=%u\n",
            (g_clv_bench.feature_flags & CLV_BENCH_FEATURE_WRAP) ? 1U : 0U);
    fprintf(fp, "icons_enabled=%u\n",
            (g_clv_bench.feature_flags & CLV_BENCH_FEATURE_ICONS) ? 1U : 0U);
    fprintf(fp, "styles_enabled=%u\n",
            (g_clv_bench.feature_flags & CLV_BENCH_FEATURE_STYLES) ? 1U : 0U);
    fprintf(fp, "details_enabled=%u\n",
            (g_clv_bench.feature_flags & CLV_BENCH_FEATURE_DETAILS) ? 1U : 0U);
    fprintf(fp, "selection_adapter_enabled=%u\n",
            (g_clv_bench.feature_flags & CLV_BENCH_FEATURE_SELECTION_MAP)
                ? 1U : 0U);
    fprintf(fp, "smart_scroll_enabled=%u\n",
            (g_clv_bench.feature_flags & CLV_BENCH_FEATURE_SMART_SCROLL)
                ? 1U : 0U);
    if (g_clv_bench.environment_note[0] != '\0') {
        fprintf(fp, "environment_note=%s\n", g_clv_bench.environment_note);
    }
    fprintf(fp, "\n");
}

static VOID clv_bench_print_timings(FILE *fp)
{
    ULONG i;

    fprintf(fp, "[TIMING]\n");
    fprintf(fp,
            "name                         calls      ticks_total         ms_total   us_avg   us_min   us_max\n");
    for (i = 0; i < (ULONG)CLV_BENCH_TIMING_COUNT; i++) {
        CLV_BenchTimingStat *stat;
        char total_ticks[32];
        ULONG avg_us;
        ULONG min_us;
        ULONG max_us;
        CLV_BenchU64 avg_ticks;

        stat = &g_clv_bench.timings[i];
        clv_bench_format_u64_hex(stat->total_ticks, total_ticks,
                                 (UWORD)sizeof(total_ticks));
        avg_ticks = clv_bench_u64_zero();
        avg_us = 0;
        min_us = 0;
        max_us = 0;
        if (stat->calls > 0) {
            clv_bench_u64_div_u32(stat->total_ticks, stat->calls,
                                  &avg_ticks.lo, 0);
            avg_us = clv_bench_ticks_to_us(avg_ticks, g_clv_bench.timer_freq);
            min_us = clv_bench_ticks_to_us(stat->min_ticks, g_clv_bench.timer_freq);
            max_us = clv_bench_ticks_to_us(stat->max_ticks, g_clv_bench.timer_freq);
        }
        fprintf(fp, "%-28s %8lu  %-18s %8lu %8lu %8lu %8lu\n",
                g_clv_bench_timing_names[i],
                (unsigned long)stat->calls,
                total_ticks,
                (unsigned long)clv_bench_ticks_to_ms(stat->total_ticks,
                                                     g_clv_bench.timer_freq),
                (unsigned long)avg_us,
                (unsigned long)min_us,
                (unsigned long)max_us);
    }
    fprintf(fp, "\n");
}

static VOID clv_bench_print_counters(FILE *fp)
{
    ULONG i;

    fprintf(fp, "[COUNTERS]\n");
    for (i = 0; i < (ULONG)CLV_BENCH_COUNTER_COUNT; i++) {
        fprintf(fp, "%s=%lu\n",
                g_clv_bench_counter_names[i],
                (unsigned long)g_clv_bench.counters[i]);
    }
    fprintf(fp, "\n");
}

static VOID clv_bench_print_tests(FILE *fp)
{
    UWORD i;

    for (i = 0; i < (UWORD)CLV_BENCH_TEST_COUNT; i++) {
        CLV_BenchTestRecord *test;
        CLV_BenchU64 elapsed_ticks;
        ULONG elapsed_us;
        ULONG avg_us;
        ULONG viewport_moves;
        ULONG full_redraws;
        ULONG partial_redraws;
        ULONG selection_redraws;
        ULONG scroll_copy_ops;
        ULONG steps_requested;
        CONST_STRPTR label;

        test = clv_bench_test_record(i);
        if (test == 0) {
            continue;
        }
        if (!test->ran) {
            continue;
        }
        label = clv_bench_test_label(i);
        steps_requested = clv_bench_test_steps_requested(i);
        elapsed_ticks = clv_bench_u64_sub(
            clv_bench_u64_from_eclock(&test->end_tick),
            clv_bench_u64_from_eclock(&test->start_tick));
        elapsed_us = clv_bench_ticks_to_us(elapsed_ticks, g_clv_bench.timer_freq);
        avg_us = (test->steps_completed > 0)
            ? (elapsed_us / test->steps_completed)
            : 0;

        viewport_moves =
            test->counters_end[CLV_BENCH_COUNTER_VIEWPORT_SCROLLS] -
            test->counters_begin[CLV_BENCH_COUNTER_VIEWPORT_SCROLLS];
        full_redraws =
            test->counters_end[CLV_BENCH_COUNTER_FULL_REDRAWS] -
            test->counters_begin[CLV_BENCH_COUNTER_FULL_REDRAWS];
        partial_redraws =
            test->counters_end[CLV_BENCH_COUNTER_PARTIAL_REDRAWS] -
            test->counters_begin[CLV_BENCH_COUNTER_PARTIAL_REDRAWS];
        selection_redraws =
            test->counters_end[CLV_BENCH_COUNTER_SELECTION_ONLY_REDRAWS] -
            test->counters_begin[CLV_BENCH_COUNTER_SELECTION_ONLY_REDRAWS];
        scroll_copy_ops =
            test->counters_end[CLV_BENCH_COUNTER_SCROLL_COPY_SUCCESSES] -
            test->counters_begin[CLV_BENCH_COUNTER_SCROLL_COPY_SUCCESSES];

        fprintf(fp, "[TEST %s]\n", label);
        fprintf(fp, "steps_requested=%lu\n", (unsigned long)steps_requested);
        fprintf(fp, "steps_completed=%lu\n", (unsigned long)test->steps_completed);
        fprintf(fp, "viewport_moves=%lu\n", (unsigned long)viewport_moves);
        fprintf(fp, "full_redraws=%lu\n", (unsigned long)full_redraws);
        fprintf(fp, "partial_redraws=%lu\n", (unsigned long)partial_redraws);
        fprintf(fp, "selection_redraws=%lu\n", (unsigned long)selection_redraws);
        fprintf(fp, "scroll_copy_operations=%lu\n",
                (unsigned long)scroll_copy_ops);
        fprintf(fp, "elapsed_us=%lu\n", (unsigned long)elapsed_us);
        fprintf(fp, "average_us_per_step=%lu\n", (unsigned long)avg_us);
        fprintf(fp, "\n");
    }
}

BOOL clv_bench_write_report(CONST_STRPTR path)
{
    FILE *fp;
    CONST_STRPTR report_path;

    report_path = (path != 0) ? path : "PROGDIR:clv_benchmark.txt";
    fp = fopen((const char *)report_path, "w");
    if (fp == 0) {
        fp = stdout;
    }

    clv_bench_print_header(fp);
    clv_bench_print_timings(fp);
    clv_bench_print_counters(fp);
    clv_bench_print_tests(fp);

    if (fp != stdout) {
        fclose(fp);
    }
    return TRUE;
}

#endif /* CLV_ENABLE_BENCHMARKS */
