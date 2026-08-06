#ifndef RLV_ADAPTIVE_COLOUR_H
#define RLV_ADAPTIVE_COLOUR_H

/**
 * Shared Amiga V39+ adaptive-colour engine (ObtainBestPen / RGB helpers).
 * Feature policy stays in control modules; this unit owns common machinery.
 * Requires RLV_ENABLE_ADAPTIVE_COLOURS.
 */

#include "rich_listview/rlv_features.h"

#if defined(RLV_ENABLE_ADAPTIVE_COLOURS) && (RLV_ENABLE_ADAPTIVE_COLOURS != 0)

#include <exec/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Skip an avoid-pen / reject-exact slot. */
#define RLV_ADAPTIVE_AVOID_NONE  ((UWORD)0xFFFFU)

/* Default squared RGB distance budget (each channel 0..255). */
#define RLV_ADAPTIVE_DEFAULT_MAX_TARGET_DIST_SQ  (48UL * 48UL * 3UL)

typedef struct RLV_AdaptiveColourCtx
{
    APTR draw_context;
    /* Opaque ColorMap *; only for engine-internal use via helpers. */
    APTR colormap;
} RLV_AdaptiveColourCtx;

typedef struct RLV_AdaptiveColourSample
{
    UWORD pen;
    ULONG r;
    ULONG g;
    ULONG b;
    ULONG luma;
} RLV_AdaptiveColourSample;

/*
 * Validate an obtained pen. Return TRUE to keep the share; FALSE to
 * ReleasePen and fail resolve. Logging is the caller's responsibility.
 */
typedef BOOL (*RLV_AdaptiveColourValidateFn)(
    const RLV_AdaptiveColourCtx *ctx,
    const RLV_AdaptiveColourSample *candidate,
    ULONG target_r,
    ULONG target_g,
    ULONG target_b,
    APTR user_data);

/*
 * Open ColorMap + V39 gate. On FALSE, *out_ctx is undefined and no
 * resources are owned. log_tag prefixes failure messages (may be NULL).
 */
BOOL rlv_adaptive_colour_begin(APTR draw_context,
                               CONST_STRPTR log_tag,
                               RLV_AdaptiveColourCtx *out_ctx);

ULONG rlv_adaptive_colour_luma(ULONG r, ULONG g, ULONG b);
ULONG rlv_adaptive_colour_luma_delta(ULONG a, ULONG b);
ULONG rlv_adaptive_colour_rgb_dist_sq(ULONG r1, ULONG g1, ULONG b1,
                                      ULONG r2, ULONG g2, ULONG b2);

BOOL rlv_adaptive_colour_read_pen(const RLV_AdaptiveColourCtx *ctx,
                                  LONG pen,
                                  ULONG *r,
                                  ULONG *g,
                                  ULONG *b);

VOID rlv_adaptive_colour_darken(ULONG r, ULONG g, ULONG b,
                                UBYTE darken_percent,
                                ULONG *out_r,
                                ULONG *out_g,
                                ULONG *out_b);

VOID rlv_adaptive_colour_blend(ULONG r_a, ULONG g_a, ULONG b_a,
                               UBYTE weight_a,
                               ULONG r_b, ULONG g_b, ULONG b_b,
                               UBYTE weight_b,
                               ULONG *out_r,
                               ULONG *out_g,
                               ULONG *out_b);

/*
 * ObtainBestPen for target RGB. On success *out_pen is owned by the
 * caller and must be released with rlv_adaptive_colour_release. On
 * failure any temporary share is released and *out_pen is unchanged.
 * validate may be NULL (accept any obtained pen that is >= 0).
 */
BOOL rlv_adaptive_colour_resolve(const RLV_AdaptiveColourCtx *ctx,
                                 CONST_STRPTR log_tag,
                                 ULONG target_r,
                                 ULONG target_g,
                                 ULONG target_b,
                                 RLV_AdaptiveColourValidateFn validate,
                                 APTR validate_data,
                                 UWORD *out_pen);

VOID rlv_adaptive_colour_release(APTR draw_context, UWORD pen);

#ifdef __cplusplus
}
#endif

#endif /* RLV_ENABLE_ADAPTIVE_COLOURS */

#endif /* RLV_ADAPTIVE_COLOUR_H */
