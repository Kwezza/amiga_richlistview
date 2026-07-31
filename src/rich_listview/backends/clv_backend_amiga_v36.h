#ifndef CLV_BACKEND_AMIGA_V36_H
#define CLV_BACKEND_AMIGA_V36_H

/**
 * Workbench 2.x/3.x (Kickstart V36+) draw backend for the experimental control.
 * Implements CLV_DrawOps against a window RastPort. No LISTVIEW_KIND.
 */

#include "rich_listview/clv_control_draw.h"

#include <exec/types.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
#include <intuition/screens.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CLV_BackendV36 CLV_BackendV36;

/**
 * Create a draw context bound to rp. Font may be NULL (uses rp's font).
 * Caller owns the returned context; free with clv_backend_v36_destroy.
 */
CLV_BackendV36 *clv_backend_v36_create(struct RastPort *rp,
                                       struct TextFont *font);

VOID clv_backend_v36_destroy(CLV_BackendV36 *backend);

/** Update RastPort pointer after OpenWindow / refresh (same backend). */
VOID clv_backend_v36_set_rastport(CLV_BackendV36 *backend,
                                  struct RastPort *rp);

/** Borrowed font; never closed by the backend. NULL keeps current. */
VOID clv_backend_v36_set_font(CLV_BackendV36 *backend,
                              struct TextFont *font);

const CLV_DrawOps *clv_backend_v36_get_ops(void);

APTR clv_backend_v36_get_context(CLV_BackendV36 *backend);

/** Map DrawInfo pens into semantic CLV_Pens roles. */
VOID clv_backend_v36_pens_from_drawinfo(const struct DrawInfo *dri,
                                        CLV_Pens *out_pens);

#ifdef __cplusplus
}
#endif

#endif /* CLV_BACKEND_AMIGA_V36_H */
