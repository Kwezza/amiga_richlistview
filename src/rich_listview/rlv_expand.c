/**
 * Expandable / collapsible logical rows — state, layout rebuild, API.
 *
 * Compiled when RLV_ENABLE_EXPANDABLE_ROWS != 0. Ownership: the control
 * keeps an owned per-row expand snapshot; the application owns the
 * authoritative RLV_Row.flags and updates them from CELL_CONTROL or after
 * programmatic API calls. Programmatic APIs never emit CELL_CONTROL.
 */

#include "rich_listview/rlv_internal.h"
#include "rich_listview/rlv_log.h"
#include "rich_listview/rlv_platform_internal.h"

#include <string.h>

#if defined(RLV_ENABLE_EXPANDABLE_ROWS) && (RLV_ENABLE_EXPANDABLE_ROWS != 0)

VOID rlv_free_row_expand(RLV_Control *c)
{
    if (c == 0) {
        return;
    }
    if (c->row_expand != 0) {
        rlv_platform_free(c->row_expand);
        c->row_expand = 0;
    }
    c->row_expand_count = 0;
}

BOOL rlv_refresh_row_expand(RLV_Control *c)
{
    ULONG i;
    UBYTE *snap;
    UWORD flags;
    UBYTE bits;

    if (c == 0) {
        return FALSE;
    }

    rlv_free_row_expand(c);

    if (c->row_count == 0 || c->rows == 0) {
        return TRUE;
    }

    snap = (UBYTE *)rlv_platform_malloc((size_t)c->row_count);
    if (snap == 0) {
        RLV_LOG("FAIL rlv_refresh_row_expand malloc");
        return FALSE;
    }
    memset(snap, 0, (size_t)c->row_count);

    for (i = 0; i < c->row_count; i++) {
        flags = c->rows[i].flags;
        bits = 0;
        if ((flags & RLV_ROW_EXPANDABLE) != 0) {
            bits = (UBYTE)RLV_ROWEXP_EXPANDABLE;
            if (c->apply_initial_expand) {
                if (c->initial_expand
                    != (UWORD)RLV_INITIAL_EXPAND_ALL_COLLAPSED) {
                    bits = (UBYTE)(bits | RLV_ROWEXP_EXPANDED);
                }
            } else if ((flags & RLV_ROW_EXPANDED) != 0) {
                bits = (UBYTE)(bits | RLV_ROWEXP_EXPANDED);
            }
        }
        snap[i] = bits;
    }

    c->row_expand = snap;
    c->row_expand_count = c->row_count;
    c->apply_initial_expand = FALSE;
    return TRUE;
}

BOOL rlv_row_is_collapsed_compact(const RLV_Control *c, LONG logical_row)
{
    UBYTE bits;

    if (c == 0 || c->row_expand == 0) {
        return FALSE;
    }
    /* Compact height only while disclosure UI is active. */
    if (!rlv_disclosure_ui_enabled(c)) {
        return FALSE;
    }
    if (logical_row < 0 || (ULONG)logical_row >= c->row_expand_count) {
        return FALSE;
    }
    bits = c->row_expand[logical_row];
    if ((bits & RLV_ROWEXP_EXPANDABLE) == 0) {
        return FALSE;
    }
    if ((bits & RLV_ROWEXP_EXPANDED) != 0) {
        return FALSE;
    }
    /* No compact mode when expanded layout would still be one line. */
    if (!rlv_row_has_multi_line_wrap(c, logical_row)) {
        return FALSE;
    }
    return TRUE;
}

BOOL rlv_row_has_multi_line_wrap(const RLV_Control *c, LONG logical_row)
{
    UWORD col;
    UWORD col_type;
    ULONG idx;
    UWORD frag_count;

    if (c == 0 || c->cell_wraps == 0 || c->columns == 0) {
        return FALSE;
    }
    if (logical_row < 0 || (ULONG)logical_row >= c->row_count) {
        return FALSE;
    }

    for (col = 0; col < c->column_count; col++) {
        col_type = (UWORD)(c->columns[col].flags & RLV_COL_TYPE_MASK);
        if (col_type == (UWORD)RLV_COL_TYPE_CHECKBOX
            || col_type == (UWORD)RLV_COL_TYPE_DISCLOSURE) {
            continue;
        }

        idx = (ULONG)logical_row * (ULONG)c->column_count + (ULONG)col;
        if (idx >= c->cell_wrap_count) {
            continue;
        }
        frag_count = c->cell_wraps[idx].frag_count;
        if (frag_count > 1) {
            return TRUE;
        }
    }
    return FALSE;
}

static UWORD rlv_find_disclosure_column(const RLV_Control *c)
{
    UWORD col;
    UWORD col_type;

    if (c == 0 || c->columns == 0) {
        return 0;
    }
    for (col = 0; col < c->column_count; col++) {
        col_type = (UWORD)(c->columns[col].flags & RLV_COL_TYPE_MASK);
        if (col_type == (UWORD)RLV_COL_TYPE_DISCLOSURE) {
            return col;
        }
    }
    return 0;
}

static VOID rlv_fill_expand_event(RLV_Control *c,
                                       RLV_Event *result,
                                       LONG row,
                                       BOOL expanded,
                                       UBYTE previous_value)
{
    UWORD action;
    UBYTE cell_value;

    if (result == 0) {
        return;
    }

    action = expanded
             ? (UWORD)RLV_ACTION_EXPANDED
             : (UWORD)RLV_ACTION_COLLAPSED;
    cell_value = expanded
                 ? (UBYTE)RLV_CELL_EXPANDED
                 : (UBYTE)RLV_CELL_COLLAPSED;

    result->type = (UWORD)RLV_EVENT_CELL_CONTROL;
    result->row = row;
    result->previous_row = -1;
    result->value = c->scroll_y;
    result->column = rlv_find_disclosure_column(c);
    result->control_type = (UWORD)RLV_COL_TYPE_DISCLOSURE;
    result->control_action = action;
    result->row_user_data = 0;
    if (c->rows != 0 && row >= 0 && (ULONG)row < c->row_count) {
        result->row_user_data = c->rows[row].user_data;
    }
    result->previous_value = previous_value;
    result->cell_value = cell_value;
}

/*
 * Keep the first physical line of the toggled row at the same screen Y
 * when possible. Falls back to clamped scroll when anchoring is impossible.
 */
static VOID rlv_anchor_row_first_line(RLV_Control *c,
                                          LONG row,
                                          LONG anchor_screen_y)
{
    LONG new_scroll;
    LONG row_top;

    if (c == 0 || c->layout_rows == 0) {
        return;
    }
    if (row < 0 || (ULONG)row >= c->row_count) {
        return;
    }

    row_top = c->layout_rows[row].top_y;
    new_scroll = row_top
                 - (anchor_screen_y - (LONG)c->viewport_bounds.MinY);
    rlv_set_scroll_y(c, new_scroll);
}

BOOL rlv_set_row_expanded(RLV_Control *c,
                               LONG row,
                               BOOL expanded,
                               ULONG source_flags,
                               RLV_Event *result)
{
    UBYTE bits;
    UBYTE prev_bits;
    UBYTE prev_value;
    BOOL was_expanded;
    BOOL want_event;
    BOOL defer;
    LONG anchor_y;
    LONG old_scroll;

    if (c == 0) {
        return FALSE;
    }
    if (c->row_expand == 0
        || row < 0
        || (ULONG)row >= c->row_expand_count) {
        RLV_LOGF("EXPAND reject invalid row=%ld", (long)row);
        return FALSE;
    }

    bits = c->row_expand[row];
    if ((bits & RLV_ROWEXP_EXPANDABLE) == 0) {
        RLV_LOGF("EXPAND reject non-expandable row=%ld", (long)row);
        return FALSE;
    }

    /* Nothing to disclose when wrap is a single line (glyph also hidden). */
    if (!rlv_row_has_multi_line_wrap(c, row)) {
        RLV_LOGF("EXPAND reject single-line wrap row=%ld", (long)row);
        return FALSE;
    }

    /*
     * Mouse/keyboard disclosure only while COLLAPSIBLE. Programmatic API
     * may still mutate expand bits so Always/Single-line can restore them.
     */
    if ((source_flags & (RLV_EXPAND_SRC_MOUSE | RLV_EXPAND_SRC_KEY)) != 0
        && !rlv_disclosure_ui_enabled(c)) {
        RLV_LOGF("EXPAND reject disclosure UI off row=%ld", (long)row);
        return FALSE;
    }

    was_expanded = ((bits & RLV_ROWEXP_EXPANDED) != 0) ? TRUE : FALSE;
    if ((was_expanded && expanded) || (!was_expanded && !expanded)) {
        /* Same-state no-op: success, no event, no layout work. */
        RLV_LOGF("EXPAND no-op row=%ld expanded=%d",
                 (long)row, (int)expanded);
        return TRUE;
    }

    prev_bits = bits;
    prev_value = was_expanded
                 ? (UBYTE)RLV_CELL_EXPANDED
                 : (UBYTE)RLV_CELL_COLLAPSED;

    if (expanded) {
        bits = (UBYTE)(bits | RLV_ROWEXP_EXPANDED);
    } else {
        bits = (UBYTE)(bits & (UBYTE)~RLV_ROWEXP_EXPANDED);
    }
    c->row_expand[row] = bits;

    defer = ((source_flags & RLV_EXPAND_SRC_BULK) != 0) ? TRUE : FALSE;
    if (defer) {
        RLV_LOGF("EXPAND bulk mutate row=%ld expanded=%d",
                 (long)row, (int)expanded);
        return TRUE;
    }

    /* Cancel incomplete checkbox/disclosure arm — layout is changing. */
    c->control_armed = FALSE;
    c->armed_row = -1;
    c->armed_column = 0;
    c->armed_type = 0;

    old_scroll = c->scroll_y;
    anchor_y = 0;
    c->expand_old_total_h = 0;
    if (c->layout_valid && c->layout_rows != 0) {
        anchor_y = (LONG)c->viewport_bounds.MinY
                   + c->layout_rows[row].top_y
                   - c->scroll_y;
        /* Capture before reheight — required for below-row ScrollRaster. */
        c->expand_old_total_h = (LONG)c->layout_rows[row].total_height;
    }

    if (!c->layout_valid) {
        if (!rlv_layout_rebuild(c)) {
            /* Restore prior expand bit on failure. */
            c->row_expand[row] = prev_bits;
            c->expand_old_total_h = 0;
            RLV_LOG("FAIL EXPAND layout_rebuild");
            return FALSE;
        }
    } else if (!rlv_layout_reheight_from(c, (ULONG)row)) {
        c->row_expand[row] = prev_bits;
        c->expand_old_total_h = 0;
        RLV_LOG("FAIL EXPAND layout_reheight_from");
        return FALSE;
    }

    if (c->layout_rows != 0) {
        rlv_anchor_row_first_line(c, row, anchor_y);
    } else {
        rlv_set_scroll_y(c, old_scroll);
    }

    want_event = ((source_flags & (RLV_EXPAND_SRC_MOUSE | RLV_EXPAND_SRC_KEY))
                  != 0) ? TRUE : FALSE;
    if (want_event && result != 0) {
        rlv_fill_expand_event(c, result, row, expanded, prev_value);
    }

    RLV_LOGF("EXPAND row=%ld expanded=%d scroll=%ld->%ld src=0x%lx",
             (long)row, (int)expanded,
             (long)old_scroll, (long)c->scroll_y,
             (unsigned long)source_flags);
    return TRUE;
}

BOOL rlv_expand_row(RLV_Control *c, LONG row)
{
    return rlv_set_row_expanded(c, row, TRUE, RLV_EXPAND_SRC_API, 0);
}

BOOL rlv_collapse_row(RLV_Control *c, LONG row)
{
    return rlv_set_row_expanded(c, row, FALSE, RLV_EXPAND_SRC_API, 0);
}

BOOL rlv_toggle_row(RLV_Control *c, LONG row)
{
    BOOL expanded;

    if (c == 0 || c->row_expand == 0) {
        return FALSE;
    }
    if (row < 0 || (ULONG)row >= c->row_expand_count) {
        return FALSE;
    }
    if ((c->row_expand[row] & RLV_ROWEXP_EXPANDABLE) == 0) {
        return FALSE;
    }
    expanded = ((c->row_expand[row] & RLV_ROWEXP_EXPANDED) != 0)
               ? FALSE : TRUE;
    return rlv_set_row_expanded(c, row, expanded, RLV_EXPAND_SRC_API, 0);
}

VOID rlv_collapse_all(RLV_Control *c)
{
    ULONG i;
    BOOL any;

    if (c == 0 || c->row_expand == 0) {
        return;
    }

    any = FALSE;
    for (i = 0; i < c->row_expand_count; i++) {
        if ((c->row_expand[i] & RLV_ROWEXP_EXPANDABLE) == 0) {
            continue;
        }
        if ((c->row_expand[i] & RLV_ROWEXP_EXPANDED) == 0) {
            continue;
        }
        c->row_expand[i] = (UBYTE)(c->row_expand[i]
                                   & (UBYTE)~RLV_ROWEXP_EXPANDED);
        any = TRUE;
    }

    if (!any) {
        RLV_LOG("COLLAPSE_ALL no-op");
        return;
    }

    c->control_armed = FALSE;
    c->armed_row = -1;
    c->armed_column = 0;
    c->armed_type = 0;

    if (!c->layout_valid) {
        (VOID)rlv_layout_rebuild(c);
    } else {
        (VOID)rlv_layout_reheight_from(c, 0);
    }
    rlv_set_scroll_y(c, c->scroll_y);
    c->expand_old_total_h = 0;
    RLV_LOG("COLLAPSE_ALL done");
}

BOOL rlv_is_row_expandable(const RLV_Control *c, LONG row)
{
    if (c == 0 || c->row_expand == 0) {
        return FALSE;
    }
    if (row < 0 || (ULONG)row >= c->row_expand_count) {
        return FALSE;
    }
    return ((c->row_expand[row] & RLV_ROWEXP_EXPANDABLE) != 0)
           ? TRUE : FALSE;
}

BOOL rlv_is_row_expanded(const RLV_Control *c, LONG row)
{
    if (c == 0 || c->row_expand == 0) {
        return FALSE;
    }
    if (row < 0 || (ULONG)row >= c->row_expand_count) {
        return FALSE;
    }
    if ((c->row_expand[row] & RLV_ROWEXP_EXPANDABLE) == 0) {
        return FALSE;
    }
    return ((c->row_expand[row] & RLV_ROWEXP_EXPANDED) != 0)
           ? TRUE : FALSE;
}

#endif /* RLV_ENABLE_EXPANDABLE_ROWS */
