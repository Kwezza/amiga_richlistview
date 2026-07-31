#ifndef CLV_CONTROL_H
#define CLV_CONTROL_H

/**
 * Experimental custom-drawn ListView control (not GadTools LISTVIEW_KIND).
 *
 * Phase 4–5.5: variable-height wrap, logical-row selection, hit-testing,
 * line/page/proportional scroll, and keyboard NAV_* via neutral
 * CLV_InputEvent. Experimental surface only — not CLV v1 / not final v2.
 *
 * Experimental checkbox cells (CLV_CTRL_COL_TYPE_CHECKBOX): paint from an
 * internal snapshot (first-line band, plain/tick). Verified SELECT_DOWN/UP
 * arm-and-commit:
 *   SELECT_DOWN on checkbox (other selectable row) may emit
 *   CLV_EVENT_SELECTION_CHANGED and arms; same-row checkbox arms only;
 *   SELECT_UP commit emits CLV_EVENT_CELL_CONTROL only; cancel emits none.
 * At most one CLV_EventType per handle_input call.
 * Smart-scroll exposed bands repaint snapshot checkboxes; selection +
 * make_visible scroll uses full viewport paint; set_bounds / set_rows /
 * set_columns / layout invalidate cancel arm. Space → CLV_INPUT_TOGGLE
 * toggles the selected row's sole eligible checkbox; Return / NAV_ACTIVATE
 * remains row activation only.
 *
 * Cell-control notification (generic — not checkbox-only):
 *   Completed control actions fill CLV_EVENT_CELL_CONTROL synchronously into
 *   a caller-owned CLV_Event (no callbacks, no Exec messages). Checkbox
 *   commits use control_type CHECKBOX + CLV_CTRL_ACTION_VALUE_CHANGED.
 *   Future button cells can reuse the same event with
 *   CLV_CTRL_ACTION_PRESSED (previous_value / cell_value may be zero).
 *   Future cycle cells can use CLV_CTRL_ACTION_VALUE_CHANGED with
 *   previous_value / cell_value as the old/new cycle indices (or packed
 *   values). Event construction is centralised for all cell commits.
 *   Do not retain the CLV_Event (or row_user_data) past the immediate
 *   handle_input return / app handler. control_user_data is deferred
 *   (no per-cell user-data slot on CLV_ControlCell today).
 *
 * Ownership: the application owns authoritative Booleans. set_rows borrows
 * CLV_ControlRow / control_cells and copies descriptors into the control
 * snapshot; the control does not write through borrowed app memory by
 * default. On CELL_CONTROL, update the app store from the event (typically
 * via row_user_data), then call
 * clv_control_render_logical_rows(control, row, -1). Use
 * clv_control_set_checkbox_value for reject-restore / async snapshot updates
 * without a full set_rows.
 *
 * Authoritative product path for interactive checkboxes is this package.
 * GadTools clv_cellctl_* under the legacy custom_listview tree is legacy /
 * non-authoritative (frozen; do not extend for new apps).
 */

#include "rich_listview/clv_control_draw.h"
#include "rich_listview/clv_types.h"

#include <exec/types.h>
#include <graphics/gfx.h>
#include <graphics/text.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CLV_Control CLV_Control;

/* Numeric values match v1 CLV_PIXEL_WRAP_* without including clv_renderer.h. */
typedef enum CLV_ControlWrapMode
{
    CLV_CTRL_WRAP_NONE = 0,
    CLV_CTRL_WRAP_WORD,
    CLV_CTRL_WRAP_WORD_OR_CHAR,
    CLV_CTRL_WRAP_PATH
} CLV_ControlWrapMode;

typedef enum CLV_ControlRowDividerStyle
{
    CLV_CTRL_ROW_DIVIDER_NONE = 0,
    CLV_CTRL_ROW_DIVIDER_SOLID,
    CLV_CTRL_ROW_DIVIDER_DOTTED
} CLV_ControlRowDividerStyle;

/* CLV_ControlConfig.flags */
#define CLV_CTRL_CFG_NO_KEYBOARD  0x0001U  /* start with NAV_* disabled */

typedef struct CLV_ControlConfig
{
    const struct CLV_DrawOps *draw_ops; /* required */
    APTR draw_context;                  /* opaque; owned by backend */
    struct TextFont *font;              /* borrowed; NULL = use ops default */
    UWORD cell_padding_x;               /* title/body text inset, pixels */
    UWORD cell_padding_y;               /* title/body text inset, pixels */
    UWORD row_gap;                      /* pixels between logical rows */
    UWORD row_divider_style;            /* CLV_ControlRowDividerStyle */
    UWORD flags;                        /* CLV_CTRL_CFG_*; 0 = defaults */
} CLV_ControlConfig;

/* CLV_ControlRow.flags */
#define CLV_CTRL_ROW_NONSELECTABLE  0x0001U

/* CLV_ControlColumn.flags — low nibble is column type (C2). */
#define CLV_CTRL_COL_TYPE_MASK      0x000FU
#define CLV_CTRL_COL_TYPE_TEXT      0x0000U
#define CLV_CTRL_COL_TYPE_CHECKBOX  0x0001U
/* Higher bits of CLV_ControlColumn.flags reserved for independent behaviours. */

/* CLV_ControlCell.flags */
#define CLV_CTRL_CELL_F_VISIBLE       0x01U
#define CLV_CTRL_CELL_F_ENABLED       0x02U
#define CLV_CTRL_CELL_F_INTERACTIVE   0x04U

/* CLV_ControlCell.value */
#define CLV_CTRL_CELL_UNCHECKED  0U
#define CLV_CTRL_CELL_CHECKED    1U

typedef struct CLV_ControlColumn
{
    CONST_STRPTR title;   /* borrowed */
    WORD width_pixels;    /* fixed minimum content width; last column may grow */
    UWORD alignment;      /* CLV_CellAlign */
    UWORD wrap_mode;      /* CLV_ControlWrapMode */
    UWORD flags;          /* CLV_CTRL_COL_TYPE_* in low nibble; rest reserved */
} CLV_ControlColumn;

/*
 * Optional per-cell control descriptor (length == column_count when non-NULL).
 * Entries for non-checkbox columns are ignored (flags/value zero).
 * Application memory is borrowed at set_rows; the control copies into an
 * internal snapshot and does not write through this pointer by default.
 */
typedef struct CLV_ControlCell
{
    UBYTE flags;  /* CLV_CTRL_CELL_F_* */
    UBYTE value;  /* CLV_CTRL_CELL_UNCHECKED / CHECKED */
} CLV_ControlCell;

typedef struct CLV_ControlRow
{
    CONST_STRPTR *cells;                  /* borrowed; length == column_count */
    const CLV_ControlCell *control_cells; /* optional; NULL = no controls */
    UWORD flags;                          /* CLV_CTRL_ROW_NONSELECTABLE, etc. */
    APTR user_data;                       /* optional; borrowed */
} CLV_ControlRow;

/* Neutral input events (IDCMP translated by the application/demo). */
typedef enum CLV_InputType
{
    CLV_INPUT_SELECT_DOWN = 0,
    CLV_INPUT_SELECT_UP,
    CLV_INPUT_POINTER_MOVE,
    CLV_INPUT_SCROLL_LINE_UP,
    CLV_INPUT_SCROLL_LINE_DOWN,
    CLV_INPUT_SCROLL_PAGE_UP,
    CLV_INPUT_SCROLL_PAGE_DOWN,
    CLV_INPUT_SCROLL_POSITION,
    /* Selection-centric keyboard navigation (experimental). */
    CLV_INPUT_NAV_PREV,
    CLV_INPUT_NAV_NEXT,
    CLV_INPUT_NAV_PAGE_UP,
    CLV_INPUT_NAV_PAGE_DOWN,
    CLV_INPUT_NAV_FIRST,
    CLV_INPUT_NAV_LAST,
    CLV_INPUT_NAV_ACTIVATE,
    /* Space: toggle sole eligible checkbox on selected row (C7 / DC-006).
     * Not NAV_ACTIVATE — Return remains row activation only. */
    CLV_INPUT_TOGGLE
} CLV_InputType;

typedef struct CLV_InputEvent
{
    UWORD type;   /* CLV_InputType */
    WORD  x;      /* window-relative; meaningful for pointer events */
    WORD  y;
    LONG  value;  /* e.g. scroll position */
} CLV_InputEvent;

/* Outcomes from clv_control_handle_input (TRUE iff type != CLV_EVENT_NONE).
 * At most one type is filled per call. Selection and cell-control commit are
 * separate handle_input invocations (SELECT_DOWN vs SELECT_UP), never one
 * compound. CLV_EVENT_CELL_CONTROL is the single generic cell-control
 * notification (checkbox today; button/cycle later via control_type +
 * control_action). Delivery is synchronous into caller stack storage. */
typedef enum CLV_EventType
{
    CLV_EVENT_NONE = 0,
    CLV_EVENT_SELECTION_CHANGED,
    CLV_EVENT_SCROLL_CHANGED,
    CLV_EVENT_ACTIVATED,     /* NAV_ACTIVATE only; no selection/scroll change */
    CLV_EVENT_CELL_CONTROL   /* generic cell commit (checkbox SELECT_UP / TOGGLE today) */
} CLV_EventType;

/* Action for CLV_EVENT_CELL_CONTROL (orthogonal to CLV_CTRL_COL_TYPE_*).
 * VALUE_CHANGED: checkbox toggle today; future cycle index change.
 * PRESSED: future stateless button (no required value transition). */
typedef enum CLV_CellControlAction
{
    CLV_CTRL_ACTION_NONE = 0,
    CLV_CTRL_ACTION_VALUE_CHANGED,
    CLV_CTRL_ACTION_PRESSED
} CLV_CellControlAction;

/*
 * Caller-owned event filled by handle_input. Valid only until the caller
 * returns from the immediate handler; do not retain pointers into it.
 * On CELL_CONTROL: row_user_data is a borrowed copy of
 * CLV_ControlRow.user_data at commit (same lifetime as the app row array).
 * Per-cell control_user_data is not provided (deferred). For PRESSED /
 * other stateless actions, previous_value and cell_value may both be zero.
 * LONG value remains scroll_y for scroll/selection — never overload it for
 * cell state (use cell_value).
 */
typedef struct CLV_Event
{
    UWORD type;             /* CLV_EventType */
    LONG  row;              /* logical row when applicable; -1 if none */
    LONG  previous_row;     /* prior selection on SELECTION_CHANGED; else -1 */
    LONG  value;            /* e.g. new scroll_y; unused on CELL_CONTROL */
    UWORD column;           /* CELL_CONTROL */
    UWORD control_type;     /* CELL_CONTROL: CLV_CTRL_COL_TYPE_* */
    UWORD control_action;   /* CELL_CONTROL: CLV_CTRL_ACTION_* */
    APTR  row_user_data;    /* CELL_CONTROL: borrowed row user_data at commit */
    UBYTE previous_value;   /* CELL_CONTROL prior cell value (0 if unused) */
    UBYTE cell_value;       /* CELL_CONTROL new cell value (not LONG value) */
} CLV_Event;

CLV_Control *clv_control_create(const CLV_ControlConfig *cfg);
VOID         clv_control_destroy(CLV_Control *control);

BOOL clv_control_set_columns(CLV_Control *c,
                             const CLV_ControlColumn *cols,
                             UWORD count);
/* Borrow rows/cells/control_cells; copy control descriptors into the
 * internal snapshot. Does not paint. Cancels any verified-click arm. */
BOOL clv_control_set_rows(CLV_Control *c,
                          const CLV_ControlRow *rows,
                          ULONG count);
/*
 * Update the internal checkbox snapshot for one cell. Does not write the
 * application's authoritative store and does not paint — caller must
 * update app memory if needed, then render (e.g.
 * clv_control_render_logical_rows). Returns FALSE when
 * control/row/column/type/value is invalid or no snapshot exists.
 *
 * Typical uses: reject-restore after CELL_CONTROL, async/external updates
 * without a full set_rows rebuild.
 */
BOOL clv_control_set_checkbox_value(CLV_Control *control,
                                    LONG row,
                                    UWORD column,
                                    UBYTE value);
VOID clv_control_set_cell_padding(CLV_Control *c, UWORD x, UWORD y);
UWORD clv_control_get_cell_padding_x(const CLV_Control *c);
UWORD clv_control_get_cell_padding_y(const CLV_Control *c);
VOID clv_control_set_row_gap(CLV_Control *c, UWORD pixels);
UWORD clv_control_get_row_gap(const CLV_Control *c);
/* Body rows only; title/header cells are unaffected. Repaint after changing. */
VOID clv_control_set_row_divider_style(CLV_Control *c, UWORD style);
UWORD clv_control_get_row_divider_style(const CLV_Control *c);
/* Relayout transaction: rebuild wrap/row caches, clamp scroll, preserve
 * selection. Does not paint — caller must full-repaint (never smart-scroll). */
VOID clv_control_set_bounds(CLV_Control *c, const struct Rectangle *bounds);
VOID clv_control_set_pens(CLV_Control *c, const struct CLV_Pens *pens);
VOID clv_control_set_selected(CLV_Control *c, LONG logical_row);
/* Current logical selection, or -1 if none. */
LONG clv_control_get_selected(const CLV_Control *c);
VOID clv_control_make_visible(CLV_Control *c, LONG logical_row);

/* Keyboard NAV_* / ACTIVATE / TOGGLE via handle_input. Default: enabled
 * (TRUE). When FALSE, those keyboard input types are ignored
 * (mouse/scroll unchanged). */
VOID clv_control_set_keyboard_enabled(CLV_Control *c, BOOL enabled);
BOOL clv_control_get_keyboard_enabled(const CLV_Control *c);

/* flags: 0 = full (header + viewport + frame);
 * CLV_RENDER_VIEWPORT_ONLY = scroll/selection update without touching frame. */
#define CLV_RENDER_VIEWPORT_ONLY  (1UL << 0)

VOID clv_control_render(CLV_Control *c, ULONG flags);

/*
 * Repaint the visible content of up to two logical rows (and their gaps
 * when included in each row's paint band). Intended for selection changes
 * without a scroll_y change. Pass -1 for an unused slot. Falls back to a
 * full viewport paint when geometry or clipping cannot be proven valid.
 */
VOID clv_control_render_logical_rows(CLV_Control *c,
                                     LONG row_a,
                                     LONG row_b);

/*
 * Paint after scroll_y changed from previous_scroll_y (already committed).
 * Attempts a viewport pixel shift + exposed-band regional paint when
 * eligible; otherwise falls back to a full viewport paint. Header, frame,
 * and scrollbar are never shifted. Exposed-band paint redraws text and
 * checkbox cells from the current snapshot (first-line-band placement).
 *
 * Do not use this for a selection change that also moved scroll_y (for
 * example make_visible). Smart scroll preserves old selection pixels and
 * may only partially paint the new row — use a full viewport paint instead.
 * Do not use after set_bounds / set_rows / set_columns (caller full-repaints;
 * those paths also cancel any verified-click arm).
 */
VOID clv_control_render_scrolled(CLV_Control *c, LONG previous_scroll_y);

/*
 * Translate one neutral input event. Fills at most one CLV_EventType into
 * caller-owned *result (all fields cleared first). Does not paint; does not
 * invoke application callbacks or allocate Exec messages.
 *
 * Checkbox policy (§D.11 / C7): SELECT_DOWN may arm and may emit
 * SELECTION_CHANGED when the selectable row actually changes;
 * already-selected checkbox SELECT_DOWN arms with no selection event;
 * SELECT_UP may emit CELL_CONTROL only (or nothing on cancel);
 * CLV_INPUT_TOGGLE (Space) toggles the selected row when it has exactly one
 * VISIBLE|ENABLED|INTERACTIVE checkbox column — else no event;
 * NAV_ACTIVATE remains CLV_EVENT_ACTIVATED only (never toggles).
 *
 * On CELL_CONTROL the application should inspect control_type /
 * control_action (checkbox today: CHECKBOX + VALUE_CHANGED), update its
 * authoritative store (typically via row_user_data), then call
 * clv_control_render_logical_rows(c, result->row, -1). Future button /
 * cycle commits are expected to reuse this same return path.
 */
BOOL clv_control_handle_input(CLV_Control *c,
                              const struct CLV_InputEvent *event,
                              struct CLV_Event *result);

LONG clv_control_get_scroll_y(const CLV_Control *c);
VOID clv_control_set_scroll_y(CLV_Control *c, LONG scroll_y);
LONG clv_control_get_content_height(const CLV_Control *c);
LONG clv_control_get_viewport_height(const CLV_Control *c);

#ifdef __cplusplus
}
#endif

#endif /* CLV_CONTROL_H */
