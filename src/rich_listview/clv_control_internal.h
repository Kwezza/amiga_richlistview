#ifndef CLV_CONTROL_INTERNAL_H
#define CLV_CONTROL_INTERNAL_H

/**
 * Private control state and helpers. Not for application includes.
 */

#include "rich_listview/clv_control.h"
#include "rich_listview/clv_bench_internal.h"

#include <exec/types.h>
#include <graphics/gfx.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Flat one-pixel column separator. */
#define CLV_CTRL_DIVIDER_WIDTH  1

/* Single dark outline around the whole control (header + viewport). */
#define CLV_CTRL_FRAME_WIDTH    1

/* Bounded wrap fragments per cell (narrow columns / long strings). */
#define CLV_CTRL_MAX_FRAGS_PER_CELL  32

typedef struct CLV_RowLayout
{
    ULONG logical_index;
    LONG top_y;            /* content-space Y (scroll-relative origin 0) */
    UWORD content_height;
    UWORD total_height;    /* content_height + row_gap */
    UWORD maximum_line_count;
    UWORD flags;
} CLV_RowLayout;

/* One wrapped display line within a cell. Text points into borrowed cell. */
typedef struct CLV_ControlFrag
{
    CONST_STRPTR text;
    UWORD length;
    UWORD width_pixels;
    WORD  relative_x;      /* absolute window X */
} CLV_ControlFrag;

typedef struct CLV_ControlCellWrap
{
    CLV_ControlFrag *frags; /* owned; length frag_count */
    UWORD frag_count;
} CLV_ControlCellWrap;

struct CLV_Control
{
    struct Rectangle bounds;
    struct Rectangle header_bounds;
    struct Rectangle viewport_bounds;

    const CLV_ControlColumn *columns; /* borrowed */
    UWORD column_count;

    const CLV_ControlRow *rows; /* borrowed */
    ULONG row_count;

    CLV_RowLayout *layout_rows; /* owned */
    LONG content_height;
    LONG scroll_y;
    LONG selected_row; /* -1 = none; logical index when selected */
    BOOL keyboard_enabled; /* NAV_* / TOGGLE via handle_input; default TRUE */

    /*
     * Owned checkbox/control-cell snapshot (C2). Row-major:
     * index = row * column_count + col. Length cell_snapshot_count.
     * Copied from CLV_ControlRow.control_cells on set_rows / set_columns;
     * NULL when row_count or column_count is zero.
     */
    CLV_ControlCell *cell_snapshot;
    ULONG cell_snapshot_count;

    /* Private arm state for verified-click commit (C4). */
    BOOL  control_armed;
    LONG  armed_row;
    UWORD armed_column;
    UBYTE armed_type;

    UWORD cell_padding_x;
    UWORD cell_padding_y;
    UWORD row_gap;
    UWORD row_divider_style; /* CLV_ControlRowDividerStyle */
    UWORD line_height;
    UWORD header_height;

    CLV_Pens pens;
    CLV_FontMetrics font_metrics;

    const CLV_DrawOps *draw_ops;
    APTR draw_context;

    struct TextFont *font; /* borrowed; may be NULL */

    /* Column geometry relative to control origin (absolute window coords). */
    CLV_PixelColumn *col_geom; /* owned; length column_count */
    WORD *divider_x;           /* owned; X of each one-pixel divider */
    UWORD divider_count;

    /* Row-major wrap cache: index = row * column_count + col. Owned. */
    CLV_ControlCellWrap *cell_wraps;
    ULONG cell_wrap_count;

    BOOL layout_valid;
};

/* layout — TRUE when layout_valid caches are ready for paint/hit-test. */
BOOL clv_control_layout_rebuild(CLV_Control *c);
VOID clv_control_layout_invalidate(CLV_Control *c);
VOID clv_control_layout_free_wraps(CLV_Control *c);

/* wrap prepare (Phase 3) */
BOOL clv_control_wrap_prepare(CLV_Control *c);

/* hit-test: window coords → logical row, or -1 (gap / miss / header) */
LONG clv_control_hit_test(const CLV_Control *c, WORD x, WORD y);

/* render */
VOID clv_control_render_full(CLV_Control *c);
VOID clv_control_render_viewport(CLV_Control *c);

/*
 * Internal viewport-region painter (window-relative screen coords).
 * Intersects screen_area with viewport_bounds; paints only intersecting
 * logical rows, gaps, and column dividers. Leaves header/frame untouched.
 * Returns FALSE if the intersection is empty or clip setup failed.
 */
BOOL clv_control_paint_viewport_area(CLV_Control *c,
                                     const struct Rectangle *screen_area);

/*
 * Visible content rectangle for a logical row in window-relative coords
 * (excludes row_gap; intersected with the viewport). FALSE if invalid or
 * fully outside the viewport.
 */
BOOL clv_control_get_row_paint_area(const CLV_Control *c,
                                    LONG logical_row,
                                    struct Rectangle *result);

/* Post-scroll paint: smart pixel shift when eligible, else full viewport. */
VOID clv_control_render_scrolled(CLV_Control *c, LONG previous_scroll_y);

/*
 * Checkbox geometry + paint (C3). Resolve uses first-line-band placement
 * (§D.11). Paint reads the owned cell_snapshot only. Hit-test (C4+) must
 * reuse resolve_rect for the same box.
 */
BOOL clv_ctrl_checkbox_resolve_rect(const CLV_Control *c,
                                    LONG logical_row,
                                    UWORD column,
                                    struct Rectangle *out_box);
VOID clv_ctrl_checkbox_paint(CLV_Control *c,
                             LONG logical_row,
                             UWORD column,
                             BOOL selected);

#ifdef __cplusplus
}
#endif

#endif /* CLV_CONTROL_INTERNAL_H */
