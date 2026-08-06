/**
 * Shared Amiga graphics.library V39+ adaptive-colour engine.
 * RGB lookup, blending helpers, ObtainBestPen resolve, and ReleasePen.
 */

#include "rich_listview/backends/rlv_adaptive_colour.h"

#if defined(RLV_ENABLE_ADAPTIVE_COLOURS) && (RLV_ENABLE_ADAPTIVE_COLOURS != 0)

#include "rich_listview/backends/rlv_backend_amiga_v36.h"
#include "rich_listview/rlv_log.h"

#include <graphics/gfxbase.h>
#include <graphics/display.h>
#include <graphics/view.h>
#include <proto/graphics.h>

extern struct GfxBase *GfxBase;

#define RLV_RGB32_FROM_BYTE(b)  (((ULONG)(b)) << 24)
#define RLV_BYTE_FROM_RGB32(v)  ((UBYTE)(((ULONG)(v) >> 24) & 0xFFUL))

ULONG rlv_adaptive_colour_luma(ULONG r, ULONG g, ULONG b)
{
    return (77UL * r + 150UL * g + 29UL * b) >> 8;
}

ULONG rlv_adaptive_colour_luma_delta(ULONG a, ULONG b)
{
    if (a >= b) {
        return a - b;
    }
    return b - a;
}

ULONG rlv_adaptive_colour_rgb_dist_sq(ULONG r1, ULONG g1, ULONG b1,
                                      ULONG r2, ULONG g2, ULONG b2)
{
    LONG dr;
    LONG dg;
    LONG db;

    dr = (LONG)r1 - (LONG)r2;
    dg = (LONG)g1 - (LONG)g2;
    db = (LONG)b1 - (LONG)b2;
    return (ULONG)(dr * dr + dg * dg + db * db);
}

BOOL rlv_adaptive_colour_read_pen(const RLV_AdaptiveColourCtx *ctx,
                                  LONG pen,
                                  ULONG *r,
                                  ULONG *g,
                                  ULONG *b)
{
    struct ColorMap *cm;
    ULONG table[3];

    if (ctx == 0 || r == 0 || g == 0 || b == 0 || pen < 0) {
        return FALSE;
    }
    cm = (struct ColorMap *)ctx->colormap;
    if (cm == 0) {
        return FALSE;
    }
    if (GfxBase == 0 || GfxBase->LibNode.lib_Version < 39) {
        return FALSE;
    }
    table[0] = 0;
    table[1] = 0;
    table[2] = 0;
    GetRGB32(cm, (ULONG)pen, 1UL, table);
    *r = (ULONG)RLV_BYTE_FROM_RGB32(table[0]);
    *g = (ULONG)RLV_BYTE_FROM_RGB32(table[1]);
    *b = (ULONG)RLV_BYTE_FROM_RGB32(table[2]);
    return TRUE;
}

VOID rlv_adaptive_colour_darken(ULONG r, ULONG g, ULONG b,
                                UBYTE darken_percent,
                                ULONG *out_r,
                                ULONG *out_g,
                                ULONG *out_b)
{
    ULONG keep;

    if (out_r == 0 || out_g == 0 || out_b == 0) {
        return;
    }
    if (darken_percent > 100U) {
        darken_percent = 100U;
    }
    keep = 100UL - (ULONG)darken_percent;
    *out_r = (r * keep) / 100UL;
    *out_g = (g * keep) / 100UL;
    *out_b = (b * keep) / 100UL;
}

VOID rlv_adaptive_colour_blend(ULONG r_a, ULONG g_a, ULONG b_a,
                               UBYTE weight_a,
                               ULONG r_b, ULONG g_b, ULONG b_b,
                               UBYTE weight_b,
                               ULONG *out_r,
                               ULONG *out_g,
                               ULONG *out_b)
{
    ULONG sum;

    if (out_r == 0 || out_g == 0 || out_b == 0) {
        return;
    }
    sum = (ULONG)weight_a + (ULONG)weight_b;
    if (sum == 0UL) {
        *out_r = 0;
        *out_g = 0;
        *out_b = 0;
        return;
    }
    *out_r = (r_a * (ULONG)weight_a + r_b * (ULONG)weight_b) / sum;
    *out_g = (g_a * (ULONG)weight_a + g_b * (ULONG)weight_b) / sum;
    *out_b = (b_a * (ULONG)weight_a + b_b * (ULONG)weight_b) / sum;
}

BOOL rlv_adaptive_colour_begin(APTR draw_context,
                               CONST_STRPTR log_tag,
                               RLV_AdaptiveColourCtx *out_ctx)
{
    RLV_BackendV36 *backend;
    struct ColorMap *cm;
    CONST_STRPTR tag;

    tag = (log_tag != 0) ? log_tag : "ADAPTIVE";

    if (out_ctx == 0) {
        return FALSE;
    }
    out_ctx->draw_context = 0;
    out_ctx->colormap = 0;

    backend = (RLV_BackendV36 *)draw_context;
    if (backend == 0) {
        RLV_LOGF("%s adaptive fail (no draw context)", tag);
        return FALSE;
    }
    cm = rlv_backend_v36_colormap(backend);
    if (cm == 0) {
        RLV_LOGF("%s adaptive fail (no ColorMap)", tag);
        return FALSE;
    }
    if (GfxBase == 0 || GfxBase->LibNode.lib_Version < 39) {
        RLV_LOGF("%s adaptive fail (graphics.library ver=%u need=39)",
                 tag,
                 (GfxBase != 0)
                 ? (unsigned)GfxBase->LibNode.lib_Version
                 : 0U);
        return FALSE;
    }

    out_ctx->draw_context = draw_context;
    out_ctx->colormap = (APTR)cm;
    return TRUE;
}

BOOL rlv_adaptive_colour_resolve(const RLV_AdaptiveColourCtx *ctx,
                                 CONST_STRPTR log_tag,
                                 ULONG target_r,
                                 ULONG target_g,
                                 ULONG target_b,
                                 RLV_AdaptiveColourValidateFn validate,
                                 APTR validate_data,
                                 UWORD *out_pen)
{
    struct ColorMap *cm;
    LONG pen;
    UWORD pen_u;
    RLV_AdaptiveColourSample sample;
    CONST_STRPTR tag;

    tag = (log_tag != 0) ? log_tag : "ADAPTIVE";

    if (ctx == 0 || out_pen == 0 || ctx->colormap == 0) {
        return FALSE;
    }
    cm = (struct ColorMap *)ctx->colormap;

    RLV_LOGF("%s adaptive ObtainBestPen begin target=%lu,%lu,%lu gfx=%u",
             tag,
             (unsigned long)target_r,
             (unsigned long)target_g,
             (unsigned long)target_b,
             (unsigned)GfxBase->LibNode.lib_Version);

    pen = ObtainBestPen(cm,
                         RLV_RGB32_FROM_BYTE(target_r),
                         RLV_RGB32_FROM_BYTE(target_g),
                         RLV_RGB32_FROM_BYTE(target_b),
                         TAG_DONE);
    if (pen < 0) {
        RLV_LOGF("%s adaptive fail (ObtainBestPen)", tag);
        return FALSE;
    }
    pen_u = (UWORD)pen;
    RLV_LOGF("%s adaptive ObtainBestPen returned pen=%ld",
             tag, (long)pen);

    if (!rlv_adaptive_colour_read_pen(ctx, (LONG)pen_u,
                                      &sample.r, &sample.g, &sample.b)) {
        RLV_LOGF("%s adaptive reject reason=rgb_read_fail pen=%u",
                 tag, (unsigned)pen_u);
        ReleasePen(cm, (ULONG)pen);
        return FALSE;
    }
    sample.pen = pen_u;
    sample.luma = rlv_adaptive_colour_luma(sample.r, sample.g, sample.b);

    if (validate != 0) {
        if (!validate(ctx, &sample, target_r, target_g, target_b,
                      validate_data)) {
            ReleasePen(cm, (ULONG)pen);
            return FALSE;
        }
    }

    *out_pen = pen_u;
    return TRUE;
}

VOID rlv_adaptive_colour_release(APTR draw_context, UWORD pen)
{
    RLV_BackendV36 *backend;
    struct ColorMap *cm;

    if (draw_context == 0) {
        return;
    }
    if (GfxBase == 0 || GfxBase->LibNode.lib_Version < 39) {
        return;
    }
    backend = (RLV_BackendV36 *)draw_context;
    cm = rlv_backend_v36_colormap(backend);
    if (cm == 0) {
        return;
    }
    RLV_LOGF("ADAPTIVE ReleasePen pen=%u", (unsigned)pen);
    ReleasePen(cm, (ULONG)pen);
}

#endif /* RLV_ENABLE_ADAPTIVE_COLOURS */
