/**
 * Optional interactive column resizing (RLV_ENABLE_COLUMN_RESIZE).
 *
 * Two-column exchange: dragging the divider between columns L and R adjusts
 * only those widths so L+R stays constant and later columns keep the same X.
 * Live preview uses a reversible XOR vertical guide and a clipped
 * high-contrast left-header title without rebuilding wrap/layout.
 */

#include "rich_listview/rlv_internal.h"
#include "rich_listview/rlv_log.h"
#include "rich_listview/rlv_platform_internal.h"

#include <string.h>

#if defined(RLV_ENABLE_COLUMN_RESIZE) && (RLV_ENABLE_COLUMN_RESIZE != 0)

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
    c->resize_guide_visible = FALSE;
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

static VOID rlv_cr_xor_guide(RLV_Control *c, WORD x)
{
    const RLV_DrawOps *ops;
    WORD y1;
    WORD y2;

    if (c == 0 || c->draw_ops == 0 || c->draw_ops->draw_xor_vline == 0) {
        return;
    }
    ops = c->draw_ops;
    y1 = c->header_bounds.MinY;
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

/*
 * Grey-fill a vertical strip inside the header face (inset 1px from frame).
 * x_lo/x_hi are inclusive window X; empty or inverted ranges are no-ops.
 */
static VOID rlv_cr_fill_header_strip(RLV_Control *c, WORD x_lo, WORD x_hi)
{
    const RLV_DrawOps *ops;
    WORD fy1;
    WORD fy2;

    if (c == 0 || c->draw_ops == 0 || x_hi < x_lo) {
        return;
    }
    ops = c->draw_ops;
    fy1 = (WORD)(c->header_bounds.MinY + 1);
    fy2 = (WORD)(c->header_bounds.MaxY - 1);
    if (fy2 < fy1) {
        return;
    }
    ops->set_pens(c->draw_context, c->pens.background, c->pens.background);
    ops->fill_rect(c->draw_context, x_lo, fy1, x_hi, fy2);
}

/*
 * Preview left title: grey face to proposed width, then shine text clipped
 * to that face. Does not paint committed (black) titles.
 */
static VOID rlv_cr_paint_white_title(RLV_Control *c, WORD proposed_left_w)
{
    const RLV_DrawOps *ops;
    APTR ctx;
    UWORD left;
    WORD x1;
    WORD x2;
    WORD y1;
    WORD y2;
    WORD guide;
    WORD text_left;
    WORD text_right;
    WORD baseline;
    CONST_STRPTR title;
    UWORD len;
    UWORD fit;
    UWORD max_w;
    UWORD tw;
    WORD tx;
    WORD fx1;
    WORD fy1;
    WORD fx2;
    WORD fy2;
    struct Rectangle clip;
    RLV_PixelColumn geom;

    if (c == 0 || c->draw_ops == 0 || c->col_geom == 0) {
        return;
    }

    left = c->resize_left_col;
    if (left >= c->column_count) {
        return;
    }

    ops = c->draw_ops;
    ctx = c->draw_context;
    y1 = c->header_bounds.MinY;
    y2 = c->header_bounds.MaxY;
    x1 = c->col_geom[left].left;
    guide = (WORD)(x1 + proposed_left_w);
    if (guide <= x1) {
        return;
    }
    x2 = (WORD)(guide - 1);

    /*
     * Preview fill matches normal header grey. Inset by one pixel so the
     * title-cell frame is not erased. Title uses shine for drag contrast.
     */
    fx1 = (WORD)(x1 + 1);
    fy1 = (WORD)(y1 + 1);
    fx2 = (WORD)(x2 - 1);
    fy2 = (WORD)(y2 - 1);
    if (fx2 >= fx1 && fy2 >= fy1) {
        ops->set_pens(ctx, c->pens.background, c->pens.background);
        ops->fill_rect(ctx, fx1, fy1, fx2, fy2);
    }

    geom = c->col_geom[left];
    text_left = (WORD)(x1 + (WORD)c->cell_padding_x);
    text_right = (WORD)(x2 - (WORD)c->cell_padding_x);
    if (text_right < text_left) {
        return;
    }
    geom.left = x1;
    geom.right = x2;
    geom.text_left = text_left;
    geom.text_right = text_right;

    title = (c->columns != 0) ? c->columns[left].title : 0;
    if (title == 0 || title[0] == '\0') {
        return;
    }

    baseline = (WORD)(y1 + (WORD)c->cell_padding_y
                      + c->font_metrics.baseline);
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
    if (geom.alignment == (UWORD)RLV_CELL_ALIGN_CENTER) {
        tx = (WORD)(text_left + ((WORD)(text_right - text_left + 1
                                        - (WORD)tw) / 2));
    } else if (geom.alignment == (UWORD)RLV_CELL_ALIGN_RIGHT) {
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
        ops->set_pens(ctx, c->pens.shine, c->pens.background);
        ops->draw_text(ctx, tx, baseline, title, fit);
        if (ops->pop_clip != 0) {
            ops->pop_clip(ctx);
        }
    } else {
        ops->set_pens(ctx, c->pens.shine, c->pens.background);
        ops->draw_text(ctx, tx, baseline, title, fit);
    }
}

/*
 * On shrink, repair the strip the guide vacated. Left-of-divider stays in
 * preview grey; pixels at/after the committed divider restore the right
 * header via a clipped area paint (not a full column redraw).
 * Expand needs no strip work — white-title fill covers the new interior.
 */
static VOID rlv_cr_paint_preview_delta(RLV_Control *c, WORD old_w, WORD new_w)
{
    UWORD left;
    UWORD right;
    WORD x1;
    WORD old_guide;
    WORD new_guide;
    WORD strip_lo;
    WORD strip_hi;
    WORD grey_hi;
    WORD div_x;
    struct Rectangle area;

    if (c == 0 || c->col_geom == 0 || c->divider_x == 0) {
        return;
    }
    if (new_w >= old_w) {
        return;
    }

    left = c->resize_left_col;
    right = c->resize_right_col;
    if (left >= c->column_count || right >= c->column_count
        || left >= c->divider_count) {
        return;
    }

    x1 = c->col_geom[left].left;
    old_guide = (WORD)(x1 + old_w);
    new_guide = (WORD)(x1 + new_w);
    strip_lo = new_guide;
    strip_hi = (WORD)(old_guide - 1);
    if (strip_hi < strip_lo) {
        return;
    }

    div_x = c->divider_x[left];

    /* Preview territory left of the committed divider — grey only. */
    grey_hi = strip_hi;
    if (grey_hi >= div_x) {
        grey_hi = (WORD)(div_x - 1);
    }
    if (grey_hi >= strip_lo) {
        rlv_cr_fill_header_strip(c, strip_lo, grey_hi);
    }

    /* Exposed strip in/after the divider — restore right header strip. */
    if (strip_hi >= div_x) {
        area.MinX = strip_lo;
        if (area.MinX < div_x) {
            area.MinX = div_x;
        }
        area.MaxX = strip_hi;
        area.MinY = c->header_bounds.MinY;
        area.MaxY = c->header_bounds.MaxY;
        rlv_render_header_column_area(c, right, &area);
    }
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
    LONG delta;
    LONG proposed;

    if (c == 0 || c->col_geom == 0) {
        return 0;
    }
    delta = (LONG)x - (LONG)c->resize_press_x;
    proposed = (LONG)c->resize_orig_left + delta;
    if (proposed < 1L) {
        proposed = 1L;
    }
    if (proposed > 32767L) {
        proposed = 32767L;
    }
    return rlv_cr_clamp_left(c, (WORD)proposed);
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

    /* Initial guide at the committed divider, then enter preview style. */
    rlv_cr_draw_guide(c, c->resize_guide_x);
    rlv_cr_paint_white_title(c, wl);
    return TRUE;
}

VOID rlv_column_resize_handle_pointer_move(RLV_Control *c, WORD x, WORD y)
{
    WORD proposed;
    WORD guide;
    WORD old_w;

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

    old_w = c->resize_preview_left_w;
    guide = (WORD)(c->col_geom[c->resize_left_col].left + proposed);

    rlv_cr_erase_guide(c);
    rlv_cr_paint_preview_delta(c, old_w, proposed);
    rlv_cr_paint_white_title(c, proposed);
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
        return FALSE;
    }

    if (c->runtime_widths == 0
        || left >= c->runtime_width_count
        || right >= c->runtime_width_count) {
        c->resize_dragging = FALSE;
        return FALSE;
    }

    old_content_h = c->content_height;
    c->runtime_widths[left] = proposed;
    c->runtime_widths[right] = new_right;
    c->resize_dragging = FALSE;
    c->resize_guide_visible = FALSE;

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
    if (!enabled && c->resize_dragging) {
        rlv_column_resize_cancel(c, TRUE);
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
