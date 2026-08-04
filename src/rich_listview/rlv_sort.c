/**
 * Optional stable view-order sorting for RichListview.
 *
 * Compiled when RLV_ENABLE_SORTING != 0. Never mutates borrowed RLV_Row
 * arrays; owns a UWORD view↔source map. Iterative bottom-up merge sort
 * (stable, non-recursive). Sort barriers: RLV_ROW_SORT_FIXED only
 * (non-selectable is orthogonal — mark headings with both flags).
 * Attached-page only — not a global catalogue sort.
 */

#include "rich_listview/rlv_internal.h"
#include "rich_listview/rlv_platform_internal.h"
#include "rich_listview/rlv_log.h"

#include <string.h>

#if defined(RLV_ENABLE_SORTING) && (RLV_ENABLE_SORTING != 0)

#define RLV_SORT_MAX_ROWS       65535UL
#define RLV_SORT_INDICATOR_W    7
#define RLV_SORT_INDICATOR_GAP  2 /* space between title text and triangle */
#define RLV_SORT_INDICATOR_EDGE 2 /* inset from cell_right / column divider */

/* Numeric key classes for deterministic ordering. */
#define RLV_NUM_OK       0
#define RLV_NUM_EMPTY    1
#define RLV_NUM_INVALID  2

typedef struct RLV_NumKey
{
    UBYTE class;   /* RLV_NUM_* */
    UBYTE neg;     /* non-zero if signed negative */
    ULONG mag;     /* magnitude */
} RLV_NumKey;

ULONG rlv_source_for_view(const RLV_Control *c, ULONG view)
{
    if (c == 0 || c->row_count == 0 || view >= c->row_count) {
        return view;
    }
    if (c->view_to_source == 0 || c->sort_map_count != c->row_count) {
        return view;
    }
    return (ULONG)c->view_to_source[view];
}

LONG rlv_view_for_source(const RLV_Control *c, LONG source)
{
    if (c == 0 || source < 0 || (ULONG)source >= c->row_count) {
        return source;
    }
    if (c->source_to_view == 0 || c->sort_map_count != c->row_count) {
        return source;
    }
    return (LONG)c->source_to_view[source];
}

VOID rlv_sort_free_maps(RLV_Control *c)
{
    if (c == 0) {
        return;
    }
    if (c->view_to_source != 0) {
        rlv_platform_free(c->view_to_source);
        c->view_to_source = 0;
    }
    if (c->source_to_view != 0) {
        rlv_platform_free(c->source_to_view);
        c->source_to_view = 0;
    }
    c->sort_map_count = 0;
    c->sort_active = 0;
    c->sort_column = 0;
    c->sort_direction = (UWORD)RLV_SORT_ASC;
}

VOID rlv_sort_on_rows_replaced(RLV_Control *c)
{
    if (c == 0) {
        return;
    }
    /* Drop maps; keep borrowed specs. Re-sort is explicit. */
    rlv_sort_free_maps(c);
    RLV_LOG("SORT maps cleared on set_rows");
}

static VOID rlv_sort_rebuild_inverse(RLV_Control *c)
{
    ULONG i;
    ULONG src;

    if (c == 0 || c->view_to_source == 0 || c->source_to_view == 0) {
        return;
    }
    for (i = 0; i < c->sort_map_count; i++) {
        src = (ULONG)c->view_to_source[i];
        if (src < c->sort_map_count) {
            c->source_to_view[src] = (UWORD)i;
        }
    }
}

static BOOL rlv_sort_fill_identity(RLV_Control *c)
{
    ULONG i;

    if (c == 0 || c->view_to_source == 0 || c->source_to_view == 0) {
        return FALSE;
    }
    for (i = 0; i < c->sort_map_count; i++) {
        c->view_to_source[i] = (UWORD)i;
        c->source_to_view[i] = (UWORD)i;
    }
    return TRUE;
}

/* Allocate maps if missing or size mismatch; leave existing order intact. */
static BOOL rlv_sort_ensure_maps(RLV_Control *c)
{
    ULONG n;
    UWORD *v2s;
    UWORD *s2v;

    if (c == 0) {
        return FALSE;
    }
    n = c->row_count;
    if (n == 0) {
        rlv_sort_free_maps(c);
        return TRUE;
    }
    if (n > RLV_SORT_MAX_ROWS) {
        RLV_LOGF("FAIL SORT row_count=%lu exceeds UWORD map",
                 (unsigned long)n);
        return FALSE;
    }
    if (c->view_to_source != 0 && c->source_to_view != 0
        && c->sort_map_count == n) {
        return TRUE;
    }

    rlv_sort_free_maps(c);
    v2s = (UWORD *)rlv_platform_malloc((size_t)n * sizeof(UWORD));
    s2v = (UWORD *)rlv_platform_malloc((size_t)n * sizeof(UWORD));
    if (v2s == 0 || s2v == 0) {
        if (v2s != 0) {
            rlv_platform_free(v2s);
        }
        if (s2v != 0) {
            rlv_platform_free(s2v);
        }
        RLV_LOG("FAIL SORT map allocation");
        return FALSE;
    }
    c->view_to_source = v2s;
    c->source_to_view = s2v;
    c->sort_map_count = n;
    rlv_sort_fill_identity(c);
    c->sort_active = 0;
    RLV_LOGF("SORT map alloc rows=%lu bytes=%lu",
             (unsigned long)n,
             (unsigned long)(n * sizeof(UWORD) * 2UL));
    return TRUE;
}

static BOOL rlv_sort_row_is_barrier(const RLV_Control *c, ULONG source)
{
    UWORD flags;

    if (c == 0 || c->rows == 0 || source >= c->row_count) {
        return TRUE;
    }
    flags = c->rows[source].flags;
    /* Only SORT_FIXED defines a barrier; NONSELECTABLE is orthogonal. */
    if ((flags & RLV_ROW_SORT_FIXED) != 0) {
        return TRUE;
    }
    return FALSE;
}

static const RLV_SortSpec *rlv_sort_find_spec(const RLV_Control *c,
                                              UWORD column)
{
    UWORD i;

    if (c == 0 || c->sort_specs == 0) {
        return 0;
    }
    for (i = 0; i < c->sort_spec_count; i++) {
        if (c->sort_specs[i].column == column
            && c->sort_specs[i].kind != (UWORD)RLV_SORT_NONE) {
            return &c->sort_specs[i];
        }
    }
    return 0;
}

static int rlv_sort_tolower(int ch)
{
    if (ch >= 'A' && ch <= 'Z') {
        return ch + ('a' - 'A');
    }
    return ch;
}

static CONST_STRPTR rlv_sort_cell_text(const RLV_Control *c,
                                       ULONG source,
                                       UWORD column)
{
    if (c == 0 || c->rows == 0 || source >= c->row_count) {
        return 0;
    }
    if (c->rows[source].cells == 0 || column >= c->column_count) {
        return 0;
    }
    return c->rows[source].cells[column];
}

static int rlv_sort_strcmp_case(CONST_STRPTR a, CONST_STRPTR b, BOOL nocase)
{
    const unsigned char *pa;
    const unsigned char *pb;
    int ca;
    int cb;

    pa = (const unsigned char *)((a != 0) ? a : (CONST_STRPTR)"");
    pb = (const unsigned char *)((b != 0) ? b : (CONST_STRPTR)"");
    for (;;) {
        ca = (int)*pa++;
        cb = (int)*pb++;
        if (nocase) {
            ca = rlv_sort_tolower(ca);
            cb = rlv_sort_tolower(cb);
        }
        if (ca != cb) {
            return ca - cb;
        }
        if (ca == 0) {
            return 0;
        }
    }
}

/*
 * Parse complete source cell text. Empty/NULL → EMPTY. Malformed or
 * overflow → INVALID. Leading whitespace skipped; trailing junk → INVALID.
 * Unsigned rejects a leading minus. No floating point / locale.
 */
static VOID rlv_sort_parse_number(CONST_STRPTR text,
                                  BOOL allow_sign,
                                  RLV_NumKey *out)
{
    const unsigned char *p;
    ULONG mag;
    ULONG digit;
    BOOL neg;
    BOOL any;

    out->class = (UBYTE)RLV_NUM_EMPTY;
    out->neg = 0;
    out->mag = 0;
    if (text == 0 || *text == '\0') {
        return;
    }
    p = (const unsigned char *)text;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    if (*p == '\0') {
        return;
    }
    neg = FALSE;
    if (allow_sign && (*p == '-' || *p == '+')) {
        if (*p == '-') {
            neg = TRUE;
        }
        p++;
        if (*p == '\0') {
            out->class = (UBYTE)RLV_NUM_INVALID;
            return;
        }
    } else if (!allow_sign && *p == '-') {
        out->class = (UBYTE)RLV_NUM_INVALID;
        return;
    }
    mag = 0;
    any = FALSE;
    while (*p >= '0' && *p <= '9') {
        digit = (ULONG)(*p - '0');
        if (mag > (0xFFFFFFFFUL - digit) / 10UL) {
            out->class = (UBYTE)RLV_NUM_INVALID;
            return;
        }
        mag = mag * 10UL + digit;
        any = TRUE;
        p++;
    }
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    if (!any || *p != '\0') {
        out->class = (UBYTE)RLV_NUM_INVALID;
        return;
    }
    if (neg && mag == 0) {
        neg = FALSE;
    }
    out->class = (UBYTE)RLV_NUM_OK;
    out->neg = neg ? (UBYTE)1 : (UBYTE)0;
    out->mag = mag;
}

static int rlv_sort_cmp_numkeys(const RLV_NumKey *a, const RLV_NumKey *b)
{
    LONG sa;
    LONG sb;

    if (a->class != b->class) {
        /* OK < EMPTY < INVALID for ascending. */
        return (int)a->class - (int)b->class;
    }
    if (a->class != (UBYTE)RLV_NUM_OK) {
        return 0;
    }
    /* Signed compare via signs + magnitudes. */
    if (a->neg != b->neg) {
        return a->neg ? -1 : 1;
    }
    if (a->mag < b->mag) {
        sa = -1;
    } else if (a->mag > b->mag) {
        sa = 1;
    } else {
        sa = 0;
    }
    if (a->neg) {
        sa = -sa;
    }
    sb = sa;
    return (int)sb;
}

static UBYTE rlv_sort_bool_value(const RLV_Control *c,
                                 ULONG source,
                                 UWORD column)
{
    ULONG idx;

    if (c == 0 || c->cell_snapshot == 0) {
        return (UBYTE)RLV_CELL_UNCHECKED;
    }
    idx = source * (ULONG)c->column_count + (ULONG)column;
    if (idx >= c->cell_snapshot_count) {
        return (UBYTE)RLV_CELL_UNCHECKED;
    }
    return c->cell_snapshot[idx].value;
}

/*
 * Ascending primary compare for two source rows. Equal → 0 (caller keeps
 * prior relative order via stable merge).
 */
static int rlv_sort_compare_primary(const RLV_Control *c,
                                    ULONG src_a,
                                    ULONG src_b,
                                    const RLV_SortSpec *spec)
{
    CONST_STRPTR ta;
    CONST_STRPTR tb;
    RLV_NumKey ka;
    RLV_NumKey kb;
    UBYTE ba;
    UBYTE bb;
    LONG custom;

    if (spec == 0) {
        return 0;
    }
    switch (spec->kind) {
    case RLV_SORT_TEXT_NOCASE:
        ta = rlv_sort_cell_text(c, src_a, spec->column);
        tb = rlv_sort_cell_text(c, src_b, spec->column);
        return rlv_sort_strcmp_case(ta, tb, TRUE);
    case RLV_SORT_TEXT_CASE:
        ta = rlv_sort_cell_text(c, src_a, spec->column);
        tb = rlv_sort_cell_text(c, src_b, spec->column);
        return rlv_sort_strcmp_case(ta, tb, FALSE);
    case RLV_SORT_SIGNED:
        rlv_sort_parse_number(rlv_sort_cell_text(c, src_a, spec->column),
                              TRUE, &ka);
        rlv_sort_parse_number(rlv_sort_cell_text(c, src_b, spec->column),
                              TRUE, &kb);
        return rlv_sort_cmp_numkeys(&ka, &kb);
    case RLV_SORT_UNSIGNED:
        rlv_sort_parse_number(rlv_sort_cell_text(c, src_a, spec->column),
                              FALSE, &ka);
        rlv_sort_parse_number(rlv_sort_cell_text(c, src_b, spec->column),
                              FALSE, &kb);
        return rlv_sort_cmp_numkeys(&ka, &kb);
    case RLV_SORT_BOOLEAN:
        ba = rlv_sort_bool_value(c, src_a, spec->column);
        bb = rlv_sort_bool_value(c, src_b, spec->column);
        /* Ascending: unchecked (false) before checked (true). */
        if (ba < bb) {
            return -1;
        }
        if (ba > bb) {
            return 1;
        }
        return 0;
    case RLV_SORT_CUSTOM:
        if (spec->compare == 0) {
            RLV_LOG("FAIL SORT CUSTOM compare is NULL");
            return 0;
        }
        custom = (*spec->compare)(c, src_a, src_b, spec->column,
                                  spec->context);
        if (custom < 0) {
            return -1;
        }
        if (custom > 0) {
            return 1;
        }
        return 0;
    default:
        return 0;
    }
}

/* Stable iterative bottom-up merge of view_to_source[lo..hi). */
static BOOL rlv_sort_merge_range(RLV_Control *c,
                                 ULONG lo,
                                 ULONG hi,
                                 const RLV_SortSpec *spec,
                                 UWORD direction,
                                 UWORD *scratch)
{
    ULONG n;
    ULONG width;
    ULONG i;
    ULONG left;
    ULONG mid;
    ULONG right;
    ULONG p;
    ULONG q;
    ULONG k;
    ULONG src_a;
    ULONG src_b;
    int cmp;

    if (c == 0 || c->view_to_source == 0 || scratch == 0 || hi <= lo + 1) {
        return TRUE;
    }
    n = hi - lo;
    for (width = 1; width < n; width <<= 1) {
        for (i = 0; i < n; i += (width << 1)) {
            left = lo + i;
            mid = left + width;
            right = left + (width << 1);
            if (mid > hi) {
                mid = hi;
            }
            if (right > hi) {
                right = hi;
            }
            p = left;
            q = mid;
            k = 0;
            while (p < mid && q < right) {
                src_a = (ULONG)c->view_to_source[p];
                src_b = (ULONG)c->view_to_source[q];
                cmp = rlv_sort_compare_primary(c, src_a, src_b, spec);
                /*
                 * Reverse sense for DESC during merge — do not sort ASC
                 * then reverse the map (that would break equal-key
                 * stability). Flip only the sign test; never negate the
                 * raw int (INT_MIN / LONG_MIN are unrepresentable).
                 * Comparators may return any <0 / 0 / >0 value.
                 */
                if (direction == (UWORD)RLV_SORT_DESC) {
                    if (cmp < 0) {
                        cmp = 1;
                    } else if (cmp > 0) {
                        cmp = -1;
                    }
                }
                /* Stable: on equal, take left (earlier) first. */
                if (cmp <= 0) {
                    scratch[k++] = c->view_to_source[p++];
                } else {
                    scratch[k++] = c->view_to_source[q++];
                }
            }
            while (p < mid) {
                scratch[k++] = c->view_to_source[p++];
            }
            while (q < right) {
                scratch[k++] = c->view_to_source[q++];
            }
            for (p = 0; p < k; p++) {
                c->view_to_source[left + p] = scratch[p];
            }
        }
    }
    return TRUE;
}

static LONG rlv_sort_top_source(const RLV_Control *c)
{
    ULONG v;
    LONG content_y;

    if (c == 0 || c->layout_rows == 0 || c->row_count == 0) {
        return -1;
    }
    content_y = c->scroll_y;
    for (v = 0; v < c->row_count; v++) {
        if (content_y >= c->layout_rows[v].top_y
            && content_y < c->layout_rows[v].top_y
                              + (LONG)c->layout_rows[v].content_height) {
            return (LONG)c->layout_rows[v].logical_index;
        }
        if (content_y < c->layout_rows[v].top_y) {
            return (LONG)c->layout_rows[v].logical_index;
        }
    }
    return (LONG)c->layout_rows[c->row_count - 1].logical_index;
}

static BOOL rlv_sort_apply(RLV_Control *c,
                           UWORD column,
                           UWORD direction,
                           BOOL emit_event,
                           RLV_Event *result)
{
    const RLV_SortSpec *spec;
    UWORD *scratch;
    ULONG n;
    ULONG i;
    ULONG run_lo;
    LONG old_sel;
    LONG top_src;
    LONG view;
    size_t scratch_bytes;

    if (c == 0) {
        return FALSE;
    }
    if (direction != (UWORD)RLV_SORT_ASC
        && direction != (UWORD)RLV_SORT_DESC) {
        return FALSE;
    }
    if (c->column_count == 0 || column >= c->column_count) {
        RLV_LOGF("FAIL SORT invalid column=%u", (unsigned)column);
        return FALSE;
    }
    spec = rlv_sort_find_spec(c, column);
    if (spec == 0) {
        RLV_LOGF("FAIL SORT no spec for column=%u", (unsigned)column);
        return FALSE;
    }
    if (spec->kind == (UWORD)RLV_SORT_CUSTOM && spec->compare == 0) {
        RLV_LOG("FAIL SORT CUSTOM without compare");
        return FALSE;
    }

    n = c->row_count;
    RLV_LOGF("SORT begin col=%u dir=%u rows=%lu kind=%u",
             (unsigned)column, (unsigned)direction,
             (unsigned long)n, (unsigned)spec->kind);

    old_sel = c->selected_row;
    top_src = -1;
    if (c->layout_valid && c->layout_rows != 0) {
        top_src = rlv_sort_top_source(c);
    }

    if (n == 0) {
        c->sort_column = column;
        c->sort_direction = direction;
        c->sort_active = 1;
        if (emit_event && result != 0) {
            result->type = (UWORD)RLV_EVENT_SORT_CHANGED;
            result->column = column;
            result->value = (LONG)direction;
            rlv_event_set_row(c, result, c->selected_row);
        }
        return TRUE;
    }

    if (!rlv_sort_ensure_maps(c)) {
        return FALSE;
    }

    scratch_bytes = (size_t)n * sizeof(UWORD);
    scratch = (UWORD *)rlv_platform_malloc(scratch_bytes);
    if (scratch == 0) {
        RLV_LOG("FAIL SORT scratch allocation");
        /* Previous view order left intact. */
        return FALSE;
    }
    RLV_LOGF("SORT scratch bytes=%lu", (unsigned long)scratch_bytes);

    /*
     * Sort each contiguous non-barrier view run. Barrier slots
     * (RLV_ROW_SORT_FIXED sources) stay put. Existing view order is the
     * stable baseline (attachment order on first map alloc).
     */
    run_lo = 0;
    for (i = 0; i <= n; i++) {
        if (i == n
            || rlv_sort_row_is_barrier(c,
                   (ULONG)c->view_to_source[i])) {
            if (i > run_lo) {
                RLV_LOGF("SORT run view[%lu,%lu)",
                         (unsigned long)run_lo, (unsigned long)i);
                if (!rlv_sort_merge_range(c, run_lo, i, spec, direction,
                                          scratch)) {
                    rlv_platform_free(scratch);
                    return FALSE;
                }
            }
            run_lo = i + 1;
        }
    }

    rlv_sort_rebuild_inverse(c);
    rlv_platform_free(scratch);

    c->sort_column = column;
    c->sort_direction = direction;
    c->sort_active = 1;

#if defined(RLV_ENABLE_LOGGING)
    {
        ULONG v;

        RLV_LOGF("SORT map n=%lu col=%u dir=%u",
                 (unsigned long)n, (unsigned)column, (unsigned)direction);
        for (v = 0; v < n && v < 32; v++) {
            ULONG src;
            CONST_STRPTR name;
            CONST_STRPTR date;

            src = (ULONG)c->view_to_source[v];
            name = "-";
            date = "-";
            if (c->rows != 0 && src < c->row_count
                && c->rows[src].cells != 0) {
                if (c->rows[src].cells[1] != 0) {
                    name = c->rows[src].cells[1];
                }
                if (c->rows[src].cells[2] != 0) {
                    date = c->rows[src].cells[2];
                }
            }
            RLV_LOGF("SORT view[%lu]=src[%lu] name=%s date=%s",
                     (unsigned long)v, (unsigned long)src, name, date);
        }
    }
#endif

    rlv_layout_invalidate(c);
    if (!rlv_layout_rebuild(c)) {
        RLV_LOG("FAIL SORT layout rebuild");
        return FALSE;
    }

    /* Restore top source row as viewport top when possible. */
    if (top_src >= 0 && c->layout_rows != 0) {
        view = rlv_view_for_source(c, top_src);
        if (view >= 0 && (ULONG)view < c->row_count) {
            rlv_set_scroll_y(c, c->layout_rows[view].top_y);
        }
    }
    /* Selection is source-stable; ensure visible if policy would. */
    if (old_sel >= 0) {
        c->selected_row = old_sel;
        rlv_make_visible(c, old_sel);
    }

    RLV_LOGF("SORT end col=%u dir=%u sel=%ld top_src=%ld",
             (unsigned)column, (unsigned)direction,
             (long)c->selected_row, (long)top_src);

    if (emit_event && result != 0) {
        result->type = (UWORD)RLV_EVENT_SORT_CHANGED;
        result->column = column;
        result->value = (LONG)direction;
        rlv_event_set_row(c, result, c->selected_row);
        result->previous_row = old_sel;
    }
    return TRUE;
}

BOOL rlv_set_sort_specs(RLV_Control *c,
                        const RLV_SortSpec *specs,
                        UWORD count)
{
    if (c == 0) {
        return FALSE;
    }
    if (count > 0 && specs == 0) {
        return FALSE;
    }
    c->sort_specs = specs;
    c->sort_spec_count = count;
    RLV_LOGF("SORT set_specs count=%u", (unsigned)count);
    return TRUE;
}

BOOL rlv_sort(RLV_Control *c, UWORD column, UWORD direction)
{
    return rlv_sort_apply(c, column, direction, FALSE, 0);
}

BOOL rlv_get_sort_state(const RLV_Control *c,
                        UWORD *column_out,
                        UWORD *direction_out)
{
    if (c == 0 || c->sort_active == 0) {
        return FALSE;
    }
    if (column_out != 0) {
        *column_out = c->sort_column;
    }
    if (direction_out != 0) {
        *direction_out = c->sort_direction;
    }
    return TRUE;
}

BOOL rlv_clear_sort(RLV_Control *c)
{
    LONG old_sel;
    LONG top_src;
    LONG view;

    if (c == 0) {
        return FALSE;
    }
    if (c->sort_active == 0 && c->view_to_source == 0) {
        return TRUE;
    }

    old_sel = c->selected_row;
    top_src = -1;
    if (c->layout_valid && c->layout_rows != 0 && c->row_count > 0) {
        top_src = rlv_sort_top_source(c);
    }

    if (c->row_count > 0) {
        if (!rlv_sort_ensure_maps(c)) {
            return FALSE;
        }
        rlv_sort_fill_identity(c);
    } else {
        rlv_sort_free_maps(c);
    }
    c->sort_active = 0;
    c->sort_column = 0;
    c->sort_direction = (UWORD)RLV_SORT_ASC;
    rlv_layout_invalidate(c);
    if (c->row_count > 0 && !rlv_layout_rebuild(c)) {
        return FALSE;
    }

    /* Preserve top source / selection like rlv_sort; no SORT_CHANGED. */
    if (top_src >= 0 && c->layout_rows != 0) {
        view = rlv_view_for_source(c, top_src);
        if (view >= 0 && (ULONG)view < c->row_count) {
            rlv_set_scroll_y(c, c->layout_rows[view].top_y);
        }
    }
    if (old_sel >= 0) {
        c->selected_row = old_sel;
        rlv_make_visible(c, old_sel);
    }

    RLV_LOG("SORT cleared to attachment order");
    return TRUE;
}

LONG rlv_source_row_of(const RLV_Control *c, LONG view_row)
{
    if (c == 0 || view_row < 0 || (ULONG)view_row >= c->row_count) {
        return -1;
    }
    return (LONG)rlv_source_for_view(c, (ULONG)view_row);
}

LONG rlv_view_row_of(const RLV_Control *c, LONG source_row)
{
    if (c == 0 || source_row < 0 || (ULONG)source_row >= c->row_count) {
        return -1;
    }
    return rlv_view_for_source(c, source_row);
}

UWORD rlv_sort_header_reserve_px(const RLV_Control *c, UWORD column)
{
    if (c == 0 || c->sort_active == 0) {
        return 0;
    }
    if (column != c->sort_column) {
        return 0;
    }
    return (UWORD)(RLV_SORT_INDICATOR_W
                   + RLV_SORT_INDICATOR_GAP
                   + RLV_SORT_INDICATOR_EDGE);
}

VOID rlv_sort_draw_indicator(RLV_Control *c,
                             UWORD column,
                             WORD cell_left,
                             WORD cell_right,
                             WORD header_top,
                             WORD header_bottom)
{
    const RLV_DrawOps *ops;
    APTR ctx;
    WORD cx;
    WORD cy;
    WORD x0;
    WORD y0;
    WORD i;

    if (c == 0 || c->draw_ops == 0 || c->sort_active == 0) {
        return;
    }
    if (column != c->sort_column) {
        return;
    }
    if (cell_right < cell_left
        + (WORD)(RLV_SORT_INDICATOR_W + RLV_SORT_INDICATOR_EDGE)) {
        return;
    }

    ops = c->draw_ops;
    ctx = c->draw_context;
    /* Centre in the reserved band; leave EDGE pixels clear of the divider. */
    cx = (WORD)(cell_right
                - (WORD)RLV_SORT_INDICATOR_EDGE
                - (WORD)(RLV_SORT_INDICATOR_W / 2));
    if (cx < cell_left + 2) {
        cx = (WORD)(cell_left + 2);
    }
    cy = (WORD)(header_top
                + ((header_bottom - header_top) / 2));

    ops->set_pens(ctx, c->pens.text, c->pens.background);

    if (c->sort_direction == (UWORD)RLV_SORT_ASC) {
        /* Up-pointing triangle (filled via horizontal spans). */
        for (i = 0; i < 4; i++) {
            x0 = (WORD)(cx - i);
            y0 = (WORD)(cy - 2 + i);
            if (y0 < header_top || y0 > header_bottom) {
                continue;
            }
            ops->fill_rect(ctx, x0, y0, (WORD)(cx + i), y0);
        }
    } else {
        for (i = 0; i < 4; i++) {
            x0 = (WORD)(cx - i);
            y0 = (WORD)(cy + 2 - i);
            if (y0 < header_top || y0 > header_bottom) {
                continue;
            }
            ops->fill_rect(ctx, x0, y0, (WORD)(cx + i), y0);
        }
    }
}

BOOL rlv_sort_handle_header_click(RLV_Control *c,
                                  WORD x,
                                  WORD y,
                                  RLV_Event *result)
{
    UWORD col;
    UWORD dir;
    const RLV_SortSpec *spec;

    if (c == 0 || c->col_geom == 0 || c->column_count == 0) {
        return FALSE;
    }
    if (y < c->header_bounds.MinY || y > c->header_bounds.MaxY
        || x < c->header_bounds.MinX || x > c->header_bounds.MaxX) {
        return FALSE;
    }
    if (c->sort_specs == 0 || c->sort_spec_count == 0) {
        return FALSE;
    }

    for (col = 0; col < c->column_count; col++) {
        if (x >= c->col_geom[col].left && x <= c->col_geom[col].right) {
            break;
        }
    }
    if (col >= c->column_count) {
        return FALSE;
    }

    spec = rlv_sort_find_spec(c, col);
    if (spec == 0) {
        /*
         * Deliberate: consume non-sortable header hits so row selection
         * under the header cannot fire. No event today — a future
         * RLV_EVENT_HEADER_CLICK (or return-unhandled) may let apps
         * filter / configure columns without racing selection.
         */
        RLV_LOGF("SORT header click col=%u (not sortable)", (unsigned)col);
        return TRUE;
    }

    if (c->sort_active != 0 && c->sort_column == col) {
        dir = (c->sort_direction == (UWORD)RLV_SORT_ASC)
              ? (UWORD)RLV_SORT_DESC
              : (UWORD)RLV_SORT_ASC;
    } else if ((spec->flags & RLV_SORT_F_DEFAULT_DESC) != 0) {
        dir = (UWORD)RLV_SORT_DESC;
    } else {
        dir = (UWORD)RLV_SORT_ASC;
    }

    RLV_LOGF("SORT header click col=%u dir=%u", (unsigned)col,
             (unsigned)dir);
    if (!rlv_sort_apply(c, col, dir, TRUE, result)) {
        if (result != 0) {
            result->type = (UWORD)RLV_EVENT_NONE;
        }
        return TRUE;
    }
    return TRUE;
}

#endif /* RLV_ENABLE_SORTING */
