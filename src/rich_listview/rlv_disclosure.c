/**
 * Disclosure (+/-) cell geometry and paint for expandable rows.
 *
 * Drawn with ordinary Amiga font glyphs inside a compact outlined box in
 * the first content text-line band (same placement band as checkboxes).
 * Non-expandable rows leave the cell empty. Hit-test reuses resolve_rect.
 */

#include "rich_listview/rlv_internal.h"

#if defined(RLV_ENABLE_EXPANDABLE_ROWS) && (RLV_ENABLE_EXPANDABLE_ROWS != 0)

#define RLV_DISC_BOX_DEFAULT  9
#define RLV_DISC_BOX_MIN      5

static WORD rlv_disc_align_x(UWORD alignment,
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
    case RLV_CELL_ALIGN_RIGHT:
        x = (WORD)(right - (WORD)box_w + 1);
        break;
    case RLV_CELL_ALIGN_CENTER:
        x = (WORD)(left + ((span - (WORD)box_w) / 2));
        break;
    case RLV_CELL_ALIGN_LEFT:
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

static UWORD rlv_disc_fit_box(WORD avail_w, WORD avail_h)
{
    WORD side;
    WORD box;

    if (avail_w < 1 || avail_h < 1) {
        return 0;
    }

    side = avail_w;
    if (avail_h < side) {
        side = avail_h;
    }

    box = (WORD)RLV_DISC_BOX_DEFAULT;
    while (box > (WORD)RLV_DISC_BOX_MIN && box > side) {
        box--;
    }
    if (box > side) {
        return 0;
    }
    return (UWORD)box;
}

BOOL rlv_disclosure_resolve_rect(const RLV_Control *c,
                                      LONG logical_row,
                                      UWORD column,
                                      struct Rectangle *out_box)
{
    const RLV_PixelColumn *col;
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

    col_type = (UWORD)(c->columns[column].flags & RLV_COL_TYPE_MASK);
    if (col_type != (UWORD)RLV_COL_TYPE_DISCLOSURE) {
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

    {
        LONG view;

        view = rlv_view_for_source(c, logical_row);
        if (view < 0 || (ULONG)view >= c->row_count) {
            return FALSE;
        }
        row_top = (WORD)(c->viewport_bounds.MinY
                         + c->layout_rows[view].top_y
                         - c->scroll_y);
    }
    band_top = (WORD)(row_top + (WORD)c->cell_padding_y);
    band_bottom = (WORD)(band_top + line_h - 1);
    band_h = (WORD)(band_bottom - band_top + 1);
    if (band_h < 1) {
        return FALSE;
    }

    avail_w = (WORD)(right - left + 1);
    avail_h = band_h;
    box = rlv_disc_fit_box(avail_w, avail_h);
    if (box < (UWORD)RLV_DISC_BOX_MIN) {
        return FALSE;
    }

    x = rlv_disc_align_x(col->alignment, left, right, box);
    y = (WORD)(band_top + ((band_h - (WORD)box) / 2));

    out_box->MinX = x;
    out_box->MinY = y;
    out_box->MaxX = (WORD)(x + (WORD)box - 1);
    out_box->MaxY = (WORD)(y + (WORD)box - 1);
    return TRUE;
}

static VOID rlv_disc_draw_frame(const RLV_DrawOps *ops,
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

VOID rlv_disclosure_paint(RLV_Control *c,
                               LONG logical_row,
                               UWORD column,
                               BOOL selected)
{
    const RLV_DrawOps *ops;
    APTR ctx;
    const RLV_Cell *cell;
    struct Rectangle box;
    ULONG index;
    UWORD frame_pen;
    UWORD text_pen;
    UWORD back_pen;
    BOOL enabled;
    BOOL expanded;
    BOOL expandable;
    CONST_STRPTR glyph;
    UWORD glyph_w;
    WORD text_x;
    WORD baseline;
    WORD box_w;
    WORD box_h;
    UWORD col_type;

    if (c == 0 || c->draw_ops == 0) {
        return;
    }
    if (c->cell_snapshot == 0 || c->columns == 0 || c->row_expand == 0) {
        return;
    }
    if (logical_row < 0 || (ULONG)logical_row >= c->row_count) {
        return;
    }
    if (column >= c->column_count) {
        return;
    }

    col_type = (UWORD)(c->columns[column].flags & RLV_COL_TYPE_MASK);
    if (col_type != (UWORD)RLV_COL_TYPE_DISCLOSURE) {
        return;
    }

    index = (ULONG)logical_row * (ULONG)c->column_count + (ULONG)column;
    if (index >= c->cell_snapshot_count) {
        return;
    }

    cell = &c->cell_snapshot[index];
    if ((cell->flags & RLV_CELL_F_VISIBLE) == 0) {
        return;
    }

    expandable = rlv_is_row_expandable(c, logical_row);
    if (!rlv_disclosure_ui_enabled(c)
        || !expandable
        || !rlv_row_has_multi_line_wrap(c, logical_row)) {
        /* Reserved empty disclosure cell — background only (row fill).
         * Non-collapsible display modes and single-line wrap suppress +/-.
         */
        return;
    }

    if (!rlv_disclosure_resolve_rect(c, logical_row, column, &box)) {
        return;
    }

    ops = c->draw_ops;
    ctx = c->draw_context;
    enabled = ((cell->flags & RLV_CELL_F_ENABLED) != 0) ? TRUE : FALSE;
    expanded = rlv_is_row_expanded(c, logical_row);

    if (selected) {
        back_pen = c->pens.selected_background;
        frame_pen = enabled ? c->pens.selected_text : c->pens.separator;
        text_pen = frame_pen;
    } else {
        back_pen = c->pens.background;
        frame_pen = enabled ? c->pens.text : c->pens.separator;
        text_pen = frame_pen;
    }

    if (box.MaxX > box.MinX + 1 && box.MaxY > box.MinY + 1) {
        ops->set_pens(ctx, back_pen, back_pen);
        ops->fill_rect(ctx,
                       (WORD)(box.MinX + 1),
                       (WORD)(box.MinY + 1),
                       (WORD)(box.MaxX - 1),
                       (WORD)(box.MaxY - 1));
    }

    rlv_disc_draw_frame(ops, ctx, &box, frame_pen, back_pen);

    glyph = expanded ? "-" : "+";
    glyph_w = 0;
    if (ops->text_width != 0) {
        glyph_w = ops->text_width(ctx, glyph, 1);
    }
    if (glyph_w < 1) {
        glyph_w = 1;
    }

    box_w = (WORD)(box.MaxX - box.MinX + 1);
    box_h = (WORD)(box.MaxY - box.MinY + 1);
    text_x = (WORD)(box.MinX + ((box_w - (WORD)glyph_w) / 2));
    baseline = (WORD)(box.MinY
                      + ((box_h - (WORD)c->line_height) / 2)
                      + (WORD)c->font_metrics.baseline);
    if (baseline < box.MinY) {
        baseline = (WORD)(box.MinY + (WORD)c->font_metrics.baseline);
    }

    ops->set_pens(ctx, text_pen, back_pen);
    ops->draw_text(ctx, text_x, baseline, glyph, 1);
}

#endif /* RLV_ENABLE_EXPANDABLE_ROWS */
