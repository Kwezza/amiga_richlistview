/**
 * Experimental custom ListView control — create/destroy and setters.
 */

#include "rich_listview/clv_control_internal.h"
#include "rich_listview/clv_control_log.h"
#include "rich_listview/clv_platform_internal.h"

#include <string.h>

static VOID clv_control_free_layout_cache(CLV_Control *c)
{
    if (c == 0) {
        return;
    }
    clv_control_layout_free_wraps(c);
    if (c->layout_rows != 0) {
        clv_platform_free(c->layout_rows);
        c->layout_rows = 0;
    }
    if (c->col_geom != 0) {
        clv_platform_free(c->col_geom);
        c->col_geom = 0;
    }
    if (c->divider_x != 0) {
        clv_platform_free(c->divider_x);
        c->divider_x = 0;
    }
    c->divider_count = 0;
    c->layout_valid = FALSE;
}

static VOID clv_control_free_cell_snapshot(CLV_Control *c)
{
    if (c == 0) {
        return;
    }
    if (c->cell_snapshot != 0) {
        clv_platform_free(c->cell_snapshot);
        c->cell_snapshot = 0;
    }
    c->cell_snapshot_count = 0;
}

/*
 * Rebuild owned cell snapshot from borrowed row control_cells.
 * Dimensions follow current row_count * column_count. Rows with a NULL
 * control_cells pointer contribute zeroed entries. Returns FALSE only on
 * allocation failure (previous snapshot already freed).
 */
static BOOL clv_control_refresh_cell_snapshot(CLV_Control *c)
{
    ULONG need;
    ULONG i;
    ULONG ncopy;
    CLV_ControlCell *snap;

    if (c == 0) {
        return FALSE;
    }

    clv_control_free_cell_snapshot(c);

    /* Clear unused arm fields whenever the snapshot is rebuilt. */
    c->control_armed = FALSE;
    c->armed_row = -1;
    c->armed_column = 0;
    c->armed_type = 0;

    if (c->row_count == 0 || c->column_count == 0 || c->rows == 0) {
        return TRUE;
    }

    need = c->row_count * (ULONG)c->column_count;
    snap = (CLV_ControlCell *)clv_platform_malloc(need * sizeof(*snap));
    if (snap == 0) {
        CLV_LOG("FAIL clv_control_refresh_cell_snapshot malloc");
        return FALSE;
    }
    memset(snap, 0, (size_t)need * sizeof(*snap));

    ncopy = (ULONG)c->column_count * sizeof(CLV_ControlCell);
    for (i = 0; i < c->row_count; i++) {
        if (c->rows[i].control_cells != 0) {
            memcpy(&snap[i * (ULONG)c->column_count],
                   c->rows[i].control_cells,
                   (size_t)ncopy);
        }
    }

    c->cell_snapshot = snap;
    c->cell_snapshot_count = need;
    return TRUE;
}

/* Reject inverted / empty outer bounds; clamp to a non-inverted rectangle. */
static BOOL clv_ctrl_normalize_bounds(struct Rectangle *bounds)
{
    WORD tmp;

    if (bounds == 0) {
        return FALSE;
    }
    if (bounds->MaxX < bounds->MinX) {
        tmp = bounds->MinX;
        bounds->MinX = bounds->MaxX;
        bounds->MaxX = tmp;
    }
    if (bounds->MaxY < bounds->MinY) {
        tmp = bounds->MinY;
        bounds->MinY = bounds->MaxY;
        bounds->MaxY = tmp;
    }
    return TRUE;
}

static VOID clv_ctrl_preserve_selection_after_relayout(CLV_Control *c)
{
    LONG old_sel;
    LONG new_sel;

    if (c == 0) {
        return;
    }

    old_sel = c->selected_row;
    new_sel = old_sel;

    if (new_sel < 0) {
        new_sel = -1;
    } else if ((ULONG)new_sel >= c->row_count) {
        new_sel = -1;
    } else if (c->rows != 0
               && (c->rows[new_sel].flags & CLV_CTRL_ROW_NONSELECTABLE) != 0) {
        new_sel = -1;
    }

    c->selected_row = new_sel;
    CLV_LOGF("RESIZE selection old=%ld new=%ld",
             (long)old_sel, (long)new_sel);
}

static VOID clv_control_refresh_font_metrics(CLV_Control *c)
{
    if (c == 0 || c->draw_ops == 0) {
        return;
    }
    if (c->draw_ops->line_height != 0) {
        c->font_metrics.line_height =
            c->draw_ops->line_height(c->draw_context);
    } else {
        c->font_metrics.line_height = 8;
    }
    if (c->draw_ops->baseline != 0) {
        c->font_metrics.baseline =
            c->draw_ops->baseline(c->draw_context);
    } else {
        c->font_metrics.baseline = 6;
    }
    c->line_height = c->font_metrics.line_height;
    c->header_height = (UWORD)(c->line_height
                               + (2 * c->cell_padding_y));
}

static UWORD clv_ctrl_normalize_row_divider_style(UWORD style)
{
    if (style == (UWORD)CLV_CTRL_ROW_DIVIDER_SOLID
        || style == (UWORD)CLV_CTRL_ROW_DIVIDER_DOTTED) {
        return style;
    }
    return (UWORD)CLV_CTRL_ROW_DIVIDER_NONE;
}

CLV_Control *clv_control_create(const CLV_ControlConfig *cfg)
{
    CLV_Control *c;
    CLV_BENCH_DECLARE(bench_create);

    CLV_BENCH_BEGIN(CLV_BENCH_TOTAL_CREATE, bench_create);

    if (cfg == 0 || cfg->draw_ops == 0) {
        CLV_LOG("INVARIANT clv_control_create cfg or draw_ops is NULL");
        CLV_BENCH_END(CLV_BENCH_TOTAL_CREATE, bench_create);
        return 0;
    }

    c = (CLV_Control *)clv_platform_malloc(sizeof(*c));
    if (c == 0) {
        CLV_LOG("FAIL clv_control_create malloc");
        CLV_BENCH_END(CLV_BENCH_TOTAL_CREATE, bench_create);
        return 0;
    }
    memset(c, 0, sizeof(*c));

    c->draw_ops = cfg->draw_ops;
    c->draw_context = cfg->draw_context;
    c->font = cfg->font;
    c->cell_padding_x = cfg->cell_padding_x;
    c->cell_padding_y = cfg->cell_padding_y;
    c->row_gap = cfg->row_gap;
    c->row_divider_style =
        clv_ctrl_normalize_row_divider_style(cfg->row_divider_style);
    c->selected_row = -1;
    c->scroll_y = 0;
    c->keyboard_enabled = TRUE;
    c->control_armed = FALSE;
    c->armed_row = -1;
    c->armed_column = 0;
    c->armed_type = 0;
    if ((cfg->flags & CLV_CTRL_CFG_NO_KEYBOARD) != 0) {
        c->keyboard_enabled = FALSE;
    }

    clv_control_refresh_font_metrics(c);
    CLV_LOGF("clv_control_create ok control=%p", (void *)c);
    CLV_BENCH_END(CLV_BENCH_TOTAL_CREATE, bench_create);
    return c;
}

VOID clv_control_destroy(CLV_Control *control)
{
    if (control == 0) {
        return;
    }
    CLV_LOGF("clv_control_destroy control=%p", (void *)control);
    /* Cancel any verified-click arm before teardown (§D.4). */
    control->control_armed = FALSE;
    control->armed_row = -1;
    control->armed_column = 0;
    control->armed_type = 0;
    clv_control_free_layout_cache(control);
    clv_control_free_cell_snapshot(control);
    clv_platform_free(control);
}

BOOL clv_control_set_columns(CLV_Control *c,
                             const CLV_ControlColumn *cols,
                             UWORD count)
{
    if (c == 0) {
        return FALSE;
    }
    if (count > 0 && cols == 0) {
        return FALSE;
    }
    c->columns = cols;
    c->column_count = count;
    if (!clv_control_refresh_cell_snapshot(c)) {
        clv_control_layout_invalidate(c);
        return FALSE;
    }
    clv_control_layout_invalidate(c);
    return TRUE;
}

BOOL clv_control_set_rows(CLV_Control *c,
                          const CLV_ControlRow *rows,
                          ULONG count)
{
    if (c == 0) {
        return FALSE;
    }
    if (count > 0 && rows == 0) {
        return FALSE;
    }
    c->rows = rows;
    c->row_count = count;
    if (!clv_control_refresh_cell_snapshot(c)) {
        clv_control_layout_invalidate(c);
        return FALSE;
    }
    clv_control_layout_invalidate(c);
    return TRUE;
}

BOOL clv_control_set_checkbox_value(CLV_Control *control,
                                    LONG row,
                                    UWORD column,
                                    UBYTE value)
{
    ULONG index;
    UWORD col_type;

    if (control == 0) {
        return FALSE;
    }
    if (control->cell_snapshot == 0 || control->columns == 0) {
        return FALSE;
    }
    if (row < 0 || (ULONG)row >= control->row_count) {
        return FALSE;
    }
    if (column >= control->column_count) {
        return FALSE;
    }
    if (value != (UBYTE)CLV_CTRL_CELL_UNCHECKED
        && value != (UBYTE)CLV_CTRL_CELL_CHECKED) {
        return FALSE;
    }

    col_type = (UWORD)(control->columns[column].flags & CLV_CTRL_COL_TYPE_MASK);
    if (col_type != (UWORD)CLV_CTRL_COL_TYPE_CHECKBOX) {
        return FALSE;
    }

    index = (ULONG)row * (ULONG)control->column_count + (ULONG)column;
    if (index >= control->cell_snapshot_count) {
        return FALSE;
    }

    control->cell_snapshot[index].value = value;
    return TRUE;
}

VOID clv_control_set_cell_padding(CLV_Control *c, UWORD x, UWORD y)
{
    if (c == 0) {
        return;
    }
    c->cell_padding_x = x;
    c->cell_padding_y = y;
    c->header_height = (UWORD)(c->line_height + (2 * y));
    clv_control_layout_invalidate(c);
}

UWORD clv_control_get_cell_padding_x(const CLV_Control *c)
{
    if (c == 0) {
        return 0;
    }
    return c->cell_padding_x;
}

UWORD clv_control_get_cell_padding_y(const CLV_Control *c)
{
    if (c == 0) {
        return 0;
    }
    return c->cell_padding_y;
}

VOID clv_control_set_row_gap(CLV_Control *c, UWORD pixels)
{
    if (c == 0) {
        return;
    }
    c->row_gap = pixels;
    clv_control_layout_invalidate(c);
}

UWORD clv_control_get_row_gap(const CLV_Control *c)
{
    if (c == 0) {
        return 0;
    }
    return c->row_gap;
}

VOID clv_control_set_row_divider_style(CLV_Control *c, UWORD style)
{
    if (c == 0) {
        return;
    }
    c->row_divider_style = clv_ctrl_normalize_row_divider_style(style);
}

UWORD clv_control_get_row_divider_style(const CLV_Control *c)
{
    if (c == 0) {
        return (UWORD)CLV_CTRL_ROW_DIVIDER_NONE;
    }
    return c->row_divider_style;
}

/*
 * Relayout transaction for a new outer control rectangle.
 *
 * Sequence: normalize bounds → rebuild wrap/row caches transactionally →
 * clamp scroll_y → preserve selection → make selected row visible.
 * Does not paint (caller uses full render). Does not smart-scroll.
 * On allocation/prepare failure the previous valid layout is restored.
 */
VOID clv_control_set_bounds(CLV_Control *c, const struct Rectangle *bounds)
{
    struct Rectangle old_bounds;
    struct Rectangle proposed;
    LONG old_scroll;
    LONG new_scroll;
    BOOL ok;
    CLV_BENCH_DECLARE(bench_prepare);

    if (c == 0 || bounds == 0) {
        return;
    }

    CLV_BENCH_BEGIN(CLV_BENCH_TOTAL_PREPARE, bench_prepare);

    proposed = *bounds;
    if (!clv_ctrl_normalize_bounds(&proposed)) {
        CLV_LOG("RESIZE fallback/failure reason=invalid_bounds");
        CLV_BENCH_END(CLV_BENCH_TOTAL_PREPARE, bench_prepare);
        return;
    }

    old_bounds = c->bounds;
    old_scroll = c->scroll_y;

    CLV_LOGF("RESIZE begin old=%d,%d-%d,%d new=%d,%d-%d,%d",
             (int)old_bounds.MinX, (int)old_bounds.MinY,
             (int)old_bounds.MaxX, (int)old_bounds.MaxY,
             (int)proposed.MinX, (int)proposed.MinY,
             (int)proposed.MaxX, (int)proposed.MaxY);

    /*
     * Skip work when the outer rectangle is unchanged and layout is still
     * valid (repeated NEWSIZE with identical geometry).
     */
    if (c->layout_valid
        && proposed.MinX == c->bounds.MinX
        && proposed.MinY == c->bounds.MinY
        && proposed.MaxX == c->bounds.MaxX
        && proposed.MaxY == c->bounds.MaxY) {
        CLV_LOG("RESIZE begin (unchanged bounds, skip)");
        CLV_BENCH_END(CLV_BENCH_TOTAL_PREPARE, bench_prepare);
        return;
    }

    /* Geometry change cancels verified-click arm (§D.4). */
    c->control_armed = FALSE;
    c->armed_row = -1;
    c->armed_column = 0;
    c->armed_type = 0;

    c->bounds = proposed;
    CLV_LOG("RESIZE wrap rebuild begin");
    CLV_LOG("RESIZE layout rebuild begin");
    ok = clv_control_layout_rebuild(c);
    if (!ok) {
        c->bounds = old_bounds;
        CLV_LOG("RESIZE wrap rebuild end (fail)");
        CLV_LOG("RESIZE layout rebuild end (fail)");
        CLV_LOG("RESIZE fallback/failure reason=layout_rebuild");
        CLV_BENCH_END(CLV_BENCH_TOTAL_PREPARE, bench_prepare);
        return;
    }
    CLV_LOG("RESIZE wrap rebuild end");
    CLV_LOG("RESIZE layout rebuild end");

    clv_control_set_scroll_y(c, old_scroll);
    new_scroll = c->scroll_y;
    CLV_LOGF("RESIZE scroll old=%ld clamped=%ld",
             (long)old_scroll, (long)new_scroll);

    clv_ctrl_preserve_selection_after_relayout(c);
    if (c->selected_row >= 0) {
        clv_control_make_visible(c, c->selected_row);
        if (c->scroll_y != new_scroll) {
            CLV_LOGF("RESIZE scroll after make_visible=%ld",
                     (long)c->scroll_y);
        }
    }

    CLV_LOG("RESIZE set_bounds complete (caller must full-repaint)");
    CLV_BENCH_END(CLV_BENCH_TOTAL_PREPARE, bench_prepare);
}

VOID clv_control_set_pens(CLV_Control *c, const struct CLV_Pens *pens)
{
    if (c == 0 || pens == 0) {
        return;
    }
    c->pens = *pens;
}

VOID clv_control_render(CLV_Control *c, ULONG flags)
{
    CLV_BENCH_DECLARE(bench_render);

    if ((flags & CLV_RENDER_VIEWPORT_ONLY) != 0) {
        CLV_BENCH_BEGIN(CLV_BENCH_PARTIAL_REDRAW, bench_render);
    } else {
        CLV_BENCH_BEGIN(CLV_BENCH_FULL_REDRAW, bench_render);
    }
    CLV_LOG("clv_control_render begin");
    if (c == 0) {
        CLV_LOG("INVARIANT clv_control_render control is NULL");
        CLV_LOG("clv_control_render end");
        if ((flags & CLV_RENDER_VIEWPORT_ONLY) != 0) {
            CLV_BENCH_END(CLV_BENCH_PARTIAL_REDRAW, bench_render);
        } else {
            CLV_BENCH_END(CLV_BENCH_FULL_REDRAW, bench_render);
        }
        return;
    }
    if (c->draw_ops == 0) {
        CLV_LOG("INVARIANT clv_control_render draw_ops is NULL");
        CLV_LOG("clv_control_render end");
        if ((flags & CLV_RENDER_VIEWPORT_ONLY) != 0) {
            CLV_BENCH_END(CLV_BENCH_PARTIAL_REDRAW, bench_render);
        } else {
            CLV_BENCH_END(CLV_BENCH_FULL_REDRAW, bench_render);
        }
        return;
    }
    if (c->viewport_bounds.MaxX < c->viewport_bounds.MinX
        || c->viewport_bounds.MaxY < c->viewport_bounds.MinY) {
        CLV_LOGF("INVARIANT viewport invalid Min=%d,%d Max=%d,%d",
                 (int)c->viewport_bounds.MinX,
                 (int)c->viewport_bounds.MinY,
                 (int)c->viewport_bounds.MaxX,
                 (int)c->viewport_bounds.MaxY);
    }
    if (c->row_count > 0 && c->layout_rows == 0 && c->layout_valid) {
        CLV_LOGF("INVARIANT row_count=%lu but layout_rows is NULL",
                 (unsigned long)c->row_count);
    }
    if (!c->layout_valid) {
        clv_control_layout_rebuild(c);
    }
    if ((flags & CLV_RENDER_VIEWPORT_ONLY) != 0) {
        clv_control_render_viewport(c);
    } else {
        clv_control_render_full(c);
    }
    CLV_LOG("clv_control_render end");
    if ((flags & CLV_RENDER_VIEWPORT_ONLY) != 0) {
        CLV_BENCH_END(CLV_BENCH_PARTIAL_REDRAW, bench_render);
    } else {
        CLV_BENCH_END(CLV_BENCH_FULL_REDRAW, bench_render);
    }
}
