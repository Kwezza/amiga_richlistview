#ifndef RLV_DRAW_H
#define RLV_DRAW_H

/**
 * Neutral draw boundary for the RichListview control.
 * Core render/layout code calls these ops; backends implement them.
 * Public when RLV_Config / rlv_set_pens accept draw types.
 */

#include "rich_listview/rlv_platform.h"

#include <exec/types.h>
#include <graphics/gfx.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Control-local line style (reserved / unused by current paint paths).
 * Numeric values match the former v1 line-style family.
 */
typedef enum RLV_LineStyle
{
    RLV_LINE_NONE = 0,
    RLV_LINE_SINGLE,
    RLV_LINE_RECESSED,
    RLV_LINE_RAISED
} RLV_LineStyle;

typedef struct RLV_Pens
{
    UWORD text;
    UWORD background;
    UWORD selected_text;
    UWORD selected_background;
    UWORD shine;
    UWORD shadow;
    UWORD separator;
} RLV_Pens;

typedef struct RLV_FontMetrics
{
    UWORD line_height; /* typically tf_YSize */
    UWORD baseline;    /* typically tf_Baseline */
    UWORD soft_style;  /* reserved; 0 */
} RLV_FontMetrics;

/*
 * Result of an optional viewport pixel-shift attempt (smart scroll).
 * UNUSED  = optimisation not available / not attempted
 * DONE    = pixels shifted successfully; caller paints the exposed band
 * REPAINT = caller must repaint the full viewport
 */
typedef enum RLV_ViewportMoveResult
{
    RLV_VIEWPORT_MOVE_UNUSED = 0,
    RLV_VIEWPORT_MOVE_DONE,
    RLV_VIEWPORT_MOVE_REPAINT
} RLV_ViewportMoveResult;

typedef struct RLV_DrawOps
{
    VOID  (*set_pens)(APTR ctx, UWORD front, UWORD back);
    VOID  (*fill_rect)(APTR ctx, WORD x1, WORD y1, WORD x2, WORD y2);
    VOID  (*draw_line)(APTR ctx, WORD x1, WORD y1, WORD x2, WORD y2);
    VOID  (*draw_text)(APTR ctx, WORD x, WORD baseline,
                       CONST_STRPTR text, UWORD length);
    UWORD (*text_width)(APTR ctx, CONST_STRPTR text, UWORD length);
    UWORD (*text_fit)(APTR ctx, CONST_STRPTR text, UWORD length,
                      UWORD max_width); /* Phase 3; may be NULL until then */
    BOOL  (*push_clip)(APTR ctx, const struct Rectangle *rect);
    VOID  (*pop_clip)(APTR ctx);
    UWORD (*line_height)(APTR ctx);
    UWORD (*baseline)(APTR ctx);
    /*
     * Optional. NULL = smart scroll unavailable.
     * vertical_delta: content scroll change (new_scroll_y - old_scroll_y).
     * Positive delta shifts existing viewport pixels upward (ScrollRaster dy).
     */
    UWORD (*move_viewport_pixels)(APTR ctx,
                                  const struct Rectangle *viewport,
                                  WORD vertical_delta);
    /*
     * Optional. After a successful exposed-band paint, clear simple-refresh
     * damage that ScrollRaster may have recorded for the vacated strip.
     */
    VOID (*finish_viewport_move)(APTR ctx);
    /*
     * Optional one-on/one-off horizontal line. Core falls back to individual
     * one-pixel segments when a backend does not provide this operation.
     */
    VOID (*draw_dotted_hline)(APTR ctx, WORD x1, WORD x2, WORD y);
} RLV_DrawOps;

#ifdef __cplusplus
}
#endif

#endif /* RLV_DRAW_H */
