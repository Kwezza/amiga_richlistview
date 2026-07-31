/**
 * Experimental custom ListView control — create/destroy and setters.
 */

#include "rich_listview/rlv_internal.h"
#include "rich_listview/rlv_log.h"
#include "rich_listview/rlv_platform_internal.h"

#include <string.h>

static VOID rlv_free_layout_cache(RLV_Control *c)
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
    c->layout_valid = FALSE;
}

static VOID rlv_free_cell_snapshot(RLV_Control *c)
{
    if (c == 0) {
        return;
    }
    if (c->cell_snapshot != 0) {
        rlv_platform_free(c->cell_snapshot);
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
static BOOL rlv_refresh_cell_snapshot(RLV_Control *c)
{
    ULONG need;
    ULONG i;
    ULONG ncopy;
    RLV_Cell *snap;

    if (c == 0) {
        return FALSE;
    }

    rlv_free_cell_snapshot(c);

    /* Clear unused arm fields whenever the snapshot is rebuilt. */
    c->control_armed = FALSE;
    c->armed_row = -1;
    c->armed_column = 0;
    c->armed_type = 0;

    if (c->row_count == 0 || c->column_count == 0 || c->rows == 0) {
        return TRUE;
    }

    need = c->row_count * (ULONG)c->column_count;
    snap = (RLV_Cell *)rlv_platform_malloc(need * sizeof(*snap));
    if (snap == 0) {
        RLV_LOG("FAIL rlv_refresh_cell_snapshot malloc");
        return FALSE;
    }
    memset(snap, 0, (size_t)need * sizeof(*snap));

    ncopy = (ULONG)c->column_count * sizeof(RLV_Cell);
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
static BOOL rlv_normalize_bounds(struct Rectangle *bounds)
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

static VOID rlv_preserve_selection_after_relayout(RLV_Control *c)
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
               && (c->rows[new_sel].flags & RLV_ROW_NONSELECTABLE) != 0) {
        new_sel = -1;
    }

    c->selected_row = new_sel;
    RLV_LOGF("RESIZE selection old=%ld new=%ld",
             (long)old_sel, (long)new_sel);
}

static VOID rlv_refresh_font_metrics(RLV_Control *c)
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

static UWORD rlv_normalize_row_divider_style(UWORD style)
{
    if (style == (UWORD)RLV_ROW_DIVIDER_SOLID
        || style == (UWORD)RLV_ROW_DIVIDER_DOTTED) {
        return style;
    }
    return (UWORD)RLV_ROW_DIVIDER_NONE;
}

RLV_Control *rlv_create(const RLV_Config *cfg)
{
    RLV_Control *c;
    RLV_BENCH_DECLARE(bench_create);

    RLV_BENCH_BEGIN(RLV_BENCH_TOTAL_CREATE, bench_create);

    if (cfg == 0 || cfg->draw_ops == 0) {
        RLV_LOG("INVARIANT rlv_create cfg or draw_ops is NULL");
        RLV_BENCH_END(RLV_BENCH_TOTAL_CREATE, bench_create);
        return 0;
    }

    c = (RLV_Control *)rlv_platform_malloc(sizeof(*c));
    if (c == 0) {
        RLV_LOG("FAIL rlv_create malloc");
        RLV_BENCH_END(RLV_BENCH_TOTAL_CREATE, bench_create);
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
        rlv_normalize_row_divider_style(cfg->row_divider_style);
    c->selected_row = -1;
    c->scroll_y = 0;
    c->keyboard_enabled = TRUE;
    c->control_activation_policy =
        (UWORD)RLV_CONTROL_ACTIVATE_SELECT_ROW;
    c->current_row_visual = (UWORD)RLV_CURRENT_ROW_VISUAL_FULL;
    c->control_armed = FALSE;
    c->armed_row = -1;
    c->armed_column = 0;
    c->armed_type = 0;
    if ((cfg->flags & RLV_CFG_NO_KEYBOARD) != 0) {
        c->keyboard_enabled = FALSE;
    }

    rlv_refresh_font_metrics(c);
    RLV_LOGF("rlv_create ok control=%p", (void *)c);
    RLV_BENCH_END(RLV_BENCH_TOTAL_CREATE, bench_create);
    return c;
}

VOID rlv_destroy(RLV_Control *control)
{
    if (control == 0) {
        return;
    }
    RLV_LOGF("rlv_destroy control=%p", (void *)control);
    /* Cancel any verified-click arm before teardown (§D.4). */
    control->control_armed = FALSE;
    control->armed_row = -1;
    control->armed_column = 0;
    control->armed_type = 0;
    rlv_free_layout_cache(control);
    rlv_free_cell_snapshot(control);
    rlv_platform_free(control);
}

BOOL rlv_set_columns(RLV_Control *c,
                             const RLV_Column *cols,
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
    if (!rlv_refresh_cell_snapshot(c)) {
        rlv_layout_invalidate(c);
        return FALSE;
    }
    rlv_layout_invalidate(c);
    return TRUE;
}

BOOL rlv_set_rows(RLV_Control *c,
                          const RLV_Row *rows,
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
    if (!rlv_refresh_cell_snapshot(c)) {
        rlv_layout_invalidate(c);
        return FALSE;
    }
    rlv_layout_invalidate(c);
    return TRUE;
}

BOOL rlv_set_checkbox_value(RLV_Control *control,
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
    if (value != (UBYTE)RLV_CELL_UNCHECKED
        && value != (UBYTE)RLV_CELL_CHECKED) {
        return FALSE;
    }

    col_type = (UWORD)(control->columns[column].flags & RLV_COL_TYPE_MASK);
    if (col_type != (UWORD)RLV_COL_TYPE_CHECKBOX) {
        return FALSE;
    }

    index = (ULONG)row * (ULONG)control->column_count + (ULONG)column;
    if (index >= control->cell_snapshot_count) {
        return FALSE;
    }

    control->cell_snapshot[index].value = value;
    return TRUE;
}

VOID rlv_set_cell_padding(RLV_Control *c, UWORD x, UWORD y)
{
    if (c == 0) {
        return;
    }
    c->cell_padding_x = x;
    c->cell_padding_y = y;
    c->header_height = (UWORD)(c->line_height + (2 * y));
    rlv_layout_invalidate(c);
}

UWORD rlv_get_cell_padding_x(const RLV_Control *c)
{
    if (c == 0) {
        return 0;
    }
    return c->cell_padding_x;
}

UWORD rlv_get_cell_padding_y(const RLV_Control *c)
{
    if (c == 0) {
        return 0;
    }
    return c->cell_padding_y;
}

VOID rlv_set_row_gap(RLV_Control *c, UWORD pixels)
{
    if (c == 0) {
        return;
    }
    c->row_gap = pixels;
    rlv_layout_invalidate(c);
}

UWORD rlv_get_row_gap(const RLV_Control *c)
{
    if (c == 0) {
        return 0;
    }
    return c->row_gap;
}

VOID rlv_set_row_divider_style(RLV_Control *c, UWORD style)
{
    if (c == 0) {
        return;
    }
    c->row_divider_style = rlv_normalize_row_divider_style(style);
}

UWORD rlv_get_row_divider_style(const RLV_Control *c)
{
    if (c == 0) {
        return (UWORD)RLV_ROW_DIVIDER_NONE;
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
VOID rlv_set_bounds(RLV_Control *c, const struct Rectangle *bounds)
{
    struct Rectangle old_bounds;
    struct Rectangle proposed;
    LONG old_scroll;
    LONG new_scroll;
    BOOL ok;
    RLV_BENCH_DECLARE(bench_prepare);

    if (c == 0 || bounds == 0) {
        return;
    }

    RLV_BENCH_BEGIN(RLV_BENCH_TOTAL_PREPARE, bench_prepare);

    proposed = *bounds;
    if (!rlv_normalize_bounds(&proposed)) {
        RLV_LOG("RESIZE fallback/failure reason=invalid_bounds");
        RLV_BENCH_END(RLV_BENCH_TOTAL_PREPARE, bench_prepare);
        return;
    }

    old_bounds = c->bounds;
    old_scroll = c->scroll_y;

    RLV_LOGF("RESIZE begin old=%d,%d-%d,%d new=%d,%d-%d,%d",
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
        RLV_LOG("RESIZE begin (unchanged bounds, skip)");
        RLV_BENCH_END(RLV_BENCH_TOTAL_PREPARE, bench_prepare);
        return;
    }

    /* Geometry change cancels verified-click arm (§D.4). */
    c->control_armed = FALSE;
    c->armed_row = -1;
    c->armed_column = 0;
    c->armed_type = 0;

    c->bounds = proposed;
    RLV_LOG("RESIZE wrap rebuild begin");
    RLV_LOG("RESIZE layout rebuild begin");
    ok = rlv_layout_rebuild(c);
    if (!ok) {
        c->bounds = old_bounds;
        RLV_LOG("RESIZE wrap rebuild end (fail)");
        RLV_LOG("RESIZE layout rebuild end (fail)");
        RLV_LOG("RESIZE fallback/failure reason=layout_rebuild");
        RLV_BENCH_END(RLV_BENCH_TOTAL_PREPARE, bench_prepare);
        return;
    }
    RLV_LOG("RESIZE wrap rebuild end");
    RLV_LOG("RESIZE layout rebuild end");

    rlv_set_scroll_y(c, old_scroll);
    new_scroll = c->scroll_y;
    RLV_LOGF("RESIZE scroll old=%ld clamped=%ld",
             (long)old_scroll, (long)new_scroll);

    rlv_preserve_selection_after_relayout(c);
    if (c->selected_row >= 0) {
        rlv_make_visible(c, c->selected_row);
        if (c->scroll_y != new_scroll) {
            RLV_LOGF("RESIZE scroll after make_visible=%ld",
                     (long)c->scroll_y);
        }
    }

    RLV_LOG("RESIZE set_bounds complete (caller must full-repaint)");
    RLV_BENCH_END(RLV_BENCH_TOTAL_PREPARE, bench_prepare);
}

VOID rlv_set_pens(RLV_Control *c, const struct RLV_Pens *pens)
{
    if (c == 0 || pens == 0) {
        return;
    }
    c->pens = *pens;
}

VOID rlv_set_control_activation_policy(RLV_Control *c, UWORD policy)
{
    if (c == 0) {
        return;
    }
    if (policy != (UWORD)RLV_CONTROL_ACTIVATE_SELECT_ROW
        && policy != (UWORD)RLV_CONTROL_ACTIVATE_KEEP_CURRENT) {
        return;
    }
    c->control_activation_policy = policy;
    RLV_LOGF("control_activation_policy=%u", (unsigned)policy);
}

UWORD rlv_get_control_activation_policy(const RLV_Control *c)
{
    if (c == 0) {
        return (UWORD)RLV_CONTROL_ACTIVATE_SELECT_ROW;
    }
    return c->control_activation_policy;
}

VOID rlv_set_current_row_visual(RLV_Control *c, UWORD visual)
{
    if (c == 0) {
        return;
    }
    if (visual != (UWORD)RLV_CURRENT_ROW_VISUAL_FULL
        && visual != (UWORD)RLV_CURRENT_ROW_VISUAL_MARKER
        && visual != (UWORD)RLV_CURRENT_ROW_VISUAL_NONE) {
        return;
    }
    c->current_row_visual = visual;
    RLV_LOGF("current_row_visual=%u", (unsigned)visual);
}

UWORD rlv_get_current_row_visual(const RLV_Control *c)
{
    if (c == 0) {
        return (UWORD)RLV_CURRENT_ROW_VISUAL_FULL;
    }
    return c->current_row_visual;
}

VOID rlv_render(RLV_Control *c, ULONG flags)
{
    RLV_BENCH_DECLARE(bench_render);

    if ((flags & RLV_RENDER_VIEWPORT_ONLY) != 0) {
        RLV_BENCH_BEGIN(RLV_BENCH_PARTIAL_REDRAW, bench_render);
    } else {
        RLV_BENCH_BEGIN(RLV_BENCH_FULL_REDRAW, bench_render);
    }
    RLV_LOG("rlv_render begin");
    if (c == 0) {
        RLV_LOG("INVARIANT rlv_render control is NULL");
        RLV_LOG("rlv_render end");
        if ((flags & RLV_RENDER_VIEWPORT_ONLY) != 0) {
            RLV_BENCH_END(RLV_BENCH_PARTIAL_REDRAW, bench_render);
        } else {
            RLV_BENCH_END(RLV_BENCH_FULL_REDRAW, bench_render);
        }
        return;
    }
    if (c->draw_ops == 0) {
        RLV_LOG("INVARIANT rlv_render draw_ops is NULL");
        RLV_LOG("rlv_render end");
        if ((flags & RLV_RENDER_VIEWPORT_ONLY) != 0) {
            RLV_BENCH_END(RLV_BENCH_PARTIAL_REDRAW, bench_render);
        } else {
            RLV_BENCH_END(RLV_BENCH_FULL_REDRAW, bench_render);
        }
        return;
    }
    if (c->viewport_bounds.MaxX < c->viewport_bounds.MinX
        || c->viewport_bounds.MaxY < c->viewport_bounds.MinY) {
        RLV_LOGF("INVARIANT viewport invalid Min=%d,%d Max=%d,%d",
                 (int)c->viewport_bounds.MinX,
                 (int)c->viewport_bounds.MinY,
                 (int)c->viewport_bounds.MaxX,
                 (int)c->viewport_bounds.MaxY);
    }
    if (c->row_count > 0 && c->layout_rows == 0 && c->layout_valid) {
        RLV_LOGF("INVARIANT row_count=%lu but layout_rows is NULL",
                 (unsigned long)c->row_count);
    }
    if (!c->layout_valid) {
        rlv_layout_rebuild(c);
    }
    if ((flags & RLV_RENDER_VIEWPORT_ONLY) != 0) {
        rlv_render_viewport(c);
    } else {
        rlv_render_full(c);
    }
    RLV_LOG("rlv_render end");
    if ((flags & RLV_RENDER_VIEWPORT_ONLY) != 0) {
        RLV_BENCH_END(RLV_BENCH_PARTIAL_REDRAW, bench_render);
    } else {
        RLV_BENCH_END(RLV_BENCH_FULL_REDRAW, bench_render);
    }
}
