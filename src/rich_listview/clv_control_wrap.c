/**
 * Control pixel wrap prepare — Phase 3.
 *
 * Copy/adapt of the wrap algorithm from clv_pixel_wrap.c without linking
 * v1 wrap/renderer modules. No icons, styles, or continuation guides.
 * Fragment text pointers borrow into application cell strings.
 */

#include "rich_listview/clv_control_internal.h"
#include "rich_listview/clv_platform_internal.h"

#include <string.h>

static BOOL clv_ctrl_is_path_break(char c)
{
    return (c == '/' || c == '\\' || c == ':' || c == '_' || c == '-'
            || c == ' ' || c == '\t');
}

static UWORD clv_ctrl_find_pixel_break(const char *s,
                                       UWORD fit_len,
                                       UWORD mode)
{
    UWORD i;
    UWORD best_space = 0;
    UWORD best_path = 0;

    if (s == 0 || fit_len == 0) {
        return 0;
    }

    if (mode == CLV_CTRL_WRAP_NONE) {
        return fit_len;
    }

    for (i = 1; i <= fit_len && s[i - 1] != '\0'; i++) {
        char c = s[i - 1];

        if (c == ' ' || c == '\t') {
            best_space = i;
        }
        if (mode == CLV_CTRL_WRAP_PATH && clv_ctrl_is_path_break(c)) {
            best_path = i;
        }
    }

    if (mode == CLV_CTRL_WRAP_PATH) {
        if (best_path > 0) {
            return best_path;
        }
        if (best_space > 0) {
            return best_space;
        }
        return fit_len;
    }

    /* WORD / WORD_OR_CHAR */
    if (best_space > 0) {
        return best_space;
    }
    return fit_len;
}

static UWORD clv_ctrl_strlen(CONST_STRPTR s)
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

static UWORD clv_ctrl_fit_chars(CLV_Control *c,
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
        return c->draw_ops->text_fit(c->draw_context, text, length,
                                     max_width);
    }

    if (c->draw_ops->text_width == 0) {
        return 0;
    }

    if (c->draw_ops->text_width(c->draw_context, text, length)
        <= max_width) {
        return length;
    }

    lo = 0;
    hi = length;
    while (lo < hi) {
        mid = (UWORD)((lo + hi + 1) / 2);
        w = c->draw_ops->text_width(c->draw_context, text, mid);
        if (w <= max_width) {
            lo = mid;
        } else {
            hi = (UWORD)(mid - 1);
        }
    }
    return lo;
}

static WORD clv_ctrl_align_x(UWORD alignment,
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
    case CLV_CELL_ALIGN_RIGHT:
        x = (WORD)(text_right - (WORD)text_w + 1);
        break;
    case CLV_CELL_ALIGN_CENTER:
        x = (WORD)(text_left + ((span - (WORD)text_w) / 2));
        break;
    case CLV_CELL_ALIGN_LEFT:
    default:
        x = text_left;
        break;
    }
    if (x < text_left) {
        x = text_left;
    }
    return x;
}

static VOID clv_ctrl_free_cell_frags(CLV_ControlCellWrap *cell)
{
    if (cell == 0) {
        return;
    }
    if (cell->frags != 0) {
        clv_platform_free(cell->frags);
        cell->frags = 0;
    }
    cell->frag_count = 0;
}

VOID clv_control_layout_free_wraps(CLV_Control *c)
{
    ULONG i;

    if (c == 0) {
        return;
    }
    if (c->cell_wraps != 0) {
        for (i = 0; i < c->cell_wrap_count; i++) {
            clv_ctrl_free_cell_frags(&c->cell_wraps[i]);
        }
        clv_platform_free(c->cell_wraps);
        c->cell_wraps = 0;
    }
    c->cell_wrap_count = 0;
}

static BOOL clv_ctrl_wrap_cell(CLV_Control *c,
                               CONST_STRPTR src,
                               const CLV_PixelColumn *col,
                               UWORD alignment,
                               UWORD wrap_mode,
                               CLV_ControlCellWrap *out)
{
    const CLV_DrawOps *ops;
    APTR ctx;
    WORD fit_width;
    WORD text_left;
    WORD text_right;
    CLV_ControlFrag stack_frags[CLV_CTRL_MAX_FRAGS_PER_CELL];
    UWORD count;
    const char *p;
    WORD first_x;

    if (c == 0 || col == 0 || out == 0 || c->draw_ops == 0) {
        return FALSE;
    }

    CLV_BENCH_COUNT(CLV_BENCH_COUNTER_CELLS_PREPARED);

    ops = c->draw_ops;
    ctx = c->draw_context;
    out->frags = 0;
    out->frag_count = 0;

    if (src == 0) {
        src = "";
    }

    text_left = col->text_left;
    text_right = col->text_right;
    fit_width = (WORD)(text_right - text_left + 1);
    if (fit_width < 1) {
        fit_width = 0;
    }

    count = 0;
    first_x = text_left;

    /* NONE: single truncated fragment. */
    if (wrap_mode == CLV_CTRL_WRAP_NONE) {
        UWORD full_len = clv_ctrl_strlen(src);
        UWORD fitted;
        UWORD w;

        fitted = clv_ctrl_fit_chars(c, src, full_len, (UWORD)fit_width);
        w = 0;
        if (fitted > 0 && ops->text_width != 0) {
            w = ops->text_width(ctx, src, fitted);
        }
        stack_frags[0].text = src;
        stack_frags[0].length = fitted;
        stack_frags[0].width_pixels = w;
        stack_frags[0].relative_x = clv_ctrl_align_x(alignment,
                                                     text_left,
                                                     text_right,
                                                     w);
        count = 1;
    } else {
        p = (const char *)src;

        while (*p != '\0' && count < CLV_CTRL_MAX_FRAGS_PER_CELL) {
            const char *seg_start;
            UWORD seg_len;
            UWORD fitted;
            UWORD take;
            UWORD w;
            WORD x;

            /* Explicit newlines force a fragment boundary. */
            if (*p == '\n' || *p == '\r') {
                if (*p == '\r' && p[1] == '\n') {
                    p += 2;
                } else {
                    p++;
                }
                stack_frags[count].text = p; /* empty; length 0 */
                stack_frags[count].length = 0;
                stack_frags[count].width_pixels = 0;
                if (count == 0) {
                    stack_frags[count].relative_x =
                        clv_ctrl_align_x(alignment, text_left, text_right, 0);
                    first_x = stack_frags[count].relative_x;
                } else if (alignment != CLV_CELL_ALIGN_LEFT) {
                    stack_frags[count].relative_x = first_x;
                } else {
                    stack_frags[count].relative_x = text_left;
                }
                count++;
                continue;
            }

            seg_start = p;
            seg_len = 0;
            while (p[seg_len] != '\0' && p[seg_len] != '\n'
                   && p[seg_len] != '\r') {
                seg_len++;
            }

            if (fit_width < 1) {
                fitted = (seg_len > 0) ? 1 : 0;
            } else {
                CLV_BENCH_COUNT(CLV_BENCH_COUNTER_WRAP_DECISIONS);
                fitted = clv_ctrl_fit_chars(c, (CONST_STRPTR)seg_start,
                                            seg_len, (UWORD)fit_width);
            }

            if (fitted >= seg_len) {
                take = seg_len;
            } else {
                if (fitted == 0) {
                    fitted = 1;
                }
                take = clv_ctrl_find_pixel_break(seg_start, fitted,
                                                 wrap_mode);
                if (take == 0) {
                    take = fitted;
                }
                if (take > seg_len) {
                    take = seg_len;
                }
            }

            w = 0;
            if (take > 0 && ops->text_width != 0) {
                w = ops->text_width(ctx, (CONST_STRPTR)seg_start, take);
            }

            if (count == 0) {
                x = clv_ctrl_align_x(alignment, text_left, text_right, w);
                first_x = x;
            } else if (alignment != CLV_CELL_ALIGN_LEFT) {
                x = first_x;
            } else {
                x = text_left;
            }

            stack_frags[count].text = (CONST_STRPTR)seg_start;
            stack_frags[count].length = take;
            stack_frags[count].width_pixels = w;
            stack_frags[count].relative_x = x;
            count++;

            p = seg_start + take;
            /* Skip a single leading space after a word break. */
            if (*p == ' ' || *p == '\t') {
                p++;
            }
        }

        if (count == 0) {
            /* Empty string still occupies one line for row height. */
            stack_frags[0].text = src;
            stack_frags[0].length = 0;
            stack_frags[0].width_pixels = 0;
            stack_frags[0].relative_x =
                clv_ctrl_align_x(alignment, text_left, text_right, 0);
            count = 1;
        }
    }

    out->frags = (CLV_ControlFrag *)clv_platform_malloc(
        (size_t)count * sizeof(CLV_ControlFrag));
    if (out->frags == 0) {
        return FALSE;
    }
    memcpy(out->frags, stack_frags, (size_t)count * sizeof(CLV_ControlFrag));
    out->frag_count = count;
    return TRUE;
}

BOOL clv_control_wrap_prepare(CLV_Control *c)
{
    ULONG row;
    UWORD col;
    ULONG idx;
    ULONG total;
    CONST_STRPTR cell;
    UWORD mode;

    if (c == 0) {
        return FALSE;
    }

    {
        CLV_BENCH_DECLARE(bench_wrap);
        CLV_BENCH_BEGIN(CLV_BENCH_PIXEL_WRAP, bench_wrap);

        clv_control_layout_free_wraps(c);

        if (c->row_count == 0 || c->column_count == 0) {
            CLV_BENCH_END(CLV_BENCH_PIXEL_WRAP, bench_wrap);
            return TRUE;
        }

        total = c->row_count * (ULONG)c->column_count;
        c->cell_wraps = (CLV_ControlCellWrap *)clv_platform_malloc(
            (size_t)total * sizeof(CLV_ControlCellWrap));
        if (c->cell_wraps == 0) {
            CLV_BENCH_END(CLV_BENCH_PIXEL_WRAP, bench_wrap);
            return FALSE;
        }
        memset(c->cell_wraps, 0, (size_t)total * sizeof(CLV_ControlCellWrap));
        c->cell_wrap_count = total;

        for (row = 0; row < c->row_count; row++) {
            for (col = 0; col < c->column_count; col++) {
                idx = row * (ULONG)c->column_count + (ULONG)col;
                cell = 0;
                if (c->rows != 0 && c->rows[row].cells != 0) {
                    cell = c->rows[row].cells[col];
                }
                mode = CLV_CTRL_WRAP_NONE;
                if (c->columns != 0) {
                    mode = c->columns[col].wrap_mode;
                }

                if (!clv_ctrl_wrap_cell(c, cell, &c->col_geom[col],
                                        c->col_geom[col].alignment,
                                        mode, &c->cell_wraps[idx])) {
                    clv_control_layout_free_wraps(c);
                    CLV_BENCH_END(CLV_BENCH_PIXEL_WRAP, bench_wrap);
                    return FALSE;
                }
            }
        }

        CLV_BENCH_END(CLV_BENCH_PIXEL_WRAP, bench_wrap);
    }
    return TRUE;
}
