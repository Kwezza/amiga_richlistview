/**
 * Adaptive body-row divider pen policy (darker derivative of alternate backdrop).
 * Resolve outside paint; only rlv_draw_row_divider consumes the result.
 */

#include "rich_listview/rlv_internal.h"
#include "rich_listview/rich_listview.h"
#include "rich_listview/rlv_log.h"

#if defined(RLV_ENABLE_ADAPTIVE_DIVIDERS) && (RLV_ENABLE_ADAPTIVE_DIVIDERS != 0)
#include "rich_listview/backends/rlv_adaptive_colour.h"
#endif

UWORD rlv_row_divider_pen_normalize(UWORD mode)
{
#if defined(RLV_ENABLE_ADAPTIVE_DIVIDERS) && (RLV_ENABLE_ADAPTIVE_DIVIDERS != 0)
    if (mode == (UWORD)RLV_ROW_DIVIDER_PEN_ADAPTIVE) {
        return mode;
    }
#else
    if (mode == (UWORD)RLV_ROW_DIVIDER_PEN_ADAPTIVE) {
        return (UWORD)RLV_ROW_DIVIDER_PEN_SYSTEM;
    }
#endif
    return (UWORD)RLV_ROW_DIVIDER_PEN_SYSTEM;
}

#if defined(RLV_ENABLE_ADAPTIVE_DIVIDERS) && (RLV_ENABLE_ADAPTIVE_DIVIDERS != 0)

/* ~78% alternate (or bg) + ~22% SHADOWPEN. */
#define RLV_ADAPTIVE_DIV_SOURCE_PERCENT  78
#define RLV_ADAPTIVE_DIV_SHADOW_PERCENT  (100 - RLV_ADAPTIVE_DIV_SOURCE_PERCENT)
#define RLV_ADAPTIVE_DIV_MIN_BG_LUMA_DELTA  8
#define RLV_ADAPTIVE_DIV_MIN_ALT_LUMA_DELTA  6
#define RLV_ADAPTIVE_DIV_MIN_SEL_LUMA_DELTA  8
#define RLV_ADAPTIVE_DIV_MAX_NEAR_BLACK_LUMA  12

typedef struct RLV_AdaptiveDivValidateData
{
    const RLV_Pens *pens;
    UWORD source_pen;
    UWORD alt_pen; /* RLV_ADAPTIVE_AVOID_NONE if none */
    ULONG bg_luma;
    ULONG alt_luma;
    ULONG sel_luma;
    BOOL have_alt_luma;
    BOOL have_sel_luma;
} RLV_AdaptiveDivValidateData;

static UWORD rlv_adaptive_divider_source_pen(const RLV_Control *c)
{
#if defined(RLV_ENABLE_ALTERNATE_ROWS) && (RLV_ENABLE_ALTERNATE_ROWS != 0)
    if (c->row_backdrop_effective == (UWORD)RLV_ROW_BACKDROP_ALTERNATE_PEN
        && c->alternate_row_pen != c->pens.background) {
        return c->alternate_row_pen;
    }
#else
    (void)c;
#endif
    return c->pens.background;
}

static BOOL rlv_adaptive_divider_validate(
    const RLV_AdaptiveColourCtx *ctx,
    const RLV_AdaptiveColourSample *candidate,
    ULONG target_r,
    ULONG target_g,
    ULONG target_b,
    APTR user_data)
{
    RLV_AdaptiveDivValidateData *vd;
    ULONG dist_sq;
    ULONG delta;

    (void)ctx;
    vd = (RLV_AdaptiveDivValidateData *)user_data;
    if (candidate == 0 || vd == 0 || vd->pens == 0) {
        return FALSE;
    }

    RLV_LOGF("ROW_DIVIDER adaptive candidate pen=%u rgb=%lu,%lu,%lu "
             "target=%lu,%lu,%lu luma=%lu",
             (unsigned)candidate->pen,
             (unsigned long)candidate->r, (unsigned long)candidate->g,
             (unsigned long)candidate->b,
             (unsigned long)target_r, (unsigned long)target_g,
             (unsigned long)target_b,
             (unsigned long)candidate->luma);

    if (candidate->pen == vd->pens->background) {
        RLV_LOG("ROW_DIVIDER adaptive reject reason=same_as_background");
        return FALSE;
    }
    if (candidate->pen == vd->pens->separator) {
        RLV_LOG("ROW_DIVIDER adaptive reject reason=same_as_separator");
        return FALSE;
    }
    if (vd->alt_pen != RLV_ADAPTIVE_AVOID_NONE
        && candidate->pen == vd->alt_pen) {
        RLV_LOG("ROW_DIVIDER adaptive reject reason=same_as_alternate");
        return FALSE;
    }

    delta = rlv_adaptive_colour_luma_delta(candidate->luma, vd->bg_luma);
    if (delta < (ULONG)RLV_ADAPTIVE_DIV_MIN_BG_LUMA_DELTA) {
        RLV_LOGF("ROW_DIVIDER adaptive reject reason=luma_near_bg "
                 "delta=%lu min=%u",
                 (unsigned long)delta,
                 (unsigned)RLV_ADAPTIVE_DIV_MIN_BG_LUMA_DELTA);
        return FALSE;
    }

    if (vd->have_alt_luma) {
        delta = rlv_adaptive_colour_luma_delta(candidate->luma, vd->alt_luma);
        if (delta < (ULONG)RLV_ADAPTIVE_DIV_MIN_ALT_LUMA_DELTA) {
            RLV_LOGF("ROW_DIVIDER adaptive reject reason=luma_near_alt "
                     "delta=%lu min=%u",
                     (unsigned long)delta,
                     (unsigned)RLV_ADAPTIVE_DIV_MIN_ALT_LUMA_DELTA);
            return FALSE;
        }
    }

    if (candidate->luma <= (ULONG)RLV_ADAPTIVE_DIV_MAX_NEAR_BLACK_LUMA) {
        RLV_LOGF("ROW_DIVIDER adaptive reject reason=near_black luma=%lu",
                 (unsigned long)candidate->luma);
        return FALSE;
    }

    if (vd->have_sel_luma) {
        delta = rlv_adaptive_colour_luma_delta(candidate->luma, vd->sel_luma);
        if (delta < (ULONG)RLV_ADAPTIVE_DIV_MIN_SEL_LUMA_DELTA) {
            RLV_LOGF("ROW_DIVIDER adaptive reject reason=near_selection "
                     "delta=%lu min=%u",
                     (unsigned long)delta,
                     (unsigned)RLV_ADAPTIVE_DIV_MIN_SEL_LUMA_DELTA);
            return FALSE;
        }
    }

    dist_sq = rlv_adaptive_colour_rgb_dist_sq(
        candidate->r, candidate->g, candidate->b,
        target_r, target_g, target_b);
    if (dist_sq > RLV_ADAPTIVE_DEFAULT_MAX_TARGET_DIST_SQ) {
        RLV_LOGF("ROW_DIVIDER adaptive reject reason=far_from_target "
                 "dist_sq=%lu max=%lu",
                 (unsigned long)dist_sq,
                 (unsigned long)RLV_ADAPTIVE_DEFAULT_MAX_TARGET_DIST_SQ);
        return FALSE;
    }

    RLV_LOGF("ROW_DIVIDER adaptive accept pen=%u luma=%lu dist_sq=%lu",
             (unsigned)candidate->pen,
             (unsigned long)candidate->luma,
             (unsigned long)dist_sq);
    return TRUE;
}

static BOOL rlv_adaptive_divider_pen_acquire(RLV_Control *c, UWORD *out_pen)
{
    RLV_AdaptiveColourCtx ctx;
    RLV_AdaptiveDivValidateData vd;
    ULONG src_r;
    ULONG src_g;
    ULONG src_b;
    ULONG sh_r;
    ULONG sh_g;
    ULONG sh_b;
    ULONG sel_r;
    ULONG sel_g;
    ULONG sel_b;
    ULONG target_r;
    ULONG target_g;
    ULONG target_b;
    UWORD source_pen;
    UWORD sel_pen;

    if (c == 0 || out_pen == 0) {
        return FALSE;
    }
    if (!rlv_adaptive_colour_begin(c->draw_context, "ROW_DIVIDER", &ctx)) {
        return FALSE;
    }

    source_pen = rlv_adaptive_divider_source_pen(c);
    vd.pens = &c->pens;
    vd.source_pen = source_pen;
    vd.alt_pen = RLV_ADAPTIVE_AVOID_NONE;
    vd.have_alt_luma = FALSE;
    vd.have_sel_luma = FALSE;

#if defined(RLV_ENABLE_ALTERNATE_ROWS) && (RLV_ENABLE_ALTERNATE_ROWS != 0)
    if (c->row_backdrop_effective == (UWORD)RLV_ROW_BACKDROP_ALTERNATE_PEN
        && c->alternate_row_pen != c->pens.background) {
        vd.alt_pen = c->alternate_row_pen;
    }
#endif

    if (!rlv_adaptive_colour_read_pen(&ctx, (LONG)c->pens.background,
                                      &src_r, &src_g, &src_b)) {
        RLV_LOG("ROW_DIVIDER adaptive fail (background RGB)");
        return FALSE;
    }
    vd.bg_luma = rlv_adaptive_colour_luma(src_r, src_g, src_b);

    if (!rlv_adaptive_colour_read_pen(&ctx, (LONG)source_pen,
                                      &src_r, &src_g, &src_b)) {
        RLV_LOG("ROW_DIVIDER adaptive fail (source RGB)");
        return FALSE;
    }
    if (vd.alt_pen != RLV_ADAPTIVE_AVOID_NONE) {
        ULONG ar;
        ULONG ag;
        ULONG ab;

        if (rlv_adaptive_colour_read_pen(&ctx, (LONG)vd.alt_pen,
                                         &ar, &ag, &ab)) {
            vd.alt_luma = rlv_adaptive_colour_luma(ar, ag, ab);
            vd.have_alt_luma = TRUE;
        }
    }

    if (!rlv_adaptive_colour_read_pen(&ctx, (LONG)c->pens.shadow,
                                      &sh_r, &sh_g, &sh_b)) {
        RLV_LOG("ROW_DIVIDER adaptive fail (shadow RGB)");
        return FALSE;
    }

    sel_pen = rlv_selection_fill_pen(c);
    if (rlv_adaptive_colour_read_pen(&ctx, (LONG)sel_pen,
                                     &sel_r, &sel_g, &sel_b)) {
        vd.sel_luma = rlv_adaptive_colour_luma(sel_r, sel_g, sel_b);
        vd.have_sel_luma = TRUE;
    }

    rlv_adaptive_colour_blend(src_r, src_g, src_b,
                              (UBYTE)RLV_ADAPTIVE_DIV_SOURCE_PERCENT,
                              sh_r, sh_g, sh_b,
                              (UBYTE)RLV_ADAPTIVE_DIV_SHADOW_PERCENT,
                              &target_r, &target_g, &target_b);

    RLV_LOGF("ROW_DIVIDER adaptive begin source_pen=%u shadow_pen=%u "
             "target=%lu,%lu,%lu blend=%u/%u",
             (unsigned)source_pen,
             (unsigned)c->pens.shadow,
             (unsigned long)target_r, (unsigned long)target_g,
             (unsigned long)target_b,
             (unsigned)RLV_ADAPTIVE_DIV_SOURCE_PERCENT,
             (unsigned)RLV_ADAPTIVE_DIV_SHADOW_PERCENT);

    return rlv_adaptive_colour_resolve(&ctx, "ROW_DIVIDER",
                                       target_r, target_g, target_b,
                                       rlv_adaptive_divider_validate, &vd,
                                       out_pen);
}

static VOID rlv_adaptive_divider_release_owned(RLV_Control *c)
{
    if (c == 0 || !c->row_divider_pen_owned) {
        return;
    }
    rlv_adaptive_colour_release(c->draw_context, c->row_divider_pen);
    c->row_divider_pen_owned = FALSE;
    c->row_divider_pen = 0;
}

static VOID rlv_adaptive_divider_set_system(RLV_Control *c,
                                            CONST_STRPTR reason)
{
    c->row_divider_pen_effective = (UWORD)RLV_ROW_DIVIDER_PEN_SYSTEM;
    c->row_divider_pen = c->pens.separator;
    if (reason != 0) {
        RLV_LOGF("ROW_DIVIDER adaptive unavailable (%s); effective SYSTEM",
                 reason);
    } else {
        RLV_LOG("ROW_DIVIDER adaptive unavailable; effective SYSTEM");
    }
}

VOID rlv_adaptive_divider_refresh(RLV_Control *c)
{
    UWORD requested;
    UWORD acquired;

    if (c == 0) {
        return;
    }

    rlv_adaptive_divider_release_owned(c);
    requested = rlv_row_divider_pen_normalize(c->row_divider_pen_requested);
    c->row_divider_pen_requested = requested;
    c->row_divider_pen_effective = (UWORD)RLV_ROW_DIVIDER_PEN_SYSTEM;
    c->row_divider_pen = c->pens.separator;

    if (requested != (UWORD)RLV_ROW_DIVIDER_PEN_ADAPTIVE) {
        RLV_LOGF("ROW_DIVIDER effective=%u pen=%u",
                 (unsigned)requested,
                 (unsigned)c->row_divider_pen);
        return;
    }

    acquired = 0;
    if (rlv_adaptive_divider_pen_acquire(c, &acquired)) {
        c->row_divider_pen = acquired;
        c->row_divider_pen_owned = TRUE;
        c->row_divider_pen_effective = (UWORD)RLV_ROW_DIVIDER_PEN_ADAPTIVE;
        RLV_LOGF("ROW_DIVIDER adaptive ok pen=%u", (unsigned)acquired);
    } else {
        rlv_adaptive_divider_set_system(c, "acquire_failed");
    }
}

VOID rlv_adaptive_divider_init_from_config(RLV_Control *c, UWORD mode)
{
    if (c == 0) {
        return;
    }
    c->row_divider_pen_requested = rlv_row_divider_pen_normalize(mode);
    c->row_divider_pen_effective = (UWORD)RLV_ROW_DIVIDER_PEN_SYSTEM;
    c->row_divider_pen = 0;
    c->row_divider_pen_owned = FALSE;
}

VOID rlv_adaptive_divider_teardown(RLV_Control *c)
{
    rlv_adaptive_divider_release_owned(c);
}

UWORD rlv_row_divider_pen_effective_mode(const RLV_Control *c)
{
    if (c == 0) {
        return (UWORD)RLV_ROW_DIVIDER_PEN_SYSTEM;
    }
    return c->row_divider_pen_effective;
}

UWORD rlv_row_divider_pen(const RLV_Control *c)
{
    if (c == 0) {
        return 0;
    }
    if (c->row_divider_pen_owned) {
        return c->row_divider_pen;
    }
    return c->pens.separator;
}

#endif /* RLV_ENABLE_ADAPTIVE_DIVIDERS */
