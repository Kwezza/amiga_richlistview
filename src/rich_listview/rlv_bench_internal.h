#ifndef RLV_BENCH_INTERNAL_H
#define RLV_BENCH_INTERNAL_H

/*
 * Private benchmark support for benchmark-only builds.
 * Do not include from public client code.
 */

#include <exec/types.h>

#ifdef RLV_ENABLE_BENCHMARKS

#include <devices/timer.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RLV_BENCH_FORMAT_VERSION 1U
#define RLV_BENCH_MAX_TESTS 16U
#define RLV_BENCH_MAX_TEXT 64U
#define RLV_BENCH_FEATURE_WRAP          (1UL << 0)
#define RLV_BENCH_FEATURE_ICONS         (1UL << 1)
#define RLV_BENCH_FEATURE_STYLES        (1UL << 2)
#define RLV_BENCH_FEATURE_DETAILS       (1UL << 3)
#define RLV_BENCH_FEATURE_SELECTION_MAP (1UL << 4)
#define RLV_BENCH_FEATURE_SMART_SCROLL  (1UL << 5)

typedef struct RLV_BenchStamp
{
    struct EClockVal start;
    BOOL valid;
} RLV_BenchStamp;

typedef enum RLV_BenchTimingId
{
    RLV_BENCH_TOTAL_PREPARE = 0,
    RLV_BENCH_TOTAL_CREATE,
    RLV_BENCH_TOTAL_INITIAL_RENDER,
    RLV_BENCH_TOTAL_NAVIGATION_RUN,
    RLV_BENCH_TOTAL_SHUTDOWN,
    RLV_BENCH_ASCII_FORMAT,
    RLV_BENCH_COLUMN_WIDTH_CALC,
    RLV_BENCH_COLUMN_LAYOUT,
    RLV_BENCH_SORT,
    RLV_BENCH_PATH_SHORTEN,
    RLV_BENCH_CHAR_WRAP,
    RLV_BENCH_PIXEL_WRAP,
    RLV_BENCH_TEXT_MEASURE,
    RLV_BENCH_ROW_HEIGHT_CALC,
    RLV_BENCH_DISPLAY_MAP_BUILD,
    RLV_BENCH_PREPARED_LIST_BUILD,
    RLV_BENCH_RENDERER_CREATE,
    RLV_BENCH_RENDERER_SETUP,
    RLV_BENCH_DETAILS_BUILD,
    RLV_BENCH_LABEL_ATTACH,
    RLV_BENCH_KEY_EVENT_TOTAL,
    RLV_BENCH_NAV_SELECTION_UPDATE,
    RLV_BENCH_NAV_MAKE_VISIBLE,
    RLV_BENCH_SCROLLER_UPDATE,
    RLV_BENCH_SELECTION_REDRAW,
    RLV_BENCH_VIEWPORT_SCROLL,
    RLV_BENCH_FULL_REDRAW,
    RLV_BENCH_PARTIAL_REDRAW,
    RLV_BENCH_DRAW_HOOK_TOTAL,
    RLV_BENCH_DRAW_ROW_TOTAL,
    RLV_BENCH_DRAW_CELL_TOTAL,
    RLV_BENCH_TIMER_READ,
    RLV_BENCH_TIMING_COUNT
} RLV_BenchTimingId;

typedef enum RLV_BenchCounterId
{
    RLV_BENCH_COUNTER_LOGICAL_ROWS = 0,
    RLV_BENCH_COUNTER_PHYSICAL_ROWS,
    RLV_BENCH_COUNTER_WRAPPED_CONTINUATION_ROWS,
    RLV_BENCH_COUNTER_SELECTABLE_ROWS,
    RLV_BENCH_COUNTER_NONSELECTABLE_ROWS,
    RLV_BENCH_COUNTER_COLUMNS_PROCESSED,
    RLV_BENCH_COUNTER_CELLS_PREPARED,
    RLV_BENCH_COUNTER_CELLS_DRAWN,
    RLV_BENCH_COUNTER_ROWS_DRAWN,
    RLV_BENCH_COUNTER_DRAW_HOOK_CALLS,
    RLV_BENCH_COUNTER_FULL_REDRAWS,
    RLV_BENCH_COUNTER_PARTIAL_REDRAWS,
    RLV_BENCH_COUNTER_SELECTION_ONLY_REDRAWS,
    RLV_BENCH_COUNTER_VIEWPORT_SCROLLS,
    RLV_BENCH_COUNTER_SCROLL_COPY_ATTEMPTS,
    RLV_BENCH_COUNTER_SCROLL_COPY_SUCCESSES,
    RLV_BENCH_COUNTER_SCROLL_COPY_FALLBACKS,
    RLV_BENCH_COUNTER_NEWLY_EXPOSED_ROWS,
    RLV_BENCH_COUNTER_TEXT_MEASURE_CALLS,
    RLV_BENCH_COUNTER_TEXT_FIT_CALLS,
    RLV_BENCH_COUNTER_TEXT_LENGTH_CALLS,
    RLV_BENCH_COUNTER_WRAP_DECISIONS,
    RLV_BENCH_COUNTER_ROW_HEIGHT_CALCS,
    RLV_BENCH_COUNTER_DISPLAY_MAP_GROWTHS,
    RLV_BENCH_COUNTER_DISPLAY_MAP_ENTRIES,
    RLV_BENCH_COUNTER_ALLOCATIONS,
    RLV_BENCH_COUNTER_REALLOCATIONS,
    RLV_BENCH_COUNTER_FREES,
    RLV_BENCH_COUNTER_ALLOC_BYTES_REQUESTED,
    RLV_BENCH_COUNTER_ICON_DRAWS,
    RLV_BENCH_COUNTER_HORIZONTAL_LINES,
    RLV_BENCH_COUNTER_VERTICAL_LINES,
    RLV_BENCH_COUNTER_BACKGROUND_FILLS,
    RLV_BENCH_COUNTER_HIGHLIGHT_FILLS,
    RLV_BENCH_COUNTER_SCROLLBAR_UPDATES,
    RLV_BENCH_COUNTER_KEYBOARD_EVENTS,
    RLV_BENCH_COUNTER_NAV_MOVES_ACCEPTED,
    RLV_BENCH_COUNTER_NAV_MOVES_REJECTED,
    RLV_BENCH_COUNTER_NONSELECTABLE_SKIPS,
    RLV_BENCH_COUNTER_BOUNDARY_HITS,
    RLV_BENCH_COUNTER_REPEATED_KEY_EVENTS,
    RLV_BENCH_COUNTER_PREPARE_REBUILDS_DURING_NAV,
    RLV_BENCH_COUNTER_COUNT
} RLV_BenchCounterId;

typedef enum RLV_BenchTestId
{
    RLV_BENCH_TEST_SELECTION_ONLY_DOWN = 0,
    RLV_BENCH_TEST_SELECTION_ONLY_UP,
    RLV_BENCH_TEST_STEADY_SCROLL_DOWN,
    RLV_BENCH_TEST_STEADY_SCROLL_UP,
    RLV_BENCH_TEST_LARGE_MOVEMENT,
    RLV_BENCH_TEST_END_TO_END,
    RLV_BENCH_TEST_REDRAW_BASELINE,
    RLV_BENCH_TEST_PREPARE_BASELINE,
    RLV_BENCH_TEST_COUNT
} RLV_BenchTestId;

VOID rlv_bench_init(VOID);
VOID rlv_bench_shutdown(VOID);
VOID rlv_bench_reset_all(VOID);
VOID rlv_bench_reset_counters(VOID);
VOID rlv_bench_begin(UWORD id, RLV_BenchStamp *stamp);
VOID rlv_bench_end(UWORD id, RLV_BenchStamp *stamp);
VOID rlv_bench_count(UWORD id);
VOID rlv_bench_add(UWORD id, ULONG amount);
ULONG rlv_bench_get_counter(UWORD id);
VOID rlv_bench_set_profile(CONST_STRPTR profile_name);
VOID rlv_bench_set_target(CONST_STRPTR target_name);
VOID rlv_bench_set_environment_note(CONST_STRPTR note);
VOID rlv_bench_set_timer_source(CONST_STRPTR timer_source);
VOID rlv_bench_set_feature_flags(ULONG feature_flags);
VOID rlv_bench_debug_mark(CONST_STRPTR message);
VOID rlv_bench_set_dimensions(UWORD columns,
                              ULONG logical_rows,
                              ULONG physical_rows,
                              ULONG visible_rows,
                              WORD width_pixels,
                              WORD height_pixels);
VOID rlv_bench_set_screen_info(UWORD depth,
                               CONST_STRPTR font_name,
                               UWORD font_y,
                               UWORD font_x);
VOID rlv_bench_note_prepare_rebuild(VOID);
VOID rlv_bench_test_begin(UWORD id);
VOID rlv_bench_test_note_steps(ULONG steps_completed);
VOID rlv_bench_test_end(VOID);
BOOL rlv_bench_write_report(CONST_STRPTR path);

#define RLV_BENCH_DECLARE(name) \
    RLV_BenchStamp name

#define RLV_BENCH_BEGIN(id, stamp_var) \
    rlv_bench_begin((id), &(stamp_var))

#define RLV_BENCH_END(id, stamp_var) \
    rlv_bench_end((id), &(stamp_var))

#define RLV_BENCH_COUNT(id) \
    rlv_bench_count((id))

#define RLV_BENCH_ADD(id, amount) \
    rlv_bench_add((id), (ULONG)(amount))

#define RLV_BENCH_NOTE_PREPARE_REBUILD() \
    rlv_bench_note_prepare_rebuild()

#ifdef __cplusplus
}
#endif

#else

#define RLV_BENCH_DECLARE(name) \
    int name

#define RLV_BENCH_BEGIN(id, stamp_var) \
    do { } while (0)

#define RLV_BENCH_END(id, stamp_var) \
    do { } while (0)

#define RLV_BENCH_COUNT(id) \
    do { } while (0)

#define RLV_BENCH_ADD(id, amount) \
    do { } while (0)

#define RLV_BENCH_NOTE_PREPARE_REBUILD() \
    do { } while (0)

#endif /* RLV_ENABLE_BENCHMARKS */

#endif /* RLV_BENCH_INTERNAL_H */
