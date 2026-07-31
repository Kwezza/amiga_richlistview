/**
 * Control renderer — Phase 3 fixed header + variable-height wrapped rows.
 */

#include "rich_listview/rlv_internal.h"
#include "rich_listview/rlv_log.h"

#include <string.h>

static UWORD rlv_strlen(CONST_STRPTR s)
{
    UWORD n;

    if (s == 0) {
        return 0;
    }
    n = 0;
    while (s[n] != '\0' && n < 4096) {
        n++;
    }
    return n;
}

static UWORD rlv_fit_chars(RLV_Control *c,
                                CONST_STRPTR text,
                                UWORD length,
                                UWORD max_width)
{
    UWORD lo;
    UWORD hi;
    UWORD mid;
    UWORD w;

    if (c == 0 || c->draw_ops == 0 || text == 0 || length == 0
        || max_width == 0) {
        return 0;
    }

    if (c->draw_ops->text_fit != 0) {
        RLV_BENCH_COUNT(RLV_BENCH_COUNTER_TEXT_MEASURE_CALLS);
        return c->draw_ops->text_fit(c->draw_context, text, length,
                                     max_width);
    }

    if (c->draw_ops->text_width == 0) {
        return 0;
    }

    if (c->draw_ops->text_width(c->draw_context, text, length) <= max_width) {
        RLV_BENCH_COUNT(RLV_BENCH_COUNTER_TEXT_MEASURE_CALLS);
        return length;
    }

    lo = 0;
    hi = length;
    while (lo < hi) {
        mid = (UWORD)((lo + hi + 1) / 2);
        RLV_BENCH_COUNT(RLV_BENCH_COUNTER_TEXT_MEASURE_CALLS);
        w = c->draw_ops->text_width(c->draw_context, text, mid);
        if (w <= max_width) {
            lo = mid;
        } else {
            hi = (UWORD)(mid - 1);
        }
    }
    return lo;
}

static WORD rlv_align_x(UWORD alignment,
                             WORD text_left,
                             WORD text_right,
                             UWORD text_w)
{
    WORD span;
    WORD x;

    span = (WORD)(text_right - text_left + 1);
    if (span < 1) {
        span = 1;
    }
    switch (alignment) {
    case RLV_CELL_ALIGN_RIGHT:
        x = (WORD)(text_right - (WORD)text_w + 1);
        break;
    case RLV_CELL_ALIGN_CENTER:
        x = (WORD)(text_left + ((span - (WORD)text_w) / 2));
        break;
    case RLV_CELL_ALIGN_LEFT:
    default:
        x = text_left;
        break;
    }
    if (x < text_left) {
        x = text_left;
    }
    return x;
}

/* Flat one-pixel shadow outline around the complete control. */
static VOID rlv_draw_outer_frame(RLV_Control *c)
{
    const RLV_DrawOps *ops;
    APTR ctx;
    WORD x1;
    WORD y1;
    WORD x2;
    WORD y2;

    if (c == 0 || c->draw_ops == 0) {
        return;
    }

    ops = c->draw_ops;
    ctx = c->draw_context;
    x1 = c->bounds.MinX;
    y1 = c->bounds.MinY;
    x2 = c->bounds.MaxX;
    y2 = c->bounds.MaxY;
    if (x2 < x1 || y2 < y1) {
        return;
    }

    ops->set_pens(ctx, c->pens.shadow, c->pens.background);
    ops->draw_line(ctx, x1, y1, x2, y1);
    ops->draw_line(ctx, x1, y1, x1, y2);
    ops->draw_line(ctx, x1, y2, x2, y2);
    ops->draw_line(ctx, x2, y1, x2, y2);
}

static WORD rlv_cell_right(const RLV_Control *c, UWORD column)
{
    if (c == 0) {
        return 0;
    }
    if (c->divider_x != 0 && column < c->divider_count) {
        return c->divider_x[column];
    }
    return c->viewport_bounds.MaxX;
}

/*
 * Draw the complete dark box first, then overlay its top and left edges with
 * shine. This leaves the dark right edge starting one pixel below the top and
 * makes adjacent top highlights join into one continuous line.
 */
static VOID rlv_draw_cell_frame(RLV_Control *c,
                                     WORD x1,
                                     WORD y1,
                                     WORD x2,
                                     WORD y2)
{
    const RLV_DrawOps *ops;
    APTR ctx;

    if (c == 0 || c->draw_ops == 0 || x2 < x1 || y2 < y1) {
        return;
    }
    ops = c->draw_ops;
    ctx = c->draw_context;

    ops->set_pens(ctx, c->pens.separator, c->pens.background);
    ops->draw_line(ctx, x1, y1, x2, y1);
    ops->draw_line(ctx, x1, y1, x1, y2);
    ops->draw_line(ctx, x1, y2, x2, y2);
    ops->draw_line(ctx, x2, y1, x2, y2);

    ops->set_pens(ctx, c->pens.shine, c->pens.background);
    ops->draw_line(ctx, x1, y1, x2, y1);
    ops->draw_line(ctx, x1, y1, x1, y2);
}

static VOID rlv_draw_body_cell_verticals(RLV_Control *c,
                                              WORD x1,
                                              WORD y1,
                                              WORD x2,
                                              WORD y2)
{
    const RLV_DrawOps *ops;
    APTR ctx;

    if (c == 0 || c->draw_ops == 0 || x2 < x1 || y2 < y1) {
        return;
    }
    ops = c->draw_ops;
    ctx = c->draw_context;

    ops->set_pens(ctx, c->pens.separator, c->pens.background);
    ops->draw_line(ctx, x1, y1, x1, y2);
    ops->draw_line(ctx, x2, y1, x2, y2);

    ops->set_pens(ctx, c->pens.shine, c->pens.background);
    ops->draw_line(ctx, x1, y1, x1, y2);
}

static VOID rlv_draw_row_divider(RLV_Control *c,
                                      ULONG layout_index,
                                      WORD y)
{
    const RLV_DrawOps *ops;
    APTR ctx;
    WORD x;
    WORD min_x;
    WORD max_x;
    LONG next_x;

    if (c == 0 || c->draw_ops == 0
        || layout_index + 1 >= c->row_count
        || c->row_divider_style == (UWORD)RLV_ROW_DIVIDER_NONE) {
        return;
    }

    ops = c->draw_ops;
    ctx = c->draw_context;
    min_x = (WORD)(c->viewport_bounds.MinX + 1);
    max_x = (WORD)(c->viewport_bounds.MaxX - 1);
    if (max_x < min_x) {
        return;
    }
    ops->set_pens(ctx, c->pens.separator, c->pens.background);

    if (c->row_divider_style == (UWORD)RLV_ROW_DIVIDER_DOTTED) {
        if (ops->draw_dotted_hline != 0) {
            ops->draw_dotted_hline(ctx, min_x, max_x, y);
            return;
        }
        x = min_x;
        while (x <= max_x) {
            ops->draw_line(ctx, x, y, x, y);
            next_x = (LONG)x + 2L;
            if (next_x > (LONG)max_x || next_x > 32767L) {
                break;
            }
            x = (WORD)next_x;
        }
        return;
    }

    ops->draw_line(ctx, min_x, y, max_x, y);
}

/* Header titles remain single-line truncated. */
static VOID rlv_draw_cell_text(RLV_Control *c,
                                    CONST_STRPTR text,
                                    const RLV_PixelColumn *col,
                                    WORD baseline_y,
                                    UWORD text_pen,
                                    UWORD back_pen)
{
    const RLV_DrawOps *ops;
    APTR ctx;
    UWORD len;
    UWORD fit;
    UWORD max_w;
    UWORD tw;
    WORD x;

    if (c == 0 || c->draw_ops == 0 || col == 0) {
        return;
    }
    if (text == 0 || text[0] == '\0') {
        return;
    }

    ops = c->draw_ops;
    ctx = c->draw_context;
    len = rlv_strlen(text);
    max_w = 0;
    if (col->text_right >= col->text_left) {
        max_w = (UWORD)(col->text_right - col->text_left + 1);
    }
    fit = rlv_fit_chars(c, text, len, max_w);
    if (fit == 0) {
        return;
    }

    tw = ops->text_width(ctx, text, fit);
    x = rlv_align_x(col->alignment,
                         col->text_left,
                         col->text_right,
                         tw);

    ops->set_pens(ctx, text_pen, back_pen);
    ops->draw_text(ctx, x, baseline_y, text, fit);
}

static VOID rlv_draw_header(RLV_Control *c)
{
    const RLV_DrawOps *ops;
    APTR ctx;
    UWORD i;
    WORD x1, x2;
    WORD y1, y2;
    WORD baseline;
    CONST_STRPTR title;

    RLV_LOG("header render begin");
    if (c == 0 || c->draw_ops == 0) {
        RLV_LOG("header render end (null)");
        return;
    }
    if (c->header_bounds.MaxY < c->header_bounds.MinY) {
        RLV_LOG("header render end (empty bounds)");
        return;
    }

    ops = c->draw_ops;
    ctx = c->draw_context;
    y1 = c->header_bounds.MinY;
    y2 = c->header_bounds.MaxY;

    /* Header face fill */
    ops->set_pens(ctx, c->pens.background, c->pens.background);
    ops->fill_rect(ctx,
                   c->header_bounds.MinX,
                   y1,
                   c->header_bounds.MaxX,
                   y2);

    baseline = (WORD)(y1 + (WORD)c->cell_padding_y
                      + c->font_metrics.baseline);

    for (i = 0; i < c->column_count && c->col_geom != 0; i++) {
        x1 = c->col_geom[i].left;
        x2 = rlv_cell_right(c, i);
        if (x2 < x1) {
            continue;
        }

        rlv_draw_cell_frame(c, x1, y1, x2, y2);
        title = (c->columns != 0) ? c->columns[i].title : 0;
        rlv_draw_cell_text(c, title, &c->col_geom[i], baseline,
                                c->pens.text, c->pens.background);
    }
    RLV_LOG("header render end");
}

/* result = a ∩ b; returns FALSE if empty. */
static BOOL rlv_intersect_rects(const struct Rectangle *a,
                                     const struct Rectangle *b,
                                     struct Rectangle *result)
{
    if (a == 0 || b == 0 || result == 0) {
        return FALSE;
    }
    if (a->MaxX < a->MinX || a->MaxY < a->MinY
        || b->MaxX < b->MinX || b->MaxY < b->MinY) {
        return FALSE;
    }

    result->MinX = (a->MinX > b->MinX) ? a->MinX : b->MinX;
    result->MinY = (a->MinY > b->MinY) ? a->MinY : b->MinY;
    result->MaxX = (a->MaxX < b->MaxX) ? a->MaxX : b->MaxX;
    result->MaxY = (a->MaxY < b->MaxY) ? a->MaxY : b->MaxY;
    return (result->MaxX >= result->MinX
            && result->MaxY >= result->MinY) ? TRUE : FALSE;
}

/*
 * Content rectangle for layout row i in window coords (no gap, no viewport
 * clip). Returns FALSE if content height is empty.
 */
static BOOL rlv_row_content_screen_rect(const RLV_Control *c,
                                            ULONG layout_index,
                                            struct Rectangle *out)
{
    LONG content_y;
    WORD row_top;
    WORD row_bottom;

    if (c == 0 || out == 0 || c->layout_rows == 0
        || layout_index >= c->row_count) {
        return FALSE;
    }
    if (c->layout_rows[layout_index].content_height < 1) {
        return FALSE;
    }

    content_y = c->layout_rows[layout_index].top_y - c->scroll_y;
    row_top = (WORD)(c->viewport_bounds.MinY + (WORD)content_y);
    row_bottom = (WORD)(row_top
                        + (WORD)c->layout_rows[layout_index].content_height
                        - 1);
    out->MinX = c->viewport_bounds.MinX;
    out->MaxX = c->viewport_bounds.MaxX;
    out->MinY = row_top;
    out->MaxY = row_bottom;
    return TRUE;
}

static BOOL rlv_row_gap_screen_rect(const RLV_Control *c,
                                         ULONG layout_index,
                                         struct Rectangle *out)
{
    struct Rectangle content;
    WORD gap_top;
    WORD gap_bottom;

    if (c == 0 || out == 0 || c->row_gap < 1) {
        return FALSE;
    }
    if (!rlv_row_content_screen_rect(c, layout_index, &content)) {
        return FALSE;
    }

    gap_top = (WORD)(content.MaxY + 1);
    gap_bottom = (WORD)(gap_top + (WORD)c->row_gap - 1);
    out->MinX = c->viewport_bounds.MinX;
    out->MaxX = c->viewport_bounds.MaxX;
    out->MinY = gap_top;
    out->MaxY = gap_bottom;
    return TRUE;
}

BOOL rlv_get_row_paint_area(const RLV_Control *c,
                                    LONG logical_row,
                                    struct Rectangle *result)
{
    struct Rectangle content;
    struct Rectangle clipped;

    if (c == 0 || result == 0 || c->layout_rows == 0) {
        return FALSE;
    }
    if (logical_row < 0 || (ULONG)logical_row >= c->row_count) {
        return FALSE;
    }
    if (c->viewport_bounds.MaxY < c->viewport_bounds.MinY
        || c->viewport_bounds.MaxX < c->viewport_bounds.MinX) {
        return FALSE;
    }

    if (!rlv_row_content_screen_rect(c, (ULONG)logical_row, &content)) {
        return FALSE;
    }
    if (!rlv_intersect_rects(&content, &c->viewport_bounds, &clipped)) {
        return FALSE;
    }

    *result = clipped;
    return TRUE;
}

static VOID rlv_draw_frag(RLV_Control *c,
                               const RLV_Frag *frag,
                               WORD baseline_y,
                               UWORD text_pen,
                               UWORD back_pen)
{
    const RLV_DrawOps *ops;
    APTR ctx;

    if (c == 0 || c->draw_ops == 0 || frag == 0) {
        return;
    }
    if (frag->text == 0 || frag->length == 0) {
        return;
    }

    /* Do not draw scrolled lines whose baseline is outside the viewport. */
    if (baseline_y < c->viewport_bounds.MinY
        || baseline_y > c->viewport_bounds.MaxY) {
        return;
    }

    ops = c->draw_ops;
    ctx = c->draw_context;
    RLV_BENCH_COUNT(RLV_BENCH_COUNTER_CELLS_DRAWN);
    ops->set_pens(ctx, text_pen, back_pen);
    ops->draw_text(ctx, frag->relative_x, baseline_y,
                   frag->text, frag->length);
}

/*
 * Paint one logical row's content (current-row presentation + fragments)
 * when its content rectangle intersects paint_area. Baseline rejection
 * preserved. Full fill uses selected pens only when visual is FULL;
 * MARKER draws a left-edge bar after content; NONE uses normal pens.
 */
static VOID rlv_paint_row_content(RLV_Control *c,
                                       ULONG layout_index,
                                       const struct Rectangle *paint_area)
{
    const RLV_DrawOps *ops;
    APTR ctx;
    struct Rectangle content;
    struct Rectangle draw;
    UWORD col;
    UWORD line;
    WORD baseline;
    WORD line_h;
    WORD row_top;
    WORD cell_left;
    WORD cell_right;
    ULONG idx;
    const RLV_CellWrap *cell;
    const RLV_Frag *frag;
    UWORD fill_pen;
    UWORD text_pen;
    UWORD text_back;
    BOOL is_current;
    BOOL use_selected_fill;
    WORD marker_w;
    WORD marker_right;
    struct Rectangle marker;

    if (c == 0 || paint_area == 0 || c->draw_ops == 0) {
        return;
    }
    if (!rlv_row_content_screen_rect(c, layout_index, &content)) {
        return;
    }
    if (!rlv_intersect_rects(&content, paint_area, &draw)) {
        return;
    }

    ops = c->draw_ops;
    ctx = c->draw_context;
    row_top = content.MinY;
    line_h = (WORD)c->line_height;
    if (line_h < 1) {
        line_h = 1;
    }

    is_current = (c->selected_row >= 0
                  && (LONG)layout_index == c->selected_row) ? TRUE : FALSE;
    use_selected_fill = rlv_row_uses_selected_fill(c, (LONG)layout_index);
    if (use_selected_fill) {
        fill_pen = c->pens.selected_background;
        text_pen = c->pens.selected_text;
        text_back = c->pens.selected_background;
        RLV_BENCH_COUNT(RLV_BENCH_COUNTER_HIGHLIGHT_FILLS);
    } else {
        fill_pen = c->pens.background;
        text_pen = c->pens.text;
        text_back = c->pens.background;
        RLV_BENCH_COUNT(RLV_BENCH_COUNTER_BACKGROUND_FILLS);
    }

    ops->set_pens(ctx, fill_pen, fill_pen);
    ops->fill_rect(ctx, draw.MinX, draw.MinY, draw.MaxX, draw.MaxY);

    for (col = 0; col < c->column_count && c->col_geom != 0; col++) {
        cell_left = c->col_geom[col].left;
        cell_right = rlv_cell_right(c, col);
        rlv_draw_body_cell_verticals(c,
                                          cell_left, content.MinY,
                                          cell_right, content.MaxY);
    }

    for (col = 0; col < c->column_count; col++) {
        /* Checkbox columns: snapshot + first-line-band geom (scroll-safe). */
        if (c->columns != 0
            && ((c->columns[col].flags & RLV_COL_TYPE_MASK)
                == (UWORD)RLV_COL_TYPE_CHECKBOX)) {
            rlv_checkbox_paint(c, (LONG)layout_index, col, use_selected_fill);
            continue;
        }
#if defined(RLV_ENABLE_EXPANDABLE_ROWS) && (RLV_ENABLE_EXPANDABLE_ROWS != 0)
        if (c->columns != 0
            && ((c->columns[col].flags & RLV_COL_TYPE_MASK)
                == (UWORD)RLV_COL_TYPE_DISCLOSURE)) {
            rlv_disclosure_paint(c, (LONG)layout_index, col,
                                      use_selected_fill);
            continue;
        }
#endif

        idx = layout_index * (ULONG)c->column_count + (ULONG)col;
        if (c->cell_wraps == 0 || idx >= c->cell_wrap_count) {
            continue;
        }
        cell = &c->cell_wraps[idx];
        {
            UWORD paint_lines;

            paint_lines = cell->frag_count;
#if defined(RLV_ENABLE_EXPANDABLE_ROWS) && (RLV_ENABLE_EXPANDABLE_ROWS != 0)
            /* Collapsed compact: first prepared display line only. */
            if (rlv_row_is_collapsed_compact(c, (LONG)layout_index)
                && paint_lines > 1) {
                paint_lines = 1;
            }
#endif
            for (line = 0; line < paint_lines; line++) {
                frag = &cell->frags[line];
                baseline = (WORD)(row_top
                                  + (WORD)c->cell_padding_y
                                  + (WORD)(line * (UWORD)line_h)
                                  + (WORD)c->font_metrics.baseline);
                rlv_draw_frag(c, frag, baseline, text_pen, text_back);
            }
        }
    }

    /*
     * Lightweight current-row marker: narrow left-edge fill using the
     * selected-background pen. Prefer fitting inside cell_padding_x so
     * text/controls are not obscured; fall back to 1 px when padding is 0.
     * Drawn after content so it remains visible; clipped to paint_area.
     */
    if (is_current
        && c->current_row_visual == (UWORD)RLV_CURRENT_ROW_VISUAL_MARKER) {
        marker_w = 2;
        if (c->cell_padding_x > 0 && (WORD)c->cell_padding_x < marker_w) {
            marker_w = (WORD)c->cell_padding_x;
        }
        if (marker_w < 1) {
            marker_w = 1;
        }
        marker.MinX = content.MinX;
        marker.MaxX = (WORD)(content.MinX + marker_w - 1);
        marker.MinY = content.MinY;
        marker.MaxY = content.MaxY;
        if (rlv_intersect_rects(&marker, paint_area, &marker)) {
            marker_right = marker.MaxX;
            if (marker_right >= marker.MinX) {
                ops->set_pens(ctx,
                              c->pens.selected_background,
                              c->pens.selected_background);
                ops->fill_rect(ctx,
                               marker.MinX, marker.MinY,
                               marker_right, marker.MaxY);
                RLV_BENCH_COUNT(RLV_BENCH_COUNTER_HIGHLIGHT_FILLS);
            }
        }
    }

    rlv_draw_row_divider(c, layout_index, content.MaxY);
}

static VOID rlv_paint_row_gap(RLV_Control *c,
                                   ULONG layout_index,
                                   const struct Rectangle *paint_area)
{
    const RLV_DrawOps *ops;
    APTR ctx;
    struct Rectangle gap;
    struct Rectangle draw;
    WORD cell_left;
    WORD cell_right;
    UWORD col;

    if (c == 0 || paint_area == 0 || c->draw_ops == 0 || c->row_gap < 1) {
        return;
    }
    if (!rlv_row_gap_screen_rect(c, layout_index, &gap)) {
        return;
    }
    if (!rlv_intersect_rects(&gap, paint_area, &draw)) {
        return;
    }

    ops = c->draw_ops;
    ctx = c->draw_context;
    ops->set_pens(ctx, c->pens.background, c->pens.background);
    ops->fill_rect(ctx, draw.MinX, draw.MinY, draw.MaxX, draw.MaxY);

    for (col = 0; col < c->column_count && c->col_geom != 0; col++) {
        cell_left = c->col_geom[col].left;
        cell_right = rlv_cell_right(c, col);
        rlv_draw_body_cell_verticals(c,
                                          cell_left, draw.MinY,
                                          cell_right, draw.MaxY);
    }
}

/* Continue the body cell edges through unused viewport space below the data. */
static VOID rlv_draw_empty_viewport_verticals(
    RLV_Control *c,
    const struct Rectangle *paint_area)
{
    LONG empty_top;
    WORD y1;
    WORD y2;
    WORD cell_left;
    WORD cell_right;
    UWORD col;

    if (c == 0 || paint_area == 0 || c->col_geom == 0) {
        return;
    }

    empty_top = (LONG)c->viewport_bounds.MinY
                + c->content_height - c->scroll_y;
    if (empty_top > (LONG)paint_area->MaxY
        || empty_top > (LONG)c->viewport_bounds.MaxY) {
        return;
    }
    if (empty_top < (LONG)paint_area->MinY) {
        empty_top = (LONG)paint_area->MinY;
    }
    if (empty_top < (LONG)c->viewport_bounds.MinY) {
        empty_top = (LONG)c->viewport_bounds.MinY;
    }

    y1 = (WORD)empty_top;
    y2 = paint_area->MaxY;
    if (y2 > c->viewport_bounds.MaxY) {
        y2 = c->viewport_bounds.MaxY;
    }
    if (y2 < y1) {
        return;
    }

    for (col = 0; col < c->column_count; col++) {
        cell_left = c->col_geom[col].left;
        cell_right = rlv_cell_right(c, col);
        rlv_draw_body_cell_verticals(c,
                                          cell_left, y1,
                                          cell_right, y2);
    }
}

BOOL rlv_paint_viewport_area(RLV_Control *c,
                                     const struct Rectangle *screen_area)
{
    const RLV_DrawOps *ops;
    APTR ctx;
    struct Rectangle paint;
    ULONG i;
    BOOL clip_ok;
    BOOL clip_pushed;

    RLV_LOG("viewport area paint begin");
    RLV_BENCH_DECLARE(bench_draw_row);
    RLV_BENCH_BEGIN(RLV_BENCH_DRAW_ROW_TOTAL, bench_draw_row);
    if (c == 0 || screen_area == 0 || c->draw_ops == 0) {
        RLV_LOG("viewport area paint end (null)");
        RLV_BENCH_END(RLV_BENCH_DRAW_ROW_TOTAL, bench_draw_row);
        return FALSE;
    }
    if (c->viewport_bounds.MaxY < c->viewport_bounds.MinY
        || c->viewport_bounds.MaxX < c->viewport_bounds.MinX) {
        RLV_LOG("viewport area paint end (empty viewport)");
        RLV_BENCH_END(RLV_BENCH_DRAW_ROW_TOTAL, bench_draw_row);
        return FALSE;
    }
    if (!rlv_intersect_rects(screen_area, &c->viewport_bounds, &paint)) {
        RLV_LOG("viewport area paint end (empty intersection)");
        RLV_BENCH_END(RLV_BENCH_DRAW_ROW_TOTAL, bench_draw_row);
        return FALSE;
    }

    ops = c->draw_ops;
    ctx = c->draw_context;
    clip_pushed = FALSE;

    /* Restore normal background for every pixel in the paint region. */
    ops->set_pens(ctx, c->pens.background, c->pens.background);
    ops->fill_rect(ctx, paint.MinX, paint.MinY, paint.MaxX, paint.MaxY);
    RLV_BENCH_COUNT(RLV_BENCH_COUNTER_BACKGROUND_FILLS);

    clip_ok = TRUE;
    if (ops->push_clip != 0) {
        RLV_LOG("clip push begin");
        clip_ok = ops->push_clip(ctx, &paint);
        RLV_LOGF("clip push result=%d", (int)clip_ok);
        if (!clip_ok) {
            RLV_LOG("INVARIANT clip installation failed");
            RLV_LOG("viewport area paint end (clip fail)");
            RLV_BENCH_END(RLV_BENCH_DRAW_ROW_TOTAL, bench_draw_row);
            return FALSE;
        }
        clip_pushed = TRUE;
    }

    for (i = 0; i < c->row_count && c->layout_rows != 0; i++) {
        RLV_BENCH_COUNT(RLV_BENCH_COUNTER_ROWS_DRAWN);
        rlv_paint_row_content(c, i, &paint);
        rlv_paint_row_gap(c, i, &paint);
    }
    rlv_draw_empty_viewport_verticals(c, &paint);

    if (clip_pushed && ops->pop_clip != 0) {
        RLV_LOG("clip pop");
        ops->pop_clip(ctx);
    }
    RLV_LOG("viewport area paint end");
    RLV_BENCH_END(RLV_BENCH_DRAW_ROW_TOTAL, bench_draw_row);
    return TRUE;
}

VOID rlv_render_viewport(RLV_Control *c)
{
    RLV_LOG("control render_viewport begin");
    if (c == 0 || c->draw_ops == 0) {
        RLV_LOG("control render_viewport end (null)");
        return;
    }
    /* Scroll / uncertain states: leave header + outer frame untouched. */
    (VOID)rlv_paint_viewport_area(c, &c->viewport_bounds);
    RLV_LOG("control render_viewport end");
}

VOID rlv_render_logical_rows(RLV_Control *c,
                                     LONG row_a,
                                     LONG row_b)
{
    struct Rectangle area_a;
    struct Rectangle area_b;
    BOOL have_a;
    BOOL have_b;
    BOOL ok;

    RLV_LOG("control render_logical_rows begin");
    RLV_BENCH_COUNT(RLV_BENCH_COUNTER_PARTIAL_REDRAWS);
    if (c == 0 || c->draw_ops == 0) {
        RLV_LOG("control render_logical_rows end (null)");
        return;
    }
    if (!c->layout_valid) {
        rlv_layout_rebuild(c);
    }

    have_a = rlv_get_row_paint_area(c, row_a, &area_a);
    have_b = rlv_get_row_paint_area(c, row_b, &area_b);

    if (!have_a && !have_b) {
        RLV_LOG("control render_logical_rows skip (no visible rows)");
        RLV_LOG("control render_logical_rows end");
        return;
    }

    ok = TRUE;
    if (have_a) {
        ok = rlv_paint_viewport_area(c, &area_a);
        if (!ok) {
            RLV_LOG("control render_logical_rows fallback viewport");
            rlv_render_viewport(c);
            RLV_LOG("control render_logical_rows end");
            return;
        }
    }
    if (have_b) {
        /* Same row twice (identical areas): one paint is enough. */
        if (have_a && row_a == row_b) {
            RLV_LOG("control render_logical_rows end (same row)");
            return;
        }
        ok = rlv_paint_viewport_area(c, &area_b);
        if (!ok) {
            RLV_LOG("control render_logical_rows fallback viewport");
            rlv_render_viewport(c);
            RLV_LOG("control render_logical_rows end");
            return;
        }
    }
    RLV_LOG("control render_logical_rows end");
}

#if defined(RLV_ENABLE_SMART_SCROLL) && (RLV_ENABLE_SMART_SCROLL != 0)
/* Defined with the smart-scroll helpers further below. */
static BOOL rlv_exposed_band_after_shift(const struct Rectangle *viewport,
                                              LONG delta_y,
                                              struct Rectangle *exposed);
static VOID rlv_expand_exposed_for_glyphs(const RLV_Control *c,
                                               LONG delta_y,
                                               struct Rectangle *exposed);
static BOOL rlv_try_expand_row_shift(RLV_Control *c,
                                          LONG logical_row,
                                          LONG screen_top,
                                          LONG old_total_h);
#endif

VOID rlv_render_from_row(RLV_Control *c,
                                 LONG logical_row,
                                 LONG previous_scroll_y)
{
    struct Rectangle area;
    LONG screen_top;
    LONG old_total_h;
    BOOL ok;

    RLV_LOGF("render_from_row begin row=%ld prev_scroll=%ld cur_scroll=%ld",
             (long)logical_row,
             (long)previous_scroll_y,
             (c != 0) ? (long)c->scroll_y : 0L);

    if (c == 0 || c->draw_ops == 0) {
        RLV_LOG("render_from_row end (null)");
        return;
    }
    if (!c->layout_valid) {
        RLV_BENCH_NOTE_PREPARE_REBUILD();
        if (!rlv_layout_rebuild(c)) {
            RLV_LOG("render_from_row fallback viewport (rebuild fail)");
            RLV_BENCH_COUNT(RLV_BENCH_COUNTER_FULL_REDRAWS);
            rlv_render_viewport(c);
            return;
        }
    }
    if (c->layout_rows == 0
        || logical_row < 0
        || (ULONG)logical_row >= c->row_count) {
        RLV_LOG("render_from_row fallback viewport (bad row)");
        RLV_BENCH_COUNT(RLV_BENCH_COUNTER_FULL_REDRAWS);
        rlv_render_viewport(c);
        return;
    }

    /*
     * If scroll moved, rows above the toggle may have shifted on screen —
     * cannot keep the head of the viewport.
     */
    if (previous_scroll_y != c->scroll_y) {
        RLV_LOGF("render_from_row fallback viewport (scroll %ld -> %ld)",
                 (long)previous_scroll_y, (long)c->scroll_y);
#if defined(RLV_ENABLE_EXPANDABLE_ROWS) && (RLV_ENABLE_EXPANDABLE_ROWS != 0)
        c->expand_old_total_h = 0;
#endif
        RLV_BENCH_COUNT(RLV_BENCH_COUNTER_FULL_REDRAWS);
        rlv_render_viewport(c);
        return;
    }

    screen_top = (LONG)c->viewport_bounds.MinY
                 + c->layout_rows[logical_row].top_y
                 - c->scroll_y;

    old_total_h = 0;
#if defined(RLV_ENABLE_EXPANDABLE_ROWS) && (RLV_ENABLE_EXPANDABLE_ROWS != 0)
    old_total_h = c->expand_old_total_h;
    c->expand_old_total_h = 0;
#endif

    if (screen_top < (LONG)c->viewport_bounds.MinY) {
        /* Row starts above the visible top — unsafe for blit/tail. */
        RLV_LOG("render_from_row fallback viewport (row above top)");
        RLV_BENCH_COUNT(RLV_BENCH_COUNTER_FULL_REDRAWS);
        rlv_render_viewport(c);
        return;
    }
    if (screen_top > (LONG)c->viewport_bounds.MaxY) {
        RLV_LOG("render_from_row skip (row below viewport)");
        return;
    }

#if defined(RLV_ENABLE_SMART_SCROLL) && (RLV_ENABLE_SMART_SCROLL != 0)
    if (old_total_h > 0
        && rlv_try_expand_row_shift(c, logical_row, screen_top,
                                         old_total_h)) {
        RLV_LOG("render_from_row end (shift blit)");
        return;
    }
#endif

    /*
     * No blit: paint from the toggled row through the viewport bottom.
     * Safe when scroll is unchanged (rows above stay put).
     */
    area = c->viewport_bounds;
    area.MinY = (WORD)screen_top;

    RLV_BENCH_COUNT(RLV_BENCH_COUNTER_PARTIAL_REDRAWS);
    ok = rlv_paint_viewport_area(c, &area);
    if (!ok) {
        RLV_LOG("render_from_row fallback viewport (paint fail)");
        RLV_BENCH_COUNT(RLV_BENCH_COUNTER_FULL_REDRAWS);
        rlv_render_viewport(c);
        return;
    }
    RLV_LOGF("render_from_row end tail y=%ld..%d",
             (long)screen_top, (int)c->viewport_bounds.MaxY);
}

UWORD rlv_render_cell_control(RLV_Control *c,
                                      LONG row,
                                      UWORD column)
{
    const RLV_DrawOps *ops;
    APTR ctx;
    struct Rectangle box;
    struct Rectangle visible;
    UWORD col_type;
    BOOL use_selected_fill;
    BOOL clip_ok;
    BOOL clip_pushed;
    UWORD back_pen;

    RLV_LOGF("render_cell_control begin row=%ld col=%u",
             (long)row, (unsigned)column);

    if (c == 0 || c->draw_ops == 0 || c->columns == 0) {
        RLV_LOG("render_cell_control error (null)");
        return (UWORD)RLV_CELL_REPAINT_ERROR;
    }
    if (row < 0 || (ULONG)row >= c->row_count) {
        RLV_LOG("render_cell_control error (row)");
        return (UWORD)RLV_CELL_REPAINT_ERROR;
    }
    if (column >= c->column_count) {
        RLV_LOG("render_cell_control error (column)");
        return (UWORD)RLV_CELL_REPAINT_ERROR;
    }

    col_type = (UWORD)(c->columns[column].flags & RLV_COL_TYPE_MASK);
    if (col_type == (UWORD)RLV_COL_TYPE_CHECKBOX) {
        /* fall through to local checkbox paint below */
    }
#if defined(RLV_ENABLE_EXPANDABLE_ROWS) && (RLV_ENABLE_EXPANDABLE_ROWS != 0)
    else if (col_type == (UWORD)RLV_COL_TYPE_DISCLOSURE) {
        /*
         * Height changed. Prefer tail paint when scroll is unchanged; the
         * demo/event path should call rlv_render_from_row with the pre-
         * toggle scroll. Here scroll is already final — use current as
         * previous so an unchanged scroll gets a tail paint; if expand
         * clamped scroll, caller should have used render_from_row with the
         * real previous value instead of this helper.
         */
        RLV_LOG("render_cell_control disclosure -> from_row");
        rlv_render_from_row(c, row, c->scroll_y);
        return (UWORD)RLV_CELL_REPAINT_VIEWPORT;
    }
#endif
    else {
        RLV_LOG("render_cell_control error (not checkbox)");
        return (UWORD)RLV_CELL_REPAINT_ERROR;
    }

    if (!c->layout_valid) {
        RLV_LOG("render_cell_control fallback viewport (layout invalid)");
        RLV_BENCH_COUNT(RLV_BENCH_COUNTER_CONTROL_REPAINT_FALLBACKS);
        RLV_BENCH_COUNT(RLV_BENCH_COUNTER_FULL_REDRAWS);
        rlv_render_viewport(c);
        return (UWORD)RLV_CELL_REPAINT_VIEWPORT;
    }

    if (!rlv_checkbox_resolve_rect(c, row, column, &box)) {
        RLV_LOG("render_cell_control error (resolve)");
        return (UWORD)RLV_CELL_REPAINT_ERROR;
    }

    if (!rlv_intersect_rects(&box, &c->viewport_bounds, &visible)) {
        RLV_LOG("render_cell_control not_visible");
        return (UWORD)RLV_CELL_REPAINT_NOT_VISIBLE;
    }

    /*
     * Require the full control rectangle inside the viewport so local
     * background restore cannot leave stale pixels on a clipped edge.
     */
    if (visible.MinX != box.MinX || visible.MinY != box.MinY
        || visible.MaxX != box.MaxX || visible.MaxY != box.MaxY) {
        RLV_LOG("render_cell_control fallback row (partially visible)");
        RLV_BENCH_COUNT(RLV_BENCH_COUNTER_CONTROL_REPAINT_FALLBACKS);
        rlv_render_logical_rows(c, row, -1);
        return (UWORD)RLV_CELL_REPAINT_ROW;
    }

    ops = c->draw_ops;
    ctx = c->draw_context;
    use_selected_fill = rlv_row_uses_selected_fill(c, row);
    back_pen = use_selected_fill
        ? c->pens.selected_background
        : c->pens.background;

    clip_pushed = FALSE;
    clip_ok = TRUE;
    if (ops->push_clip != 0) {
        clip_ok = ops->push_clip(ctx, &box);
        if (clip_ok) {
            clip_pushed = TRUE;
        } else {
            RLV_LOG("render_cell_control fallback row (clip fail)");
            RLV_BENCH_COUNT(RLV_BENCH_COUNTER_CONTROL_REPAINT_FALLBACKS);
            rlv_render_logical_rows(c, row, -1);
            return (UWORD)RLV_CELL_REPAINT_ROW;
        }
    }

    /* Restore local background under the control, then shared paint. */
    ops->set_pens(ctx, back_pen, back_pen);
    ops->fill_rect(ctx, box.MinX, box.MinY, box.MaxX, box.MaxY);
    rlv_checkbox_paint(c, row, column, use_selected_fill);

    if (clip_pushed && ops->pop_clip != 0) {
        ops->pop_clip(ctx);
    }

    RLV_BENCH_COUNT(RLV_BENCH_COUNTER_CONTROL_ONLY_REDRAWS);
    RLV_LOG("render_cell_control ok");
    return (UWORD)RLV_CELL_REPAINT_OK;
}

#if defined(RLV_ENABLE_SMART_SCROLL) && (RLV_ENABLE_SMART_SCROLL != 0)

/*
 * After a successful pixel shift, build the newly exposed viewport band.
 * delta_y = new_scroll_y - old_scroll_y (same sign as ScrollRaster dy).
 * Positive: content scrolled down → pixels moved up → band at bottom.
 * Negative: content scrolled up → pixels moved down → band at top.
 * Inclusive Amiga rect; height == abs(delta_y).
 */
static BOOL rlv_exposed_band_after_shift(const struct Rectangle *viewport,
                                              LONG delta_y,
                                              struct Rectangle *exposed)
{
    LONG abs_delta;
    LONG vp_h;

    if (viewport == 0 || exposed == 0) {
        return FALSE;
    }
    if (delta_y == 0) {
        return FALSE;
    }
    if (viewport->MaxY < viewport->MinY || viewport->MaxX < viewport->MinX) {
        return FALSE;
    }

    abs_delta = delta_y;
    if (abs_delta < 0) {
        abs_delta = -abs_delta;
    }
    vp_h = (LONG)viewport->MaxY - (LONG)viewport->MinY + 1;
    if (abs_delta < 1 || abs_delta >= vp_h) {
        return FALSE;
    }

    exposed->MinX = viewport->MinX;
    exposed->MaxX = viewport->MaxX;
    if (delta_y > 0) {
        exposed->MaxY = viewport->MaxY;
        exposed->MinY = (WORD)(viewport->MaxY - (WORD)abs_delta + 1);
    } else {
        exposed->MinY = viewport->MinY;
        exposed->MaxY = (WORD)(viewport->MinY + (WORD)abs_delta - 1);
    }

    if (exposed->MaxY < exposed->MinY) {
        return FALSE;
    }
    return TRUE;
}

/*
 * Expand the vacated strip so regional paint can redraw complete text cells
 * and first-line-band control widgets (checkboxes).
 *
 * Text() is all-or-nothing; InstallClipRegion on the vacated strip alone
 * clips glyph ascent (below-band scrolls) or descent (above-band scrolls).
 * Those chopped stubs then shift into the viewport on later scrolls —
 * blank gaps and letter fragments, especially when scrolling down.
 *
 * Checkbox boxes live in the first content text-line band (top cell padding
 * + one line_height, §D.11). Expanding by only line_height can leave a
 * straddling box half-blit / half-clipped after a short scroll step.
 * Diagonal tick strokes also need correct soft-clip handling in the
 * v36 backend (do not all-or-nothing-reject when hardware clip is active).
 *
 * Grow against the scroll direction by one first-line cell
 * (cell_padding_y + line_height), clamped to the viewport. Extra pixels
 * overwrite still-valid shifted content with a correct full redraw of
 * straddling lines and checkbox artwork from the current snapshot.
 */
static VOID rlv_expand_exposed_for_glyphs(const RLV_Control *c,
                                               LONG delta_y,
                                               struct Rectangle *exposed)
{
    LONG expand;
    LONG line_h;
    LONG min_y;
    LONG max_y;

    if (c == 0 || exposed == 0 || delta_y == 0) {
        return;
    }

    line_h = (LONG)c->line_height;
    if (line_h < 1) {
        line_h = (LONG)c->font_metrics.line_height;
    }
    if (line_h < 1) {
        line_h = 8;
    }

    /* First-line band from row content top through one text line (C6). */
    expand = (LONG)c->cell_padding_y + line_h;
    if (expand < line_h) {
        expand = line_h;
    }

    min_y = (LONG)exposed->MinY;
    max_y = (LONG)exposed->MaxY;

    if (delta_y > 0) {
        /* Bottom vacated strip: include ascent / first-line band above. */
        min_y -= expand;
        if (min_y < (LONG)c->viewport_bounds.MinY) {
            min_y = (LONG)c->viewport_bounds.MinY;
        }
    } else {
        /* Top vacated strip: include descent / first-line band below. */
        max_y += expand;
        if (max_y > (LONG)c->viewport_bounds.MaxY) {
            max_y = (LONG)c->viewport_bounds.MaxY;
        }
    }

    exposed->MinY = (WORD)min_y;
    exposed->MaxY = (WORD)max_y;
}

static BOOL rlv_smart_scroll_eligible(const RLV_Control *c,
                                           LONG old_scroll,
                                           LONG new_scroll,
                                           LONG *out_delta)
{
    LONG delta;
    LONG vp_h;
    LONG abs_delta;

    if (out_delta != 0) {
        *out_delta = 0;
    }
    if (c == 0 || c->draw_ops == 0 || c->draw_context == 0) {
        return FALSE;
    }
    if (c->draw_ops->move_viewport_pixels == 0) {
        return FALSE;
    }
    if (!c->layout_valid) {
        return FALSE;
    }
    if (c->viewport_bounds.MaxY < c->viewport_bounds.MinY
        || c->viewport_bounds.MaxX < c->viewport_bounds.MinX) {
        return FALSE;
    }

    delta = new_scroll - old_scroll;
    if (delta == 0) {
        return FALSE;
    }
    abs_delta = delta;
    if (abs_delta < 0) {
        abs_delta = -abs_delta;
    }
    vp_h = (LONG)c->viewport_bounds.MaxY - (LONG)c->viewport_bounds.MinY + 1;
    if (vp_h < 2 || abs_delta >= vp_h) {
        return FALSE;
    }
    if (abs_delta > 32767) {
        return FALSE; /* must fit in WORD for the backend */
    }

    if (out_delta != 0) {
        *out_delta = delta;
    }
    return TRUE;
}

/*
 * After expand/collapse: ScrollRaster content below the toggled row by the
 * height delta, then paint the toggled row and the exposed band.
 * scroll_dy convention matches smart scroll (positive = bits move up).
 * Returns TRUE if the blit path completed (including row-only when nothing
 * lies below to shift). FALSE = caller should use tail/full fallback.
 */
static BOOL rlv_try_expand_row_shift(RLV_Control *c,
                                          LONG logical_row,
                                          LONG screen_top,
                                          LONG old_total_h)
{
    LONG new_total_h;
    LONG delta_h;
    LONG abs_delta;
    LONG split_y;
    LONG shift_h;
    LONG row_bottom;
    WORD scroll_dy;
    UWORD move_result;
    struct Rectangle shift_rect;
    struct Rectangle row_area;
    struct Rectangle exposed;
    BOOL painted;

    if (c == 0 || c->draw_ops == 0 || c->layout_rows == 0) {
        return FALSE;
    }
    if (c->draw_ops->move_viewport_pixels == 0) {
        return FALSE;
    }
    if (old_total_h < 1) {
        return FALSE;
    }
    if (logical_row < 0 || (ULONG)logical_row >= c->row_count) {
        return FALSE;
    }

    new_total_h = (LONG)c->layout_rows[logical_row].total_height;
    delta_h = new_total_h - old_total_h;
    if (delta_h == 0) {
        return FALSE;
    }

    abs_delta = delta_h;
    if (abs_delta < 0) {
        abs_delta = -abs_delta;
    }
    if (abs_delta > 32767) {
        return FALSE;
    }

    /*
     * split_y = first pixel that must move with rows below.
     * Expand: old bottom (make room below the short row).
     * Collapse: new bottom (overwrite the freed strip that still holds
     * the old tall-row pixels — e.g. selection fill bleeding into Gamma).
     */
    split_y = screen_top + old_total_h;
    if (new_total_h < old_total_h) {
        split_y = screen_top + new_total_h;
    }
    row_bottom = screen_top + new_total_h - 1;

    RLV_LOGF("EXPAND_SHIFT row=%ld old_h=%ld new_h=%ld split=%ld top=%ld",
             (long)logical_row, (long)old_total_h, (long)new_total_h,
             (long)split_y, (long)screen_top);

    if (split_y > (LONG)c->viewport_bounds.MaxY) {
        /* Nothing below visible to shift — paint toggled row only. */
        row_area = c->viewport_bounds;
        row_area.MinY = (WORD)screen_top;
        if (row_bottom < (LONG)c->viewport_bounds.MaxY) {
            row_area.MaxY = (WORD)row_bottom;
        }
        if (row_area.MaxY < row_area.MinY) {
            return FALSE;
        }
        RLV_BENCH_COUNT(RLV_BENCH_COUNTER_PARTIAL_REDRAWS);
        return rlv_paint_viewport_area(c, &row_area);
    }

    if (split_y < (LONG)c->viewport_bounds.MinY) {
        return FALSE;
    }

    shift_rect = c->viewport_bounds;
    shift_rect.MinY = (WORD)split_y;
    shift_h = (LONG)shift_rect.MaxY - (LONG)shift_rect.MinY + 1;
    if (shift_h < 2 || abs_delta >= shift_h) {
        RLV_LOG("EXPAND_SHIFT reject delta_ge_shift_rect");
        return FALSE;
    }

    /*
     * Expand (delta_h > 0): content below moves down → ScrollRaster dy < 0.
     * Collapse (delta_h < 0): content below moves up → dy > 0.
     */
    scroll_dy = (WORD)(old_total_h - new_total_h);

    RLV_BENCH_COUNT(RLV_BENCH_COUNTER_SCROLL_COPY_ATTEMPTS);
    move_result = c->draw_ops->move_viewport_pixels(c->draw_context,
                                                    &shift_rect,
                                                    scroll_dy);
    if (move_result != (UWORD)RLV_VIEWPORT_MOVE_DONE) {
        RLV_LOGF("EXPAND_SHIFT blit rejected result=%u",
                 (unsigned)move_result);
        return FALSE;
    }

    /* Repaint the toggled row at its new height (includes growth strip). */
    row_area = c->viewport_bounds;
    row_area.MinY = (WORD)screen_top;
    if (row_bottom < (LONG)c->viewport_bounds.MaxY) {
        row_area.MaxY = (WORD)row_bottom;
    }
    painted = FALSE;
    if (row_area.MaxY >= row_area.MinY) {
        painted = rlv_paint_viewport_area(c, &row_area);
        if (!painted) {
            RLV_LOG("EXPAND_SHIFT row paint fail -> viewport");
            RLV_BENCH_COUNT(RLV_BENCH_COUNTER_FULL_REDRAWS);
            rlv_render_viewport(c);
            if (c->draw_ops->finish_viewport_move != 0) {
                c->draw_ops->finish_viewport_move(c->draw_context);
            }
            return TRUE;
        }
    }

    /* Vacated band from the shift (collapse bottom / expand growth). */
    if (rlv_exposed_band_after_shift(&shift_rect, (LONG)scroll_dy,
                                          &exposed)) {
        rlv_expand_exposed_for_glyphs(c, (LONG)scroll_dy, &exposed);
        RLV_LOGF("EXPAND_SHIFT exposed %d..%d",
                 (int)exposed.MinY, (int)exposed.MaxY);
        if (!rlv_paint_viewport_area(c, &exposed)) {
            RLV_LOG("EXPAND_SHIFT exposed paint fail -> viewport");
            RLV_BENCH_COUNT(RLV_BENCH_COUNTER_FULL_REDRAWS);
            rlv_render_viewport(c);
            if (c->draw_ops->finish_viewport_move != 0) {
                c->draw_ops->finish_viewport_move(c->draw_context);
            }
            return TRUE;
        }
    }

    if (c->draw_ops->finish_viewport_move != 0) {
        c->draw_ops->finish_viewport_move(c->draw_context);
    }

    RLV_BENCH_COUNT(RLV_BENCH_COUNTER_PARTIAL_REDRAWS);
    RLV_LOG("EXPAND_SHIFT done");
    return TRUE;
}

#endif /* RLV_ENABLE_SMART_SCROLL */

VOID rlv_render_scrolled(RLV_Control *c, LONG previous_scroll_y)
{
    LONG new_scroll;
    RLV_BENCH_DECLARE(bench_scroll);
#if defined(RLV_ENABLE_SMART_SCROLL) && (RLV_ENABLE_SMART_SCROLL != 0)
    LONG delta;
    UWORD move_result;
    struct Rectangle exposed;
    BOOL painted;
#endif

    RLV_LOG("control render_scrolled begin");
    RLV_BENCH_BEGIN(RLV_BENCH_VIEWPORT_SCROLL, bench_scroll);
    if (c == 0 || c->draw_ops == 0) {
        RLV_LOG("control render_scrolled end (null)");
        RLV_BENCH_END(RLV_BENCH_VIEWPORT_SCROLL, bench_scroll);
        return;
    }
    if (!c->layout_valid) {
        rlv_layout_rebuild(c);
    }

    new_scroll = c->scroll_y;
    RLV_LOGF("SMART_SCROLL request old=%ld new=%ld delta=%ld",
             (long)previous_scroll_y,
             (long)new_scroll,
             (long)(new_scroll - previous_scroll_y));

#if defined(RLV_ENABLE_SMART_SCROLL) && (RLV_ENABLE_SMART_SCROLL != 0)
    if (!rlv_smart_scroll_eligible(c, previous_scroll_y, new_scroll,
                                        &delta)) {
        RLV_LOG("SMART_SCROLL rejected reason=core_ineligible");
        RLV_LOG("SMART_SCROLL fallback full viewport");
        rlv_render_viewport(c);
        RLV_LOG("control render_scrolled end");
        RLV_BENCH_COUNT(RLV_BENCH_COUNTER_SCROLL_COPY_FALLBACKS);
        RLV_BENCH_END(RLV_BENCH_VIEWPORT_SCROLL, bench_scroll);
        return;
    }

    RLV_BENCH_COUNT(RLV_BENCH_COUNTER_SCROLL_COPY_ATTEMPTS);
    move_result = c->draw_ops->move_viewport_pixels(c->draw_context,
                                                    &c->viewport_bounds,
                                                    (WORD)delta);
    if (move_result == (UWORD)RLV_VIEWPORT_MOVE_DONE) {
        RLV_BENCH_COUNT(RLV_BENCH_COUNTER_SCROLL_COPY_SUCCESSES);
        if (!rlv_exposed_band_after_shift(&c->viewport_bounds, delta,
                                               &exposed)) {
            RLV_LOG("SMART_SCROLL rejected reason=bad_exposed_band");
            RLV_LOG("SMART_SCROLL fallback full viewport");
            rlv_render_viewport(c);
            RLV_LOG("control render_scrolled end");
            RLV_BENCH_COUNT(RLV_BENCH_COUNTER_SCROLL_COPY_FALLBACKS);
            RLV_BENCH_END(RLV_BENCH_VIEWPORT_SCROLL, bench_scroll);
            return;
        }
        RLV_LOGF("SMART_SCROLL vacated top=%d bottom=%d",
                 (int)exposed.MinY, (int)exposed.MaxY);
        rlv_expand_exposed_for_glyphs(c, delta, &exposed);
        RLV_LOGF("SMART_SCROLL exposed top=%d bottom=%d",
                 (int)exposed.MinY, (int)exposed.MaxY);
        RLV_BENCH_COUNT(RLV_BENCH_COUNTER_NEWLY_EXPOSED_ROWS);
        RLV_LOG("SMART_SCROLL regional paint begin");
        painted = rlv_paint_viewport_area(c, &exposed);
        RLV_LOG("SMART_SCROLL regional paint end");
        if (!painted) {
            RLV_LOG("SMART_SCROLL fallback full viewport");
            rlv_render_viewport(c);
            RLV_BENCH_COUNT(RLV_BENCH_COUNTER_SCROLL_COPY_FALLBACKS);
        } else if (c->draw_ops->finish_viewport_move != 0) {
            c->draw_ops->finish_viewport_move(c->draw_context);
        }
        RLV_LOG("control render_scrolled end");
        RLV_BENCH_END(RLV_BENCH_VIEWPORT_SCROLL, bench_scroll);
        return;
    }

    if (move_result == (UWORD)RLV_VIEWPORT_MOVE_UNUSED) {
        RLV_LOG("SMART_SCROLL rejected reason=backend_unused");
    } else {
        RLV_LOG("SMART_SCROLL rejected reason=backend_repaint");
    }
#else
    RLV_LOG("SMART_SCROLL rejected reason=compiled_out");
#endif

    RLV_LOG("SMART_SCROLL fallback full viewport");
    RLV_BENCH_COUNT(RLV_BENCH_COUNTER_SCROLL_COPY_FALLBACKS);
    rlv_render_viewport(c);
    RLV_LOG("control render_scrolled end");
    RLV_BENCH_END(RLV_BENCH_VIEWPORT_SCROLL, bench_scroll);
}

VOID rlv_render_full(RLV_Control *c)
{
    RLV_LOG("control render_full begin");
    if (c == 0 || c->draw_ops == 0) {
        RLV_LOG("control render_full end (null)");
        return;
    }
    /* Frame first so mid-paint damage is limited; again last to repair. */
    rlv_draw_outer_frame(c);
    rlv_draw_header(c);
    (VOID)rlv_paint_viewport_area(c, &c->viewport_bounds);
    rlv_draw_outer_frame(c);
    RLV_LOG("control render_full end");
}
