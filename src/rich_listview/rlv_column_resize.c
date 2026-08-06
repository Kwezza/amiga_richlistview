/**
 * Optional interactive column resizing (RLV_ENABLE_COLUMN_RESIZE).
 *
 * Two-column exchange: dragging the divider between columns L and R adjusts
 * only those widths so L+R stays constant and later columns keep the same X.
 * Live preview paints both affected header interiors with the configured
 * title fill (no full-header redraw, no sort glyphs), a moving header
 * divider, and a body-only reversible COMPLEMENT guide — without rebuilding
 * wrap/layout.
 */

#include "rich_listview/rlv_internal.h"
#include "rich_listview/rlv_log.h"
#include "rich_listview/rlv_platform_internal.h"

#include <string.h>

#if defined(RLV_ENABLE_COLUMN_RESIZE) && (RLV_ENABLE_COLUMN_RESIZE != 0)

static BOOL rlv_cr_hit_divider(const RLV_Control *c,
                               WORD x,
                               WORD y,
                               UWORD *left_out);

static UWORD rlv_cr_strlen(CONST_STRPTR s)
{
    UWORD n;

    n = 0;
    if (s == 0) {
        return 0;
    }
    while (s[n] != '\0' && n < 0xFFF0U) {
        n++;
    }
    return n;
}

static WORD rlv_cr_min_width(const RLV_Control *c, UWORD column)
{
    WORD m;

    if (c == 0 || c->runtime_mins == 0 || column >= c->runtime_width_count) {
        return (WORD)RLV_COL_RESIZE_MIN_DEFAULT;
    }
    m = c->runtime_mins[column];
    if (m < 1) {
        m = (WORD)RLV_COL_RESIZE_MIN_DEFAULT;
    }
    return m;
}

WORD rlv_column_effective_width(const RLV_Control *c, UWORD column)
{
    WORD w;

    if (c == 0 || c->columns == 0 || column >= c->column_count) {
        return 1;
    }
    if (c->runtime_widths != 0 && column < c->runtime_width_count) {
        w = c->runtime_widths[column];
    } else {
        w = c->columns[column].width_pixels;
    }
    if (w < 1) {
        w = 1;
    }
    return w;
}

VOID rlv_column_resize_free(RLV_Control *c)
{
    if (c == 0) {
        return;
    }
    if (c->runtime_widths != 0) {
        rlv_platform_free(c->runtime_widths);
        c->runtime_widths = 0;
    }
    if (c->runtime_mins != 0) {
        rlv_platform_free(c->runtime_mins);
        c->runtime_mins = 0;
    }
    c->runtime_width_count = 0;
    c->resize_dragging = FALSE;
    c->resize_pointer_wanted = FALSE;
    c->resize_guide_visible = FALSE;
}

static VOID rlv_cr_set_pointer_wanted(RLV_Control *c, BOOL wanted)
{
    if (c == 0) {
        return;
    }
    c->resize_pointer_wanted = wanted ? TRUE : FALSE;
}

static VOID rlv_cr_clear_pointer_wanted(RLV_Control *c)
{
    rlv_cr_set_pointer_wanted(c, FALSE);
}

static VOID rlv_cr_update_hover(RLV_Control *c, WORD x, WORD y)
{
    UWORD dummy;

    if (c == 0 || c->resize_dragging || !c->column_resize_enabled) {
        return;
    }
    rlv_cr_set_pointer_wanted(c, rlv_cr_hit_divider(c, x, y, &dummy));
}

static BOOL rlv_cr_column_locked(const RLV_Control *c, UWORD column)
{
    if (c == 0 || c->columns == 0 || column >= c->column_count) {
        return TRUE;
    }
    return ((c->columns[column].flags & RLV_COL_F_NO_RESIZE) != 0)
           ? TRUE : FALSE;
}

BOOL rlv_column_resize_on_columns_set(RLV_Control *c)
{
    UWORD i;
    WORD *widths;
    WORD *mins;
    WORD w;

    if (c == 0) {
        return FALSE;
    }

    rlv_column_resize_cancel(c, TRUE);
    rlv_column_resize_free(c);

    if (c->column_count == 0 || c->columns == 0) {
        return TRUE;
    }

    widths = (WORD *)rlv_platform_malloc(
        (size_t)c->column_count * sizeof(WORD));
    mins = (WORD *)rlv_platform_malloc(
        (size_t)c->column_count * sizeof(WORD));
    if (widths == 0 || mins == 0) {
        if (widths != 0) {
            rlv_platform_free(widths);
        }
        if (mins != 0) {
            rlv_platform_free(mins);
        }
        RLV_LOG("FAIL column resize width alloc");
        return FALSE;
    }

    for (i = 0; i < c->column_count; i++) {
        w = c->columns[i].width_pixels;
        if (w < 1) {
            w = 1;
        }
        widths[i] = w;
        mins[i] = (WORD)RLV_COL_RESIZE_MIN_DEFAULT;
    }

    c->runtime_widths = widths;
    c->runtime_mins = mins;
    c->runtime_width_count = c->column_count;
    return TRUE;
}

/*
 * Body-only reversible guide (COMPLEMENT). Header uses a normal-pen moving
 * divider; the guide must not damage the fixed title bevel.
 */
static VOID rlv_cr_xor_guide(RLV_Control *c, WORD x)
{
    const RLV_DrawOps *ops;
    WORD y1;
    WORD y2;

    if (c == 0 || c->draw_ops == 0 || c->draw_ops->draw_xor_vline == 0) {
        return;
    }
    ops = c->draw_ops;
    y1 = c->viewport_bounds.MinY;
    y2 = c->viewport_bounds.MaxY;
    if (y2 < y1) {
        return;
    }
    ops->draw_xor_vline(c->draw_context, x, y1, y2);
}

static VOID rlv_cr_erase_guide(RLV_Control *c)
{
    if (c == 0 || !c->resize_guide_visible) {
        return;
    }
    rlv_cr_xor_guide(c, c->resize_guide_x);
    c->resize_guide_visible = FALSE;
}

static VOID rlv_cr_draw_guide(RLV_Control *c, WORD x)
{
    if (c == 0) {
        return;
    }
    rlv_cr_xor_guide(c, x);
    c->resize_guide_x = x;
    c->resize_guide_visible = TRUE;
}

/* Inclusive right edge of a header cell (matches rlv_render cell framing). */
static WORD rlv_cr_cell_right(const RLV_Control *c, UWORD column)
{
    if (c == 0 || c->col_geom == 0 || column >= c->column_count) {
        return 0;
    }
    if (c->divider_x != 0 && column < c->divider_count) {
        return c->divider_x[column];
    }
    return c->col_geom[column].right;
}

/*
 * Symmetric step quantisation relative to the press origin.
 * Toward-zero buckets: |delta| < step stays 0; avoids C truncating
 * toward zero asymmetrically when using a single signed division.
 */
static LONG rlv_cr_quantize_delta(LONG delta)
{
    LONG step;
    LONG ad;
    LONG q;

    step = (LONG)RLV_COLUMN_RESIZE_STEP;
    if (step < 1L) {
        return delta;
    }
    if (delta >= 0L) {
        q = (delta / step) * step;
    } else {
        ad = -delta;
        q = -((ad / step) * step);
    }
    return q;
}

static WORD rlv_cr_clamp_left(const RLV_Control *c, WORD proposed)
{
    WORD min_l;
    WORD min_r;
    WORD max_l;

    min_l = rlv_cr_min_width(c, c->resize_left_col);
    min_r = rlv_cr_min_width(c, c->resize_right_col);
    max_l = (WORD)(c->resize_pair_total - min_r);
    if (max_l < min_l) {
        /* Impossible mins — keep original. */
        return c->resize_orig_left;
    }
    if (proposed < min_l) {
        RLV_LOGF("COLUMN_RESIZE clamp left to min=%d", (int)min_l);
        proposed = min_l;
    }
    if (proposed > max_l) {
        RLV_LOGF("COLUMN_RESIZE clamp left to max=%d", (int)max_l);
        proposed = max_l;
    }
    return proposed;
}

static WORD rlv_cr_proposed_from_x(const RLV_Control *c, WORD x)
{
    LONG raw_delta;
    LONG snapped_delta;
    LONG proposed;

    if (c == 0 || c->col_geom == 0) {
        return 0;
    }
    raw_delta = (LONG)x - (LONG)c->resize_press_x;
    snapped_delta = rlv_cr_quantize_delta(raw_delta);
    proposed = (LONG)c->resize_orig_left + snapped_delta;
    if (proposed < 1L) {
        proposed = 1L;
    }
    if (proposed > 32767L) {
        proposed = 32767L;
    }
    return rlv_cr_clamp_left(c, (WORD)proposed);
}

/*
 * Shine title inside [text_left, text_right], clipped. No sort reserve.
 * Alignment comes from the column's committed geometry policy.
 */
static VOID rlv_cr_draw_preview_title(RLV_Control *c,
                                     UWORD column,
                                     WORD text_left,
                                     WORD text_right,
                                     WORD y1,
                                     WORD y2,
                                     WORD baseline)
{
    const RLV_DrawOps *ops;
    APTR ctx;
    CONST_STRPTR title;
    UWORD len;
    UWORD fit;
    UWORD max_w;
    UWORD tw;
    WORD tx;
    UWORD alignment;
    struct Rectangle clip;

    if (c == 0 || c->draw_ops == 0 || text_right < text_left
        || column >= c->column_count) {
        return;
    }

    ops = c->draw_ops;
    ctx = c->draw_context;
    title = (c->columns != 0) ? c->columns[column].title : 0;
    if (title == 0 || title[0] == '\0') {
        return;
    }

    alignment = (UWORD)RLV_CELL_ALIGN_LEFT;
    if (c->col_geom != 0) {
        alignment = c->col_geom[column].alignment;
    }

    len = rlv_cr_strlen(title);
    max_w = (UWORD)(text_right - text_left + 1);
    if (ops->text_fit != 0) {
        fit = ops->text_fit(ctx, title, len, max_w);
    } else {
        fit = len;
        while (fit > 0 && ops->text_width(ctx, title, fit) > max_w) {
            fit--;
        }
    }
    if (fit == 0) {
        return;
    }

    tw = ops->text_width(ctx, title, fit);
    tx = text_left;
    if (alignment == (UWORD)RLV_CELL_ALIGN_CENTER) {
        tx = (WORD)(text_left + ((WORD)(text_right - text_left + 1
                                        - (WORD)tw) / 2));
    } else if (alignment == (UWORD)RLV_CELL_ALIGN_RIGHT) {
        tx = (WORD)(text_right - (WORD)tw + 1);
    }
    if (tx < text_left) {
        tx = text_left;
    }

    clip.MinX = text_left;
    clip.MinY = (WORD)(y1 + 1);
    clip.MaxX = text_right;
    clip.MaxY = (WORD)(y2 - 1);
    if (clip.MaxY < clip.MinY) {
        clip.MinY = y1;
        clip.MaxY = y2;
    }

    if (ops->push_clip != 0 && ops->push_clip(ctx, &clip)) {
        if (rlv_title_fill_is_patterned(c) && ops->draw_text_jam1 != 0) {
            ops->set_pens(ctx, c->pens.text, c->pens.text);
            ops->draw_text_jam1(ctx, tx, baseline, title, fit);
        } else {
            ops->set_pens(ctx, c->pens.shine,
                          rlv_title_fill_text_back_pen(c));
            ops->draw_text(ctx, tx, baseline, title, fit);
        }
        if (ops->pop_clip != 0) {
            ops->pop_clip(ctx);
        }
    } else {
        if (rlv_title_fill_is_patterned(c) && ops->draw_text_jam1 != 0) {
            ops->set_pens(ctx, c->pens.text, c->pens.text);
            ops->draw_text_jam1(ctx, tx, baseline, title, fit);
        } else {
            ops->set_pens(ctx, c->pens.shine,
                          rlv_title_fill_text_back_pen(c));
            ops->draw_text(ctx, tx, baseline, title, fit);
        }
    }
}

/*
 * Dedicated drag-preview header pass for the two affected columns.
 * Clears only the combined interior (fixed 3D outer bevels untouched),
 * fills with the configured title pattern, paints both titles, draws the
 * moving divider, and never invokes the ordinary header renderer or sort
 * indicator. Solid fill keeps shine-on-grey preview titles; patterned fills
 * use transparent TEXTPEN like the committed header.
 */
static VOID rlv_cr_paint_pair_preview(RLV_Control *c, WORD proposed_left_w)
{
    const RLV_DrawOps *ops;
    APTR ctx;
    UWORD left;
    UWORD right;
    WORD pair_left;
    WORD pair_right;
    WORD guide;
    WORD right_left;
    WORD right_w;
    WORD y1;
    WORD y2;
    WORD fy1;
    WORD fy2;
    WORD fx1;
    WORD fx2;
    WORD baseline;
    WORD pad;
    WORD left_text_l;
    WORD left_text_r;
    WORD right_text_l;
    WORD right_text_r;
    WORD left_content_r;
    WORD right_content_r;

    if (c == 0 || c->draw_ops == 0 || c->col_geom == 0) {
        return;
    }

    left = c->resize_left_col;
    right = c->resize_right_col;
    if (left >= c->column_count || right >= c->column_count) {
        return;
    }

    ops = c->draw_ops;
    ctx = c->draw_context;
    y1 = c->header_bounds.MinY;
    y2 = c->header_bounds.MaxY;
    if (y2 < y1) {
        return;
    }

    pair_left = c->col_geom[left].left;
    pair_right = rlv_cr_cell_right(c, right);
    guide = (WORD)(pair_left + proposed_left_w);
    if (guide <= pair_left || guide >= pair_right) {
        RLV_LOGF("INVARIANT COLUMN_RESIZE preview guide=%d pair=%d..%d",
                 (int)guide, (int)pair_left, (int)pair_right);
        return;
    }

    right_left = (WORD)(guide + (WORD)RLV_DIVIDER_WIDTH);
    right_w = (WORD)(c->resize_pair_total - proposed_left_w);
    if (right_w < 1 || right_left > pair_right) {
        RLV_LOG("INVARIANT COLUMN_RESIZE preview right geometry");
        return;
    }

    /* Combined interior: exclude fixed outer bevel pixels. */
    fx1 = (WORD)(pair_left + 1);
    fx2 = (WORD)(pair_right - 1);
    fy1 = (WORD)(y1 + 1);
    fy2 = (WORD)(y2 - 1);
    if (fx2 < fx1 || fy2 < fy1) {
        RLV_LOG("INVARIANT COLUMN_RESIZE preview interior empty");
        return;
    }

    rlv_title_fill_area(c, fx1, fy1, fx2, fy2);

    pad = (WORD)c->cell_padding_x;
    left_content_r = (WORD)(guide - 1);
    right_content_r = (WORD)(right_left + right_w - 1);
    if (right_content_r > pair_right) {
        right_content_r = pair_right;
    }

    left_text_l = (WORD)(pair_left + pad);
    left_text_r = (WORD)(left_content_r - pad);
    right_text_l = (WORD)(right_left + pad);
    right_text_r = (WORD)(right_content_r - pad);

    baseline = (WORD)(y1 + (WORD)c->cell_padding_y
                      + c->font_metrics.baseline);

    /* Left title, then moving divider, then right title (tight pass). */
    if (left_text_r >= left_text_l) {
        rlv_cr_draw_preview_title(c, left, left_text_l, left_text_r,
                                  y1, y2, baseline);
    }

    /*
     * Moving header divider matches adjacent cell framing: dark right edge
     * of the left cell at guide, shine left edge of the right cell at
     * guide+DIVIDER_WIDTH. Verticals stay inside the interior so the
     * fixed top/bottom bevels are not overwritten.
     */
    if (guide >= fx1 && guide <= fx2) {
        ops->set_pens(ctx, c->pens.separator, c->pens.background);
        ops->draw_line(ctx, guide, fy1, guide, fy2);
    }
    if (right_left >= fx1 && right_left <= fx2) {
        ops->set_pens(ctx, c->pens.shine, c->pens.background);
        ops->draw_line(ctx, right_left, fy1, right_left, fy2);
    }

    if (right_text_r >= right_text_l) {
        rlv_cr_draw_preview_title(c, right, right_text_l, right_text_r,
                                  y1, y2, baseline);
    }
}

VOID rlv_column_resize_cancel(RLV_Control *c, BOOL erase_visual)
{
    if (c == 0) {
        return;
    }
    if (c->resize_dragging) {
        RLV_LOG("COLUMN_RESIZE cancel");
        if (erase_visual) {
            rlv_cr_erase_guide(c);
            if (c->layout_valid && c->col_geom != 0) {
                rlv_render_header_column(c, c->resize_left_col);
                rlv_render_header_column(c, c->resize_right_col);
            }
        } else {
            c->resize_guide_visible = FALSE;
        }
    }
    c->resize_dragging = FALSE;
    rlv_cr_clear_pointer_wanted(c);
    c->resize_guide_visible = FALSE;
}

VOID rlv_column_resize_handle_cancel(RLV_Control *c)
{
    rlv_column_resize_cancel(c, TRUE);
}

/*
 * Hit the divider between columns left and left+1. Returns TRUE and fills
 * left_out when the pointer is in the narrow header slack zone.
 */
static BOOL rlv_cr_hit_divider(const RLV_Control *c,
                               WORD x,
                               WORD y,
                               UWORD *left_out)
{
    UWORD i;
    WORD dx;
    WORD slack;

    if (c == 0 || !c->column_resize_enabled || c->col_geom == 0
        || c->divider_x == 0 || c->column_count < 2
        || c->runtime_widths == 0) {
        return FALSE;
    }
    if (y < c->header_bounds.MinY || y > c->header_bounds.MaxY
        || x < c->header_bounds.MinX || x > c->header_bounds.MaxX) {
        return FALSE;
    }

    slack = (WORD)RLV_COL_RESIZE_HIT_SLACK;
    for (i = 0; i < c->divider_count; i++) {
        if (rlv_cr_column_locked(c, i)
            || rlv_cr_column_locked(c, (UWORD)(i + 1))) {
            continue;
        }
        dx = (WORD)(x - c->divider_x[i]);
        if (dx < 0) {
            dx = (WORD)(-dx);
        }
        if (dx <= slack) {
            if (left_out != 0) {
                *left_out = i;
            }
            return TRUE;
        }
    }
    return FALSE;
}

BOOL rlv_column_resize_handle_select_down(RLV_Control *c, WORD x, WORD y)
{
    UWORD left;
    UWORD right;
    WORD wl;
    WORD wr;

    if (c == 0) {
        return FALSE;
    }
    if (c->resize_dragging) {
        rlv_column_resize_cancel(c, TRUE);
    }
    if (!rlv_cr_hit_divider(c, x, y, &left)) {
        return FALSE;
    }

    right = (UWORD)(left + 1);
    if (right >= c->column_count || c->runtime_widths == 0) {
        RLV_LOG("COLUMN_RESIZE rejected divider (invariant)");
        return FALSE;
    }

    wl = c->runtime_widths[left];
    wr = c->runtime_widths[right];
    if (wl < 1) {
        wl = 1;
    }
    if (wr < 1) {
        wr = 1;
    }

    c->resize_dragging = TRUE;
    rlv_cr_set_pointer_wanted(c, TRUE);
    c->resize_left_col = left;
    c->resize_right_col = right;
    c->resize_orig_left = wl;
    c->resize_orig_right = wr;
    c->resize_pair_total = (WORD)(wl + wr);
    c->resize_preview_left_w = wl;
    c->resize_press_x = x;
    c->resize_guide_visible = FALSE;
    c->resize_guide_x = c->divider_x[left];

    RLV_LOGF("COLUMN_RESIZE arm left=%u right=%u wL=%d wR=%d",
             (unsigned)left, (unsigned)right, (int)wl, (int)wr);

    /* Initial body guide at the committed divider, then pair preview. */
    rlv_cr_paint_pair_preview(c, wl);
    rlv_cr_draw_guide(c, c->resize_guide_x);
    return TRUE;
}

VOID rlv_column_resize_handle_hover_move(RLV_Control *c, WORD x, WORD y)
{
    if (c == 0 || c->resize_dragging || !c->column_resize_enabled) {
        return;
    }
    rlv_cr_update_hover(c, x, y);
}

VOID rlv_column_resize_handle_pointer_move(RLV_Control *c, WORD x, WORD y)
{
    WORD proposed;
    WORD guide;
    LONG raw_delta;
    LONG snapped_delta;

    if (c == 0 || !c->resize_dragging || c->col_geom == 0) {
        return;
    }
    /* Release/move Y is ignored; arm validated header Y. */
    if (y != y) {
        return;
    }

    proposed = rlv_cr_proposed_from_x(c, x);
    if (proposed == c->resize_preview_left_w) {
        return;
    }

    raw_delta = (LONG)x - (LONG)c->resize_press_x;
    snapped_delta = rlv_cr_quantize_delta(raw_delta);
    guide = (WORD)(c->col_geom[c->resize_left_col].left + proposed);
    RLV_LOGF("COLUMN_RESIZE preview raw_d=%ld snap_d=%ld w=%d->%d guide=%d",
             (long)raw_delta, (long)snapped_delta,
             (int)c->resize_preview_left_w, (int)proposed, (int)guide);

    rlv_cr_erase_guide(c);
    rlv_cr_paint_pair_preview(c, proposed);
    rlv_cr_draw_guide(c, guide);

    c->resize_preview_left_w = proposed;
}

static BOOL rlv_cr_pair_needs_full_repaint(const RLV_Control *c,
                                          UWORD left,
                                          UWORD right,
                                          LONG old_content_h)
{
    UWORD wrap_l;
    UWORD wrap_r;

    if (c == 0 || c->columns == 0) {
        return TRUE;
    }
    wrap_l = c->columns[left].wrap_mode;
    wrap_r = c->columns[right].wrap_mode;
    if (wrap_l != (UWORD)RLV_WRAP_NONE
        || wrap_r != (UWORD)RLV_WRAP_NONE) {
        return TRUE;
    }
    if (c->content_height != old_content_h) {
        return TRUE;
    }
    return FALSE;
}

BOOL rlv_column_resize_handle_select_up(RLV_Control *c,
                                        WORD x,
                                        WORD y,
                                        RLV_Event *result)
{
    WORD proposed;
    WORD new_right;
    LONG old_content_h;
    BOOL full;
    UWORD left;
    UWORD right;

    if (c == 0 || !c->resize_dragging) {
        return FALSE;
    }
    /* Release Y may be outside the header; X drives the commit width. */
    if (y != y) {
        return FALSE;
    }

    proposed = rlv_cr_proposed_from_x(c, x);
    new_right = (WORD)(c->resize_pair_total - proposed);
    left = c->resize_left_col;
    right = c->resize_right_col;

    rlv_cr_erase_guide(c);

    /* Restore header before commit paint; layout may move geometry. */
    if (c->layout_valid && c->col_geom != 0) {
        rlv_render_header_column(c, left);
        rlv_render_header_column(c, right);
    }

    if (proposed == c->resize_orig_left
        && new_right == c->resize_orig_right) {
        RLV_LOG("COLUMN_RESIZE commit unchanged");
        c->resize_dragging = FALSE;
        c->resize_guide_visible = FALSE;
        rlv_cr_update_hover(c, x, y);
        return FALSE;
    }

    if (c->runtime_widths == 0
        || left >= c->runtime_width_count
        || right >= c->runtime_width_count) {
        c->resize_dragging = FALSE;
        rlv_cr_clear_pointer_wanted(c);
        return FALSE;
    }

    old_content_h = c->content_height;
    c->runtime_widths[left] = proposed;
    c->runtime_widths[right] = new_right;
    c->resize_dragging = FALSE;
    c->resize_guide_visible = FALSE;
    rlv_cr_update_hover(c, x, y);

    rlv_layout_invalidate(c);
    if (!rlv_layout_rebuild(c)) {
        RLV_LOG("FAIL COLUMN_RESIZE layout rebuild");
        /* Restore previous widths. */
        c->runtime_widths[left] = c->resize_orig_left;
        c->runtime_widths[right] = c->resize_orig_right;
        rlv_layout_invalidate(c);
        rlv_layout_rebuild(c);
        return FALSE;
    }

    full = rlv_cr_pair_needs_full_repaint(c, left, right, old_content_h);
    RLV_LOGF("COLUMN_RESIZE commit left=%u right=%u %d/%d -> %d/%d repaint=%s",
             (unsigned)left, (unsigned)right,
             (int)c->resize_orig_left, (int)c->resize_orig_right,
             (int)proposed, (int)new_right,
             full ? "full" : "regional");

    if (result != 0) {
        result->type = (UWORD)RLV_EVENT_COLUMN_RESIZED;
        result->row = -1;
        result->previous_row = -1;
        result->value = full ? RLV_RESIZE_REPAINT_FULL
                             : RLV_RESIZE_REPAINT_REGIONAL;
        result->column = left;
        result->control_type = 0;
        result->control_action = 0;
        result->row_user_data = 0;
        result->previous_value = 0;
        result->cell_value = 0;
        result->resize_left = left;
        result->resize_right = right;
        result->old_left_width = c->resize_orig_left;
        result->old_right_width = c->resize_orig_right;
        result->new_left_width = proposed;
        result->new_right_width = new_right;
    }
    return TRUE;
}

/* ---- Public API (enabled build) ---- */

VOID rlv_set_column_resize_enabled(RLV_Control *c, BOOL enabled)
{
    if (c == 0) {
        return;
    }
    if (!enabled) {
        if (c->resize_dragging) {
            rlv_column_resize_cancel(c, TRUE);
        } else {
            rlv_cr_clear_pointer_wanted(c);
        }
    }
    c->column_resize_enabled = enabled ? TRUE : FALSE;
    RLV_LOGF("COLUMN_RESIZE enabled=%d", (int)c->column_resize_enabled);
}

BOOL rlv_get_column_resize_enabled(const RLV_Control *c)
{
    if (c == 0) {
        return FALSE;
    }
    return c->column_resize_enabled;
}

BOOL rlv_column_resize_is_active(const RLV_Control *c)
{
    if (c == 0) {
        return FALSE;
    }
    return c->resize_dragging;
}

BOOL rlv_column_resize_wants_pointer(const RLV_Control *c)
{
    if (c == 0 || !c->column_resize_enabled) {
        return FALSE;
    }
    return c->resize_pointer_wanted;
}

BOOL rlv_column_resize_needs_report_mouse(const RLV_Control *c)
{
    if (c == 0 || !c->column_resize_enabled) {
        return FALSE;
    }
    /* Hover detection and drag tracking both require MOUSEMOVE delivery. */
    return TRUE;
}

BOOL rlv_get_column_width(const RLV_Control *c, UWORD column, WORD *out_width)
{
    if (c == 0 || out_width == 0 || column >= c->column_count) {
        return FALSE;
    }
    *out_width = rlv_column_effective_width(c, column);
    return TRUE;
}

BOOL rlv_set_column_width(RLV_Control *c, UWORD column, WORD width)
{
    WORD min_w;

    if (c == 0 || c->runtime_widths == 0 || column >= c->runtime_width_count) {
        return FALSE;
    }
    if (c->resize_dragging) {
        rlv_column_resize_cancel(c, TRUE);
    }
    min_w = rlv_cr_min_width(c, column);
    if (width < min_w) {
        width = min_w;
    }
    if (width < 1) {
        width = 1;
    }
    c->runtime_widths[column] = width;
    rlv_layout_invalidate(c);
    return TRUE;
}

BOOL rlv_set_column_widths(RLV_Control *c, const WORD *widths, UWORD count)
{
    UWORD i;
    WORD w;
    WORD min_w;

    if (c == 0 || widths == 0 || c->runtime_widths == 0
        || count != c->runtime_width_count) {
        return FALSE;
    }
    if (c->resize_dragging) {
        rlv_column_resize_cancel(c, TRUE);
    }
    for (i = 0; i < count; i++) {
        w = widths[i];
        min_w = rlv_cr_min_width(c, i);
        if (w < min_w) {
            w = min_w;
        }
        if (w < 1) {
            w = 1;
        }
        c->runtime_widths[i] = w;
    }
    rlv_layout_invalidate(c);
    return TRUE;
}

BOOL rlv_reset_column_widths(RLV_Control *c)
{
    UWORD i;
    WORD w;

    if (c == 0 || c->runtime_widths == 0 || c->columns == 0) {
        return FALSE;
    }
    if (c->resize_dragging) {
        rlv_column_resize_cancel(c, TRUE);
    }
    for (i = 0; i < c->runtime_width_count && i < c->column_count; i++) {
        w = c->columns[i].width_pixels;
        if (w < 1) {
            w = 1;
        }
        c->runtime_widths[i] = w;
    }
    rlv_layout_invalidate(c);
    return TRUE;
}

BOOL rlv_set_column_min_width(RLV_Control *c, UWORD column, WORD min_width)
{
    if (c == 0 || c->runtime_mins == 0 || column >= c->runtime_width_count) {
        return FALSE;
    }
    if (min_width < 1) {
        min_width = 1;
    }
    c->runtime_mins[column] = min_width;
    return TRUE;
}

#endif /* RLV_ENABLE_COLUMN_RESIZE */
