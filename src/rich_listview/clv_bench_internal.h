#ifndef CLV_BENCH_INTERNAL_H
#define CLV_BENCH_INTERNAL_H

/*
 * Private benchmark support for benchmark-only builds.
 * Do not include from public client code.
 */

#include <exec/types.h>

#ifdef CLV_ENABLE_BENCHMARKS

#include <devices/timer.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CLV_BENCH_FORMAT_VERSION 1U
#define CLV_BENCH_MAX_TESTS 16U
#define CLV_BENCH_MAX_TEXT 64U
#define CLV_BENCH_FEATURE_WRAP          (1UL << 0)
#define CLV_BENCH_FEATURE_ICONS         (1UL << 1)
#define CLV_BENCH_FEATURE_STYLES        (1UL << 2)
#define CLV_BENCH_FEATURE_DETAILS       (1UL << 3)
#define CLV_BENCH_FEATURE_SELECTION_MAP (1UL << 4)
#define CLV_BENCH_FEATURE_SMART_SCROLL  (1UL << 5)

typedef struct CLV_BenchStamp
{
    struct EClockVal start;
    BOOL valid;
} CLV_BenchStamp;

typedef enum CLV_BenchTimingId
{
    CLV_BENCH_TOTAL_PREPARE = 0,
    CLV_BENCH_TOTAL_CREATE,
    CLV_BENCH_TOTAL_INITIAL_RENDER,
    CLV_BENCH_TOTAL_NAVIGATION_RUN,
    CLV_BENCH_TOTAL_SHUTDOWN,
    CLV_BENCH_ASCII_FORMAT,
    CLV_BENCH_COLUMN_WIDTH_CALC,
    CLV_BENCH_COLUMN_LAYOUT,
    CLV_BENCH_SORT,
    CLV_BENCH_PATH_SHORTEN,
    CLV_BENCH_CHAR_WRAP,
    CLV_BENCH_PIXEL_WRAP,
    CLV_BENCH_TEXT_MEASURE,
    CLV_BENCH_ROW_HEIGHT_CALC,
    CLV_BENCH_DISPLAY_MAP_BUILD,
    CLV_BENCH_PREPARED_LIST_BUILD,
    CLV_BENCH_RENDERER_CREATE,
    CLV_BENCH_RENDERER_SETUP,
    CLV_BENCH_DETAILS_BUILD,
    CLV_BENCH_LABEL_ATTACH,
    CLV_BENCH_KEY_EVENT_TOTAL,
    CLV_BENCH_NAV_SELECTION_UPDATE,
    CLV_BENCH_NAV_MAKE_VISIBLE,
    CLV_BENCH_SCROLLER_UPDATE,
    CLV_BENCH_SELECTION_REDRAW,
    CLV_BENCH_VIEWPORT_SCROLL,
    CLV_BENCH_FULL_REDRAW,
    CLV_BENCH_PARTIAL_REDRAW,
    CLV_BENCH_DRAW_HOOK_TOTAL,
    CLV_BENCH_DRAW_ROW_TOTAL,
    CLV_BENCH_DRAW_CELL_TOTAL,
    CLV_BENCH_TIMER_READ,
    CLV_BENCH_TIMING_COUNT
} CLV_BenchTimingId;

typedef enum CLV_BenchCounterId
{
    CLV_BENCH_COUNTER_LOGICAL_ROWS = 0,
    CLV_BENCH_COUNTER_PHYSICAL_ROWS,
    CLV_BENCH_COUNTER_WRAPPED_CONTINUATION_ROWS,
    CLV_BENCH_COUNTER_SELECTABLE_ROWS,
    CLV_BENCH_COUNTER_NONSELECTABLE_ROWS,
    CLV_BENCH_COUNTER_COLUMNS_PROCESSED,
    CLV_BENCH_COUNTER_CELLS_PREPARED,
    CLV_BENCH_COUNTER_CELLS_DRAWN,
    CLV_BENCH_COUNTER_ROWS_DRAWN,
    CLV_BENCH_COUNTER_DRAW_HOOK_CALLS,
    CLV_BENCH_COUNTER_FULL_REDRAWS,
    CLV_BENCH_COUNTER_PARTIAL_REDRAWS,
    CLV_BENCH_COUNTER_SELECTION_ONLY_REDRAWS,
    CLV_BENCH_COUNTER_VIEWPORT_SCROLLS,
    CLV_BENCH_COUNTER_SCROLL_COPY_ATTEMPTS,
    CLV_BENCH_COUNTER_SCROLL_COPY_SUCCESSES,
    CLV_BENCH_COUNTER_SCROLL_COPY_FALLBACKS,
    CLV_BENCH_COUNTER_NEWLY_EXPOSED_ROWS,
    CLV_BENCH_COUNTER_TEXT_MEASURE_CALLS,
    CLV_BENCH_COUNTER_TEXT_FIT_CALLS,
    CLV_BENCH_COUNTER_TEXT_LENGTH_CALLS,
    CLV_BENCH_COUNTER_WRAP_DECISIONS,
    CLV_BENCH_COUNTER_ROW_HEIGHT_CALCS,
    CLV_BENCH_COUNTER_DISPLAY_MAP_GROWTHS,
    CLV_BENCH_COUNTER_DISPLAY_MAP_ENTRIES,
    CLV_BENCH_COUNTER_ALLOCATIONS,
    CLV_BENCH_COUNTER_REALLOCATIONS,
    CLV_BENCH_COUNTER_FREES,
    CLV_BENCH_COUNTER_ALLOC_BYTES_REQUESTED,
    CLV_BENCH_COUNTER_ICON_DRAWS,
    CLV_BENCH_COUNTER_HORIZONTAL_LINES,
    CLV_BENCH_COUNTER_VERTICAL_LINES,
    CLV_BENCH_COUNTER_BACKGROUND_FILLS,
    CLV_BENCH_COUNTER_HIGHLIGHT_FILLS,
    CLV_BENCH_COUNTER_SCROLLBAR_UPDATES,
    CLV_BENCH_COUNTER_KEYBOARD_EVENTS,
    CLV_BENCH_COUNTER_NAV_MOVES_ACCEPTED,
    CLV_BENCH_COUNTER_NAV_MOVES_REJECTED,
    CLV_BENCH_COUNTER_NONSELECTABLE_SKIPS,
    CLV_BENCH_COUNTER_BOUNDARY_HITS,
    CLV_BENCH_COUNTER_REPEATED_KEY_EVENTS,
    CLV_BENCH_COUNTER_PREPARE_REBUILDS_DURING_NAV,
    CLV_BENCH_COUNTER_COUNT
} CLV_BenchCounterId;

typedef enum CLV_BenchTestId
{
    CLV_BENCH_TEST_SELECTION_ONLY_DOWN = 0,
    CLV_BENCH_TEST_SELECTION_ONLY_UP,
    CLV_BENCH_TEST_STEADY_SCROLL_DOWN,
    CLV_BENCH_TEST_STEADY_SCROLL_UP,
    CLV_BENCH_TEST_LARGE_MOVEMENT,
    CLV_BENCH_TEST_END_TO_END,
    CLV_BENCH_TEST_REDRAW_BASELINE,
    CLV_BENCH_TEST_PREPARE_BASELINE,
    CLV_BENCH_TEST_COUNT
} CLV_BenchTestId;

VOID clv_bench_init(VOID);
VOID clv_bench_shutdown(VOID);
VOID clv_bench_reset_all(VOID);
VOID clv_bench_reset_counters(VOID);
VOID clv_bench_begin(UWORD id, CLV_BenchStamp *stamp);
VOID clv_bench_end(UWORD id, CLV_BenchStamp *stamp);
VOID clv_bench_count(UWORD id);
VOID clv_bench_add(UWORD id, ULONG amount);
ULONG clv_bench_get_counter(UWORD id);
VOID clv_bench_set_profile(CONST_STRPTR profile_name);
VOID clv_bench_set_target(CONST_STRPTR target_name);
VOID clv_bench_set_environment_note(CONST_STRPTR note);
VOID clv_bench_set_timer_source(CONST_STRPTR timer_source);
VOID clv_bench_set_feature_flags(ULONG feature_flags);
VOID clv_bench_debug_mark(CONST_STRPTR message);
VOID clv_bench_set_dimensions(UWORD columns,
                              ULONG logical_rows,
                              ULONG physical_rows,
                              ULONG visible_rows,
                              WORD width_pixels,
                              WORD height_pixels);
VOID clv_bench_set_screen_info(UWORD depth,
                               CONST_STRPTR font_name,
                               UWORD font_y,
                               UWORD font_x);
VOID clv_bench_note_prepare_rebuild(VOID);
VOID clv_bench_test_begin(UWORD id);
VOID clv_bench_test_note_steps(ULONG steps_completed);
VOID clv_bench_test_end(VOID);
BOOL clv_bench_write_report(CONST_STRPTR path);

#define CLV_BENCH_DECLARE(name) \
    CLV_BenchStamp name

#define CLV_BENCH_BEGIN(id, stamp_var) \
    clv_bench_begin((id), &(stamp_var))

#define CLV_BENCH_END(id, stamp_var) \
    clv_bench_end((id), &(stamp_var))

#define CLV_BENCH_COUNT(id) \
    clv_bench_count((id))

#define CLV_BENCH_ADD(id, amount) \
    clv_bench_add((id), (ULONG)(amount))

#define CLV_BENCH_NOTE_PREPARE_REBUILD() \
    clv_bench_note_prepare_rebuild()

#ifdef __cplusplus
}
#endif

#else

#define CLV_BENCH_DECLARE(name) \
    int name

#define CLV_BENCH_BEGIN(id, stamp_var) \
    do { } while (0)

#define CLV_BENCH_END(id, stamp_var) \
    do { } while (0)

#define CLV_BENCH_COUNT(id) \
    do { } while (0)

#define CLV_BENCH_ADD(id, amount) \
    do { } while (0)

#define CLV_BENCH_NOTE_PREPARE_REBUILD() \
    do { } while (0)

#endif /* CLV_ENABLE_BENCHMARKS */

#endif /* CLV_BENCH_INTERNAL_H */
