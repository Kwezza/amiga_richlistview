/**
 * Optional alternating-row backdrop mode resolution and pen/pattern paint.
 */

#include "rich_listview/rlv_alternate_rows.h"
#include "rich_listview/rlv_internal.h"
#include "rich_listview/rlv_log.h"
#if defined(RLV_ENABLE_ADAPTIVE_ROW_PEN) && (RLV_ENABLE_ADAPTIVE_ROW_PEN != 0)
#include "rich_listview/backends/rlv_adaptive_colour.h"
#endif

#if defined(RLV_ENABLE_ALTERNATE_ROWS) && (RLV_ENABLE_ALTERNATE_ROWS != 0)

/*
 * Sparse FILLPEN stipple on BACKGROUNDPEN (same density as title sparse
 * blue stipple). Phase shifts between rows avoid continuous lines.
 */
#define RLV_ROW_STIPPLE_HEIGHT_EXP 2U

#if defined(RLV_ENABLE_ADAPTIVE_ROW_PEN) && (RLV_ENABLE_ADAPTIVE_ROW_PEN != 0)
/* Target darkness relative to the mapped background RGB (integer percent). */
#define RLV_ADAPTIVE_ROW_DARKEN_PERCENT  10
#define RLV_ADAPTIVE_ROW_MIN_LUMA_DELTA  10
#define RLV_ADAPTIVE_ROW_MIN_SEMANTIC_LUMA_DELTA  8
#define RLV_ADAPTIVE_ROW_MIN_TEXT_LUMA_DELTA  24

typedef struct RLV_AdaptiveRowValidateData
{
    const RLV_Pens *pens;
    ULONG bg_r;
    ULONG bg_g;
    ULONG bg_b;
    ULONG bg_luma;
} RLV_AdaptiveRowValidateData;

static CONST_STRPTR rlv_adaptive_row_semantic_name(UWORD index)
{
    switch (index) {
    case 0:
        return "text";
    case 1:
        return "filltext";
    case 2:
        return "fill";
    case 3:
        return "shine";
    case 4:
        return "shadow";
    default:
        return "?";
    }
}

static BOOL rlv_adaptive_row_validate(
    const RLV_AdaptiveColourCtx *ctx,
    const RLV_AdaptiveColourSample *candidate,
    ULONG target_r,
    ULONG target_g,
    ULONG target_b,
    APTR user_data)
{
    RLV_AdaptiveRowValidateData *vd;
    ULONG luma_delta;
    ULONG dist_sq;
    ULONG text_r;
    ULONG text_g;
    ULONG text_b;
    ULONG text_luma;
    UWORD semantic_pens[5];
    UWORD i;

    vd = (RLV_AdaptiveRowValidateData *)user_data;
    if (ctx == 0 || candidate == 0 || vd == 0 || vd->pens == 0) {
        return FALSE;
    }

    RLV_LOGF("ROW_BACKDROP adaptive candidate pen=%u rgb=%lu,%lu,%lu "
             "target=%lu,%lu,%lu bg=%lu,%lu,%lu",
             (unsigned)candidate->pen,
             (unsigned long)candidate->r, (unsigned long)candidate->g,
             (unsigned long)candidate->b,
             (unsigned long)target_r, (unsigned long)target_g,
             (unsigned long)target_b,
             (unsigned long)vd->bg_r, (unsigned long)vd->bg_g,
             (unsigned long)vd->bg_b);

    if (candidate->pen == vd->pens->background) {
        RLV_LOG("ROW_BACKDROP adaptive reject reason=same_as_background");
        return FALSE;
    }

    luma_delta = rlv_adaptive_colour_luma_delta(candidate->luma, vd->bg_luma);
    if (luma_delta < (ULONG)RLV_ADAPTIVE_ROW_MIN_LUMA_DELTA) {
        RLV_LOGF("ROW_BACKDROP adaptive reject reason=luma_too_close "
                 "delta=%lu min=%u alt_luma=%lu bg_luma=%lu",
                 (unsigned long)luma_delta,
                 (unsigned)RLV_ADAPTIVE_ROW_MIN_LUMA_DELTA,
                 (unsigned long)candidate->luma,
                 (unsigned long)vd->bg_luma);
        return FALSE;
    }

    dist_sq = rlv_adaptive_colour_rgb_dist_sq(
        candidate->r, candidate->g, candidate->b,
        target_r, target_g, target_b);
    if (dist_sq > RLV_ADAPTIVE_DEFAULT_MAX_TARGET_DIST_SQ) {
        RLV_LOGF("ROW_BACKDROP adaptive reject reason=far_from_target "
                 "dist_sq=%lu max=%lu",
                 (unsigned long)dist_sq,
                 (unsigned long)RLV_ADAPTIVE_DEFAULT_MAX_TARGET_DIST_SQ);
        return FALSE;
    }

    semantic_pens[0] = vd->pens->text;
    semantic_pens[1] = vd->pens->selected_text;
    semantic_pens[2] = vd->pens->selected_background;
    semantic_pens[3] = vd->pens->shine;
    semantic_pens[4] = vd->pens->shadow;
    for (i = 0; i < (UWORD)(sizeof(semantic_pens) / sizeof(semantic_pens[0]));
         i++) {
        ULONG sr;
        ULONG sg;
        ULONG sb;
        ULONG sem_luma;
        ULONG sem_delta;

        if (semantic_pens[i] == candidate->pen) {
            RLV_LOGF("ROW_BACKDROP adaptive reject reason=matches_semantic "
                     "role=%s pen=%u",
                     rlv_adaptive_row_semantic_name(i),
                     (unsigned)candidate->pen);
            return FALSE;
        }
        if (!rlv_adaptive_colour_read_pen(ctx, (LONG)semantic_pens[i],
                                          &sr, &sg, &sb)) {
            continue;
        }
        sem_luma = rlv_adaptive_colour_luma(sr, sg, sb);
        sem_delta = rlv_adaptive_colour_luma_delta(candidate->luma, sem_luma);
        if (sem_delta < (ULONG)RLV_ADAPTIVE_ROW_MIN_SEMANTIC_LUMA_DELTA) {
            RLV_LOGF("ROW_BACKDROP adaptive reject reason=near_semantic "
                     "role=%s pen=%u luma_delta=%lu min=%u",
                     rlv_adaptive_row_semantic_name(i),
                     (unsigned)semantic_pens[i],
                     (unsigned long)sem_delta,
                     (unsigned)RLV_ADAPTIVE_ROW_MIN_SEMANTIC_LUMA_DELTA);
            return FALSE;
        }
    }

    if (!rlv_adaptive_colour_read_pen(ctx, (LONG)vd->pens->text,
                                      &text_r, &text_g, &text_b)) {
        RLV_LOG("ROW_BACKDROP adaptive reject reason=text_rgb_fail");
        return FALSE;
    }
    text_luma = rlv_adaptive_colour_luma(text_r, text_g, text_b);
    luma_delta = rlv_adaptive_colour_luma_delta(text_luma, candidate->luma);
    if (luma_delta < (ULONG)RLV_ADAPTIVE_ROW_MIN_TEXT_LUMA_DELTA) {
        RLV_LOGF("ROW_BACKDROP adaptive reject reason=text_contrast "
                 "delta=%lu min=%u text_luma=%lu alt_luma=%lu",
                 (unsigned long)luma_delta,
                 (unsigned)RLV_ADAPTIVE_ROW_MIN_TEXT_LUMA_DELTA,
                 (unsigned long)text_luma,
                 (unsigned long)candidate->luma);
        return FALSE;
    }

    RLV_LOGF("ROW_BACKDROP adaptive accept pen=%u rgb=%lu,%lu,%lu "
             "alt_luma=%lu bg_luma=%lu dist_sq=%lu",
             (unsigned)candidate->pen,
             (unsigned long)candidate->r, (unsigned long)candidate->g,
             (unsigned long)candidate->b,
             (unsigned long)candidate->luma,
             (unsigned long)vd->bg_luma,
             (unsigned long)dist_sq);
    return TRUE;
}

static BOOL rlv_adaptive_row_pen_acquire(APTR draw_context,
                                         const RLV_Pens *pens,
                                         UWORD *out_pen)
{
    RLV_AdaptiveColourCtx ctx;
    RLV_AdaptiveRowValidateData vd;
    ULONG target_r;
    ULONG target_g;
    ULONG target_b;

    if (out_pen == 0 || pens == 0) {
        return FALSE;
    }
    if (!rlv_adaptive_colour_begin(draw_context, "ROW_BACKDROP", &ctx)) {
        return FALSE;
    }
    if (!rlv_adaptive_colour_read_pen(&ctx, (LONG)pens->background,
                                      &vd.bg_r, &vd.bg_g, &vd.bg_b)) {
        RLV_LOG("ROW_BACKDROP adaptive fail (background RGB)");
        return FALSE;
    }
    vd.pens = pens;
    vd.bg_luma = rlv_adaptive_colour_luma(vd.bg_r, vd.bg_g, vd.bg_b);
    rlv_adaptive_colour_darken(vd.bg_r, vd.bg_g, vd.bg_b,
                               (UBYTE)RLV_ADAPTIVE_ROW_DARKEN_PERCENT,
                               &target_r, &target_g, &target_b);

    RLV_LOGF("ROW_BACKDROP adaptive begin bg_pen=%u rgb=%lu,%lu,%lu "
             "target=%lu,%lu,%lu darken=%u%%",
             (unsigned)pens->background,
             (unsigned long)vd.bg_r, (unsigned long)vd.bg_g,
             (unsigned long)vd.bg_b,
             (unsigned long)target_r, (unsigned long)target_g,
             (unsigned long)target_b,
             (unsigned)RLV_ADAPTIVE_ROW_DARKEN_PERCENT);

    return rlv_adaptive_colour_resolve(&ctx, "ROW_BACKDROP",
                                       target_r, target_g, target_b,
                                       rlv_adaptive_row_validate, &vd,
                                       out_pen);
}
#endif /* RLV_ENABLE_ADAPTIVE_ROW_PEN */

static const UWORD g_rlv_row_stipple_pattern[] =
{
    0x8888U,
    0x2222U,
    0x4444U,
    0x1111U
};

static VOID rlv_alternate_rows_release_owned(RLV_Control *c)
{
    if (c == 0) {
        return;
    }
    if (!c->alternate_pen_owned) {
        return;
    }
#if defined(RLV_ENABLE_ADAPTIVE_ROW_PEN) && (RLV_ENABLE_ADAPTIVE_ROW_PEN != 0)
    rlv_adaptive_colour_release(c->draw_context, c->alternate_row_pen);
#endif
    c->alternate_pen_owned = FALSE;
    c->alternate_row_pen = 0;
}

static VOID rlv_alternate_rows_set_pattern_fallback(RLV_Control *c,
                                                   CONST_STRPTR reason)
{
    c->row_backdrop_effective =
        (UWORD)RLV_ROW_BACKDROP_ALTERNATE_PATTERN;
    if (reason != 0) {
        RLV_LOGF("ROW_BACKDROP adaptive unavailable (%s); "
                 "effective ALTERNATE_PATTERN",
                 reason);
    } else {
        RLV_LOG("ROW_BACKDROP adaptive unavailable; "
                "effective ALTERNATE_PATTERN");
    }
}

UWORD rlv_alternate_rows_normalize_mode(UWORD mode)
{
    if (mode == (UWORD)RLV_ROW_BACKDROP_ALTERNATE_PEN
        || mode == (UWORD)RLV_ROW_BACKDROP_ADAPTIVE
        || mode == (UWORD)RLV_ROW_BACKDROP_ALTERNATE_PATTERN) {
        return mode;
    }
    return (UWORD)RLV_ROW_BACKDROP_STANDARD;
}

BOOL rlv_alternate_rows_caller_pen_valid(const RLV_Control *c, UWORD pen)
{
    if (c == 0) {
        return FALSE;
    }
    if (pen == c->pens.background) {
        return FALSE;
    }
    return TRUE;
}

VOID rlv_alternate_rows_refresh(RLV_Control *c)
{
    UWORD mode;

    if (c == 0) {
        return;
    }

    rlv_alternate_rows_release_owned(c);
    c->row_backdrop_effective = (UWORD)RLV_ROW_BACKDROP_STANDARD;
    c->alternate_row_pen = 0;

    mode = rlv_alternate_rows_normalize_mode(c->row_backdrop_requested);

    if (mode == (UWORD)RLV_ROW_BACKDROP_ALTERNATE_PEN) {
        if (rlv_alternate_rows_caller_pen_valid(c, c->caller_alternate_pen)) {
            c->alternate_row_pen = c->caller_alternate_pen;
            c->row_backdrop_effective =
                (UWORD)RLV_ROW_BACKDROP_ALTERNATE_PEN;
            RLV_LOGF("ROW_BACKDROP alternate pen=%u",
                     (unsigned)c->alternate_row_pen);
        } else {
            RLV_LOG("ROW_BACKDROP alternate pen invalid; effective STANDARD");
        }
        return;
    }

    if (mode == (UWORD)RLV_ROW_BACKDROP_ALTERNATE_PATTERN) {
        c->row_backdrop_effective =
            (UWORD)RLV_ROW_BACKDROP_ALTERNATE_PATTERN;
        RLV_LOG("ROW_BACKDROP effective ALTERNATE_PATTERN");
        return;
    }

    if (mode == (UWORD)RLV_ROW_BACKDROP_ADAPTIVE) {
#if defined(RLV_ENABLE_ADAPTIVE_ROW_PEN) && (RLV_ENABLE_ADAPTIVE_ROW_PEN != 0)
        {
            UWORD acquired;

            acquired = 0;
            if (rlv_adaptive_row_pen_acquire(c->draw_context,
                                             &c->pens,
                                             &acquired)) {
                c->alternate_row_pen = acquired;
                c->alternate_pen_owned = TRUE;
                c->row_backdrop_effective =
                    (UWORD)RLV_ROW_BACKDROP_ALTERNATE_PEN;
                RLV_LOGF("ROW_BACKDROP adaptive ok pen=%u",
                         (unsigned)acquired);
            } else {
                rlv_alternate_rows_set_pattern_fallback(c, "acquire_failed");
            }
        }
#else
        rlv_alternate_rows_set_pattern_fallback(c, "not_compiled");
#endif
        return;
    }
}

VOID rlv_alternate_rows_init_from_config(RLV_Control *c,
                                        UWORD requested_mode,
                                        UWORD caller_alternate_pen)
{
    if (c == 0) {
        return;
    }
    c->row_backdrop_requested =
        rlv_alternate_rows_normalize_mode(requested_mode);
    c->caller_alternate_pen = caller_alternate_pen;
    c->row_backdrop_effective = (UWORD)RLV_ROW_BACKDROP_STANDARD;
    c->alternate_row_pen = 0;
    c->alternate_pen_owned = FALSE;
}

VOID rlv_alternate_rows_teardown(RLV_Control *c)
{
    rlv_alternate_rows_release_owned(c);
}

VOID rlv_alternate_rows_set_mode(RLV_Control *c,
                                 UWORD mode,
                                 UWORD caller_alternate_pen)
{
    if (c == 0) {
        return;
    }
    c->row_backdrop_requested = rlv_alternate_rows_normalize_mode(mode);
    c->caller_alternate_pen = caller_alternate_pen;
    rlv_alternate_rows_refresh(c);
}

UWORD rlv_row_normal_backdrop_pen(const RLV_Control *c, LONG logical_row)
{
    if (c == 0) {
        return 0;
    }
    if (c->row_backdrop_effective != (UWORD)RLV_ROW_BACKDROP_ALTERNATE_PEN) {
        return c->pens.background;
    }
    if (logical_row >= 0 && ((ULONG)logical_row & 1UL) != 0UL) {
        return c->alternate_row_pen;
    }
    return c->pens.background;
}

BOOL rlv_row_uses_pattern_backdrop(const RLV_Control *c, LONG logical_row)
{
    if (c == 0 || logical_row < 0) {
        return FALSE;
    }
    if (c->row_backdrop_effective
        != (UWORD)RLV_ROW_BACKDROP_ALTERNATE_PATTERN) {
        return FALSE;
    }
    return (((ULONG)logical_row & 1UL) != 0UL) ? TRUE : FALSE;
}

VOID rlv_row_fill_normal_backdrop(RLV_Control *c,
                                  WORD x1, WORD y1, WORD x2, WORD y2,
                                  LONG logical_row)
{
    const RLV_DrawOps *ops;
    APTR ctx;
    UWORD pen;

    if (c == 0 || c->draw_ops == 0 || c->draw_ops->fill_rect == 0) {
        return;
    }
    if (x2 < x1 || y2 < y1) {
        return;
    }

    ops = c->draw_ops;
    ctx = c->draw_context;

    if (rlv_row_uses_pattern_backdrop(c, logical_row)
        && ops->fill_rect_pattern != 0) {
        ops->fill_rect_pattern(ctx, x1, y1, x2, y2,
                               c->pens.selected_background,
                               c->pens.background,
                               g_rlv_row_stipple_pattern,
                               RLV_ROW_STIPPLE_HEIGHT_EXP);
        return;
    }

    pen = rlv_row_normal_backdrop_pen(c, logical_row);
    ops->set_pens(ctx, pen, pen);
    ops->fill_rect(ctx, x1, y1, x2, y2);
}

#endif /* RLV_ENABLE_ALTERNATE_ROWS */
