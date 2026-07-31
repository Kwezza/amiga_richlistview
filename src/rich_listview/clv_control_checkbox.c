/**
 * Checkbox geometry + paint for the experimental custom ListView control.
 *
 * Phase C3: snapshot-driven draw inside the first content text-line band.
 * Hit-test / arm / commit (C4) reuse clv_ctrl_checkbox_resolve_rect().
 */

#include "rich_listview/clv_control_internal.h"

/* Architecture-neutral appearance defaults (§C / §D.3). */
#define CLV_CTRL_CB_BOX_DEFAULT  9
#define CLV_CTRL_CB_BOX_MIN      5
#define CLV_CTRL_CB_INNER_PAD    1

static WORD clv_ctrl_cb_align_x(UWORD alignment,
                                WORD left,
                                WORD right,
                                UWORD box_w)
{
    WORD span;
    WORD x;

    span = (WORD)(right - left + 1);
    if (span < 1) {
        span = 1;
    }
    switch (alignment) {
    case CLV_CELL_ALIGN_RIGHT:
        x = (WORD)(right - (WORD)box_w + 1);
        break;
    case CLV_CELL_ALIGN_CENTER:
        x = (WORD)(left + ((span - (WORD)box_w) / 2));
        break;
    case CLV_CELL_ALIGN_LEFT:
    default:
        x = left;
        break;
    }
    if (x < left) {
        x = left;
    }
    if ((WORD)(x + (WORD)box_w - 1) > right) {
        x = (WORD)(right - (WORD)box_w + 1);
        if (x < left) {
            x = left;
        }
    }
    return x;
}

static UWORD clv_ctrl_cb_fit_box(WORD avail_w, WORD avail_h)
{
    WORD side;
    WORD pad;
    WORD box;

    if (avail_w < 1 || avail_h < 1) {
        return 0;
    }

    side = avail_w;
    if (avail_h < side) {
        side = avail_h;
    }

    pad = (WORD)CLV_CTRL_CB_INNER_PAD;
    box = (WORD)CLV_CTRL_CB_BOX_DEFAULT;

    /* Shrink pad first, then box, down to minimum. */
    while ((box + (2 * pad)) > side && pad > 0) {
        pad--;
    }
    while ((box + (2 * pad)) > side && box > (WORD)CLV_CTRL_CB_BOX_MIN) {
        box--;
    }
    if ((box + (2 * pad)) > side) {
        return 0;
    }
    return (UWORD)box;
}

BOOL clv_ctrl_checkbox_resolve_rect(const CLV_Control *c,
                                    LONG logical_row,
                                    UWORD column,
                                    struct Rectangle *out_box)
{
    const CLV_PixelColumn *col;
    WORD row_top;
    WORD line_h;
    WORD band_top;
    WORD band_bottom;
    WORD band_h;
    WORD left;
    WORD right;
    WORD avail_w;
    WORD avail_h;
    UWORD box;
    WORD x;
    WORD y;
    UWORD col_type;

    if (c == 0 || out_box == 0) {
        return FALSE;
    }
    if (logical_row < 0 || (ULONG)logical_row >= c->row_count) {
        return FALSE;
    }
    if (c->columns == 0 || column >= c->column_count || c->col_geom == 0) {
        return FALSE;
    }
    if (c->layout_rows == 0) {
        return FALSE;
    }

    col_type = (UWORD)(c->columns[column].flags & CLV_CTRL_COL_TYPE_MASK);
    if (col_type != (UWORD)CLV_CTRL_COL_TYPE_CHECKBOX) {
        return FALSE;
    }

    col = &c->col_geom[column];
    left = col->text_left;
    right = col->text_right;
    if (right < left) {
        return FALSE;
    }

    line_h = (WORD)c->line_height;
    if (line_h < 1) {
        line_h = 1;
    }

    /*
     * First content text-line band: top cell padding + one line_height.
     * Do not centre in the full multi-line content height (§D.11).
     */
    row_top = (WORD)(c->viewport_bounds.MinY
                     + c->layout_rows[logical_row].top_y
                     - c->scroll_y);
    band_top = (WORD)(row_top + (WORD)c->cell_padding_y);
    band_bottom = (WORD)(band_top + line_h - 1);
    band_h = (WORD)(band_bottom - band_top + 1);
    if (band_h < 1) {
        return FALSE;
    }

    avail_w = (WORD)(right - left + 1);
    avail_h = band_h;
    box = clv_ctrl_cb_fit_box(avail_w, avail_h);
    if (box < (UWORD)CLV_CTRL_CB_BOX_MIN) {
        return FALSE;
    }

    x = clv_ctrl_cb_align_x(col->alignment, left, right, box);
    y = (WORD)(band_top + ((band_h - (WORD)box) / 2));

    out_box->MinX = x;
    out_box->MinY = y;
    out_box->MaxX = (WORD)(x + (WORD)box - 1);
    out_box->MaxY = (WORD)(y + (WORD)box - 1);
    return TRUE;
}

static VOID clv_ctrl_cb_draw_plain_frame(const CLV_DrawOps *ops,
                                         APTR ctx,
                                         const struct Rectangle *box,
                                         UWORD pen,
                                         UWORD back)
{
    ops->set_pens(ctx, pen, back);
    ops->draw_line(ctx, box->MinX, box->MinY, box->MaxX, box->MinY);
    ops->draw_line(ctx, box->MinX, box->MinY, box->MinX, box->MaxY);
    ops->draw_line(ctx, box->MinX, box->MaxY, box->MaxX, box->MaxY);
    ops->draw_line(ctx, box->MaxX, box->MinY, box->MaxX, box->MaxY);
}

static VOID clv_ctrl_cb_draw_tick(const CLV_DrawOps *ops,
                                  APTR ctx,
                                  const struct Rectangle *box,
                                  UWORD pen,
                                  UWORD back,
                                  BOOL dither)
{
    WORD x1;
    WORD y1;
    WORD x2;
    WORD y2;
    WORD mid_x;
    WORD mid_y;
    WORD w;
    WORD h;

    x1 = (WORD)(box->MinX + 2);
    y1 = (WORD)(box->MinY + 2);
    x2 = (WORD)(box->MaxX - 2);
    y2 = (WORD)(box->MaxY - 2);
    if (x2 < x1 || y2 < y1) {
        /* Tiny box: fill interior instead of a tick. */
        if (box->MaxX > box->MinX + 2 && box->MaxY > box->MinY + 2) {
            ops->set_pens(ctx, pen, back);
            ops->fill_rect(ctx,
                           (WORD)(box->MinX + 1),
                           (WORD)(box->MinY + 1),
                           (WORD)(box->MaxX - 1),
                           (WORD)(box->MaxY - 1));
        }
        return;
    }

    w = (WORD)(x2 - x1 + 1);
    h = (WORD)(y2 - y1 + 1);
    mid_x = (WORD)(x1 + (w / 3));
    mid_y = (WORD)(y1 + ((2 * h) / 3));

    ops->set_pens(ctx, pen, back);
    if (!dither) {
        ops->draw_line(ctx, x1, mid_y, mid_x, y2);
        ops->draw_line(ctx, mid_x, y2, x2, y1);
        return;
    }

    /* Disabled: sparse tick (every other pixel along each stroke). */
    {
        WORD x;
        WORD y;
        WORD steps;
        WORD i;
        WORD dx;
        WORD dy;

        steps = (WORD)(mid_x - x1);
        if ((y2 - mid_y) > steps) {
            steps = (WORD)(y2 - mid_y);
        }
        if (steps < 1) {
            steps = 1;
        }
        for (i = 0; i <= steps; i += 2) {
            x = (WORD)(x1 + ((i * (mid_x - x1)) / steps));
            y = (WORD)(mid_y + ((i * (y2 - mid_y)) / steps));
            ops->fill_rect(ctx, x, y, x, y);
        }

        dx = (WORD)(x2 - mid_x);
        dy = (WORD)(y1 - y2);
        steps = dx;
        if (dy < 0) {
            dy = (WORD)(-dy);
        }
        if (dy > steps) {
            steps = dy;
        }
        if (steps < 1) {
            steps = 1;
        }
        for (i = 0; i <= steps; i += 2) {
            x = (WORD)(mid_x + ((i * (x2 - mid_x)) / steps));
            y = (WORD)(y2 + ((i * (y1 - y2)) / steps));
            ops->fill_rect(ctx, x, y, x, y);
        }
    }
}

VOID clv_ctrl_checkbox_paint(CLV_Control *c,
                             LONG logical_row,
                             UWORD column,
                             BOOL selected)
{
    const CLV_DrawOps *ops;
    APTR ctx;
    const CLV_ControlCell *cell;
    struct Rectangle box;
    ULONG index;
    UWORD frame_pen;
    UWORD mark_pen;
    UWORD back_pen;
    BOOL enabled;
    BOOL checked;

    if (c == 0 || c->draw_ops == 0) {
        return;
    }
    if (c->cell_snapshot == 0 || c->columns == 0) {
        return;
    }
    if (logical_row < 0 || (ULONG)logical_row >= c->row_count) {
        return;
    }
    if (column >= c->column_count) {
        return;
    }

    index = (ULONG)logical_row * (ULONG)c->column_count + (ULONG)column;
    if (index >= c->cell_snapshot_count) {
        return;
    }

    cell = &c->cell_snapshot[index];
    if ((cell->flags & CLV_CTRL_CELL_F_VISIBLE) == 0) {
        return;
    }

    if (!clv_ctrl_checkbox_resolve_rect(c, logical_row, column, &box)) {
        return;
    }

    ops = c->draw_ops;
    ctx = c->draw_context;
    enabled = ((cell->flags & CLV_CTRL_CELL_F_ENABLED) != 0) ? TRUE : FALSE;
    checked = (cell->value == (UBYTE)CLV_CTRL_CELL_CHECKED) ? TRUE : FALSE;

    if (selected) {
        back_pen = c->pens.selected_background;
        frame_pen = enabled ? c->pens.selected_text : c->pens.separator;
        mark_pen = frame_pen;
    } else {
        back_pen = c->pens.background;
        frame_pen = enabled ? c->pens.text : c->pens.separator;
        mark_pen = frame_pen;
    }

    /* Clear interior so prior ticks do not linger under unchecked. */
    if (box.MaxX > box.MinX + 1 && box.MaxY > box.MinY + 1) {
        ops->set_pens(ctx, back_pen, back_pen);
        ops->fill_rect(ctx,
                       (WORD)(box.MinX + 1),
                       (WORD)(box.MinY + 1),
                       (WORD)(box.MaxX - 1),
                       (WORD)(box.MaxY - 1));
    }

    clv_ctrl_cb_draw_plain_frame(ops, ctx, &box, frame_pen, back_pen);

    if (checked) {
        clv_ctrl_cb_draw_tick(ops, ctx, &box, mark_pen, back_pen,
                              enabled ? FALSE : TRUE);
    }
}
