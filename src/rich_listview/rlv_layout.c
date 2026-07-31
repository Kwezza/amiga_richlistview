/**
 * Control layout: header/viewport split, column geometry, variable-height rows.
 *
 * Relayout is transactional: owned caches stay live until a full replacement
 * is prepared, then old storage is freed. Failure restores the prior caches.
 */

#include "rich_listview/rlv_internal.h"
#include "rich_listview/rlv_log.h"
#include "rich_listview/rlv_platform_internal.h"

#include <string.h>

VOID rlv_layout_invalidate(RLV_Control *c)
{
    if (c == 0) {
        return;
    }
    c->layout_valid = FALSE;
    /*
     * Structural/layout change cancels verified-click arm (C6 / §D.4).
     * set_rows / set_columns also clear via snapshot refresh; set_bounds
     * clears explicitly. Padding / gap setters reach here only.
     */
    c->control_armed = FALSE;
    c->armed_row = -1;
    c->armed_column = 0;
    c->armed_type = 0;
}

static VOID rlv_layout_free_owned(RLV_Control *c)
{
    if (c == 0) {
        return;
    }
    rlv_layout_free_wraps(c);
    if (c->layout_rows != 0) {
        rlv_platform_free(c->layout_rows);
        c->layout_rows = 0;
    }
    if (c->col_geom != 0) {
        rlv_platform_free(c->col_geom);
        c->col_geom = 0;
    }
    if (c->divider_x != 0) {
        rlv_platform_free(c->divider_x);
        c->divider_x = 0;
    }
    c->divider_count = 0;
}

/*
 * Snapshot of heap-owned layout fields for transactional rebuild.
 * Rectangles and metrics are restored together on failure.
 */
typedef struct RLV_LayoutSnapshot
{
    struct Rectangle header_bounds;
    struct Rectangle viewport_bounds;
    RLV_PixelColumn *col_geom;
    WORD *divider_x;
    UWORD divider_count;
    RLV_RowLayout *layout_rows;
    RLV_CellWrap *cell_wraps;
    ULONG cell_wrap_count;
    LONG content_height;
    UWORD header_height;
    UWORD line_height;
    BOOL layout_valid;
} RLV_LayoutSnapshot;

static VOID rlv_layout_take_snapshot(RLV_Control *c,
                                          RLV_LayoutSnapshot *snap)
{
    if (c == 0 || snap == 0) {
        return;
    }
    snap->header_bounds = c->header_bounds;
    snap->viewport_bounds = c->viewport_bounds;
    snap->col_geom = c->col_geom;
    snap->divider_x = c->divider_x;
    snap->divider_count = c->divider_count;
    snap->layout_rows = c->layout_rows;
    snap->cell_wraps = c->cell_wraps;
    snap->cell_wrap_count = c->cell_wrap_count;
    snap->content_height = c->content_height;
    snap->header_height = c->header_height;
    snap->line_height = c->line_height;
    snap->layout_valid = c->layout_valid;

    /* Detach without freeing — rebuild allocates replacements. */
    c->col_geom = 0;
    c->divider_x = 0;
    c->divider_count = 0;
    c->layout_rows = 0;
    c->cell_wraps = 0;
    c->cell_wrap_count = 0;
    c->content_height = 0;
    c->layout_valid = FALSE;
}

static VOID rlv_layout_restore_snapshot(RLV_Control *c,
                                             RLV_LayoutSnapshot *snap)
{
    if (c == 0 || snap == 0) {
        return;
    }
    /* Drop any partial new allocations. */
    rlv_layout_free_owned(c);

    c->header_bounds = snap->header_bounds;
    c->viewport_bounds = snap->viewport_bounds;
    c->col_geom = snap->col_geom;
    c->divider_x = snap->divider_x;
    c->divider_count = snap->divider_count;
    c->layout_rows = snap->layout_rows;
    c->cell_wraps = snap->cell_wraps;
    c->cell_wrap_count = snap->cell_wrap_count;
    c->content_height = snap->content_height;
    c->header_height = snap->header_height;
    c->line_height = snap->line_height;
    c->layout_valid = snap->layout_valid;

    snap->col_geom = 0;
    snap->divider_x = 0;
    snap->layout_rows = 0;
    snap->cell_wraps = 0;
    snap->cell_wrap_count = 0;
    snap->divider_count = 0;
}

static VOID rlv_layout_discard_snapshot(RLV_LayoutSnapshot *snap)
{
    ULONG i;

    if (snap == 0) {
        return;
    }
    if (snap->cell_wraps != 0) {
        for (i = 0; i < snap->cell_wrap_count; i++) {
            if (snap->cell_wraps[i].frags != 0) {
                rlv_platform_free(snap->cell_wraps[i].frags);
                snap->cell_wraps[i].frags = 0;
            }
            snap->cell_wraps[i].frag_count = 0;
        }
        rlv_platform_free(snap->cell_wraps);
        snap->cell_wraps = 0;
    }
    if (snap->layout_rows != 0) {
        rlv_platform_free(snap->layout_rows);
        snap->layout_rows = 0;
    }
    if (snap->col_geom != 0) {
        rlv_platform_free(snap->col_geom);
        snap->col_geom = 0;
    }
    if (snap->divider_x != 0) {
        rlv_platform_free(snap->divider_x);
        snap->divider_x = 0;
    }
    snap->cell_wrap_count = 0;
    snap->divider_count = 0;
}

static VOID rlv_layout_split_bounds(RLV_Control *c)
{
    WORD header_h;
    WORD max_y;
    WORD inner_min_x;
    WORD inner_max_x;
    WORD inner_min_y;
    WORD inner_max_y;
    WORD frame;

    if (c == 0) {
        return;
    }

    if (c->draw_ops != 0 && c->draw_ops->line_height != 0) {
        c->font_metrics.line_height =
            c->draw_ops->line_height(c->draw_context);
        c->line_height = c->font_metrics.line_height;
    }
    if (c->draw_ops != 0 && c->draw_ops->baseline != 0) {
        c->font_metrics.baseline =
            c->draw_ops->baseline(c->draw_context);
    }

    if (c->line_height < 1) {
        c->line_height = 1;
    }

    c->header_height = (UWORD)(c->line_height
                               + (2 * c->cell_padding_y));
    header_h = (WORD)c->header_height;

    frame = (WORD)RLV_FRAME_WIDTH;
    inner_min_x = (WORD)(c->bounds.MinX + frame);
    inner_max_x = (WORD)(c->bounds.MaxX - frame);
    inner_min_y = (WORD)(c->bounds.MinY + frame);
    inner_max_y = (WORD)(c->bounds.MaxY - frame);
    if (inner_max_x < inner_min_x) {
        inner_max_x = inner_min_x;
    }
    if (inner_max_y < inner_min_y) {
        inner_max_y = inner_min_y;
    }

    c->header_bounds.MinX = inner_min_x;
    c->header_bounds.MaxX = inner_max_x;
    c->header_bounds.MinY = inner_min_y;
    c->viewport_bounds.MinX = inner_min_x;
    c->viewport_bounds.MaxX = inner_max_x;
    c->viewport_bounds.MaxY = inner_max_y;

    max_y = inner_max_y;
    if (header_h <= 0) {
        c->header_bounds.MaxY = (WORD)(inner_min_y - 1); /* empty */
        c->viewport_bounds.MinY = inner_min_y;
        return;
    }

    if ((WORD)(inner_min_y + header_h - 1) > max_y) {
        /* Degenerate: header consumes all inner space. */
        c->header_bounds.MaxY = max_y;
        c->viewport_bounds.MinY = (WORD)(max_y + 1);
        c->viewport_bounds.MaxY = max_y;
        return;
    }

    c->header_bounds.MaxY = (WORD)(inner_min_y + header_h - 1);
    c->viewport_bounds.MinY = (WORD)(c->header_bounds.MaxY + 1);
}

static VOID rlv_finish_column_text(RLV_PixelColumn *col, WORD inset)
{
    if (col == 0) {
        return;
    }
    col->text_left = (WORD)(col->left + inset);
    col->text_right = (WORD)(col->right - inset);
    if (col->text_left > col->text_right) {
        col->text_left = col->left;
        col->text_right = col->right;
    }
}

/*
 * Column policy: width_pixels is a fixed minimum pixel width.
 * Columns 0..n-2 stay exactly at width_pixels (never shrink/stretch).
 * The last column grows to fill leftover viewport width up to MaxX
 * (flush next to the scrollbar); it never shrinks below width_pixels.
 * Dividers stay reserved. The demo WA_MinWidth keeps the viewport at
 * least the configured column + divider sum so earlier columns never
 * collapse.
 */
static BOOL rlv_layout_columns(RLV_Control *c)
{
    UWORD i;
    WORD x;
    WORD w;
    WORD inset;
    WORD content_right;
    UWORD div_count;
    UWORD last;

    if (c == 0) {
        return FALSE;
    }
    RLV_BENCH_ADD(RLV_BENCH_COUNTER_COLUMNS_PROCESSED, c->column_count);

    /* Owned caches were detached by the transactional rebuild path. */
    c->col_geom = 0;
    c->divider_x = 0;
    c->divider_count = 0;

    if (c->column_count == 0 || c->columns == 0) {
        return TRUE;
    }

    c->col_geom = (RLV_PixelColumn *)rlv_platform_malloc(
        (size_t)c->column_count * sizeof(RLV_PixelColumn));
    if (c->col_geom == 0) {
        return FALSE;
    }
    memset(c->col_geom, 0,
           (size_t)c->column_count * sizeof(RLV_PixelColumn));

    div_count = (c->column_count > 1)
                ? (UWORD)(c->column_count - 1)
                : 0;
    if (div_count > 0) {
        c->divider_x = (WORD *)rlv_platform_malloc(
            (size_t)div_count * sizeof(WORD));
        if (c->divider_x == 0) {
            rlv_platform_free(c->col_geom);
            c->col_geom = 0;
            return FALSE;
        }
        c->divider_count = div_count;
    }

    x = c->viewport_bounds.MinX;
    content_right = c->viewport_bounds.MaxX;
    inset = (WORD)c->cell_padding_x;

    for (i = 0; i < c->column_count; i++) {
        w = c->columns[i].width_pixels;
        if (w < 1) {
            w = 1;
        }

        c->col_geom[i].left = x;
        c->col_geom[i].right = (WORD)(x + w - 1);
        /* Do not clip earlier columns — fixed minima must remain intact. */

        rlv_finish_column_text(&c->col_geom[i], inset);
        c->col_geom[i].alignment = c->columns[i].alignment;
        c->col_geom[i].flags = 0;

        x = (WORD)(c->col_geom[i].right + 1);

        if (i + 1 < c->column_count) {
            if (c->divider_x != 0) {
                c->divider_x[i] = x;
            }
            x = (WORD)(x + RLV_DIVIDER_WIDTH);
        }
    }

    /* Grow-only: absorb spare viewport width into the final column. */
    last = (UWORD)(c->column_count - 1);
    if (c->col_geom[last].right < content_right
        && c->col_geom[last].left <= content_right) {
        c->col_geom[last].right = content_right;
        rlv_finish_column_text(&c->col_geom[last], inset);
    }

    return TRUE;
}

static BOOL rlv_layout_rows(RLV_Control *c)
{
    ULONG i;
    UWORD col;
    LONG top;
    UWORD line_h;
    UWORD max_lines;
    UWORD content_h;
    UWORD total_h;
    ULONG idx;
    UWORD frag_count;

    if (c == 0) {
        return FALSE;
    }

    RLV_BENCH_ADD(RLV_BENCH_COUNTER_LOGICAL_ROWS, c->row_count);

    c->layout_rows = 0;

    line_h = c->line_height;
    if (line_h < 1) {
        line_h = 1;
    }

    if (c->row_count == 0) {
        c->content_height = 0;
        return TRUE;
    }

    if (!rlv_wrap_prepare(c)) {
        return FALSE;
    }

    c->layout_rows = (RLV_RowLayout *)rlv_platform_malloc(
        (size_t)c->row_count * sizeof(RLV_RowLayout));
    if (c->layout_rows == 0) {
        rlv_layout_free_wraps(c);
        return FALSE;
    }
    memset(c->layout_rows, 0,
           (size_t)c->row_count * sizeof(RLV_RowLayout));

    top = 0;
    for (i = 0; i < c->row_count; i++) {
        RLV_BENCH_COUNT(RLV_BENCH_COUNTER_ROW_HEIGHT_CALCS);
        max_lines = 1;
#if defined(RLV_ENABLE_EXPANDABLE_ROWS) && (RLV_ENABLE_EXPANDABLE_ROWS != 0)
        if (rlv_row_is_collapsed_compact(c, (LONG)i)) {
            /* Intentional one-line compact height; wrap cache retained. */
            max_lines = 1;
        } else
#endif
        {
            for (col = 0; col < c->column_count; col++) {
                idx = i * (ULONG)c->column_count + (ULONG)col;
                frag_count = 0;
                if (c->cell_wraps != 0 && idx < c->cell_wrap_count) {
                    frag_count = c->cell_wraps[idx].frag_count;
                }
                if (frag_count > max_lines) {
                    max_lines = frag_count;
                }
            }
        }

        content_h = (UWORD)((max_lines * line_h)
                            + (2 * c->cell_padding_y));
        total_h = (UWORD)(content_h + c->row_gap);

        c->layout_rows[i].logical_index = i;
        c->layout_rows[i].top_y = top;
        c->layout_rows[i].content_height = content_h;
        c->layout_rows[i].total_height = total_h;
        c->layout_rows[i].maximum_line_count = max_lines;
        c->layout_rows[i].flags = 0;
        top += (LONG)total_h;
        if (max_lines > 1) {
            RLV_BENCH_ADD(RLV_BENCH_COUNTER_WRAPPED_CONTINUATION_ROWS,
                          (ULONG)(max_lines - 1));
        }
    }
    c->content_height = top;
    return TRUE;
}

#if defined(RLV_ENABLE_EXPANDABLE_ROWS) && (RLV_ENABLE_EXPANDABLE_ROWS != 0)
/*
 * After an expand/collapse state change: recompute heights and top_y from
 * from_row onward without rewrapping. Entries before from_row stay valid.
 */
BOOL rlv_layout_reheight_from(RLV_Control *c, ULONG from_row)
{
    ULONG i;
    UWORD col;
    LONG top;
    UWORD line_h;
    UWORD max_lines;
    UWORD content_h;
    UWORD total_h;
    ULONG idx;
    UWORD frag_count;

    if (c == 0 || c->layout_rows == 0) {
        return FALSE;
    }
    if (from_row > c->row_count) {
        return FALSE;
    }
    if (c->row_count == 0) {
        c->content_height = 0;
        return TRUE;
    }

    line_h = c->line_height;
    if (line_h < 1) {
        line_h = 1;
    }

    if (from_row == 0) {
        top = 0;
    } else {
        top = c->layout_rows[from_row - 1].top_y
              + (LONG)c->layout_rows[from_row - 1].total_height;
    }

    for (i = from_row; i < c->row_count; i++) {
        RLV_BENCH_COUNT(RLV_BENCH_COUNTER_ROW_HEIGHT_CALCS);
        max_lines = 1;
        if (rlv_row_is_collapsed_compact(c, (LONG)i)) {
            max_lines = 1;
        } else {
            for (col = 0; col < c->column_count; col++) {
                idx = i * (ULONG)c->column_count + (ULONG)col;
                frag_count = 0;
                if (c->cell_wraps != 0 && idx < c->cell_wrap_count) {
                    frag_count = c->cell_wraps[idx].frag_count;
                }
                if (frag_count > max_lines) {
                    max_lines = frag_count;
                }
            }
        }

        content_h = (UWORD)((max_lines * line_h)
                            + (2 * c->cell_padding_y));
        total_h = (UWORD)(content_h + c->row_gap);

        c->layout_rows[i].logical_index = i;
        c->layout_rows[i].top_y = top;
        c->layout_rows[i].content_height = content_h;
        c->layout_rows[i].total_height = total_h;
        c->layout_rows[i].maximum_line_count = max_lines;
        c->layout_rows[i].flags = 0;
        top += (LONG)total_h;
    }

    c->content_height = top;
    RLV_LOGF("LAYOUT reheight_from=%lu content_h=%ld",
             (unsigned long)from_row, (long)c->content_height);
    return TRUE;
}
#endif /* RLV_ENABLE_EXPANDABLE_ROWS */

/*
 * Non-transactional rebuild used when layout is already invalid and no
 * prior caches must be preserved (first layout, or after invalidate).
 */
static BOOL rlv_layout_rebuild_fresh(RLV_Control *c)
{
    if (c == 0) {
        return FALSE;
    }

    rlv_layout_split_bounds(c);

    if (!rlv_layout_columns(c)) {
        rlv_layout_free_owned(c);
        c->layout_valid = FALSE;
        return FALSE;
    }
    if (!rlv_layout_rows(c)) {
        rlv_layout_free_owned(c);
        c->layout_valid = FALSE;
        return FALSE;
    }

    c->layout_valid = TRUE;
    return TRUE;
}

BOOL rlv_layout_rebuild(RLV_Control *c)
{
    RLV_LayoutSnapshot snap;
    BOOL had_valid;
    RLV_BENCH_DECLARE(bench_column_layout);
    RLV_BENCH_DECLARE(bench_row_height);

    if (c == 0) {
        return FALSE;
    }

    RLV_BENCH_BEGIN(RLV_BENCH_COLUMN_LAYOUT, bench_column_layout);
    RLV_BENCH_BEGIN(RLV_BENCH_ROW_HEIGHT_CALC, bench_row_height);

    had_valid = c->layout_valid
                && (c->col_geom != 0 || c->column_count == 0);

    if (!had_valid) {
        /* Drop any stale partial state, then build from scratch. */
        rlv_layout_free_owned(c);
        {
            BOOL fresh_ok = rlv_layout_rebuild_fresh(c);
            RLV_BENCH_END(RLV_BENCH_ROW_HEIGHT_CALC, bench_row_height);
            RLV_BENCH_END(RLV_BENCH_COLUMN_LAYOUT, bench_column_layout);
            return fresh_ok;
        }
    }

    memset(&snap, 0, sizeof(snap));
    rlv_layout_take_snapshot(c, &snap);

    rlv_layout_split_bounds(c);
    RLV_LOGF("RESIZE proposed viewport=%d,%d-%d,%d",
             (int)c->viewport_bounds.MinX,
             (int)c->viewport_bounds.MinY,
             (int)c->viewport_bounds.MaxX,
             (int)c->viewport_bounds.MaxY);

    if (!rlv_layout_columns(c)
        || !rlv_layout_rows(c)) {
        RLV_LOG("RESIZE fallback/failure reason=layout_prepare");
        rlv_layout_restore_snapshot(c, &snap);
        RLV_BENCH_END(RLV_BENCH_ROW_HEIGHT_CALC, bench_row_height);
        RLV_BENCH_END(RLV_BENCH_COLUMN_LAYOUT, bench_column_layout);
        return FALSE;
    }

    c->layout_valid = TRUE;
    rlv_layout_discard_snapshot(&snap);
    RLV_BENCH_END(RLV_BENCH_ROW_HEIGHT_CALC, bench_row_height);
    RLV_BENCH_END(RLV_BENCH_COLUMN_LAYOUT, bench_column_layout);
    return TRUE;
}
