/**
 * Hit-testing, selection, make-visible, and neutral input handling
 * (Phase 4 + Phase 5.5 keyboard NAV_* + C4/C5 checkbox arm/commit +
 * C7 Space / RLV_INPUT_TOGGLE + E2 generic CELL_CONTROL event +
 * optional expandable-row disclosure / EXPAND_ROW / COLLAPSE_ROW).
 *
 * handle_input returns TRUE iff result->type != RLV_EVENT_NONE.
 * At most one RLV_EventType is filled per successful call (§D.11).
 * Selection and checkbox toggle never share one compound event:
 * SELECT_DOWN may emit SELECTION_CHANGED; SELECT_UP or TOGGLE may emit
 * CELL_CONTROL. NAV_ACTIVATE never toggles. Disclosure SELECT_UP and
 * Left/Right expand/collapse also emit CELL_CONTROL only. Does not paint —
 * the application refreshes after events.
 */

#include "rich_listview/rlv_internal.h"
#include "rich_listview/rlv_log.h"

/* All three required for verified-click arm (§D.4 / §D.11). */
#define RLV_CELL_ARM_MASK \
    ((UBYTE)(RLV_CELL_F_VISIBLE | RLV_CELL_F_ENABLED \
             | RLV_CELL_F_INTERACTIVE))

#if defined(RLV_ENABLE_EXPANDABLE_ROWS) && (RLV_ENABLE_EXPANDABLE_ROWS != 0)
/* Extra hit slack around the visible +/- box; clamped to the cell band. */
#define RLV_DISC_HIT_PAD  1
#endif

static VOID rlv_clear_event(RLV_Event *result)
{
    if (result != 0) {
        result->type = (UWORD)RLV_EVENT_NONE;
        result->row = -1;
        result->previous_row = -1;
        result->value = 0;
        result->column = 0;
        result->control_type = 0;
        result->control_action = 0;
        result->row_user_data = 0;
        result->previous_value = 0;
        result->cell_value = 0;
        result->resize_left = 0;
        result->resize_right = 0;
        result->old_left_width = 0;
        result->old_right_width = 0;
        result->new_left_width = 0;
        result->new_right_width = 0;
    }
}

/*
 * Central row identity fill: logical index plus borrowed opaque tag.
 * Future row-related event types should call this instead of copying
 * user_data by hand.
 */
VOID rlv_event_set_row(const RLV_Control *control,
                             RLV_Event *event,
                             LONG logical_row)
{
    if (event == 0) {
        return;
    }
    event->row = logical_row;
    event->row_user_data = 0;
    if (control != 0
        && control->rows != 0
        && logical_row >= 0
        && (ULONG)logical_row < control->row_count) {
        event->row_user_data = control->rows[logical_row].user_data;
    }
}

static VOID rlv_clear_arm(RLV_Control *c)
{
    if (c == 0) {
        return;
    }
    c->control_armed = FALSE;
    c->armed_row = -1;
    c->armed_column = 0;
    c->armed_type = 0;
}

static BOOL rlv_point_in_rect(WORD x, WORD y, const struct Rectangle *r)
{
    if (r == 0) {
        return FALSE;
    }
    if (x < r->MinX || x > r->MaxX || y < r->MinY || y > r->MaxY) {
        return FALSE;
    }
    return TRUE;
}

/*
 * Hit an interactive checkbox box at window (x,y). Reuses paint geometry
 * (rlv_checkbox_resolve_rect). Requires snapshot VISIBLE|ENABLED|
 * INTERACTIVE. Returns TRUE and fills out_row/out_col on hit.
 */
static BOOL rlv_hit_interactive_checkbox(const RLV_Control *c,
                                              WORD x,
                                              WORD y,
                                              LONG *out_row,
                                              UWORD *out_col)
{
    LONG row;
    UWORD col;
    UWORD col_type;
    ULONG index;
    UBYTE flags;
    struct Rectangle box;

    if (c == 0 || c->columns == 0 || c->cell_snapshot == 0) {
        return FALSE;
    }

    row = rlv_hit_test(c, x, y);
    if (row < 0) {
        return FALSE;
    }

    for (col = 0; col < c->column_count; col++) {
        col_type = (UWORD)(c->columns[col].flags & RLV_COL_TYPE_MASK);
        if (col_type != (UWORD)RLV_COL_TYPE_CHECKBOX) {
            continue;
        }

        index = (ULONG)row * (ULONG)c->column_count + (ULONG)col;
        if (index >= c->cell_snapshot_count) {
            continue;
        }

        flags = c->cell_snapshot[index].flags;
        if ((flags & RLV_CELL_ARM_MASK) != RLV_CELL_ARM_MASK) {
            continue;
        }

        if (!rlv_checkbox_resolve_rect(c, row, col, &box)) {
            continue;
        }
        if (!rlv_point_in_rect(x, y, &box)) {
            continue;
        }

        if (out_row != 0) {
            *out_row = row;
        }
        if (out_col != 0) {
            *out_col = col;
        }
        return TRUE;
    }

    return FALSE;
}

static VOID rlv_arm_checkbox(RLV_Control *c, LONG row, UWORD column)
{
    if (c == 0) {
        return;
    }
    c->control_armed = TRUE;
    c->armed_row = row;
    c->armed_column = column;
    c->armed_type = (UBYTE)RLV_COL_TYPE_CHECKBOX;
}

#if defined(RLV_ENABLE_EXPANDABLE_ROWS) && (RLV_ENABLE_EXPANDABLE_ROWS != 0)
static VOID rlv_arm_disclosure(RLV_Control *c, LONG row, UWORD column)
{
    if (c == 0) {
        return;
    }
    c->control_armed = TRUE;
    c->armed_row = row;
    c->armed_column = column;
    c->armed_type = (UBYTE)RLV_COL_TYPE_DISCLOSURE;
}

/*
 * Hit an interactive disclosure control. Requires expandable row and
 * snapshot VISIBLE|ENABLED|INTERACTIVE. Hit rectangle may be one pixel
 * larger than the visible outline, clamped to the column text inset.
 */
static BOOL rlv_hit_interactive_disclosure(const RLV_Control *c,
                                                WORD x,
                                                WORD y,
                                                LONG *out_row,
                                                UWORD *out_col)
{
    LONG row;
    UWORD col;
    UWORD col_type;
    ULONG index;
    UBYTE flags;
    struct Rectangle box;
    WORD pad;
    const RLV_PixelColumn *pcol;

    if (c == 0 || c->columns == 0 || c->cell_snapshot == 0
        || c->row_expand == 0 || c->col_geom == 0) {
        return FALSE;
    }

    row = rlv_hit_test(c, x, y);
    if (row < 0) {
        return FALSE;
    }
    if (!rlv_is_row_expandable(c, row)) {
        return FALSE;
    }
    if (!rlv_disclosure_ui_enabled(c)) {
        return FALSE;
    }
    if (!rlv_row_has_multi_line_wrap(c, row)) {
        return FALSE;
    }

    for (col = 0; col < c->column_count; col++) {
        col_type = (UWORD)(c->columns[col].flags & RLV_COL_TYPE_MASK);
        if (col_type != (UWORD)RLV_COL_TYPE_DISCLOSURE) {
            continue;
        }

        index = (ULONG)row * (ULONG)c->column_count + (ULONG)col;
        if (index >= c->cell_snapshot_count) {
            continue;
        }

        flags = c->cell_snapshot[index].flags;
        if ((flags & RLV_CELL_ARM_MASK) != RLV_CELL_ARM_MASK) {
            continue;
        }

        if (!rlv_disclosure_resolve_rect(c, row, col, &box)) {
            continue;
        }

        pad = (WORD)RLV_DISC_HIT_PAD;
        box.MinX = (WORD)(box.MinX - pad);
        box.MinY = (WORD)(box.MinY - pad);
        box.MaxX = (WORD)(box.MaxX + pad);
        box.MaxY = (WORD)(box.MaxY + pad);

        pcol = &c->col_geom[col];
        if (box.MinX < pcol->text_left) {
            box.MinX = pcol->text_left;
        }
        if (box.MaxX > pcol->text_right) {
            box.MaxX = pcol->text_right;
        }

        if (!rlv_point_in_rect(x, y, &box)) {
            continue;
        }

        if (out_row != 0) {
            *out_row = row;
        }
        if (out_col != 0) {
            *out_col = col;
        }
        return TRUE;
    }
    return FALSE;
}

static BOOL rlv_commit_disclosure_toggle(RLV_Control *c, RLV_Event *result)
{
    BOOL expanded;
    BOOL want;

    if (c == 0 || !c->control_armed) {
        return FALSE;
    }
    if (c->armed_type != (UBYTE)RLV_COL_TYPE_DISCLOSURE) {
        return FALSE;
    }
    if (!rlv_is_row_expandable(c, c->armed_row)) {
        return FALSE;
    }

    expanded = rlv_is_row_expanded(c, c->armed_row);
    want = expanded ? FALSE : TRUE;
    return rlv_set_row_expanded(c, c->armed_row, want,
                                     RLV_EXPAND_SRC_MOUSE, result);
}
#endif /* RLV_ENABLE_EXPANDABLE_ROWS */

/*
 * Fill a generic RLV_EVENT_CELL_CONTROL payload (centralised for all cell
 * commits — checkbox today; button/cycle later). row_user_data is borrowed
 * from the control's current row array via rlv_event_set_row (same lifetime
 * as app rows). For stateless PRESSED actions, previous_value/cell_value
 * may both be zero.
 */
static VOID rlv_fill_cell_event(RLV_Control *control,
                                     RLV_Event *event,
                                     LONG row,
                                     UWORD column,
                                     UWORD control_type,
                                     UWORD action,
                                     UBYTE previous_value,
                                     UBYTE cell_value)
{
    if (event == 0) {
        return;
    }

    event->type = (UWORD)RLV_EVENT_CELL_CONTROL;
    rlv_event_set_row(control, event, row);
    event->previous_row = -1;
    event->value = 0;
    event->column = column;
    event->control_type = control_type;
    event->control_action = action;
    event->previous_value = previous_value;
    event->cell_value = cell_value;
    RLV_LOGF("CELL_CONTROL row=%ld col=%u type=%u action=%u tag=%lu",
             (long)row,
             (unsigned)column,
             (unsigned)control_type,
             (unsigned)action,
             (unsigned long)(ULONG)event->row_user_data);
}

/*
 * Toggle owned snapshot at (row, column); fill CELL_CONTROL.
 * Does not write borrowed app control_cells. Does not paint.
 * Caller must ensure column is a checkbox type (arm path / sole finder).
 */
static BOOL rlv_toggle_checkbox_at(RLV_Control *c,
                                        LONG row,
                                        UWORD column,
                                        RLV_Event *result)
{
    ULONG index;
    UBYTE prev;
    UBYTE next;

    if (c == 0 || c->cell_snapshot == 0) {
        return FALSE;
    }
    if (row < 0 || (ULONG)row >= c->row_count) {
        return FALSE;
    }
    if (column >= c->column_count) {
        return FALSE;
    }

    index = (ULONG)row * (ULONG)c->column_count + (ULONG)column;
    if (index >= c->cell_snapshot_count) {
        return FALSE;
    }

    prev = c->cell_snapshot[index].value;
    if (prev == (UBYTE)RLV_CELL_CHECKED) {
        next = (UBYTE)RLV_CELL_UNCHECKED;
    } else {
        next = (UBYTE)RLV_CELL_CHECKED;
    }
    c->cell_snapshot[index].value = next;

    if (result != 0) {
        rlv_fill_cell_event(c,
                                 result,
                                 row,
                                 column,
                                 (UWORD)RLV_COL_TYPE_CHECKBOX,
                                 (UWORD)RLV_ACTION_VALUE_CHANGED,
                                 prev,
                                 next);
    }

    RLV_LOGF("CELL_CONTROL row=%ld col=%u type=%u action=%u prev=%u new=%u",
             (long)row, (unsigned)column,
             (unsigned)RLV_COL_TYPE_CHECKBOX,
             (unsigned)RLV_ACTION_VALUE_CHANGED,
             (unsigned)prev, (unsigned)next);
    return TRUE;
}

/*
 * Toggle owned snapshot for the armed checkbox; fill CELL_CONTROL.
 */
static BOOL rlv_commit_checkbox_toggle(RLV_Control *c, RLV_Event *result)
{
    if (c == 0 || !c->control_armed) {
        return FALSE;
    }
    if (c->armed_type != (UBYTE)RLV_COL_TYPE_CHECKBOX) {
        return FALSE;
    }
    return rlv_toggle_checkbox_at(c, c->armed_row, c->armed_column,
                                       result);
}

/*
 * Find the sole VISIBLE|ENABLED|INTERACTIVE checkbox column on a row.
 * Zero or multiple eligible columns → FALSE (Space defers; no focus model).
 */
static BOOL rlv_find_sole_eligible_checkbox(const RLV_Control *c,
                                                 LONG row,
                                                 UWORD *out_col)
{
    UWORD col;
    UWORD col_type;
    ULONG index;
    UBYTE flags;
    UWORD found_col;
    UWORD found_count;

    if (c == 0 || c->columns == 0 || c->cell_snapshot == 0) {
        return FALSE;
    }
    if (row < 0 || (ULONG)row >= c->row_count) {
        return FALSE;
    }

    found_count = 0;
    found_col = 0;
    for (col = 0; col < c->column_count; col++) {
        col_type = (UWORD)(c->columns[col].flags & RLV_COL_TYPE_MASK);
        if (col_type != (UWORD)RLV_COL_TYPE_CHECKBOX) {
            continue;
        }

        index = (ULONG)row * (ULONG)c->column_count + (ULONG)col;
        if (index >= c->cell_snapshot_count) {
            continue;
        }

        flags = c->cell_snapshot[index].flags;
        if ((flags & RLV_CELL_ARM_MASK) != RLV_CELL_ARM_MASK) {
            continue;
        }

        found_count++;
        found_col = col;
        if (found_count > 1) {
            return FALSE;
        }
    }

    if (found_count != 1) {
        return FALSE;
    }
    if (out_col != 0) {
        *out_col = found_col;
    }
    return TRUE;
}

static BOOL rlv_row_selectable(const RLV_Control *c, LONG logical_row)
{
    if (c == 0 || c->rows == 0) {
        return FALSE;
    }
    if (logical_row < 0 || (ULONG)logical_row >= c->row_count) {
        return FALSE;
    }
    if ((c->rows[logical_row].flags & RLV_ROW_NONSELECTABLE) != 0) {
        return FALSE;
    }
    return TRUE;
}

static LONG rlv_viewport_height(const RLV_Control *c)
{
    LONG vp_h;

    if (c == 0) {
        return 0;
    }
    vp_h = (LONG)c->viewport_bounds.MaxY - (LONG)c->viewport_bounds.MinY + 1;
    if (vp_h < 0) {
        vp_h = 0;
    }
    return vp_h;
}

/*
 * Walk selectable logical (source) rows in view/display order.
 * dir +1 searches after start; dir -1 before.
 * Pass start = -1 with dir +1 for first selectable; start = row_count with
 * dir -1 for last. Returns source index, or -1 if none.
 */
static LONG rlv_find_selectable(const RLV_Control *c, LONG start, LONG dir)
{
    LONG v;
    LONG n;
    LONG start_view;
    LONG src;

    if (c == 0 || c->rows == 0 || c->row_count == 0) {
        return -1;
    }
    if (dir == 0) {
        return -1;
    }

    n = (LONG)c->row_count;
    if (start < 0) {
        start_view = -1;
    } else if (start >= n) {
        start_view = n;
    } else {
        start_view = rlv_view_for_source(c, start);
        if (start_view < 0) {
            start_view = start;
        }
    }

    if (dir > 0) {
        v = start_view + 1;
        if (v < 0) {
            v = 0;
        }
        for (; v < n; v++) {
            src = (LONG)rlv_source_for_view(c, (ULONG)v);
            if (rlv_row_selectable(c, src)) {
                return src;
            }
            RLV_BENCH_COUNT(RLV_BENCH_COUNTER_NONSELECTABLE_SKIPS);
        }
    } else {
        v = start_view - 1;
        if (v >= n) {
            v = n - 1;
        }
        for (; v >= 0; v--) {
            src = (LONG)rlv_source_for_view(c, (ULONG)v);
            if (rlv_row_selectable(c, src)) {
                return src;
            }
            RLV_BENCH_COUNT(RLV_BENCH_COUNTER_NONSELECTABLE_SKIPS);
        }
    }
    RLV_BENCH_COUNT(RLV_BENCH_COUNTER_BOUNDARY_HITS);
    return -1;
}

static LONG rlv_page_step(const RLV_Control *c)
{
    LONG line_h;
    LONG vp_h;
    LONG step;

    line_h = (LONG)c->line_height;
    if (line_h < 1) {
        line_h = 1;
    }
    vp_h = rlv_viewport_height(c);
    step = vp_h - line_h;
    if (step < line_h) {
        step = line_h;
    }
    return step;
}

/*
 * Selection-centric page target using layout_rows[].top_y (view order).
 * dir +1 = page down; dir -1 = page up. Returns source index, or -1 if
 * empty / all non-selectable.
 */
static LONG rlv_page_nav_target(const RLV_Control *c, LONG dir)
{
    LONG step;
    LONG origin_top;
    LONG target_y;
    LONG v;
    LONG n;
    LONG first_sel;
    LONG last_sel;
    LONG best;
    LONG src;
    LONG sel_view;

    if (c == 0 || c->layout_rows == 0 || c->row_count == 0) {
        return -1;
    }

    n = (LONG)c->row_count;
    first_sel = rlv_find_selectable(c, -1, 1);
    last_sel = rlv_find_selectable(c, n, -1);
    if (first_sel < 0) {
        return -1;
    }

    step = rlv_page_step(c);
    if (c->selected_row >= 0 && (ULONG)c->selected_row < c->row_count) {
        sel_view = rlv_view_for_source(c, c->selected_row);
        if (sel_view < 0 || sel_view >= n) {
            sel_view = 0;
        }
        origin_top = c->layout_rows[sel_view].top_y;
    } else {
        origin_top = c->scroll_y;
    }

    if (dir > 0) {
        target_y = origin_top + step;
        best = -1;
        for (v = 0; v < n; v++) {
            src = (LONG)rlv_source_for_view(c, (ULONG)v);
            if (!rlv_row_selectable(c, src)) {
                continue;
            }
            if (c->layout_rows[v].top_y >= target_y) {
                best = src;
                break;
            }
        }
        if (best >= 0) {
            return best;
        }
        return last_sel;
    }

    target_y = origin_top - step;
    best = -1;
    for (v = n - 1; v >= 0; v--) {
        src = (LONG)rlv_source_for_view(c, (ULONG)v);
        if (!rlv_row_selectable(c, src)) {
            continue;
        }
        if (c->layout_rows[v].top_y <= target_y) {
            best = src;
            break;
        }
    }
    if (best >= 0) {
        return best;
    }
    return first_sel;
}

/*
 * Assign selection, make_visible, fill SELECTION_CHANGED. No-op (FALSE)
 * when target equals current selection.
 */
static BOOL rlv_nav_select(RLV_Control *c, LONG hit, RLV_Event *result)
{
    LONG old_sel;
    RLV_BENCH_DECLARE(bench_select);

    RLV_BENCH_BEGIN(RLV_BENCH_NAV_SELECTION_UPDATE, bench_select);
    if (c == 0 || hit < 0 || !rlv_row_selectable(c, hit)) {
        RLV_BENCH_COUNT(RLV_BENCH_COUNTER_NAV_MOVES_REJECTED);
        RLV_BENCH_END(RLV_BENCH_NAV_SELECTION_UPDATE, bench_select);
        return FALSE;
    }
    if (hit == c->selected_row) {
        RLV_BENCH_COUNT(RLV_BENCH_COUNTER_NAV_MOVES_REJECTED);
        RLV_BENCH_END(RLV_BENCH_NAV_SELECTION_UPDATE, bench_select);
        return FALSE;
    }

    old_sel = c->selected_row;
    c->selected_row = hit;
    rlv_make_visible(c, hit);

    if (result != 0) {
        result->type = (UWORD)RLV_EVENT_SELECTION_CHANGED;
        rlv_event_set_row(c, result, hit);
        result->previous_row = old_sel;
        result->value = c->scroll_y;
        RLV_LOGF("SELECTION_CHANGED row=%ld prev=%ld tag=%lu",
                 (long)hit, (long)old_sel,
                 (unsigned long)(ULONG)result->row_user_data);
    }
    RLV_BENCH_COUNT(RLV_BENCH_COUNTER_NAV_MOVES_ACCEPTED);
    RLV_BENCH_END(RLV_BENCH_NAV_SELECTION_UPDATE, bench_select);
    return TRUE;
}

LONG rlv_hit_test(const RLV_Control *c, WORD x, WORD y)
{
    LONG content_y;
    ULONG i;
    LONG top;
    LONG content_bottom;
    LONG hit;

    if (c == 0 || c->layout_rows == 0 || c->row_count == 0) {
        RLV_LOGF("hit_test early miss x=%d y=%d (null/empty layout)",
                 (int)x, (int)y);
        return -1;
    }

    if (x < c->viewport_bounds.MinX || x > c->viewport_bounds.MaxX
        || y < c->viewport_bounds.MinY || y > c->viewport_bounds.MaxY) {
        RLV_LOGF("hit_test outside viewport x=%d y=%d vp=%d,%d-%d,%d",
                 (int)x, (int)y,
                 (int)c->viewport_bounds.MinX,
                 (int)c->viewport_bounds.MinY,
                 (int)c->viewport_bounds.MaxX,
                 (int)c->viewport_bounds.MaxY);
        return -1;
    }

    content_y = c->scroll_y + (LONG)y - (LONG)c->viewport_bounds.MinY;
    RLV_LOGF("hit_test content_y=%ld scroll_y=%ld viewport_y=%d",
             (long)content_y, (long)c->scroll_y,
             (int)c->viewport_bounds.MinY);

    for (i = 0; i < c->row_count; i++) {
        top = c->layout_rows[i].top_y;
        content_bottom = top + (LONG)c->layout_rows[i].content_height;

        if (content_y >= top && content_y < content_bottom) {
            hit = (LONG)c->layout_rows[i].logical_index;
            RLV_LOGF("hit_test hit row=%ld flags=0x%lx",
                     (long)hit,
                     (c->rows != 0 && hit >= 0
                      && (ULONG)hit < c->row_count)
                         ? (unsigned long)c->rows[hit].flags
                         : 0UL);
            return hit;
        }

        /* Gap band after content: no row. */
        if (c->layout_rows[i].total_height > c->layout_rows[i].content_height) {
            if (content_y >= content_bottom
                && content_y < top + (LONG)c->layout_rows[i].total_height) {
                RLV_LOG("hit_test gap band (no row)");
                return -1;
            }
        }
    }

    RLV_LOG("hit_test no row matched");
    return -1;
}

VOID rlv_set_selected(RLV_Control *c, LONG logical_row)
{
    if (c == 0) {
        return;
    }

    if (logical_row < 0) {
        c->selected_row = -1;
        return;
    }

    if (!rlv_row_selectable(c, logical_row)) {
        return; /* leave prior selection unchanged */
    }

    c->selected_row = logical_row;
}

LONG rlv_get_selected(const RLV_Control *c)
{
    if (c == 0) {
        return -1;
    }
    return c->selected_row;
}

VOID rlv_set_keyboard_enabled(RLV_Control *c, BOOL enabled)
{
    if (c == 0) {
        return;
    }
    c->keyboard_enabled = enabled ? TRUE : FALSE;
}

BOOL rlv_get_keyboard_enabled(const RLV_Control *c)
{
    if (c == 0) {
        return FALSE;
    }
    return c->keyboard_enabled ? TRUE : FALSE;
}

VOID rlv_make_visible(RLV_Control *c, LONG logical_row)
{
    LONG vp_h;
    LONG top;
    LONG bottom;
    LONG scroll;
    LONG view;
    RLV_BENCH_DECLARE(bench_make_visible);

    RLV_BENCH_BEGIN(RLV_BENCH_NAV_MAKE_VISIBLE, bench_make_visible);
    if (c == 0 || c->layout_rows == 0) {
        RLV_BENCH_END(RLV_BENCH_NAV_MAKE_VISIBLE, bench_make_visible);
        return;
    }
    if (logical_row < 0 || (ULONG)logical_row >= c->row_count) {
        RLV_BENCH_END(RLV_BENCH_NAV_MAKE_VISIBLE, bench_make_visible);
        return;
    }

    view = rlv_view_for_source(c, logical_row);
    if (view < 0 || (ULONG)view >= c->row_count) {
        RLV_BENCH_END(RLV_BENCH_NAV_MAKE_VISIBLE, bench_make_visible);
        return;
    }

    vp_h = rlv_viewport_height(c);
    top = c->layout_rows[view].top_y;
    bottom = top + (LONG)c->layout_rows[view].content_height;
    scroll = c->scroll_y;

    if (top < scroll) {
        rlv_set_scroll_y(c, top);
        RLV_BENCH_COUNT(RLV_BENCH_COUNTER_VIEWPORT_SCROLLS);
        RLV_BENCH_END(RLV_BENCH_NAV_MAKE_VISIBLE, bench_make_visible);
        return;
    }

    if (vp_h > 0 && bottom > scroll + vp_h) {
        rlv_set_scroll_y(c, bottom - vp_h);
        RLV_BENCH_COUNT(RLV_BENCH_COUNTER_VIEWPORT_SCROLLS);
    }
    RLV_BENCH_END(RLV_BENCH_NAV_MAKE_VISIBLE, bench_make_visible);
}

BOOL rlv_handle_input(RLV_Control *c,
                              const struct RLV_InputEvent *event,
                              struct RLV_Event *result)
{
    LONG hit;
    LONG old_sel;
    LONG old_scroll;
    LONG step;
    LONG vp_h;
    LONG line_h;
    LONG new_scroll;
    LONG cb_row;
    UWORD cb_col;
    BOOL handled;
    BOOL cb_hit;
#if defined(RLV_ENABLE_EXPANDABLE_ROWS) && (RLV_ENABLE_EXPANDABLE_ROWS != 0)
    LONG disc_row;
    UWORD disc_col;
    BOOL disc_hit;
#endif
    RLV_BENCH_DECLARE(bench_key_total);

    RLV_BENCH_BEGIN(RLV_BENCH_KEY_EVENT_TOTAL, bench_key_total);
    rlv_clear_event(result);

    if (c == 0 || event == 0) {
        RLV_BENCH_END(RLV_BENCH_KEY_EVENT_TOTAL, bench_key_total);
        return FALSE;
    }

    if (event->type >= RLV_INPUT_NAV_NEXT
        && event->type <= RLV_INPUT_COLLAPSE_ROW) {
        RLV_BENCH_COUNT(RLV_BENCH_COUNTER_KEYBOARD_EVENTS);
    }

    if (!c->layout_valid) {
        RLV_BENCH_NOTE_PREPARE_REBUILD();
        rlv_layout_rebuild(c);
    }

    switch (event->type) {
    case RLV_INPUT_SELECT_DOWN:
        RLV_LOGF("SELECT_DOWN mouse=%d,%d viewport=%d,%d-%d,%d",
                 (int)event->x, (int)event->y,
                 (int)c->viewport_bounds.MinX,
                 (int)c->viewport_bounds.MinY,
                 (int)c->viewport_bounds.MaxX,
                 (int)c->viewport_bounds.MaxY);

#if defined(RLV_ENABLE_COLUMN_RESIZE) && (RLV_ENABLE_COLUMN_RESIZE != 0)
        /* Divider hit zone precedes sorting and body selection. */
        if (rlv_column_resize_handle_select_down(c, event->x, event->y)) {
            rlv_clear_arm(c);
            RLV_BENCH_END(RLV_BENCH_KEY_EVENT_TOTAL, bench_key_total);
            return FALSE;
        }
#endif

#if defined(RLV_ENABLE_SORTING) && (RLV_ENABLE_SORTING != 0)
        /* Sortable column headers: never select a data row. */
        if (rlv_sort_handle_header_click(c, event->x, event->y, result)) {
            RLV_BENCH_END(RLV_BENCH_KEY_EVENT_TOTAL, bench_key_total);
            return (result != 0 && result->type != (UWORD)RLV_EVENT_NONE)
                   ? TRUE : FALSE;
        }
#endif

        /*
         * Disclosure before checkbox before ordinary row selection.
         * Disclosure arms only — never changes selection / scroll.
         * Checkbox policy unchanged (SELECT_ROW / KEEP_CURRENT).
         * Toggle is SELECT_UP only (CELL_CONTROL); never merged here.
         */
#if defined(RLV_ENABLE_EXPANDABLE_ROWS) && (RLV_ENABLE_EXPANDABLE_ROWS != 0)
        disc_row = -1;
        disc_col = 0;
        disc_hit = rlv_hit_interactive_disclosure(c, event->x, event->y,
                                                       &disc_row, &disc_col);
        if (disc_hit) {
            rlv_arm_disclosure(c, disc_row, disc_col);
            RLV_LOGF("SELECT_DOWN disclosure arm row=%ld col=%u",
                     (long)disc_row, (unsigned)disc_col);
            RLV_BENCH_END(RLV_BENCH_KEY_EVENT_TOTAL, bench_key_total);
            return FALSE;
        }
#endif

        cb_row = -1;
        cb_col = 0;
        cb_hit = rlv_hit_interactive_checkbox(c, event->x, event->y,
                                                   &cb_row, &cb_col);
        if (cb_hit) {
            rlv_arm_checkbox(c, cb_row, cb_col);
            RLV_LOGF("SELECT_DOWN arm row=%ld col=%u policy=%u",
                     (long)cb_row, (unsigned)cb_col,
                     (unsigned)c->control_activation_policy);

            if (c->control_activation_policy
                == (UWORD)RLV_CONTROL_ACTIVATE_KEEP_CURRENT) {
                RLV_LOGF("SELECT_DOWN keep_current arm_only selected=%ld",
                         (long)c->selected_row);
                RLV_BENCH_END(RLV_BENCH_KEY_EVENT_TOTAL, bench_key_total);
                return FALSE;
            }

            if (!rlv_row_selectable(c, cb_row)) {
                RLV_LOGF("SELECT_DOWN checkbox nonselectable flags=0x%lx arm_only",
                         (c->rows != 0)
                             ? (unsigned long)c->rows[cb_row].flags
                             : 0UL);
                RLV_BENCH_END(RLV_BENCH_KEY_EVENT_TOTAL, bench_key_total);
                return FALSE;
            }

            if (cb_row == c->selected_row) {
                /* Already selected: arm only — no selection/scroll event. */
                RLV_LOGF("SELECT_DOWN checkbox same-row arm_only selected=%ld",
                         (long)c->selected_row);
                RLV_BENCH_END(RLV_BENCH_KEY_EVENT_TOTAL, bench_key_total);
                return FALSE;
            }

            /* Different selectable row: select + make_visible; may emit. */
            hit = cb_row;
        } else {
            rlv_clear_arm(c);
            hit = rlv_hit_test(c, event->x, event->y);
        }

        RLV_LOGF("SELECT_DOWN hit=%ld old_selected=%ld armed=%d",
                 (long)hit, (long)c->selected_row,
                 (int)c->control_armed);
        if (hit < 0) {
            RLV_LOG("SELECT_DOWN result=miss render_skipped");
            RLV_BENCH_END(RLV_BENCH_KEY_EVENT_TOTAL, bench_key_total);
            return FALSE;
        }
        if (!rlv_row_selectable(c, hit)) {
            RLV_LOGF("SELECT_DOWN nonselectable flags=0x%lx render_skipped",
                     (c->rows != 0)
                         ? (unsigned long)c->rows[hit].flags
                         : 0UL);
            RLV_BENCH_END(RLV_BENCH_KEY_EVENT_TOTAL, bench_key_total);
            return FALSE;
        }
        old_sel = c->selected_row;
        old_scroll = c->scroll_y;
        if (hit != old_sel) {
            c->selected_row = hit;
        }
        rlv_make_visible(c, hit);
        if (result != 0) {
            if (hit != old_sel) {
                result->type = (UWORD)RLV_EVENT_SELECTION_CHANGED;
                rlv_event_set_row(c, result, hit);
                result->previous_row = old_sel;
                result->value = c->scroll_y;
                RLV_LOGF("SELECTION_CHANGED row=%ld prev=%ld tag=%lu",
                         (long)hit, (long)old_sel,
                         (unsigned long)(ULONG)result->row_user_data);
            } else if (c->scroll_y != old_scroll) {
                /* Outside-box same-row path only (checkbox same-row returned). */
                result->type = (UWORD)RLV_EVENT_SCROLL_CHANGED;
                rlv_event_set_row(c, result, hit);
                result->previous_row = -1;
                result->value = c->scroll_y;
            }
        }
        RLV_LOGF("SELECT_DOWN new_selected=%ld ev.type=%u render=%s",
                 (long)c->selected_row,
                 (result != 0) ? (unsigned)result->type : 0U,
                 (result != 0 && result->type != (UWORD)RLV_EVENT_NONE)
                     ? "requested" : "skipped");
        handled = (result != 0 && result->type != (UWORD)RLV_EVENT_NONE);
        RLV_BENCH_END(RLV_BENCH_KEY_EVENT_TOTAL, bench_key_total);
        return handled;

    case RLV_INPUT_SELECT_UP:
        /*
         * Commit only when release is still on the same armed control →
         * CELL_CONTROL only (never SELECTION_CHANGED). Otherwise cancel
         * arm with no event. Does not paint.
         */
#if defined(RLV_ENABLE_COLUMN_RESIZE) && (RLV_ENABLE_COLUMN_RESIZE != 0)
        if (c->resize_dragging) {
            if (rlv_column_resize_handle_select_up(c, event->x, event->y,
                                                   result)) {
                RLV_BENCH_END(RLV_BENCH_KEY_EVENT_TOTAL, bench_key_total);
                return TRUE;
            }
            RLV_BENCH_END(RLV_BENCH_KEY_EVENT_TOTAL, bench_key_total);
            return FALSE;
        }
#endif
        if (!c->control_armed) {
            RLV_BENCH_END(RLV_BENCH_KEY_EVENT_TOTAL, bench_key_total);
            return FALSE;
        }

#if defined(RLV_ENABLE_EXPANDABLE_ROWS) && (RLV_ENABLE_EXPANDABLE_ROWS != 0)
        if (c->armed_type == (UBYTE)RLV_COL_TYPE_DISCLOSURE) {
            disc_row = -1;
            disc_col = 0;
            disc_hit = rlv_hit_interactive_disclosure(c, event->x, event->y,
                                                           &disc_row,
                                                           &disc_col);
            if (disc_hit
                && disc_row == c->armed_row
                && disc_col == c->armed_column) {
                handled = rlv_commit_disclosure_toggle(c, result);
                rlv_clear_arm(c);
                RLV_BENCH_END(RLV_BENCH_KEY_EVENT_TOTAL, bench_key_total);
                return handled;
            }
            RLV_LOGF("SELECT_UP disclosure cancel arm row=%ld col=%u",
                     (long)c->armed_row, (unsigned)c->armed_column);
            rlv_clear_arm(c);
            RLV_BENCH_END(RLV_BENCH_KEY_EVENT_TOTAL, bench_key_total);
            return FALSE;
        }
#endif

        cb_row = -1;
        cb_col = 0;
        cb_hit = rlv_hit_interactive_checkbox(c, event->x, event->y,
                                                   &cb_row, &cb_col);
        if (cb_hit
            && cb_row == c->armed_row
            && cb_col == c->armed_column
            && c->armed_type == (UBYTE)RLV_COL_TYPE_CHECKBOX) {
            handled = rlv_commit_checkbox_toggle(c, result);
            rlv_clear_arm(c);
            RLV_BENCH_END(RLV_BENCH_KEY_EVENT_TOTAL, bench_key_total);
            return handled;
        }

        RLV_LOGF("SELECT_UP cancel arm row=%ld col=%u",
                 (long)c->armed_row, (unsigned)c->armed_column);
        rlv_clear_arm(c);
        RLV_BENCH_END(RLV_BENCH_KEY_EVENT_TOTAL, bench_key_total);
        return FALSE;

    case RLV_INPUT_POINTER_MOVE:
#if defined(RLV_ENABLE_COLUMN_RESIZE) && (RLV_ENABLE_COLUMN_RESIZE != 0)
        if (c->resize_dragging) {
            rlv_column_resize_handle_pointer_move(c, event->x, event->y);
        } else if (c->column_resize_enabled) {
            rlv_column_resize_handle_hover_move(c, event->x, event->y);
        }
#endif
        RLV_BENCH_END(RLV_BENCH_KEY_EVENT_TOTAL, bench_key_total);
        return FALSE;

    case RLV_INPUT_CANCEL:
#if defined(RLV_ENABLE_COLUMN_RESIZE) && (RLV_ENABLE_COLUMN_RESIZE != 0)
        if (c->resize_dragging) {
            rlv_column_resize_handle_cancel(c);
            RLV_BENCH_END(RLV_BENCH_KEY_EVENT_TOTAL, bench_key_total);
            return FALSE;
        }
#endif
        rlv_clear_arm(c);
        RLV_BENCH_END(RLV_BENCH_KEY_EVENT_TOTAL, bench_key_total);
        return FALSE;

    case RLV_INPUT_SCROLL_LINE_UP:
    case RLV_INPUT_SCROLL_LINE_DOWN:
        line_h = (LONG)c->line_height;
        if (line_h < 1) {
            line_h = 1;
        }
        old_scroll = c->scroll_y;
        if (event->type == RLV_INPUT_SCROLL_LINE_UP) {
            rlv_set_scroll_y(c, old_scroll - line_h);
        } else {
            rlv_set_scroll_y(c, old_scroll + line_h);
        }
        if (c->scroll_y == old_scroll) {
            return FALSE;
        }
        if (result != 0) {
            result->type = (UWORD)RLV_EVENT_SCROLL_CHANGED;
            rlv_event_set_row(c, result, c->selected_row);
            result->value = c->scroll_y;
        }
        RLV_BENCH_COUNT(RLV_BENCH_COUNTER_VIEWPORT_SCROLLS);
        RLV_BENCH_END(RLV_BENCH_KEY_EVENT_TOTAL, bench_key_total);
        return TRUE;

    case RLV_INPUT_SCROLL_PAGE_UP:
    case RLV_INPUT_SCROLL_PAGE_DOWN:
        line_h = (LONG)c->line_height;
        if (line_h < 1) {
            line_h = 1;
        }
        vp_h = rlv_viewport_height(c);
        step = vp_h - line_h;
        if (step < line_h) {
            step = line_h;
        }
        old_scroll = c->scroll_y;
        if (event->type == RLV_INPUT_SCROLL_PAGE_UP) {
            rlv_set_scroll_y(c, old_scroll - step);
        } else {
            rlv_set_scroll_y(c, old_scroll + step);
        }
        if (c->scroll_y == old_scroll) {
            return FALSE;
        }
        if (result != 0) {
            result->type = (UWORD)RLV_EVENT_SCROLL_CHANGED;
            rlv_event_set_row(c, result, c->selected_row);
            result->value = c->scroll_y;
        }
        RLV_BENCH_COUNT(RLV_BENCH_COUNTER_VIEWPORT_SCROLLS);
        RLV_BENCH_END(RLV_BENCH_KEY_EVENT_TOTAL, bench_key_total);
        return TRUE;

    case RLV_INPUT_SCROLL_POSITION:
        old_scroll = c->scroll_y;
        new_scroll = event->value;
        rlv_set_scroll_y(c, new_scroll);
        if (c->scroll_y == old_scroll) {
            return FALSE;
        }
        if (result != 0) {
            result->type = (UWORD)RLV_EVENT_SCROLL_CHANGED;
            rlv_event_set_row(c, result, c->selected_row);
            result->value = c->scroll_y;
        }
        RLV_BENCH_COUNT(RLV_BENCH_COUNTER_VIEWPORT_SCROLLS);
        RLV_BENCH_END(RLV_BENCH_KEY_EVENT_TOTAL, bench_key_total);
        return TRUE;

    case RLV_INPUT_NAV_NEXT:
    case RLV_INPUT_NAV_PREV:
    case RLV_INPUT_NAV_FIRST:
    case RLV_INPUT_NAV_LAST:
    case RLV_INPUT_NAV_PAGE_DOWN:
    case RLV_INPUT_NAV_PAGE_UP:
    case RLV_INPUT_NAV_ACTIVATE:
        if (!c->keyboard_enabled) {
            RLV_BENCH_END(RLV_BENCH_KEY_EVENT_TOTAL, bench_key_total);
            return FALSE;
        }
        if (event->type == RLV_INPUT_NAV_NEXT) {
            hit = rlv_find_selectable(c, c->selected_row, 1);
            handled = rlv_nav_select(c, hit, result);
            RLV_BENCH_END(RLV_BENCH_KEY_EVENT_TOTAL, bench_key_total);
            return handled;
        }
        if (event->type == RLV_INPUT_NAV_PREV) {
            if (c->selected_row < 0) {
                hit = rlv_find_selectable(c, (LONG)c->row_count, -1);
            } else {
                hit = rlv_find_selectable(c, c->selected_row, -1);
            }
            handled = rlv_nav_select(c, hit, result);
            RLV_BENCH_END(RLV_BENCH_KEY_EVENT_TOTAL, bench_key_total);
            return handled;
        }
        if (event->type == RLV_INPUT_NAV_FIRST) {
            hit = rlv_find_selectable(c, -1, 1);
            handled = rlv_nav_select(c, hit, result);
            RLV_BENCH_END(RLV_BENCH_KEY_EVENT_TOTAL, bench_key_total);
            return handled;
        }
        if (event->type == RLV_INPUT_NAV_LAST) {
            hit = rlv_find_selectable(c, (LONG)c->row_count, -1);
            handled = rlv_nav_select(c, hit, result);
            RLV_BENCH_END(RLV_BENCH_KEY_EVENT_TOTAL, bench_key_total);
            return handled;
        }
        if (event->type == RLV_INPUT_NAV_PAGE_DOWN) {
            hit = rlv_page_nav_target(c, 1);
            handled = rlv_nav_select(c, hit, result);
            RLV_BENCH_END(RLV_BENCH_KEY_EVENT_TOTAL, bench_key_total);
            return handled;
        }
        if (event->type == RLV_INPUT_NAV_PAGE_UP) {
            hit = rlv_page_nav_target(c, -1);
            handled = rlv_nav_select(c, hit, result);
            RLV_BENCH_END(RLV_BENCH_KEY_EVENT_TOTAL, bench_key_total);
            return handled;
        }
        /* RLV_INPUT_NAV_ACTIVATE */
        if (!rlv_row_selectable(c, c->selected_row)) {
            RLV_BENCH_END(RLV_BENCH_KEY_EVENT_TOTAL, bench_key_total);
            return FALSE;
        }
        if (result != 0) {
            result->type = (UWORD)RLV_EVENT_ACTIVATED;
            rlv_event_set_row(c, result, c->selected_row);
            result->previous_row = -1;
            result->value = c->scroll_y;
            RLV_LOGF("ACTIVATED row=%ld tag=%lu",
                     (long)c->selected_row,
                     (unsigned long)(ULONG)result->row_user_data);
        }
        RLV_BENCH_END(RLV_BENCH_KEY_EVENT_TOTAL, bench_key_total);
        return TRUE;

    case RLV_INPUT_TOGGLE:
        /*
         * C7 / §D.11: Space toggles the selected row's sole eligible
         * checkbox column. Zero or multiple eligible → no toggle.
         * Display-only / disabled fail the VISIBLE|ENABLED|INTERACTIVE
         * mask. NAV_ACTIVATE is separate and never reaches here.
         * Clears any pending mouse arm (keyboard supersedes incomplete click).
         */
        if (!c->keyboard_enabled) {
            RLV_BENCH_END(RLV_BENCH_KEY_EVENT_TOTAL, bench_key_total);
            return FALSE;
        }
        rlv_clear_arm(c);
        if (c->selected_row < 0
            || (ULONG)c->selected_row >= c->row_count) {
            RLV_LOG("TOGGLE no selection");
            RLV_BENCH_END(RLV_BENCH_KEY_EVENT_TOTAL, bench_key_total);
            return FALSE;
        }
        cb_col = 0;
        if (!rlv_find_sole_eligible_checkbox(c, c->selected_row,
                                                 &cb_col)) {
            RLV_LOGF("TOGGLE deferred row=%ld (zero/multi/ineligible)",
                     (long)c->selected_row);
            RLV_BENCH_END(RLV_BENCH_KEY_EVENT_TOTAL, bench_key_total);
            return FALSE;
        }
        handled = rlv_toggle_checkbox_at(c, c->selected_row, cb_col,
                                             result);
        RLV_BENCH_END(RLV_BENCH_KEY_EVENT_TOTAL, bench_key_total);
        return handled;

#if defined(RLV_ENABLE_EXPANDABLE_ROWS) && (RLV_ENABLE_EXPANDABLE_ROWS != 0)
    case RLV_INPUT_EXPAND_ROW:
    case RLV_INPUT_COLLAPSE_ROW:
        /*
         * Right expands / Left collapses the current row when applicable.
         * No-ops leave no event. Never auto-expand on Up/Down.
         */
        if (!c->keyboard_enabled) {
            RLV_BENCH_END(RLV_BENCH_KEY_EVENT_TOTAL, bench_key_total);
            return FALSE;
        }
        rlv_clear_arm(c);
        if (c->selected_row < 0
            || (ULONG)c->selected_row >= c->row_count) {
            RLV_BENCH_END(RLV_BENCH_KEY_EVENT_TOTAL, bench_key_total);
            return FALSE;
        }
        if (!rlv_is_row_expandable(c, c->selected_row)) {
            RLV_BENCH_END(RLV_BENCH_KEY_EVENT_TOTAL, bench_key_total);
            return FALSE;
        }
        if (!rlv_row_has_multi_line_wrap(c, c->selected_row)) {
            RLV_BENCH_END(RLV_BENCH_KEY_EVENT_TOTAL, bench_key_total);
            return FALSE;
        }
        if (event->type == RLV_INPUT_EXPAND_ROW) {
            if (rlv_is_row_expanded(c, c->selected_row)) {
                RLV_BENCH_END(RLV_BENCH_KEY_EVENT_TOTAL, bench_key_total);
                return FALSE;
            }
            handled = rlv_set_row_expanded(c, c->selected_row, TRUE,
                                                RLV_EXPAND_SRC_KEY, result);
        } else {
            if (!rlv_is_row_expanded(c, c->selected_row)) {
                RLV_BENCH_END(RLV_BENCH_KEY_EVENT_TOTAL, bench_key_total);
                return FALSE;
            }
            handled = rlv_set_row_expanded(c, c->selected_row, FALSE,
                                                RLV_EXPAND_SRC_KEY, result);
        }
        RLV_BENCH_END(RLV_BENCH_KEY_EVENT_TOTAL, bench_key_total);
        return handled;
#else
    case RLV_INPUT_EXPAND_ROW:
    case RLV_INPUT_COLLAPSE_ROW:
        RLV_BENCH_END(RLV_BENCH_KEY_EVENT_TOTAL, bench_key_total);
        return FALSE;
#endif

    default:
        RLV_BENCH_END(RLV_BENCH_KEY_EVENT_TOTAL, bench_key_total);
        return FALSE;
    }
}
