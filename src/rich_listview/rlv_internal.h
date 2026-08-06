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

/* RLV_CellWrap.flags */
#define RLV_CELLWRAP_F_HORIZ_CLIPPED  0x0001U /* undisplayed source remains */

typedef struct RLV_CellWrap
{
    RLV_Frag *frags; /* owned; length frag_count */
    UWORD frag_count;
    UWORD flags;     /* RLV_CELLWRAP_F_* */
} RLV_CellWrap;

/* Compact three-dot ellipsis metrics (hand-drawn via fill_rect). */
#define RLV_ELLIPSIS_DOT_STEP   2  /* pixel centres at 0, 2, 4 */
#define RLV_ELLIPSIS_WIDTH_PX   5  /* inclusive span of three dots */
#define RLV_ELLIPSIS_TEXT_GAP   2  /* gap between fitted text and dots */

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
    UWORD row_display_mode;          /* RLV_RowDisplayMode */
    UWORD long_word_mode;            /* RLV_LongWordMode */
    UWORD ellipsis_flags;            /* RLV_ELLIPSIS_* */
    UWORD initial_expand;            /* RLV_InitialExpandMode */
    BOOL  apply_initial_expand;      /* TRUE until first set_rows after create */

#if defined(RLV_ENABLE_EXPANDABLE_ROWS) && (RLV_ENABLE_EXPANDABLE_ROWS != 0)
    /*
     * Owned per-row expand snapshot (length == row_count). Copied from
     * RLV_Row.flags on set_rows; mutated by expand APIs / disclosure input.
     * Does not write through borrowed RLV_Row memory.
     */
    UBYTE *row_expand;
    ULONG row_expand_count;
    /*
     * One-shot hint for rlv_render_from_row blit: total_height before the
     * last expand/collapse reheight. 0 = none / consumed.
     */
    LONG expand_old_total_h;
#endif

#if defined(RLV_ENABLE_SORTING) && (RLV_ENABLE_SORTING != 0)
    /*
     * Optional view-order map (UWORD indices). NULL = identity (attachment
     * order). Allocated only when sorting is configured / applied.
     * view_to_source[view] = source; source_to_view[source] = view.
     * Specs are borrowed. Active sort: sort_active != 0.
     */
    UWORD *view_to_source;
    UWORD *source_to_view;
    ULONG sort_map_count;
    const RLV_SortSpec *sort_specs; /* borrowed */
    UWORD sort_spec_count;
    UWORD sort_column;     /* meaningful when sort_active */
    UWORD sort_direction;  /* RLV_SORT_ASC / DESC */
    UWORD sort_active;     /* 0 = identity attachment order */
#endif

#if defined(RLV_ENABLE_COLUMN_RESIZE) && (RLV_ENABLE_COLUMN_RESIZE != 0)
    /*
     * Control-owned runtime content widths / minima (length == column_count).
     * Copied from RLV_Column.width_pixels on set_columns. Layout reads these
     * instead of borrowed columns[]. Never written back to app columns.
     */
    WORD *runtime_widths;
    WORD *runtime_mins;
    UWORD runtime_width_count;
    BOOL column_resize_enabled;
    /* Drag session (preview only until commit). */
    BOOL resize_dragging;
    BOOL resize_pointer_wanted; /* host should show horizontal resize pointer */
    BOOL resize_guide_visible;
    UWORD resize_left_col;
    UWORD resize_right_col;
    WORD resize_orig_left;
    WORD resize_orig_right;
    WORD resize_pair_total;
    WORD resize_preview_left_w;
    WORD resize_guide_x;
    WORD resize_press_x;
#endif
};

/* Internal expand-state bits in row_expand[]. */
#define RLV_ROWEXP_EXPANDABLE  0x01U
#define RLV_ROWEXP_EXPANDED    0x02U

/* Source flags for rlv_set_row_expanded (may be combined). */
#define RLV_EXPAND_SRC_API     0x0001UL /* programmatic; no CELL_CONTROL */
#define RLV_EXPAND_SRC_MOUSE   0x0002UL /* emit CELL_CONTROL when changed */
#define RLV_EXPAND_SRC_KEY     0x0004UL /* emit CELL_CONTROL when changed */
#define RLV_EXPAND_SRC_BULK    0x0008UL /* defer reheight/scroll/anchor */

/* layout — TRUE when layout_valid caches are ready for paint/hit-test. */
BOOL rlv_layout_rebuild(RLV_Control *c);
VOID rlv_layout_invalidate(RLV_Control *c);
VOID rlv_layout_free_wraps(RLV_Control *c);
#if defined(RLV_ENABLE_EXPANDABLE_ROWS) && (RLV_ENABLE_EXPANDABLE_ROWS != 0)
/* Recompute row heights / top_y from from_row onward; keep wrap cache. */
BOOL rlv_layout_reheight_from(RLV_Control *c, ULONG from_row);
#endif

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

#if defined(RLV_ENABLE_EXPANDABLE_ROWS) && (RLV_ENABLE_EXPANDABLE_ROWS != 0)
BOOL rlv_disclosure_resolve_rect(const RLV_Control *c,
                                      LONG logical_row,
                                      UWORD column,
                                      struct Rectangle *out_box);
VOID rlv_disclosure_paint(RLV_Control *c,
                               LONG logical_row,
                               UWORD column,
                               BOOL selected);

/*
 * Central expand-state transition. Validates expandable; no-op same-state
 * returns TRUE. With SRC_BULK, only mutates row_expand[]. Otherwise rebuilds
 * heights from the row, anchors viewport, and may fill CELL_CONTROL when
 * SRC_MOUSE or SRC_KEY and state actually changed.
 */
BOOL rlv_set_row_expanded(RLV_Control *c,
                               LONG row,
                               BOOL expanded,
                               ULONG source_flags,
                               RLV_Event *result);

BOOL rlv_row_is_collapsed_compact(const RLV_Control *c, LONG logical_row);
/*
 * TRUE when the wrap cache has more than one display line in any cell.
 * Disclosure +/- is suppressed when FALSE so single-line expandable rows
 * do not show a no-op control (resize may later produce multi-line wrap).
 */
BOOL rlv_row_has_multi_line_wrap(const RLV_Control *c, LONG logical_row);
VOID rlv_free_row_expand(RLV_Control *c);
BOOL rlv_refresh_row_expand(RLV_Control *c);
#endif

/*
 * TRUE when disclosure glyphs, hit targets, and expand/collapse input are
 * active (COLLAPSIBLE row-display mode only). Expand snapshot bits remain
 * owned while this returns FALSE.
 */
BOOL rlv_disclosure_ui_enabled(const RLV_Control *c);

/* TRUE when the logical row should use full selected fill/text pens. */
BOOL rlv_row_uses_selected_fill(const RLV_Control *c, LONG logical_row);

/*
 * Set event->row and event->row_user_data from a logical row index.
 * Copies the borrowed RLV_Row.user_data tag when the row is in range;
 * otherwise sets row_user_data to NULL. Does not touch other event fields.
 */
VOID rlv_event_set_row(const RLV_Control *control,
                             RLV_Event *event,
                             LONG logical_row);

/*
 * View-order helpers. When sorting is disabled or the map is inactive,
 * view == source (identity). layout_rows[] is indexed by view position;
 * logical_index / public row APIs use source (attachment) indices.
 */
#if defined(RLV_ENABLE_SORTING) && (RLV_ENABLE_SORTING != 0)
ULONG rlv_source_for_view(const RLV_Control *c, ULONG view);
LONG rlv_view_for_source(const RLV_Control *c, LONG source);
VOID rlv_sort_free_maps(RLV_Control *c);
/* After set_rows: drop maps and clear active sort; keep borrowed specs. */
VOID rlv_sort_on_rows_replaced(RLV_Control *c);
/* Header click: may sort and fill SORT_CHANGED. TRUE if handled. */
BOOL rlv_sort_handle_header_click(RLV_Control *c,
                                  WORD x,
                                  WORD y,
                                  RLV_Event *result);
/* Extra right inset for the active sort column title (pixels). */
UWORD rlv_sort_header_reserve_px(const RLV_Control *c, UWORD column);
VOID rlv_sort_draw_indicator(RLV_Control *c,
                             UWORD column,
                             WORD cell_left,
                             WORD cell_right,
                             WORD header_top,
                             WORD header_bottom);
#else
#define rlv_source_for_view(c, view) ((ULONG)(view))
#define rlv_view_for_source(c, source) ((LONG)(source))
#endif

#if defined(RLV_ENABLE_COLUMN_RESIZE) && (RLV_ENABLE_COLUMN_RESIZE != 0)
/* Default minimum content width when no per-column override is set. */
#define RLV_COL_RESIZE_MIN_DEFAULT  16
/* Half-width of the divider hit zone (pixels each side of divider_x). */
#define RLV_COL_RESIZE_HIT_SLACK    3
/* Drag preview / commit quantisation step (pixels), relative to press. */
#define RLV_COLUMN_RESIZE_STEP      4

VOID rlv_column_resize_free(RLV_Control *c);
/* After set_columns: alloc/copy widths from borrowed columns. */
BOOL rlv_column_resize_on_columns_set(RLV_Control *c);
/* Erase guide / clear drag without committing (teardown, set_rows, cancel). */
VOID rlv_column_resize_cancel(RLV_Control *c, BOOL erase_visual);
/*
 * SELECT_DOWN: TRUE if a divider drag was armed (consumes the click).
 * Does not fill an application event.
 */
BOOL rlv_column_resize_handle_select_down(RLV_Control *c, WORD x, WORD y);
/* POINTER_MOVE while not dragging: updates hover pointer state only. */
VOID rlv_column_resize_handle_hover_move(RLV_Control *c, WORD x, WORD y);
/* POINTER_MOVE while dragging: updates preview; no event. */
VOID rlv_column_resize_handle_pointer_move(RLV_Control *c, WORD x, WORD y);
/*
 * SELECT_UP while dragging: commit or no-op. TRUE if COLUMN_RESIZED filled.
 */
BOOL rlv_column_resize_handle_select_up(RLV_Control *c,
                                        WORD x,
                                        WORD y,
                                        RLV_Event *result);
/* CANCEL / teardown path while dragging. */
VOID rlv_column_resize_handle_cancel(RLV_Control *c);

/* Effective content width for layout (runtime copy, else column width). */
WORD rlv_column_effective_width(const RLV_Control *c, UWORD column);

/* Internal: redraw one header cell from committed geometry. */
VOID rlv_render_header_column(RLV_Control *c, UWORD column);
/*
 * Redraw the intersection of one header column with screen_area (committed
 * style). Available for regional header repair; the drag preview path no
 * longer uses it (avoids flashing the committed divider / sort glyph).
 */
VOID rlv_render_header_column_area(RLV_Control *c,
                                   UWORD column,
                                   const struct Rectangle *screen_area);
#else
#define rlv_column_effective_width(c, column) \
    (((c) != 0 && (c)->columns != 0 && (column) < (c)->column_count) \
     ? ((c)->columns[(column)].width_pixels < 1 \
        ? (WORD)1 : (c)->columns[(column)].width_pixels) \
     : (WORD)1)
#endif

#ifdef __cplusplus
}
#endif

#endif /* RLV_INTERNAL_H */
