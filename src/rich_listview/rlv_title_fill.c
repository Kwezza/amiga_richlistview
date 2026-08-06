/**
 * Predefined title-row (header) fill patterns and fill helper.
 * Descriptor table is extensible for a future custom-pattern API.
 * Optional adaptive blend pen resolves outside the paint hot path.
 */

#include "rich_listview/rlv_internal.h"
#include "rich_listview/rlv_log.h"
#if defined(RLV_ENABLE_ADAPTIVE_TITLE_PEN) && (RLV_ENABLE_ADAPTIVE_TITLE_PEN != 0)
#include "rich_listview/backends/rlv_adaptive_colour.h"
#endif

/* Semantic pen roles resolved through the control's RLV_Pens snapshot. */
#define RLV_TITLE_PEN_BACKGROUND  0U
#define RLV_TITLE_PEN_FILL        1U /* DrawInfo FILLPEN / highlight */
#define RLV_TITLE_PEN_SHINE       2U /* DrawInfo SHINEPEN */

/*
 * Dense alternating vertical stripes (1-pixel columns) for classic
 * four-colour Workbench. Bits in 0xAAAA select APen; cleared bits BPen.
 */
#define RLV_TITLE_STRIPE_PATTERN   0xAAAAU
#define RLV_TITLE_STRIPE_HEIGHT_EXP 0U /* SetAfPt: 2^0 = one pattern row */

/*
 * 2-row blue/grey checkerboard. Row 0 starts with blue; row 1 is phase-
 * reversed so adjacent rows alternate the foreground/background pixels.
 */
#define RLV_TITLE_CHECKER_HEIGHT_EXP 1U

/*
 * Sparse blue stipple on grey. Each row has 4 blue pixels out of 16 and the
 * phase shifts between rows to avoid continuous vertical or horizontal lines.
 */
#define RLV_TITLE_STIPPLE_HEIGHT_EXP 2U

/*
 * Grey-dominant wider vertical stripes: one blue column for every three grey
 * columns. One row is enough because the intended texture is vertical.
 */
#define RLV_TITLE_WIDE_STRIPE_PATTERN 0x1111U
#define RLV_TITLE_WIDE_STRIPE_HEIGHT_EXP 0U

/* Classic patterned fallback when adaptive acquisition fails. */
#define RLV_TITLE_FILL_ADAPTIVE_FALLBACK \
    ((UWORD)RLV_TITLE_FILL_GREY_BLUE_STRIPES)

#if defined(RLV_ENABLE_ADAPTIVE_TITLE_PEN) && (RLV_ENABLE_ADAPTIVE_TITLE_PEN != 0)
#define RLV_ADAPTIVE_TITLE_FILL_PERCENT  45
#define RLV_ADAPTIVE_TITLE_BG_PERCENT    (100 - RLV_ADAPTIVE_TITLE_FILL_PERCENT)
#define RLV_ADAPTIVE_TITLE_MIN_BG_LUMA_DELTA  10
#define RLV_ADAPTIVE_TITLE_MIN_FILL_LUMA_DELTA  8
#define RLV_ADAPTIVE_TITLE_MIN_SEMANTIC_LUMA_DELTA  8
#define RLV_ADAPTIVE_TITLE_MIN_TEXT_LUMA_DELTA  24

typedef struct RLV_AdaptiveTitleValidateData
{
    const RLV_Pens *pens;
    ULONG bg_r;
    ULONG bg_g;
    ULONG bg_b;
    ULONG bg_luma;
    ULONG fill_luma;
} RLV_AdaptiveTitleValidateData;

static BOOL rlv_adaptive_title_validate(
    const RLV_AdaptiveColourCtx *ctx,
    const RLV_AdaptiveColourSample *candidate,
    ULONG target_r,
    ULONG target_g,
    ULONG target_b,
    APTR user_data)
{
    RLV_AdaptiveTitleValidateData *vd;
    ULONG luma_delta;
    ULONG dist_sq;
    ULONG text_r;
    ULONG text_g;
    ULONG text_b;
    ULONG text_luma;
    ULONG shine_r;
    ULONG shine_g;
    ULONG shine_b;
    ULONG shadow_r;
    ULONG shadow_g;
    ULONG shadow_b;
    ULONG shine_luma;
    ULONG shadow_luma;

    vd = (RLV_AdaptiveTitleValidateData *)user_data;
    if (ctx == 0 || candidate == 0 || vd == 0 || vd->pens == 0) {
        return FALSE;
    }

    RLV_LOGF("TITLE_FILL adaptive candidate pen=%u rgb=%lu,%lu,%lu "
             "target=%lu,%lu,%lu bg=%lu,%lu,%lu",
             (unsigned)candidate->pen,
             (unsigned long)candidate->r, (unsigned long)candidate->g,
             (unsigned long)candidate->b,
             (unsigned long)target_r, (unsigned long)target_g,
             (unsigned long)target_b,
             (unsigned long)vd->bg_r, (unsigned long)vd->bg_g,
             (unsigned long)vd->bg_b);

    if (candidate->pen == vd->pens->background) {
        RLV_LOG("TITLE_FILL adaptive reject reason=same_as_background");
        return FALSE;
    }
    if (candidate->pen == vd->pens->selected_background) {
        RLV_LOG("TITLE_FILL adaptive reject reason=same_as_fillpen");
        return FALSE;
    }

    luma_delta = rlv_adaptive_colour_luma_delta(candidate->luma, vd->bg_luma);
    if (luma_delta < (ULONG)RLV_ADAPTIVE_TITLE_MIN_BG_LUMA_DELTA) {
        RLV_LOGF("TITLE_FILL adaptive reject reason=luma_near_bg "
                 "delta=%lu min=%u",
                 (unsigned long)luma_delta,
                 (unsigned)RLV_ADAPTIVE_TITLE_MIN_BG_LUMA_DELTA);
        return FALSE;
    }

    luma_delta = rlv_adaptive_colour_luma_delta(candidate->luma, vd->fill_luma);
    if (luma_delta < (ULONG)RLV_ADAPTIVE_TITLE_MIN_FILL_LUMA_DELTA) {
        RLV_LOGF("TITLE_FILL adaptive reject reason=luma_near_fill "
                 "delta=%lu min=%u",
                 (unsigned long)luma_delta,
                 (unsigned)RLV_ADAPTIVE_TITLE_MIN_FILL_LUMA_DELTA);
        return FALSE;
    }

    dist_sq = rlv_adaptive_colour_rgb_dist_sq(
        candidate->r, candidate->g, candidate->b,
        target_r, target_g, target_b);
    if (dist_sq > RLV_ADAPTIVE_DEFAULT_MAX_TARGET_DIST_SQ) {
        RLV_LOGF("TITLE_FILL adaptive reject reason=far_from_target "
                 "dist_sq=%lu max=%lu",
                 (unsigned long)dist_sq,
                 (unsigned long)RLV_ADAPTIVE_DEFAULT_MAX_TARGET_DIST_SQ);
        return FALSE;
    }

    if (rlv_adaptive_colour_read_pen(ctx, (LONG)vd->pens->shine,
                                     &shine_r, &shine_g, &shine_b)) {
        shine_luma = rlv_adaptive_colour_luma(shine_r, shine_g, shine_b);
        if (rlv_adaptive_colour_luma_delta(candidate->luma, shine_luma)
            < (ULONG)RLV_ADAPTIVE_TITLE_MIN_SEMANTIC_LUMA_DELTA) {
            RLV_LOG("TITLE_FILL adaptive reject reason=near_shine");
            return FALSE;
        }
    }
    if (rlv_adaptive_colour_read_pen(ctx, (LONG)vd->pens->shadow,
                                     &shadow_r, &shadow_g, &shadow_b)) {
        shadow_luma = rlv_adaptive_colour_luma(shadow_r, shadow_g, shadow_b);
        if (rlv_adaptive_colour_luma_delta(candidate->luma, shadow_luma)
            < (ULONG)RLV_ADAPTIVE_TITLE_MIN_SEMANTIC_LUMA_DELTA) {
            RLV_LOG("TITLE_FILL adaptive reject reason=near_shadow");
            return FALSE;
        }
    }

    if (!rlv_adaptive_colour_read_pen(ctx, (LONG)vd->pens->text,
                                      &text_r, &text_g, &text_b)) {
        RLV_LOG("TITLE_FILL adaptive reject reason=text_rgb_fail");
        return FALSE;
    }
    text_luma = rlv_adaptive_colour_luma(text_r, text_g, text_b);
    luma_delta = rlv_adaptive_colour_luma_delta(text_luma, candidate->luma);
    if (luma_delta < (ULONG)RLV_ADAPTIVE_TITLE_MIN_TEXT_LUMA_DELTA) {
        RLV_LOGF("TITLE_FILL adaptive reject reason=text_contrast "
                 "delta=%lu min=%u text_luma=%lu alt_luma=%lu",
                 (unsigned long)luma_delta,
                 (unsigned)RLV_ADAPTIVE_TITLE_MIN_TEXT_LUMA_DELTA,
                 (unsigned long)text_luma,
                 (unsigned long)candidate->luma);
        return FALSE;
    }

    RLV_LOGF("TITLE_FILL adaptive accept pen=%u rgb=%lu,%lu,%lu "
             "alt_luma=%lu bg_luma=%lu fill_luma=%lu dist_sq=%lu",
             (unsigned)candidate->pen,
             (unsigned long)candidate->r, (unsigned long)candidate->g,
             (unsigned long)candidate->b,
             (unsigned long)candidate->luma,
             (unsigned long)vd->bg_luma,
             (unsigned long)vd->fill_luma,
             (unsigned long)dist_sq);
    return TRUE;
}

static BOOL rlv_adaptive_title_pen_acquire(APTR draw_context,
                                           const RLV_Pens *pens,
                                           UWORD *out_pen)
{
    RLV_AdaptiveColourCtx ctx;
    RLV_AdaptiveTitleValidateData vd;
    ULONG fill_r;
    ULONG fill_g;
    ULONG fill_b;
    ULONG target_r;
    ULONG target_g;
    ULONG target_b;

    if (out_pen == 0 || pens == 0) {
        return FALSE;
    }
    if (!rlv_adaptive_colour_begin(draw_context, "TITLE_FILL", &ctx)) {
        return FALSE;
    }
    if (!rlv_adaptive_colour_read_pen(&ctx, (LONG)pens->background,
                                      &vd.bg_r, &vd.bg_g, &vd.bg_b)) {
        RLV_LOG("TITLE_FILL adaptive fail (background RGB)");
        return FALSE;
    }
    if (!rlv_adaptive_colour_read_pen(&ctx, (LONG)pens->selected_background,
                                      &fill_r, &fill_g, &fill_b)) {
        RLV_LOG("TITLE_FILL adaptive fail (FILLPEN RGB)");
        return FALSE;
    }
    vd.pens = pens;
    vd.bg_luma = rlv_adaptive_colour_luma(vd.bg_r, vd.bg_g, vd.bg_b);
    vd.fill_luma = rlv_adaptive_colour_luma(fill_r, fill_g, fill_b);
    rlv_adaptive_colour_blend(fill_r, fill_g, fill_b,
                              (UBYTE)RLV_ADAPTIVE_TITLE_FILL_PERCENT,
                              vd.bg_r, vd.bg_g, vd.bg_b,
                              (UBYTE)RLV_ADAPTIVE_TITLE_BG_PERCENT,
                              &target_r, &target_g, &target_b);

    RLV_LOGF("TITLE_FILL adaptive begin fill_pen=%u rgb=%lu,%lu,%lu "
             "bg_pen=%u rgb=%lu,%lu,%lu target=%lu,%lu,%lu blend=%u/%u",
             (unsigned)pens->selected_background,
             (unsigned long)fill_r, (unsigned long)fill_g,
             (unsigned long)fill_b,
             (unsigned)pens->background,
             (unsigned long)vd.bg_r, (unsigned long)vd.bg_g,
             (unsigned long)vd.bg_b,
             (unsigned long)target_r, (unsigned long)target_g,
             (unsigned long)target_b,
             (unsigned)RLV_ADAPTIVE_TITLE_FILL_PERCENT,
             (unsigned)RLV_ADAPTIVE_TITLE_BG_PERCENT);

    return rlv_adaptive_colour_resolve(&ctx, "TITLE_FILL",
                                       target_r, target_g, target_b,
                                       rlv_adaptive_title_validate, &vd,
                                       out_pen);
}
#endif /* RLV_ENABLE_ADAPTIVE_TITLE_PEN */

static const UWORD g_rlv_title_stripe_pattern[] =
{
    RLV_TITLE_STRIPE_PATTERN
};

static const UWORD g_rlv_title_checker_pattern[] =
{
    0xAAAAU,
    0x5555U
};

static const UWORD g_rlv_title_stipple_pattern[] =
{
    0x8888U,
    0x2222U,
    0x4444U,
    0x1111U
};

static const UWORD g_rlv_title_wide_stripe_pattern[] =
{
    RLV_TITLE_WIDE_STRIPE_PATTERN
};

typedef struct RLV_TitleFillDesc
{
    UWORD style;
    UWORD fg_role;
    UWORD bg_role;
    const UWORD *pattern;
    UWORD pat_height_exp;
    UBYTE solid;
} RLV_TitleFillDesc;

static const RLV_TitleFillDesc g_rlv_title_fill_table[] =
{
    {
        (UWORD)RLV_TITLE_FILL_SOLID,
        RLV_TITLE_PEN_BACKGROUND,
        RLV_TITLE_PEN_BACKGROUND,
        0,
        0U,
        1U
    },
    {
        (UWORD)RLV_TITLE_FILL_GREY_BLUE_STRIPES,
        RLV_TITLE_PEN_FILL,
        RLV_TITLE_PEN_BACKGROUND,
        g_rlv_title_stripe_pattern,
        RLV_TITLE_STRIPE_HEIGHT_EXP,
        0U
    },
    {
        (UWORD)RLV_TITLE_FILL_GREY_WHITE_STRIPES,
        RLV_TITLE_PEN_SHINE,
        RLV_TITLE_PEN_BACKGROUND,
        g_rlv_title_stripe_pattern,
        RLV_TITLE_STRIPE_HEIGHT_EXP,
        0U
    },
    {
        (UWORD)RLV_TITLE_FILL_BLUE_GREY_CHECKERBOARD,
        RLV_TITLE_PEN_FILL,
        RLV_TITLE_PEN_BACKGROUND,
        g_rlv_title_checker_pattern,
        RLV_TITLE_CHECKER_HEIGHT_EXP,
        0U
    },
    {
        (UWORD)RLV_TITLE_FILL_SPARSE_BLUE_STIPPLE,
        RLV_TITLE_PEN_FILL,
        RLV_TITLE_PEN_BACKGROUND,
        g_rlv_title_stipple_pattern,
        RLV_TITLE_STIPPLE_HEIGHT_EXP,
        0U
    },
    {
        (UWORD)RLV_TITLE_FILL_WIDE_GREY_BLUE_STRIPES,
        RLV_TITLE_PEN_FILL,
        RLV_TITLE_PEN_BACKGROUND,
        g_rlv_title_wide_stripe_pattern,
        RLV_TITLE_WIDE_STRIPE_HEIGHT_EXP,
        0U
    }
};

static const RLV_TitleFillDesc *rlv_title_fill_lookup(UWORD style)
{
    UWORD i;

    for (i = 0; i < (UWORD)(sizeof(g_rlv_title_fill_table)
                            / sizeof(g_rlv_title_fill_table[0])); i++) {
        if (g_rlv_title_fill_table[i].style == style) {
            return &g_rlv_title_fill_table[i];
        }
    }
    return &g_rlv_title_fill_table[0];
}

UWORD rlv_title_fill_normalize(UWORD style)
{
    UWORD i;

#if defined(RLV_ENABLE_ADAPTIVE_TITLE_PEN) && (RLV_ENABLE_ADAPTIVE_TITLE_PEN != 0)
    if (style == (UWORD)RLV_TITLE_FILL_ADAPTIVE_BLEND) {
        return style;
    }
#else
    if (style == (UWORD)RLV_TITLE_FILL_ADAPTIVE_BLEND) {
        /* Feature omitted: keep a classic patterned appearance. */
        return RLV_TITLE_FILL_ADAPTIVE_FALLBACK;
    }
#endif

    for (i = 0; i < (UWORD)(sizeof(g_rlv_title_fill_table)
                            / sizeof(g_rlv_title_fill_table[0])); i++) {
        if (g_rlv_title_fill_table[i].style == style) {
            return style;
        }
    }
    return (UWORD)RLV_TITLE_FILL_SOLID;
}

static UWORD rlv_title_pen_for_role(const RLV_Pens *pens, UWORD role)
{
    if (pens == 0) {
        return 0;
    }
    switch (role) {
    case RLV_TITLE_PEN_FILL:
        return pens->selected_background;
    case RLV_TITLE_PEN_SHINE:
        return pens->shine;
    case RLV_TITLE_PEN_BACKGROUND:
    default:
        return pens->background;
    }
}

#if defined(RLV_ENABLE_ADAPTIVE_TITLE_PEN) && (RLV_ENABLE_ADAPTIVE_TITLE_PEN != 0)

static VOID rlv_title_fill_release_owned(RLV_Control *c)
{
    if (c == 0 || !c->adaptive_title_pen_owned) {
        return;
    }
    rlv_adaptive_colour_release(c->draw_context, c->adaptive_title_pen);
    c->adaptive_title_pen_owned = FALSE;
    c->adaptive_title_pen = 0;
}

static VOID rlv_title_fill_set_pattern_fallback(RLV_Control *c,
                                                CONST_STRPTR reason)
{
    c->title_fill_effective = RLV_TITLE_FILL_ADAPTIVE_FALLBACK;
    if (reason != 0) {
        RLV_LOGF("TITLE_FILL adaptive unavailable (%s); effective GREY_BLUE",
                 reason);
    } else {
        RLV_LOG("TITLE_FILL adaptive unavailable; effective GREY_BLUE");
    }
}

VOID rlv_title_fill_refresh(RLV_Control *c)
{
    UWORD requested;

    if (c == 0) {
        return;
    }

    rlv_title_fill_release_owned(c);
    requested = rlv_title_fill_normalize(c->title_fill_style);
    c->title_fill_style = requested;
    c->title_fill_effective = requested;
    c->adaptive_title_pen = 0;

    if (requested != (UWORD)RLV_TITLE_FILL_ADAPTIVE_BLEND) {
        RLV_LOGF("TITLE_FILL effective=%u", (unsigned)requested);
        return;
    }

    {
        UWORD acquired;

        acquired = 0;
        if (rlv_adaptive_title_pen_acquire(c->draw_context,
                                           &c->pens,
                                           &acquired)) {
            c->adaptive_title_pen = acquired;
            c->adaptive_title_pen_owned = TRUE;
            /* Paint path treats owned adaptive as solid fill. */
            c->title_fill_effective = (UWORD)RLV_TITLE_FILL_SOLID;
            RLV_LOGF("TITLE_FILL adaptive ok pen=%u", (unsigned)acquired);
        } else {
            rlv_title_fill_set_pattern_fallback(c, "acquire_failed");
        }
    }
}

VOID rlv_title_fill_init_from_config(RLV_Control *c, UWORD style)
{
    if (c == 0) {
        return;
    }
    c->title_fill_style = rlv_title_fill_normalize(style);
    c->title_fill_effective = c->title_fill_style;
    if (c->title_fill_style == (UWORD)RLV_TITLE_FILL_ADAPTIVE_BLEND) {
        /* Resolve after pens / ColorMap are available (set_pens). */
        c->title_fill_effective = RLV_TITLE_FILL_ADAPTIVE_FALLBACK;
    }
    c->adaptive_title_pen = 0;
    c->adaptive_title_pen_owned = FALSE;
}

VOID rlv_title_fill_teardown(RLV_Control *c)
{
    rlv_title_fill_release_owned(c);
}

UWORD rlv_title_fill_effective_style(const RLV_Control *c)
{
    if (c == 0) {
        return (UWORD)RLV_TITLE_FILL_SOLID;
    }
    if (c->title_fill_style == (UWORD)RLV_TITLE_FILL_ADAPTIVE_BLEND
        && c->adaptive_title_pen_owned) {
        return (UWORD)RLV_TITLE_FILL_ADAPTIVE_BLEND;
    }
    return c->title_fill_effective;
}

#endif /* RLV_ENABLE_ADAPTIVE_TITLE_PEN */

VOID rlv_title_fill_area(RLV_Control *c,
                         WORD x1,
                         WORD y1,
                         WORD x2,
                         WORD y2)
{
    const RLV_DrawOps *ops;
    APTR ctx;
    const RLV_TitleFillDesc *desc;
    UWORD fg;
    UWORD bg;
    UWORD paint_style;

    if (c == 0 || c->draw_ops == 0 || x2 < x1 || y2 < y1) {
        return;
    }

    ops = c->draw_ops;
    ctx = c->draw_context;

#if defined(RLV_ENABLE_ADAPTIVE_TITLE_PEN) && (RLV_ENABLE_ADAPTIVE_TITLE_PEN != 0)
    if (c->title_fill_style == (UWORD)RLV_TITLE_FILL_ADAPTIVE_BLEND
        && c->adaptive_title_pen_owned) {
        bg = c->adaptive_title_pen;
        ops->set_pens(ctx, bg, bg);
        ops->fill_rect(ctx, x1, y1, x2, y2);
        return;
    }
    paint_style = c->title_fill_effective;
#else
    paint_style = c->title_fill_style;
#endif

    desc = rlv_title_fill_lookup(paint_style);

    if (desc->solid != 0) {
        bg = rlv_title_pen_for_role(&c->pens, RLV_TITLE_PEN_BACKGROUND);
        ops->set_pens(ctx, bg, bg);
        ops->fill_rect(ctx, x1, y1, x2, y2);
        return;
    }

    fg = rlv_title_pen_for_role(&c->pens, desc->fg_role);
    bg = rlv_title_pen_for_role(&c->pens, desc->bg_role);

    if (ops->fill_rect_pattern != 0 && desc->pattern != 0) {
        ops->fill_rect_pattern(ctx, x1, y1, x2, y2,
                               fg, bg, desc->pattern, desc->pat_height_exp);
        return;
    }

    /* Non-pattern backend: solid background fallback. */
    RLV_LOG("TITLE_FILL fallback solid (no fill_rect_pattern op)");
    ops->set_pens(ctx, bg, bg);
    ops->fill_rect(ctx, x1, y1, x2, y2);
}

BOOL rlv_title_fill_is_patterned(const RLV_Control *c)
{
    const RLV_TitleFillDesc *desc;
    UWORD paint_style;

    if (c == 0) {
        return FALSE;
    }
#if defined(RLV_ENABLE_ADAPTIVE_TITLE_PEN) && (RLV_ENABLE_ADAPTIVE_TITLE_PEN != 0)
    if (c->title_fill_style == (UWORD)RLV_TITLE_FILL_ADAPTIVE_BLEND
        && c->adaptive_title_pen_owned) {
        return FALSE;
    }
    paint_style = c->title_fill_effective;
#else
    paint_style = c->title_fill_style;
#endif
    desc = rlv_title_fill_lookup(paint_style);
    return (desc->solid == 0U) ? TRUE : FALSE;
}

UWORD rlv_title_fill_text_back_pen(const RLV_Control *c)
{
    if (c == 0) {
        return 0;
    }
#if defined(RLV_ENABLE_ADAPTIVE_TITLE_PEN) && (RLV_ENABLE_ADAPTIVE_TITLE_PEN != 0)
    if (c->title_fill_style == (UWORD)RLV_TITLE_FILL_ADAPTIVE_BLEND
        && c->adaptive_title_pen_owned) {
        return c->adaptive_title_pen;
    }
#endif
    return c->pens.background;
}
