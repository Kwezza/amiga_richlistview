#ifndef CLV_CONTROL_DRAW_H
#define CLV_CONTROL_DRAW_H

/**
 * Neutral draw boundary for the experimental custom ListView control.
 * Core render/layout code calls these ops; backends implement them.
 */

#include "rich_listview/clv_control_platform.h"

#include <exec/types.h>
#include <graphics/gfx.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Control-local line style (same numeric meaning as v1 CLV_LineStyle).
 * Do not include clv_renderer.h for this alone.
 */
typedef enum CLV_ControlLineStyle
{
    CLV_CTRL_LINE_NONE = 0,
    CLV_CTRL_LINE_SINGLE,
    CLV_CTRL_LINE_RECESSED,
    CLV_CTRL_LINE_RAISED
} CLV_ControlLineStyle;

typedef struct CLV_Pens
{
    UWORD text;
    UWORD background;
    UWORD selected_text;
    UWORD selected_background;
    UWORD shine;
    UWORD shadow;
    UWORD separator;
} CLV_Pens;

typedef struct CLV_FontMetrics
{
    UWORD line_height; /* typically tf_YSize */
    UWORD baseline;    /* typically tf_Baseline */
    UWORD soft_style;  /* reserved; 0 */
} CLV_FontMetrics;

/*
 * Result of an optional viewport pixel-shift attempt (smart scroll).
 * UNUSED  = optimisation not available / not attempted
 * DONE    = pixels shifted successfully; caller paints the exposed band
 * REPAINT = caller must repaint the full viewport
 */
typedef enum CLV_ViewportMoveResult
{
    CLV_VIEWPORT_MOVE_UNUSED = 0,
    CLV_VIEWPORT_MOVE_DONE,
    CLV_VIEWPORT_MOVE_REPAINT
} CLV_ViewportMoveResult;

typedef struct CLV_DrawOps
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
} CLV_DrawOps;

#ifdef __cplusplus
}
#endif

#endif /* CLV_CONTROL_DRAW_H */
