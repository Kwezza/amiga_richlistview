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
 * Paint one logical row's content (selection fill + fragments) when its
 * content rectangle intersects paint_area. Baseline rejection preserved.
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
    BOOL selected;

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

    selected = (c->selected_row >= 0
                && (LONG)layout_index == c->selected_row) ? TRUE : FALSE;
    if (selected) {
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
            rlv_checkbox_paint(c, (LONG)layout_index, col, selected);
            continue;
        }

        idx = layout_index * (ULONG)c->column_count + (ULONG)col;
        if (c->cell_wraps == 0 || idx >= c->cell_wrap_count) {
            continue;
        }
        cell = &c->cell_wraps[idx];
        for (line = 0; line < cell->frag_count; line++) {
            frag = &cell->frags[line];
            baseline = (WORD)(row_top
                              + (WORD)c->cell_padding_y
                              + (WORD)(line * (UWORD)line_h)
                              + (WORD)c->font_metrics.baseline);
            /* Viewport baseline rejection lives in rlv_draw_frag;
             * soft/hardware clip handles glyphs that only partly meet paint. */
            rlv_draw_frag(c, frag, baseline, text_pen, text_back);
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
