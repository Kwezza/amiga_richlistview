#ifndef RLV_ALTERNATE_ROWS_H
#define RLV_ALTERNATE_ROWS_H

/**
 * Internal alternating-row backdrop helpers (rlv_alternate_rows.c).
 * Compiled only when RLV_ENABLE_ALTERNATE_ROWS != 0.
 */

#include "rich_listview/rlv_features.h"

#if defined(RLV_ENABLE_ALTERNATE_ROWS) && (RLV_ENABLE_ALTERNATE_ROWS != 0)

#include <exec/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RLV_Control RLV_Control;

VOID rlv_alternate_rows_init_from_config(RLV_Control *c,
                                        UWORD requested_mode,
                                        UWORD caller_alternate_pen);
VOID rlv_alternate_rows_teardown(RLV_Control *c);
VOID rlv_alternate_rows_refresh(RLV_Control *c);

VOID rlv_alternate_rows_set_mode(RLV_Control *c,
                                 UWORD mode,
                                 UWORD caller_alternate_pen);

UWORD rlv_alternate_rows_normalize_mode(UWORD mode);
BOOL rlv_alternate_rows_caller_pen_valid(const RLV_Control *c, UWORD pen);

/*
 * Normal (non-selected) row backdrop pen for a source logical row index.
 * Selected rows must not call this — use selected_background instead.
 * Pattern effective mode returns background (fill is patterned separately).
 */
UWORD rlv_row_normal_backdrop_pen(const RLV_Control *c, LONG logical_row);

/* Odd logical rows when effective mode is ALTERNATE_PATTERN. */
BOOL rlv_row_uses_pattern_backdrop(const RLV_Control *c, LONG logical_row);

/* Solid alternate pen, patterned stipple, or background for the row. */
VOID rlv_row_fill_normal_backdrop(RLV_Control *c,
                                  WORD x1, WORD y1, WORD x2, WORD y2,
                                  LONG logical_row);

#ifdef __cplusplus
}
#endif

#endif /* RLV_ENABLE_ALTERNATE_ROWS */

#endif /* RLV_ALTERNATE_ROWS_H */
