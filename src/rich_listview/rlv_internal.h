#ifndef RLV_INTERNAL_H
#define RLV_INTERNAL_H

/**
 * Private control state and helpers. Not for application includes.
 */

#include "rich_listview/rich_listview.h"
#include "rich_listview/rlv_bench_internal.h"

#include <exec/types.h>
#include <graphics/gfx.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Flat one-pixel column separator. */
#define RLV_DIVIDER_WIDTH  1

/* Single dark outline around the whole control (header + viewport). */
#define RLV_FRAME_WIDTH    1

/* Bounded wrap fragments per cell (narrow columns / long strings). */
#define RLV_MAX_FRAGS_PER_CELL  32

/*
 * Internal column geometry in window-relative pixel coordinates.
 * Owned by the control (col_geom); not part of the public API.
 */
typedef struct RLV_PixelColumn
{
    WORD left;
    WORD right;
    WORD text_left;
    WORD text_right;
    UWORD alignment;
    UWORD flags;
} RLV_PixelColumn;

typedef struct RLV_RowLayout
{
    ULONG logical_index;
    LONG top_y;            /* content-space Y (scroll-relative origin 0) */
    UWORD content_height;
    UWORD total_height;    /* content_height + row_gap */
    UWORD maximum_line_count;
    UWORD flags;
} RLV_RowLayout;

/* One wrapped display line within a cell. Text points into borrowed cell. */
typedef struct RLV_Frag
{
    CONST_STRPTR text;
    UWORD length;
    UWORD width_pixels;
    WORD  relative_x;      /* absolute window X */
} RLV_Frag;

typedef struct RLV_CellWrap
{
    RLV_Frag *frags; /* owned; length frag_count */
    UWORD frag_count;
} RLV_CellWrap;

struct RLV_Control
{
    struct Rectangle bounds;
    struct Rectangle header_bounds;
    struct Rectangle viewport_bounds;

    const RLV_Column *columns; /* borrowed */
    UWORD column_count;

    const RLV_Row *rows; /* borrowed */
    ULONG row_count;

    RLV_RowLayout *layout_rows; /* owned */
    LONG content_height;
    LONG scroll_y;
    LONG selected_row; /* -1 = none; logical navigation/current index */
    BOOL keyboard_enabled; /* NAV_* / TOGGLE via handle_input; default TRUE */

    /*
     * Owned checkbox/control-cell snapshot (C2). Row-major:
     * index = row * column_count + col. Length cell_snapshot_count.
     * Copied from RLV_Row.control_cells on set_rows / set_columns;
     * NULL when row_count or column_count is zero.
     */
    RLV_Cell *cell_snapshot;
    ULONG cell_snapshot_count;

    /* Private arm state for verified-click commit (C4). */
    BOOL  control_armed;
    LONG  armed_row;
    UWORD armed_column;
    UBYTE armed_type;

    UWORD cell_padding_x;
    UWORD cell_padding_y;
    UWORD row_gap;
    UWORD row_divider_style; /* RLV_RowDividerStyle */
    UWORD line_height;
    UWORD header_height;

    RLV_Pens pens;
    RLV_FontMetrics font_metrics;

    const RLV_DrawOps *draw_ops;
    APTR draw_context;

    struct TextFont *font; /* borrowed; may be NULL */

    /* Column geometry relative to control origin (absolute window coords). */
    RLV_PixelColumn *col_geom; /* owned; length column_count */
    WORD *divider_x;           /* owned; X of each one-pixel divider */
    UWORD divider_count;

    /* Row-major wrap cache: index = row * column_count + col. Owned. */
    RLV_CellWrap *cell_wraps;
    ULONG cell_wrap_count;

    BOOL layout_valid;

    /* Appended policies (keep at end — safer for incremental rebuilds). */
    UWORD control_activation_policy; /* RLV_ControlActivationPolicy */
    UWORD current_row_visual;        /* RLV_CurrentRowVisual */
};

/* layout — TRUE when layout_valid caches are ready for paint/hit-test. */
BOOL rlv_layout_rebuild(RLV_Control *c);
VOID rlv_layout_invalidate(RLV_Control *c);
VOID rlv_layout_free_wraps(RLV_Control *c);

/* wrap prepare (Phase 3) */
BOOL rlv_wrap_prepare(RLV_Control *c);

/* hit-test: window coords → logical row, or -1 (gap / miss / header) */
LONG rlv_hit_test(const RLV_Control *c, WORD x, WORD y);

/* render */
VOID rlv_render_full(RLV_Control *c);
VOID rlv_render_viewport(RLV_Control *c);

/*
 * Internal viewport-region painter (window-relative screen coords).
 * Intersects screen_area with viewport_bounds; paints only intersecting
 * logical rows, gaps, and column dividers. Leaves header/frame untouched.
 * Returns FALSE if the intersection is empty or clip setup failed.
 */
BOOL rlv_paint_viewport_area(RLV_Control *c,
                                     const struct Rectangle *screen_area);

/*
 * Visible content rectangle for a logical row in window-relative coords
 * (excludes row_gap; intersected with the viewport). FALSE if invalid or
 * fully outside the viewport.
 */
BOOL rlv_get_row_paint_area(const RLV_Control *c,
                                    LONG logical_row,
                                    struct Rectangle *result);

/* Post-scroll paint: smart pixel shift when eligible, else full viewport. */
VOID rlv_render_scrolled(RLV_Control *c, LONG previous_scroll_y);

/*
 * Checkbox geometry + paint (C3). Resolve uses first-line-band placement
 * (§D.11). Paint reads the owned cell_snapshot only. Hit-test (C4+) must
 * reuse resolve_rect for the same box.
 */
BOOL rlv_checkbox_resolve_rect(const RLV_Control *c,
                                    LONG logical_row,
                                    UWORD column,
                                    struct Rectangle *out_box);
VOID rlv_checkbox_paint(RLV_Control *c,
                             LONG logical_row,
                             UWORD column,
                             BOOL selected);

/* TRUE when the logical row should use full selected fill/text pens. */
BOOL rlv_row_uses_selected_fill(const RLV_Control *c, LONG logical_row);

#ifdef __cplusplus
}
#endif

#endif /* RLV_INTERNAL_H */
