#ifndef RLV_BACKEND_AMIGA_V36_H
#define RLV_BACKEND_AMIGA_V36_H

/**
 * Workbench 2.x/3.x (Kickstart V36+) draw backend for RichListview.
 * Implements RLV_DrawOps against a window RastPort. No LISTVIEW_KIND.
 * Optional public include for applications that create the backend.
 */

#include "rich_listview/rlv_draw.h"

#include <exec/types.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
#include <intuition/screens.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RLV_BackendV36 RLV_BackendV36;

/**
 * Create a draw context bound to rp. Font may be NULL (uses rp's font).
 * Caller owns the returned context; free with rlv_backend_v36_destroy.
 */
RLV_BackendV36 *rlv_backend_v36_create(struct RastPort *rp,
                                       struct TextFont *font);

VOID rlv_backend_v36_destroy(RLV_BackendV36 *backend);

/** Update RastPort pointer after OpenWindow / refresh (same backend). */
VOID rlv_backend_v36_set_rastport(RLV_BackendV36 *backend,
                                  struct RastPort *rp);

/** Borrowed font; never closed by the backend. NULL keeps current. */
VOID rlv_backend_v36_set_font(RLV_BackendV36 *backend,
                              struct TextFont *font);

const RLV_DrawOps *rlv_backend_v36_get_ops(void);

APTR rlv_backend_v36_get_context(RLV_BackendV36 *backend);

/** Map DrawInfo pens into semantic RLV_Pens roles. */
VOID rlv_backend_v36_pens_from_drawinfo(const struct DrawInfo *dri,
                                        RLV_Pens *out_pens);

#ifdef __cplusplus
}
#endif

#endif /* RLV_BACKEND_AMIGA_V36_H */
