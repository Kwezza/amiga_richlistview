#ifndef CLV_TYPES_H
#define CLV_TYPES_H

/**
 * Shared low-level CLV types used by more than one feature family.
 *
 * Kept deliberately small so ASCII / bridge code can compile without
 * pulling the full renderer public header.
 */

#include "rich_listview/clv_platform.h"
#include <exec/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum CLV_CellAlign
{
    CLV_CELL_ALIGN_LEFT = 0,
    CLV_CELL_ALIGN_CENTER,
    CLV_CELL_ALIGN_RIGHT
} CLV_CellAlign;

/**
 * Pixel reserve for a GadTools ListView vertical scrollbar plus list frame
 * when converting gadget width into usable content width. Shared by ASCII
 * char-count helpers and details prepare so wrap/fit stop before the
 * scrollbar rather than under it.
 */
#define CLV_LISTVIEW_SCROLLBAR_BORDER  36

/**
 * Pixel column rectangle relative to lvdm_Bounds.MinX.
 * Used by the ASCII→drawn bridge and by the renderer prepare path.
 */
typedef struct CLV_PixelColumn
{
    WORD left;
    WORD right;
    WORD text_left;
    WORD text_right;
    UWORD alignment;
    UWORD flags;
} CLV_PixelColumn;

#ifdef __cplusplus
}
#endif

#endif /* CLV_TYPES_H */
