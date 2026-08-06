/**
 * Selection-fill mode resolution and owned adaptive pen lifecycle.
 * Paint paths use rlv_selection_fill_pen / rlv_selection_text_pen only.
 */

#include "rich_listview/rlv_internal.h"
#include "rich_listview/rich_listview.h"
#include "rich_listview/rlv_log.h"

#if defined(RLV_ENABLE_ADAPTIVE_SELECTION_PEN) \
    && (RLV_ENABLE_ADAPTIVE_SELECTION_PEN != 0)
#include "rich_listview/backends/rlv_adaptive_colour.h"
#endif

UWORD rlv_selection_fill_normalize(UWORD mode)
{
#if defined(RLV_ENABLE_ADAPTIVE_SELECTION_PEN) \
    && (RLV_ENABLE_ADAPTIVE_SELECTION_PEN != 0)
    if (mode == (UWORD)RLV_SELECTION_FILL_ADAPTIVE) {
        return mode;
    }
#else
    if (mode == (UWORD)RLV_SELECTION_FILL_ADAPTIVE) {
        /* Feature omitted: keep Workbench system selection. */
        return (UWORD)RLV_SELECTION_FILL_SYSTEM;
    }
#endif
    return (UWORD)RLV_SELECTION_FILL_SYSTEM;
}

#if defined(RLV_ENABLE_ADAPTIVE_SELECTION_PEN) \
    && (RLV_ENABLE_ADAPTIVE_SELECTION_PEN != 0)

#define RLV_ADAPTIVE_SEL_FILL_PERCENT  65
#define RLV_ADAPTIVE_SEL_BG_PERCENT    (100 - RLV_ADAPTIVE_SEL_FILL_PERCENT)
#define RLV_ADAPTIVE_SEL_MIN_BG_LUMA_DELTA  10
#define RLV_ADAPTIVE_SEL_MIN_DISTINCT_PERCENT  55
#define RLV_ADAPTIVE_SEL_MIN_AVOID_LUMA_DELTA  8
#define RLV_ADAPTIVE_SEL_MIN_SEMANTIC_LUMA_DELTA  8
#define RLV_ADAPTIVE_SEL_MIN_TEXT_LUMA_DELTA  24

typedef struct RLV_AdaptiveSelValidateData
{
    const RLV_Pens *pens;
    ULONG bg_r;
    ULONG bg_g;
    ULONG bg_b;
    ULONG bg_luma;
    ULONG fill_luma;
    UWORD avoid_pen0;
    UWORD avoid_pen1;
    UWORD *out_text_pen;
} RLV_AdaptiveSelValidateData;

static BOOL rlv_adaptive_sel_choose_text_pen(
    const RLV_AdaptiveColourCtx *ctx,
    const RLV_Pens *pens,
    ULONG fill_luma,
    UWORD *out_text_pen)
{
    ULONG filltext_r;
    ULONG filltext_g;
    ULONG filltext_b;
    ULONG text_r;
    ULONG text_g;
    ULONG text_b;
    ULONG filltext_luma;
    ULONG text_luma;
    ULONG filltext_delta;
    ULONG text_delta;

    if (!rlv_adaptive_colour_read_pen(ctx, (LONG)pens->selected_text,
                                      &filltext_r, &filltext_g,
                                      &filltext_b)) {
        RLV_LOG("SELECTION_FILL adaptive reject reason=filltext_rgb_fail");
        return FALSE;
    }
    filltext_luma = rlv_adaptive_colour_luma(filltext_r, filltext_g,
                                             filltext_b);
    filltext_delta = rlv_adaptive_colour_luma_delta(filltext_luma, fill_luma);

    if (filltext_delta >= (ULONG)RLV_ADAPTIVE_SEL_MIN_TEXT_LUMA_DELTA) {
        *out_text_pen = pens->selected_text;
        RLV_LOGF("SELECTION_FILL adaptive text=FILLTEXTPEN pen=%u "
                 "delta=%lu",
                 (unsigned)pens->selected_text,
                 (unsigned long)filltext_delta);
        return TRUE;
    }

    if (!rlv_adaptive_colour_read_pen(ctx, (LONG)pens->text,
                                      &text_r, &text_g, &text_b)) {
        RLV_LOG("SELECTION_FILL adaptive reject reason=text_rgb_fail");
        return FALSE;
    }
    text_luma = rlv_adaptive_colour_luma(text_r, text_g, text_b);
    text_delta = rlv_adaptive_colour_luma_delta(text_luma, fill_luma);

    if (text_delta >= (ULONG)RLV_ADAPTIVE_SEL_MIN_TEXT_LUMA_DELTA
        && text_delta > filltext_delta) {
        *out_text_pen = pens->text;
        RLV_LOGF("SELECTION_FILL adaptive text=TEXTPEN pen=%u "
                 "delta=%lu filltext_delta=%lu",
                 (unsigned)pens->text,
                 (unsigned long)text_delta,
                 (unsigned long)filltext_delta);
        return TRUE;
    }

    RLV_LOGF("SELECTION_FILL adaptive reject reason=text_contrast "
             "filltext_delta=%lu text_delta=%lu min=%u",
             (unsigned long)filltext_delta,
             (unsigned long)text_delta,
             (unsigned)RLV_ADAPTIVE_SEL_MIN_TEXT_LUMA_DELTA);
    return FALSE;
}

static BOOL rlv_adaptive_sel_check_avoid(
    const RLV_AdaptiveColourCtx *ctx,
    UWORD avoid_pen,
    ULONG cand_luma,
    CONST_STRPTR role)
{
    ULONG ar;
    ULONG ag;
    ULONG ab;
    ULONG avoid_luma;
    ULONG delta;

    if (avoid_pen == RLV_ADAPTIVE_AVOID_NONE) {
        return TRUE;
    }
    if (!rlv_adaptive_colour_read_pen(ctx, (LONG)avoid_pen, &ar, &ag, &ab)) {
        return TRUE;
    }
    avoid_luma = rlv_adaptive_colour_luma(ar, ag, ab);
    delta = rlv_adaptive_colour_luma_delta(cand_luma, avoid_luma);
    if (delta < (ULONG)RLV_ADAPTIVE_SEL_MIN_AVOID_LUMA_DELTA) {
        RLV_LOGF("SELECTION_FILL adaptive reject reason=near_%s "
                 "pen=%u delta=%lu min=%u",
                 role,
                 (unsigned)avoid_pen,
                 (unsigned long)delta,
                 (unsigned)RLV_ADAPTIVE_SEL_MIN_AVOID_LUMA_DELTA);
        return FALSE;
    }
    return TRUE;
}

static BOOL rlv_adaptive_sel_validate(
    const RLV_AdaptiveColourCtx *ctx,
    const RLV_AdaptiveColourSample *candidate,
    ULONG target_r,
    ULONG target_g,
    ULONG target_b,
    APTR user_data)
{
    RLV_AdaptiveSelValidateData *vd;
    ULONG luma_delta;
    ULONG fill_bg_delta;
    ULONG min_distinct;
    ULONG dist_sq;
    ULONG shine_r;
    ULONG shine_g;
    ULONG shine_b;
    ULONG shadow_r;
    ULONG shadow_g;
    ULONG shadow_b;
    ULONG shine_luma;
    ULONG shadow_luma;

    vd = (RLV_AdaptiveSelValidateData *)user_data;
    if (ctx == 0 || candidate == 0 || vd == 0 || vd->pens == 0
        || vd->out_text_pen == 0) {
        return FALSE;
    }

    RLV_LOGF("SELECTION_FILL adaptive candidate pen=%u rgb=%lu,%lu,%lu "
             "target=%lu,%lu,%lu bg=%lu,%lu,%lu",
             (unsigned)candidate->pen,
             (unsigned long)candidate->r, (unsigned long)candidate->g,
             (unsigned long)candidate->b,
             (unsigned long)target_r, (unsigned long)target_g,
             (unsigned long)target_b,
             (unsigned long)vd->bg_r, (unsigned long)vd->bg_g,
             (unsigned long)vd->bg_b);

    if (candidate->pen == vd->pens->background) {
        RLV_LOG("SELECTION_FILL adaptive reject reason=same_as_background");
        return FALSE;
    }
    if (candidate->pen == vd->pens->selected_background) {
        RLV_LOG("SELECTION_FILL adaptive reject reason=same_as_fillpen");
        return FALSE;
    }

    luma_delta = rlv_adaptive_colour_luma_delta(candidate->luma, vd->bg_luma);
    if (luma_delta < (ULONG)RLV_ADAPTIVE_SEL_MIN_BG_LUMA_DELTA) {
        RLV_LOGF("SELECTION_FILL adaptive reject reason=luma_near_bg "
                 "delta=%lu min=%u",
                 (unsigned long)luma_delta,
                 (unsigned)RLV_ADAPTIVE_SEL_MIN_BG_LUMA_DELTA);
        return FALSE;
    }

    fill_bg_delta = rlv_adaptive_colour_luma_delta(vd->fill_luma, vd->bg_luma);
    min_distinct = (fill_bg_delta
                    * (ULONG)RLV_ADAPTIVE_SEL_MIN_DISTINCT_PERCENT) / 100UL;
    if (min_distinct < (ULONG)RLV_ADAPTIVE_SEL_MIN_BG_LUMA_DELTA) {
        min_distinct = (ULONG)RLV_ADAPTIVE_SEL_MIN_BG_LUMA_DELTA;
    }
    if (luma_delta < min_distinct) {
        RLV_LOGF("SELECTION_FILL adaptive reject reason=weak_vs_system "
                 "delta=%lu min_distinct=%lu fill_bg=%lu",
                 (unsigned long)luma_delta,
                 (unsigned long)min_distinct,
                 (unsigned long)fill_bg_delta);
        return FALSE;
    }

    dist_sq = rlv_adaptive_colour_rgb_dist_sq(
        candidate->r, candidate->g, candidate->b,
        target_r, target_g, target_b);
    if (dist_sq > RLV_ADAPTIVE_DEFAULT_MAX_TARGET_DIST_SQ) {
        RLV_LOGF("SELECTION_FILL adaptive reject reason=far_from_target "
                 "dist_sq=%lu max=%lu",
                 (unsigned long)dist_sq,
                 (unsigned long)RLV_ADAPTIVE_DEFAULT_MAX_TARGET_DIST_SQ);
        return FALSE;
    }

    if (rlv_adaptive_colour_read_pen(ctx, (LONG)vd->pens->shine,
                                     &shine_r, &shine_g, &shine_b)) {
        shine_luma = rlv_adaptive_colour_luma(shine_r, shine_g, shine_b);
        if (rlv_adaptive_colour_luma_delta(candidate->luma, shine_luma)
            < (ULONG)RLV_ADAPTIVE_SEL_MIN_SEMANTIC_LUMA_DELTA) {
            RLV_LOG("SELECTION_FILL adaptive reject reason=near_shine");
            return FALSE;
        }
    }
    if (rlv_adaptive_colour_read_pen(ctx, (LONG)vd->pens->shadow,
                                     &shadow_r, &shadow_g, &shadow_b)) {
        shadow_luma = rlv_adaptive_colour_luma(shadow_r, shadow_g, shadow_b);
        if (rlv_adaptive_colour_luma_delta(candidate->luma, shadow_luma)
            < (ULONG)RLV_ADAPTIVE_SEL_MIN_SEMANTIC_LUMA_DELTA) {
            RLV_LOG("SELECTION_FILL adaptive reject reason=near_shadow");
            return FALSE;
        }
    }

    if (!rlv_adaptive_sel_check_avoid(ctx, vd->avoid_pen0, candidate->luma,
                                      "avoid0")) {
        return FALSE;
    }
    if (!rlv_adaptive_sel_check_avoid(ctx, vd->avoid_pen1, candidate->luma,
                                      "avoid1")) {
        return FALSE;
    }

    if (!rlv_adaptive_sel_choose_text_pen(ctx, vd->pens, candidate->luma,
                                          vd->out_text_pen)) {
        return FALSE;
    }

    RLV_LOGF("SELECTION_FILL adaptive accept pen=%u rgb=%lu,%lu,%lu "
             "cand_luma=%lu bg_luma=%lu fill_luma=%lu dist_sq=%lu "
             "text_pen=%u",
             (unsigned)candidate->pen,
             (unsigned long)candidate->r, (unsigned long)candidate->g,
             (unsigned long)candidate->b,
             (unsigned long)candidate->luma,
             (unsigned long)vd->bg_luma,
             (unsigned long)vd->fill_luma,
             (unsigned long)dist_sq,
             (unsigned)*vd->out_text_pen);
    return TRUE;
}

static BOOL rlv_adaptive_selection_pen_acquire(APTR draw_context,
                                               const RLV_Pens *pens,
                                               UWORD avoid_pen0,
                                               UWORD avoid_pen1,
                                               UWORD *out_fill_pen,
                                               UWORD *out_text_pen)
{
    RLV_AdaptiveColourCtx ctx;
    RLV_AdaptiveSelValidateData vd;
    ULONG fill_r;
    ULONG fill_g;
    ULONG fill_b;
    ULONG target_r;
    ULONG target_g;
    ULONG target_b;
    UWORD pen_u;
    UWORD text_pen;

    if (out_fill_pen == 0 || out_text_pen == 0 || pens == 0) {
        return FALSE;
    }
    if (!rlv_adaptive_colour_begin(draw_context, "SELECTION_FILL", &ctx)) {
        return FALSE;
    }
    if (!rlv_adaptive_colour_read_pen(&ctx, (LONG)pens->background,
                                      &vd.bg_r, &vd.bg_g, &vd.bg_b)) {
        RLV_LOG("SELECTION_FILL adaptive fail (background RGB)");
        return FALSE;
    }
    if (!rlv_adaptive_colour_read_pen(&ctx, (LONG)pens->selected_background,
                                      &fill_r, &fill_g, &fill_b)) {
        RLV_LOG("SELECTION_FILL adaptive fail (FILLPEN RGB)");
        return FALSE;
    }

    vd.pens = pens;
    vd.bg_luma = rlv_adaptive_colour_luma(vd.bg_r, vd.bg_g, vd.bg_b);
    vd.fill_luma = rlv_adaptive_colour_luma(fill_r, fill_g, fill_b);
    vd.avoid_pen0 = avoid_pen0;
    vd.avoid_pen1 = avoid_pen1;
    text_pen = pens->selected_text;
    vd.out_text_pen = &text_pen;

    rlv_adaptive_colour_blend(fill_r, fill_g, fill_b,
                              (UBYTE)RLV_ADAPTIVE_SEL_FILL_PERCENT,
                              vd.bg_r, vd.bg_g, vd.bg_b,
                              (UBYTE)RLV_ADAPTIVE_SEL_BG_PERCENT,
                              &target_r, &target_g, &target_b);

    RLV_LOGF("SELECTION_FILL adaptive begin fill_pen=%u rgb=%lu,%lu,%lu "
             "bg_pen=%u rgb=%lu,%lu,%lu target=%lu,%lu,%lu blend=%u/%u",
             (unsigned)pens->selected_background,
             (unsigned long)fill_r, (unsigned long)fill_g,
             (unsigned long)fill_b,
             (unsigned)pens->background,
             (unsigned long)vd.bg_r, (unsigned long)vd.bg_g,
             (unsigned long)vd.bg_b,
             (unsigned long)target_r, (unsigned long)target_g,
             (unsigned long)target_b,
             (unsigned)RLV_ADAPTIVE_SEL_FILL_PERCENT,
             (unsigned)RLV_ADAPTIVE_SEL_BG_PERCENT);

    pen_u = 0;
    if (!rlv_adaptive_colour_resolve(&ctx, "SELECTION_FILL",
                                     target_r, target_g, target_b,
                                     rlv_adaptive_sel_validate, &vd,
                                     &pen_u)) {
        return FALSE;
    }

    *out_fill_pen = pen_u;
    *out_text_pen = text_pen;
    return TRUE;
}

static VOID rlv_selection_fill_release_owned(RLV_Control *c)
{
    if (c == 0 || !c->adaptive_selection_pen_owned) {
        return;
    }
    rlv_adaptive_colour_release(c->draw_context,
                                c->adaptive_selection_pen);
    c->adaptive_selection_pen_owned = FALSE;
    c->adaptive_selection_pen = 0;
    c->adaptive_selection_text_pen = 0;
}

static VOID rlv_selection_fill_set_system(RLV_Control *c,
                                          CONST_STRPTR reason)
{
    c->selection_fill_effective = (UWORD)RLV_SELECTION_FILL_SYSTEM;
    c->adaptive_selection_pen = c->pens.selected_background;
    c->adaptive_selection_text_pen = c->pens.selected_text;
    if (reason != 0) {
        RLV_LOGF("SELECTION_FILL adaptive unavailable (%s); "
                 "effective SYSTEM",
                 reason);
    } else {
        RLV_LOG("SELECTION_FILL adaptive unavailable; effective SYSTEM");
    }
}

static UWORD rlv_selection_fill_avoid_alt(const RLV_Control *c)
{
#if defined(RLV_ENABLE_ALTERNATE_ROWS) && (RLV_ENABLE_ALTERNATE_ROWS != 0)
    if (c->row_backdrop_effective == (UWORD)RLV_ROW_BACKDROP_ALTERNATE_PEN
        && c->alternate_row_pen != c->pens.background) {
        return c->alternate_row_pen;
    }
#else
    (void)c;
#endif
    return RLV_ADAPTIVE_AVOID_NONE;
}

/*
 * Adaptive title is also a FILLPEN/BACKGROUNDPEN blend (weaker ratio). On
 * typical palettes its luma sits within a few steps of the selection blend,
 * so treating it as an avoid-pen falsely rejects a valid selection colour
 * and forces SYSTEM until title adaptive is cleared. Title and body
 * selection occupy different regions; keep only the alternate-row avoid.
 */

VOID rlv_selection_fill_refresh(RLV_Control *c)
{
    UWORD requested;
    UWORD avoid_alt;
    UWORD acquired_fill;
    UWORD acquired_text;

    if (c == 0) {
        return;
    }

    rlv_selection_fill_release_owned(c);
    requested = rlv_selection_fill_normalize(c->selection_fill_requested);
    c->selection_fill_requested = requested;
    c->selection_fill_effective = (UWORD)RLV_SELECTION_FILL_SYSTEM;
    c->adaptive_selection_pen = c->pens.selected_background;
    c->adaptive_selection_text_pen = c->pens.selected_text;

    if (requested != (UWORD)RLV_SELECTION_FILL_ADAPTIVE) {
        RLV_LOGF("SELECTION_FILL effective=%u", (unsigned)requested);
        return;
    }

    avoid_alt = rlv_selection_fill_avoid_alt(c);
    acquired_fill = 0;
    acquired_text = 0;

    if (rlv_adaptive_selection_pen_acquire(c->draw_context,
                                           &c->pens,
                                           avoid_alt,
                                           RLV_ADAPTIVE_AVOID_NONE,
                                           &acquired_fill,
                                           &acquired_text)) {
        c->adaptive_selection_pen = acquired_fill;
        c->adaptive_selection_text_pen = acquired_text;
        c->adaptive_selection_pen_owned = TRUE;
        c->selection_fill_effective = (UWORD)RLV_SELECTION_FILL_ADAPTIVE;
        RLV_LOGF("SELECTION_FILL adaptive ok fill=%u text=%u",
                 (unsigned)acquired_fill,
                 (unsigned)acquired_text);
    } else {
        rlv_selection_fill_set_system(c, "acquire_failed");
    }
}

VOID rlv_selection_fill_init_from_config(RLV_Control *c, UWORD mode)
{
    if (c == 0) {
        return;
    }
    c->selection_fill_requested = rlv_selection_fill_normalize(mode);
    c->selection_fill_effective = (UWORD)RLV_SELECTION_FILL_SYSTEM;
    c->adaptive_selection_pen = 0;
    c->adaptive_selection_text_pen = 0;
    c->adaptive_selection_pen_owned = FALSE;
}

VOID rlv_selection_fill_teardown(RLV_Control *c)
{
    rlv_selection_fill_release_owned(c);
}

UWORD rlv_selection_fill_effective_mode(const RLV_Control *c)
{
    if (c == 0) {
        return (UWORD)RLV_SELECTION_FILL_SYSTEM;
    }
    return c->selection_fill_effective;
}

UWORD rlv_selection_fill_pen(const RLV_Control *c)
{
    if (c == 0) {
        return 0;
    }
    if (c->adaptive_selection_pen_owned) {
        return c->adaptive_selection_pen;
    }
    return c->pens.selected_background;
}

UWORD rlv_selection_text_pen(const RLV_Control *c)
{
    if (c == 0) {
        return 0;
    }
    if (c->adaptive_selection_pen_owned) {
        return c->adaptive_selection_text_pen;
    }
    return c->pens.selected_text;
}

#endif /* RLV_ENABLE_ADAPTIVE_SELECTION_PEN */
