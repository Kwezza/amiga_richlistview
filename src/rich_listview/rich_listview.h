#ifndef RICH_LISTVIEW_H
#define RICH_LISTVIEW_H

/**
 * Experimental custom-drawn ListView control (not GadTools LISTVIEW_KIND).
 *
 * Phase 4–5.5: variable-height wrap, logical-row selection, hit-testing,
 * line/page/proportional scroll, and keyboard NAV_* via neutral
 * RLV_InputEvent. Experimental surface only — not CLV v1 / not final v2.
 *
 * Experimental checkbox cells (RLV_COL_TYPE_CHECKBOX): paint from an
 * internal snapshot (first-line band, plain/tick). Verified SELECT_DOWN/UP
 * arm-and-commit (default RLV_CONTROL_ACTIVATE_SELECT_ROW):
 *   SELECT_DOWN on checkbox (other selectable row) may emit
 *   RLV_EVENT_SELECTION_CHANGED and arms; same-row checkbox arms only;
 *   SELECT_UP commit emits RLV_EVENT_CELL_CONTROL only; cancel emits none.
 * Opt-in RLV_CONTROL_ACTIVATE_KEEP_CURRENT: checkbox SELECT_DOWN arms
 * without changing the current/selected row, scroll, or emitting
 * SELECTION_CHANGED; SELECT_UP still emits CELL_CONTROL only.
 * At most one RLV_EventType per handle_input call.
 * Smart-scroll exposed bands repaint snapshot checkboxes; selection +
 * make_visible scroll uses full viewport paint; set_bounds / set_rows /
 * set_columns / layout invalidate cancel arm. Space → RLV_INPUT_TOGGLE
 * toggles the selected row's sole eligible checkbox; Return / NAV_ACTIVATE
 * remains row activation only.
 * Current-row presentation is separate from selection state via
 * RLV_CurrentRowVisual (full highlight default; marker; none).
 *
 * Cell-control notification (generic — not checkbox-only):
 *   Completed control actions fill RLV_EVENT_CELL_CONTROL synchronously into
 *   a caller-owned RLV_Event (no callbacks, no Exec messages). Checkbox
 *   commits use control_type CHECKBOX + RLV_ACTION_VALUE_CHANGED.
 *   Future button cells can reuse the same event with
 *   RLV_ACTION_PRESSED (previous_value / cell_value may be zero).
 *   Future cycle cells can use RLV_ACTION_VALUE_CHANGED with
 *   previous_value / cell_value as the old/new cycle indices (or packed
 *   values). Event construction is centralised for all cell commits.
 *   Do not retain the RLV_Event (or row_user_data) past the immediate
 *   handle_input return / app handler. control_user_data is deferred
 *   (no per-cell user-data slot on RLV_Cell today).
 *
 * Ownership: the application owns authoritative Booleans. set_rows borrows
 * RLV_Row / control_cells and copies descriptors into the control
 * snapshot; the control does not write through borrowed app memory by
 * default. On CELL_CONTROL, update the app store from the event (typically
 * via row_user_data), then prefer
 * rlv_render_cell_control(control, row, column) for a fully visible
 * checkbox; fall back to rlv_render_logical_rows when that returns a
 * row/viewport result. Use rlv_set_checkbox_value for reject-restore /
 * async snapshot updates without a full set_rows, then the same repaint.
 *
 * Authoritative product path for interactive checkboxes is this package.
 * GadTools clv_cellctl_* under the legacy custom_listview tree is legacy /
 * non-authoritative (frozen; do not extend for new apps).
 *
 * Optional expandable rows (RLV_ENABLE_EXPANDABLE_ROWS, default on):
 *   Mark rows with RLV_ROW_EXPANDABLE and optionally RLV_ROW_EXPANDED.
 *   Add a narrow RLV_COL_TYPE_DISCLOSURE column for +/- controls (cell
 *   flags VISIBLE|ENABLED|INTERACTIVE on expandable rows; empty otherwise).
 *   Collapsed expandable rows use one compact display line; expanded rows
 *   use the full wrapped layout. Multiple rows may stay expanded.
 *   Disclosure +/- is drawn only when the prepared wrap has more than one
 *   display line (single-line expandable rows leave an empty cell so the
 *   control is not a visual no-op; resize may reveal multi-line wrap).
 *   Expansion, checkbox, selection, and focus states are independent.
 *   Mouse: arm/commit on the disclosure control (same verified-click model
 *   as checkboxes) emits CELL_CONTROL with DISCLOSURE + EXPANDED/COLLAPSED;
 *   does not select the row or toggle checkboxes. Keyboard: Right expands,
 *   Left collapses the current row (no-ops when inapplicable); Up/Down never
 *   auto-expand. Programmatic rlv_expand_row / rlv_collapse_row /
 *   rlv_toggle_row / rlv_collapse_all do not emit CELL_CONTROL (same policy
 *   as rlv_set_checkbox_value). Layout rebuild and viewport anchoring run
 *   inside those APIs and the input path; the application must repaint
 *   (typically full viewport) after a disclosure CELL_CONTROL or API call.
 *
 * Display policies (defaults preserve historical appearance):
 *   rlv_set_row_display_mode -- COLLAPSIBLE (default) / ALWAYS_EXPANDED /
 *     SINGLE_LINE. Non-collapsible modes suppress disclosure UI but retain
 *     per-row expand bits. Geometry change: invalidate + rebuild + full paint.
 *   rlv_set_long_word_mode -- CLIP (default) / WRAP for RLV_WRAP_WORD only;
 *     WORD_OR_CHAR and PATH keep explicit column semantics; NONE always clips.
 *   rlv_set_ellipsis_flags -- independent COLLAPSED_CONTENT and HORIZONTAL_CLIP
 *     markers (three hand-drawn dots, not three periods). Default is
 *     COLLAPSED_CONTENT on; HORIZONTAL_CLIP off.
 */

#include "rich_listview/rlv_draw.h"

#include <exec/types.h>
#include <graphics/gfx.h>
#include <graphics/text.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RLV_Control RLV_Control;

typedef enum RLV_CellAlign
{
    RLV_CELL_ALIGN_LEFT = 0,
    RLV_CELL_ALIGN_CENTER,
    RLV_CELL_ALIGN_RIGHT
} RLV_CellAlign;

/* Numeric values match v1 CLV_PIXEL_WRAP_* without including clv_renderer.h. */
typedef enum RLV_WrapMode
{
    RLV_WRAP_NONE = 0,
    RLV_WRAP_WORD,
    RLV_WRAP_WORD_OR_CHAR,
    RLV_WRAP_PATH
} RLV_WrapMode;

typedef enum RLV_RowDividerStyle
{
    RLV_ROW_DIVIDER_NONE = 0,
    RLV_ROW_DIVIDER_SOLID,
    RLV_ROW_DIVIDER_DOTTED
} RLV_RowDividerStyle;

/* RLV_Config.flags */
#define RLV_CFG_NO_KEYBOARD  0x0001U  /* start with NAV_* disabled */

/*
 * Initial expand state for expandable rows when rows are first loaded
 * after rlv_create (and again after a new create). Default 0 opens every
 * expandable row. Per-row RLV_ROW_EXPANDED is overridden by this policy
 * on that first load; later set_rows calls honor row flags so live
 * expand/collapse can be preserved across data refresh.
 */
typedef enum RLV_InitialExpandMode
{
    RLV_INITIAL_EXPAND_ALL_OPEN = 0,
    RLV_INITIAL_EXPAND_ALL_COLLAPSED
} RLV_InitialExpandMode;

/*
 * How embedded cell controls interact with the current/selected row.
 * Default 0 preserves historical SELECT_DOWN selection behaviour.
 * Policies are presentation/input only — they do not rebuild layout.
 */
typedef enum RLV_ControlActivationPolicy
{
    RLV_CONTROL_ACTIVATE_SELECT_ROW = 0, /* checkbox may select its row */
    RLV_CONTROL_ACTIVATE_KEEP_CURRENT    /* checkbox arms/commits without
                                          * changing current/selected row */
} RLV_ControlActivationPolicy;

/*
 * Visual treatment of the current/selected navigation row.
 * Default 0 is the historical full-row highlight. Independent of
 * checkbox state and of RLV_ControlActivationPolicy.
 */
typedef enum RLV_CurrentRowVisual
{
    RLV_CURRENT_ROW_VISUAL_FULL = 0, /* full-row selected fill + text pens */
    RLV_CURRENT_ROW_VISUAL_MARKER,   /* narrow left-edge marker only */
    RLV_CURRENT_ROW_VISUAL_NONE      /* no current-row decoration */
} RLV_CurrentRowVisual;

/*
 * Global row-height / disclosure policy. Default 0 preserves historical
 * collapsible expandable-row behaviour. Changing mode invalidates layout;
 * caller must rebuild (e.g. rlv_set_bounds) and full-repaint -- never
 * smart-scroll. Per-row expand bits are retained while disclosure UI is
 * suppressed so returning to COLLAPSIBLE restores prior row states.
 */
typedef enum RLV_RowDisplayMode
{
    RLV_ROWS_COLLAPSIBLE = 0,   /* wrap + optional collapse/disclosure */
    RLV_ROWS_ALWAYS_EXPANDED,   /* full natural height; no disclosure UI */
    RLV_ROWS_SINGLE_LINE        /* one text line + padding/gap; no disclosure */
} RLV_RowDisplayMode;

/*
 * Control-level default for an indivisible word wider than the column.
 * Applies to RLV_WRAP_WORD only. Explicit RLV_WRAP_WORD_OR_CHAR and
 * RLV_WRAP_PATH keep their column semantics. RLV_WRAP_NONE always clips.
 * Default 0 = clip (compatibility with Truncated-style prefixes).
 */
typedef enum RLV_LongWordMode
{
    RLV_LONG_WORD_CLIP = 0, /* do not character-split; clip the prefix */
    RLV_LONG_WORD_WRAP      /* measured character fallback after breaks fail */
} RLV_LongWordMode;

/* Ellipsis policy flags (independent).
 * Default after rlv_create: RLV_ELLIPSIS_COLLAPSED_CONTENT (horizontal off). */
#define RLV_ELLIPSIS_NONE                 0U
#define RLV_ELLIPSIS_COLLAPSED_CONTENT    (1U << 0) /* hidden wrap lines */
#define RLV_ELLIPSIS_HORIZONTAL_CLIP      (1U << 1) /* width-clipped text */

/*
 * Result of rlv_render_cell_control. Distinguishes a successful local
 * paint, nothing visible, documented fallbacks, and hard errors.
 */
typedef enum RLV_CellControlRepaintResult
{
    RLV_CELL_REPAINT_OK = 0,       /* painted the control rectangle only */
    RLV_CELL_REPAINT_NOT_VISIBLE,  /* control fully outside viewport */
    RLV_CELL_REPAINT_ROW,          /* escalated to logical-row regional paint */
    RLV_CELL_REPAINT_VIEWPORT,     /* escalated to full viewport paint */
    RLV_CELL_REPAINT_ERROR         /* invalid args / missing layout or ops */
} RLV_CellControlRepaintResult;

typedef struct RLV_Config
{
    const struct RLV_DrawOps *draw_ops; /* required */
    APTR draw_context;                  /* opaque; owned by backend */
    struct TextFont *font;              /* borrowed; NULL = use ops default */
    UWORD cell_padding_x;               /* title/body text inset, pixels */
    UWORD cell_padding_y;               /* title/body text inset, pixels */
    UWORD row_gap;                      /* pixels between logical rows */
    UWORD row_divider_style;            /* RLV_RowDividerStyle */
    UWORD flags;                        /* RLV_CFG_*; 0 = defaults */
    /* Appended: zero-init selects RLV_INITIAL_EXPAND_ALL_OPEN. */
    UWORD initial_expand;               /* RLV_InitialExpandMode */
} RLV_Config;

/* RLV_Row.flags */
#define RLV_ROW_NONSELECTABLE  0x0001U
#define RLV_ROW_EXPANDABLE     0x0002U  /* may collapse/expand */
#define RLV_ROW_EXPANDED       0x0004U  /* meaningful only with EXPANDABLE */

/* RLV_Column.flags — low nibble is column type (C2). */
#define RLV_COL_TYPE_MASK      0x000FU
#define RLV_COL_TYPE_TEXT      0x0000U
#define RLV_COL_TYPE_CHECKBOX  0x0001U
#define RLV_COL_TYPE_DISCLOSURE 0x0002U /* +/- expand control */
/* Higher bits of RLV_Column.flags reserved for independent behaviours. */

/* Compact expand-state values (CELL_CONTROL previous_value / cell_value). */
#define RLV_CELL_COLLAPSED  0U
#define RLV_CELL_EXPANDED   1U

/* RLV_Cell.flags */
#define RLV_CELL_F_VISIBLE       0x01U
#define RLV_CELL_F_ENABLED       0x02U
#define RLV_CELL_F_INTERACTIVE   0x04U

/* RLV_Cell.value */
#define RLV_CELL_UNCHECKED  0U
#define RLV_CELL_CHECKED    1U

typedef struct RLV_Column
{
    CONST_STRPTR title;   /* borrowed */
    WORD width_pixels;    /* fixed minimum content width; last column may grow */
    UWORD alignment;      /* RLV_CellAlign */
    UWORD wrap_mode;      /* RLV_WrapMode */
    UWORD flags;          /* RLV_COL_TYPE_* in low nibble; rest reserved */
} RLV_Column;

/*
 * Optional per-cell control descriptor (length == column_count when non-NULL).
 * Entries for non-control columns are ignored (flags/value zero).
 * Checkbox: value is UNCHECKED/CHECKED. Disclosure: value unused (row flags
 * own expansion); flags still gate visibility / interactivity.
 * Application memory is borrowed at set_rows; the control copies into an
 * internal snapshot and does not write through this pointer by default.
 */
typedef struct RLV_Cell
{
    UBYTE flags;  /* RLV_CELL_F_* */
    UBYTE value;  /* RLV_CELL_UNCHECKED / CHECKED (checkbox) */
} RLV_Cell;

typedef struct RLV_Row
{
    CONST_STRPTR *cells;                  /* borrowed; length == column_count */
    const RLV_Cell *control_cells; /* optional; NULL = no controls */
    UWORD flags;                          /* RLV_ROW_NONSELECTABLE,
                                           * RLV_ROW_EXPANDABLE,
                                           * RLV_ROW_EXPANDED, etc. */
    APTR user_data;                       /* optional; borrowed */
} RLV_Row;

/* Neutral input events (IDCMP translated by the application/demo). */
typedef enum RLV_InputType
{
    RLV_INPUT_SELECT_DOWN = 0,
    RLV_INPUT_SELECT_UP,
    RLV_INPUT_POINTER_MOVE,
    RLV_INPUT_SCROLL_LINE_UP,
    RLV_INPUT_SCROLL_LINE_DOWN,
    RLV_INPUT_SCROLL_PAGE_UP,
    RLV_INPUT_SCROLL_PAGE_DOWN,
    RLV_INPUT_SCROLL_POSITION,
    /* Selection-centric keyboard navigation (experimental). */
    RLV_INPUT_NAV_PREV,
    RLV_INPUT_NAV_NEXT,
    RLV_INPUT_NAV_PAGE_UP,
    RLV_INPUT_NAV_PAGE_DOWN,
    RLV_INPUT_NAV_FIRST,
    RLV_INPUT_NAV_LAST,
    RLV_INPUT_NAV_ACTIVATE,
    /* Space: toggle sole eligible checkbox on selected row (C7 / DC-006).
     * Not NAV_ACTIVATE — Return remains row activation only. */
    RLV_INPUT_TOGGLE,
    /* Expandable rows: Right / Left on the current row (no auto-expand). */
    RLV_INPUT_EXPAND_ROW,
    RLV_INPUT_COLLAPSE_ROW
} RLV_InputType;

typedef struct RLV_InputEvent
{
    UWORD type;   /* RLV_InputType */
    WORD  x;      /* window-relative; meaningful for pointer events */
    WORD  y;
    LONG  value;  /* e.g. scroll position */
} RLV_InputEvent;

/* Outcomes from rlv_handle_input (TRUE iff type != RLV_EVENT_NONE).
 * At most one type is filled per call. Selection and cell-control commit are
 * separate handle_input invocations (SELECT_DOWN vs SELECT_UP), never one
 * compound. RLV_EVENT_CELL_CONTROL is the single generic cell-control
 * notification (checkbox today; button/cycle later via control_type +
 * control_action). Delivery is synchronous into caller stack storage. */
typedef enum RLV_EventType
{
    RLV_EVENT_NONE = 0,
    RLV_EVENT_SELECTION_CHANGED,
    RLV_EVENT_SCROLL_CHANGED,
    RLV_EVENT_ACTIVATED,     /* NAV_ACTIVATE only; no selection/scroll change */
    RLV_EVENT_CELL_CONTROL   /* generic cell commit (checkbox SELECT_UP / TOGGLE today) */
} RLV_EventType;

/* Action for RLV_EVENT_CELL_CONTROL (orthogonal to RLV_COL_TYPE_*).
 * VALUE_CHANGED: checkbox toggle today; future cycle index change.
 * PRESSED: future stateless button (no required value transition).
 * EXPANDED / COLLAPSED: disclosure commit (mouse or keyboard). */
typedef enum RLV_CellControlAction
{
    RLV_ACTION_NONE = 0,
    RLV_ACTION_VALUE_CHANGED,
    RLV_ACTION_PRESSED,
    RLV_ACTION_EXPANDED,
    RLV_ACTION_COLLAPSED
} RLV_CellControlAction;

/*
 * Caller-owned event filled by handle_input. Valid only until the caller
 * returns from the immediate handler; do not retain pointers into it.
 * On CELL_CONTROL: row_user_data is a borrowed copy of
 * RLV_Row.user_data at commit (same lifetime as the app row array).
 * Per-cell control_user_data is not provided (deferred). For PRESSED /
 * other stateless actions, previous_value and cell_value may both be zero.
 * LONG value remains scroll_y for scroll/selection — never overload it for
 * cell state (use cell_value).
 */
typedef struct RLV_Event
{
    UWORD type;             /* RLV_EventType */
    LONG  row;              /* logical row when applicable; -1 if none */
    LONG  previous_row;     /* prior selection on SELECTION_CHANGED; else -1 */
    LONG  value;            /* e.g. new scroll_y; unused on CELL_CONTROL */
    UWORD column;           /* CELL_CONTROL */
    UWORD control_type;     /* CELL_CONTROL: RLV_COL_TYPE_* */
    UWORD control_action;   /* CELL_CONTROL: RLV_ACTION_* */
    APTR  row_user_data;    /* CELL_CONTROL: borrowed row user_data at commit */
    UBYTE previous_value;   /* CELL_CONTROL prior cell value (0 if unused) */
    UBYTE cell_value;       /* CELL_CONTROL new cell value (not LONG value) */
} RLV_Event;

RLV_Control *rlv_create(const RLV_Config *cfg);
VOID         rlv_destroy(RLV_Control *control);

BOOL rlv_set_columns(RLV_Control *c,
                             const RLV_Column *cols,
                             UWORD count);
/* Borrow rows/cells/control_cells; copy control descriptors into the
 * internal snapshot. Does not paint. Cancels any verified-click arm. */
BOOL rlv_set_rows(RLV_Control *c,
                          const RLV_Row *rows,
                          ULONG count);
/*
 * Update the internal checkbox snapshot for one cell. Does not write the
 * application's authoritative store and does not paint — caller must
 * update app memory if needed, then render (e.g.
 * rlv_render_logical_rows). Returns FALSE when
 * control/row/column/type/value is invalid or no snapshot exists.
 *
 * Typical uses: reject-restore after CELL_CONTROL, async/external updates
 * without a full set_rows rebuild.
 */
BOOL rlv_set_checkbox_value(RLV_Control *control,
                                    LONG row,
                                    UWORD column,
                                    UBYTE value);

/*
 * Expandable-row operations (no-ops / stubs when
 * RLV_ENABLE_EXPANDABLE_ROWS is 0). Do not emit CELL_CONTROL.
 * Expand/collapse of an already-matching state succeeds as a no-op.
 * Expanding a non-expandable row returns FALSE without changing state.
 * Invalid row returns FALSE. Layout / scroll are updated; caller paints.
 * Collapse All rebuilds once after clearing all expanded bits.
 */
BOOL rlv_expand_row(RLV_Control *c, LONG row);
BOOL rlv_collapse_row(RLV_Control *c, LONG row);
BOOL rlv_toggle_row(RLV_Control *c, LONG row);
VOID rlv_collapse_all(RLV_Control *c);
BOOL rlv_is_row_expandable(const RLV_Control *c, LONG row);
BOOL rlv_is_row_expanded(const RLV_Control *c, LONG row);

VOID rlv_set_cell_padding(RLV_Control *c, UWORD x, UWORD y);
UWORD rlv_get_cell_padding_x(const RLV_Control *c);
UWORD rlv_get_cell_padding_y(const RLV_Control *c);
VOID rlv_set_row_gap(RLV_Control *c, UWORD pixels);
UWORD rlv_get_row_gap(const RLV_Control *c);
/* Body rows only; title/header cells are unaffected. Repaint after changing. */
VOID rlv_set_row_divider_style(RLV_Control *c, UWORD style);
UWORD rlv_get_row_divider_style(const RLV_Control *c);
/* Relayout transaction: rebuild wrap/row caches, clamp scroll, preserve
 * selection. Does not paint — caller must full-repaint (never smart-scroll). */
VOID rlv_set_bounds(RLV_Control *c, const struct Rectangle *bounds);
VOID rlv_set_pens(RLV_Control *c, const struct RLV_Pens *pens);
VOID rlv_set_selected(RLV_Control *c, LONG logical_row);
/* Current logical selection / navigation row, or -1 if none. */
LONG rlv_get_selected(const RLV_Control *c);
VOID rlv_make_visible(RLV_Control *c, LONG logical_row);

/* Keyboard NAV_* / ACTIVATE / TOGGLE via handle_input. Default: enabled
 * (TRUE). When FALSE, those keyboard input types are ignored
 * (mouse/scroll unchanged). */
VOID rlv_set_keyboard_enabled(RLV_Control *c, BOOL enabled);
BOOL rlv_get_keyboard_enabled(const RLV_Control *c);

/*
 * Embedded-control activation vs current/selected row. Default
 * RLV_CONTROL_ACTIVATE_SELECT_ROW. Does not rebuild layout or wrap.
 * Invalid values are ignored (state unchanged).
 */
VOID rlv_set_control_activation_policy(RLV_Control *c, UWORD policy);
UWORD rlv_get_control_activation_policy(const RLV_Control *c);

/*
 * Current/selected-row visual style. Default RLV_CURRENT_ROW_VISUAL_FULL.
 * Presentation only — does not rebuild layout or wrap. Caller must
 * repaint (typically viewport or affected logical rows) after changing.
 * Invalid values are ignored. For MARKER, cell_padding_x >= 2 is
 * recommended so the left-edge bar sits in the text inset.
 */
VOID rlv_set_current_row_visual(RLV_Control *c, UWORD visual);
UWORD rlv_get_current_row_visual(const RLV_Control *c);

/*
 * Row-display / long-word / ellipsis policies. Defaults match current
 * product behaviour (collapsible, long-word clip, collapsed-content
 * ellipsis on, horizontal ellipsis off).
 * Invalid enum/flag bits are ignored (state unchanged). Geometry-changing
 * modes invalidate layout; caller rebuilds once then full-repaints.
 * Ellipsis markers are three compact hand-drawn dots (not "..." text).
 */
VOID rlv_set_row_display_mode(RLV_Control *c, UWORD mode);
UWORD rlv_get_row_display_mode(const RLV_Control *c);
VOID rlv_set_long_word_mode(RLV_Control *c, UWORD mode);
UWORD rlv_get_long_word_mode(const RLV_Control *c);
VOID rlv_set_ellipsis_flags(RLV_Control *c, UWORD flags);
UWORD rlv_get_ellipsis_flags(const RLV_Control *c);

/* flags: 0 = full (header + viewport + frame);
 * RLV_RENDER_VIEWPORT_ONLY = scroll/selection update without touching frame. */
#define RLV_RENDER_VIEWPORT_ONLY  (1UL << 0)

VOID rlv_render(RLV_Control *c, ULONG flags);

/*
 * Repaint the visible content of up to two logical rows (and their gaps
 * when included in each row's paint band). Intended for selection changes
 * without a scroll_y change. Pass -1 for an unused slot. Falls back to a
 * full viewport paint when geometry or clipping cannot be proven valid.
 */
VOID rlv_render_logical_rows(RLV_Control *c,
                                     LONG row_a,
                                     LONG row_b);

/*
 * After an expand/collapse height change: prefer a ScrollRaster of content
 * below the toggled row (when smart scroll is enabled and
 * expand_old_total_h was captured), then paint the toggled row and any
 * exposed band. Otherwise repaint from the first line of logical_row
 * through the viewport bottom. Pass previous_scroll_y from before the
 * layout change.
 *
 * Falls back to a full viewport paint when:
 *   - previous_scroll_y != current scroll_y (anchor/clamp moved content);
 *   - the row starts above the viewport top;
 *   - layout/clip/blit is unsafe.
 * Header and outer frame are never touched.
 */
VOID rlv_render_from_row(RLV_Control *c,
                                 LONG logical_row,
                                 LONG previous_scroll_y);

/*
 * Repaint one embedded cell control from the current snapshot without a
 * layout rebuild. Intended after RLV_EVENT_CELL_CONTROL when only the
 * control value changed. Does not modify scroll_y, selection, arm state,
 * or emit events. Does not paint inside handle_input.
 *
 * Contract (checkbox today; other control types may reuse later):
 *   RLV_CELL_REPAINT_OK          — control fully visible; local paint done
 *   RLV_CELL_REPAINT_NOT_VISIBLE — fully off-screen; nothing drawn
 *   RLV_CELL_REPAINT_ROW         — escalated to rlv_render_logical_rows
 *   RLV_CELL_REPAINT_VIEWPORT    — escalated to full viewport paint
 *   RLV_CELL_REPAINT_ERROR       — invalid args / unresolved geometry
 *
 * Partial visibility, stale layout, or missing clip support escalate; the
 * function never silently leaves stale pixels when a control is visible.
 */
UWORD rlv_render_cell_control(RLV_Control *c,
                                      LONG row,
                                      UWORD column);

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
VOID rlv_render_scrolled(RLV_Control *c, LONG previous_scroll_y);

/*
 * Translate one neutral input event. Fills at most one RLV_EventType into
 * caller-owned *result (all fields cleared first). Does not paint; does not
 * invoke application callbacks or allocate Exec messages.
 *
 * Checkbox policy (§D.11 / C7): under default
 * RLV_CONTROL_ACTIVATE_SELECT_ROW, SELECT_DOWN may arm and may emit
 * SELECTION_CHANGED when the selectable row actually changes;
 * already-selected checkbox SELECT_DOWN arms with no selection event.
 * Under RLV_CONTROL_ACTIVATE_KEEP_CURRENT, checkbox SELECT_DOWN arms
 * only (no selection / make_visible / SELECTION_CHANGED).
 * SELECT_UP may emit CELL_CONTROL only (or nothing on cancel);
 * RLV_INPUT_TOGGLE (Space) toggles the selected row when it has exactly one
 * VISIBLE|ENABLED|INTERACTIVE checkbox column — else no event;
 * NAV_ACTIVATE remains RLV_EVENT_ACTIVATED only (never toggles).
 *
 * On CELL_CONTROL the application should inspect control_type /
 * control_action (checkbox today: CHECKBOX + VALUE_CHANGED), update its
 * authoritative store (typically via row_user_data), then prefer
 * rlv_render_cell_control(c, result->row, result->column). Future button /
 * cycle commits are expected to reuse this same return path.
 */
BOOL rlv_handle_input(RLV_Control *c,
                              const struct RLV_InputEvent *event,
                              struct RLV_Event *result);

LONG rlv_get_scroll_y(const RLV_Control *c);
VOID rlv_set_scroll_y(RLV_Control *c, LONG scroll_y);
LONG rlv_get_content_height(const RLV_Control *c);
LONG rlv_get_viewport_height(const RLV_Control *c);

#ifdef __cplusplus
}
#endif

#endif /* RICH_LISTVIEW_H */
