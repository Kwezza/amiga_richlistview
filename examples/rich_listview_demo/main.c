/**
 * Example — Custom Control Demo (Phase E4 event notification closure)
 *
 * Demonstrates:
 *   - Experimental RLV_Control with highlighted fixed header + viewport
 *   - Variable-height logical rows with pixel word wrapping
 *   - Whole logical-row selection (highlight excludes row_gap)
 *   - Mouse hit-testing via RLV_InputEvent
 *   - Keyboard NAV_* (cursor / Shift+cursor / Ctrl+cursor / Return) and
 *     Space → RLV_INPUT_TOGGLE for the selected row's sole checkbox
 *   - App-owned active_control focus pointer (no globals)
 *   - Non-selectable logical rows
 *   - Pixel SCROLLER_KIND synced to scroll_y (line/page/proportional)
 *   - Resizable window: IDCMP_NEWSIZE relayout + full control repaint
 *   - Experimental checkbox column (On): verified SELECT_DOWN/UP;
 *     default other-row checkbox may emit SELECTION_CHANGED then
 *     CELL_CONTROL; same-row arms only; cancel emits none.
 *     Opt-in KEEP_CURRENT: checkbox arms/commits without moving the
 *     current row. Space → RLV_INPUT_TOGGLE; Return → NAV_ACTIVATE only.
 *     Keys A / V cycle activation policy and current-row visual.
 *     Interactive / display-only / disabled rows. App owns authoritative
 *     Booleans; each RLV_Row.user_data holds a stable numeric row tag.
 *     On CELL_CONTROL the demo finds the store by tag, syncs the Boolean,
 *     then rlv_render_cell_control (row fallback when needed).
 *   - Read-only TEXT_KIND status field under the ListView showing the
 *     latest row-related event with logical index and tag (selection,
 *     activation, CELL_CONTROL). Policy changes also update this status.
 *   - Settings menu selects pending divider / X pad / Y pad / row gap /
 *     row-display / long-word / ellipsis policies; Apply commits them
 *     transactionally (recreate + one full paint). Apply stays disabled
 *     while pending matches applied.
 *
 *   - Optional expandable rows (RLV_ENABLE_EXPANDABLE_ROWS): narrow
 *     disclosure column (+/-), RLV_ROW_EXPANDABLE / EXPANDED flags,
 *     Right/Left expand/collapse, key C = Collapse All. Disclosure and
 *     checkbox states remain independent. CELL_CONTROL reports
 *     DISCLOSURE + EXPANDED/COLLAPSED; app syncs row flags then
 *     rlv_render_from_row (shift-blit or tail paint when scroll unchanged).
 *
 *   - Optional sorting (RLV_ENABLE_SORTING / make rich-listview-demo-sort):
 *     click sortable headers to sort; triangle indicator; Name nocase,
 *     Date via CUSTOM DateStamp (context), Pos unsigned, On boolean.
 *     Fixed heading is a top sort barrier. Status line shows
 *     sort + view/source/tag. Logging twin: rich-listview-demo-sort-log.
 *
 *   - Optional column resize (RLV_ENABLE_COLUMN_RESIZE /
 *     make rich-listview-demo-colresize or -sort-resize): drag header
 *     dividers (narrow hit zone) for two-column exchange; XOR guide +
 *     clipped highlight title preview; disclosure/On columns locked.
 *     Right button cancels. Key R resets widths. COLUMN_RESIZED updates
 *     the status line. Keep delivering MOUSEMOVE while dragging (ReportMouse).
 *
 * Required modules: rlv_*.o (incl. wrap + checkbox + expand/disclosure),
 *                   rlv_backend_amiga_v36.o, rlv_platform.o
 * Sorting build also links rlv_sort.o.
 * Column-resize builds also link rlv_column_resize.o.
 * Deliberately excluded: clv_renderer_*.o, clv_selection.o, clv_pixel_wrap.o,
 *                        ASCII formatters, clv_cellctl_* (legacy GadTools path)
 *
 * Optional logging build (make rich-listview-demo-log) also links
 * rlv_log.o and writes PROGDIR:rlv.log.
 *
 * Optional console stdout traces (make rich-listview-demo-console) enable
 * DEMO_ENABLE_CONSOLE; default builds compile printf/fflush to no-ops so
 * CLI/Output Window traffic stays off on low-res Workbench screens.
 */

#include "rich_listview/rich_listview.h"
#include "rich_listview/backends/rlv_backend_amiga_v36.h"
#include "rich_listview/rlv_log.h"
#include "rich_listview/rlv_bench_internal.h"

#include <exec/types.h>
#include <intuition/intuition.h>
#include <libraries/gadtools.h>
#include <graphics/gfxmacros.h>
#include <devices/inputevent.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <proto/graphics.h>
#include <proto/dos.h>
#include <libraries/dos.h>

#include <stdio.h>
#include <string.h>

/*
 * Demo-only stdout traces. Off unless DEMO_ENABLE_CONSOLE is defined.
 * do/while(0) — not ((void)0) — so VBCC does not emit warning 153.
 */
#ifdef DEMO_ENABLE_CONSOLE
#define DEMO_PRINTF(...)  do { printf(__VA_ARGS__); } while (0)
#define DEMO_FFLUSH()     do { fflush(stdout); } while (0)
#else
#define DEMO_PRINTF(...)  do { } while (0)
#define DEMO_FFLUSH()     do { } while (0)
#endif

long __stack = 80000L;

#define GID_SCROLL 1
#define GID_APPLY  2
#define GID_BENCH  3
#define GID_EVENT_STATUS  8
#define NUM_COLS   6
#define DEMO_EVENT_TEXT_LEN 160

/* Settings menu command / value packing (nm_UserData). */
#define DEMO_MENU_CMD_DIVIDER  1
#define DEMO_MENU_CMD_XPAD     2
#define DEMO_MENU_CMD_YPAD     3
#define DEMO_MENU_CMD_GAP      4
#define DEMO_MENU_CMD_RESET    5
#define DEMO_MENU_CMD_ROWDISP  6
#define DEMO_MENU_CMD_LONGWORD 7
#define DEMO_MENU_CMD_ELLIPSIS 8
#define DEMO_MENU_CMD_INITEXP  9
#define DEMO_MENU_ID(cmd, val) ((ULONG)(((ULONG)(cmd) << 8) | (ULONG)(val)))
#define DEMO_MENU_CMD(id)      ((UWORD)(((ULONG)(id) >> 8) & 0xFFUL))
#define DEMO_MENU_VAL(id)      ((UWORD)((ULONG)(id) & 0xFFUL))

/* Ellipsis submenu value bits (toggle independently). */
#define DEMO_ELLIP_VAL_COLLAPSED  1
#define DEMO_ELLIP_VAL_HORIZONTAL 2
#ifdef RLV_ENABLE_BENCHMARKS
#define DEMO_MAX_ROWS 96
#define RLV_BENCH_NAV_STEPS 50
#define RLV_BENCH_PREPARE_RUNS 3
#define RLV_BENCH_WARMUP_STEPS 5
#else
#define DEMO_MAX_ROWS 9
#endif

/* Classic Return / Space / A / V / C raw keys (not OS3.2-only names). */
#define DEMO_RAWKEY_RETURN  0x44
#define DEMO_RAWKEY_SPACE   0x40
#define DEMO_RAWKEY_A       0x20
#define DEMO_RAWKEY_V       0x34
#define DEMO_RAWKEY_C       0x33
#define DEMO_RAWKEY_R       0x13  /* R — reset column widths when resize on */

/* Interior padding around the control + scroller strip. */
#define DEMO_PAD              8
/* Must match RLV_FRAME_WIDTH / RLV_DIVIDER_WIDTH in the control. */
#define DEMO_CTRL_FRAME_W     1
#define DEMO_CTRL_DIVIDER_W   1
/* Minimum outer control height (viewport + frame + header still derived). */
#define DEMO_MIN_CTRL_H       48

#ifdef RLV_ENABLE_LOGGING
#define DEMO_IDCMP_EXTRA IDCMP_INTUITICKS
#else
#define DEMO_IDCMP_EXTRA 0UL
#endif

static LONG g_demo_refresh_depth = 0;
#if defined(RLV_ENABLE_COLUMN_RESIZE) && (RLV_ENABLE_COLUMN_RESIZE != 0)
static BOOL g_demo_resize_report_mouse = FALSE;
#endif
static BOOL g_demo_gadgets_detached = FALSE;

/*
 * Authoritative visual-test settings. Defaults match historical startup:
 * dotted dividers, X pad 1, Y pad 2, row gap 1, collapsible rows,
 * long-word clip, collapsed-content ellipsis on, expandable rows start open.
 */
typedef struct DemoSettings
{
    UWORD divider_style;
    UWORD padding_x;
    UWORD padding_y;
    UWORD row_gap;
    UWORD row_display_mode;
    UWORD long_word_mode;
    UWORD ellipsis_flags;
    UWORD initial_expand;
} DemoSettings;

static DemoSettings g_demo_applied;
static DemoSettings g_demo_pending;
static struct Menu *g_demo_menu_strip = 0;
static struct Gadget *g_demo_apply_gad = 0;

static UWORD g_demo_activation_policy =
    (UWORD)RLV_CONTROL_ACTIVATE_SELECT_ROW;
static UWORD g_demo_current_row_visual =
    (UWORD)RLV_CURRENT_ROW_VISUAL_FULL;
/* Fixed column+divider content width (pixels); set once after font scale. */
static WORD g_demo_fixed_content_w = 0;
static WORD g_demo_min_ctrl_w = 0;

/*
 * Cached layout metrics derived from the window interior.
 * Control owns frame/header/viewport insets; demo only places the outer
 * control rectangle, scroller, status, and Apply strip.
 */
typedef struct DemoGeom
{
    WORD ctrl_left;
    WORD ctrl_top;
    WORD ctrl_w;
    WORD ctrl_h;
    WORD scroll_left;
    WORD scroll_top;
    WORD scroll_w;
    WORD scroll_h;
    WORD status_left;
    WORD status_top;
    WORD status_w;
    WORD status_h;
    WORD apply_left;
    WORD apply_top;
    WORD apply_w;
    WORD apply_h;
    WORD bench_left;
    WORD bench_top;
    WORD bench_w;
    WORD bench_h;
} DemoGeom;

/*
 * NewMenu template for Settings. CHECKIT groups are mutually exclusive
 * (no MENUTOGGLE so re-selecting the checked item does not clear it).
 * Checked state is synchronised from g_demo_pending after LayoutMenus.
 */
static struct NewMenu g_demo_newmenu[] =
{
    { NM_TITLE, "Settings", NULL, 0, 0, NULL },
    { NM_ITEM,  "Column dividers", NULL, 0, 0, NULL },
    { NM_SUB,   "None",   NULL, CHECKIT, ~1,  (APTR)DEMO_MENU_ID(DEMO_MENU_CMD_DIVIDER, RLV_ROW_DIVIDER_NONE) },
    { NM_SUB,   "Solid",  NULL, CHECKIT, ~2,  (APTR)DEMO_MENU_ID(DEMO_MENU_CMD_DIVIDER, RLV_ROW_DIVIDER_SOLID) },
    { NM_SUB,   "Dotted", NULL, CHECKIT, ~4,  (APTR)DEMO_MENU_ID(DEMO_MENU_CMD_DIVIDER, RLV_ROW_DIVIDER_DOTTED) },
    { NM_ITEM,  "X padding", NULL, 0, 0, NULL },
    { NM_SUB,   "0", NULL, CHECKIT, ~1,  (APTR)DEMO_MENU_ID(DEMO_MENU_CMD_XPAD, 0) },
    { NM_SUB,   "1", NULL, CHECKIT, ~2,  (APTR)DEMO_MENU_ID(DEMO_MENU_CMD_XPAD, 1) },
    { NM_SUB,   "2", NULL, CHECKIT, ~4,  (APTR)DEMO_MENU_ID(DEMO_MENU_CMD_XPAD, 2) },
    { NM_SUB,   "3", NULL, CHECKIT, ~8,  (APTR)DEMO_MENU_ID(DEMO_MENU_CMD_XPAD, 3) },
    { NM_SUB,   "4", NULL, CHECKIT, ~16, (APTR)DEMO_MENU_ID(DEMO_MENU_CMD_XPAD, 4) },
    { NM_ITEM,  "Y padding", NULL, 0, 0, NULL },
    { NM_SUB,   "0", NULL, CHECKIT, ~1,  (APTR)DEMO_MENU_ID(DEMO_MENU_CMD_YPAD, 0) },
    { NM_SUB,   "1", NULL, CHECKIT, ~2,  (APTR)DEMO_MENU_ID(DEMO_MENU_CMD_YPAD, 1) },
    { NM_SUB,   "2", NULL, CHECKIT, ~4,  (APTR)DEMO_MENU_ID(DEMO_MENU_CMD_YPAD, 2) },
    { NM_SUB,   "3", NULL, CHECKIT, ~8,  (APTR)DEMO_MENU_ID(DEMO_MENU_CMD_YPAD, 3) },
    { NM_SUB,   "4", NULL, CHECKIT, ~16, (APTR)DEMO_MENU_ID(DEMO_MENU_CMD_YPAD, 4) },
    { NM_ITEM,  "Row gap", NULL, 0, 0, NULL },
    { NM_SUB,   "0", NULL, CHECKIT, ~1,  (APTR)DEMO_MENU_ID(DEMO_MENU_CMD_GAP, 0) },
    { NM_SUB,   "1", NULL, CHECKIT, ~2,  (APTR)DEMO_MENU_ID(DEMO_MENU_CMD_GAP, 1) },
    { NM_SUB,   "2", NULL, CHECKIT, ~4,  (APTR)DEMO_MENU_ID(DEMO_MENU_CMD_GAP, 2) },
    { NM_SUB,   "3", NULL, CHECKIT, ~8,  (APTR)DEMO_MENU_ID(DEMO_MENU_CMD_GAP, 3) },
    { NM_SUB,   "4", NULL, CHECKIT, ~16, (APTR)DEMO_MENU_ID(DEMO_MENU_CMD_GAP, 4) },
    { NM_ITEM,  NM_BARLABEL, NULL, 0, 0, NULL },
    { NM_ITEM,  "Row display", NULL, 0, 0, NULL },
    { NM_SUB,   "Collapsible", NULL, CHECKIT, ~1,
      (APTR)DEMO_MENU_ID(DEMO_MENU_CMD_ROWDISP, RLV_ROWS_COLLAPSIBLE) },
    { NM_SUB,   "Always expanded", NULL, CHECKIT, ~2,
      (APTR)DEMO_MENU_ID(DEMO_MENU_CMD_ROWDISP, RLV_ROWS_ALWAYS_EXPANDED) },
    { NM_SUB,   "Single line only", NULL, CHECKIT, ~4,
      (APTR)DEMO_MENU_ID(DEMO_MENU_CMD_ROWDISP, RLV_ROWS_SINGLE_LINE) },
    { NM_ITEM,  "Start rows", NULL, 0, 0, NULL },
    { NM_SUB,   "All open", NULL, CHECKIT, ~1,
      (APTR)DEMO_MENU_ID(DEMO_MENU_CMD_INITEXP, RLV_INITIAL_EXPAND_ALL_OPEN) },
    { NM_SUB,   "All collapsed", NULL, CHECKIT, ~2,
      (APTR)DEMO_MENU_ID(DEMO_MENU_CMD_INITEXP,
                         RLV_INITIAL_EXPAND_ALL_COLLAPSED) },
    { NM_ITEM,  "Long words", NULL, 0, 0, NULL },
    { NM_SUB,   "Preserve clipping", NULL, CHECKIT, ~1,
      (APTR)DEMO_MENU_ID(DEMO_MENU_CMD_LONGWORD, RLV_LONG_WORD_CLIP) },
    { NM_SUB,   "Wrap by character", NULL, CHECKIT, ~2,
      (APTR)DEMO_MENU_ID(DEMO_MENU_CMD_LONGWORD, RLV_LONG_WORD_WRAP) },
    { NM_ITEM,  "Ellipsis", NULL, 0, 0, NULL },
    { NM_SUB,   "Hidden collapsed text", NULL, CHECKIT | MENUTOGGLE, 0,
      (APTR)DEMO_MENU_ID(DEMO_MENU_CMD_ELLIPSIS, DEMO_ELLIP_VAL_COLLAPSED) },
    { NM_SUB,   "Horizontally clipped", NULL, CHECKIT | MENUTOGGLE, 0,
      (APTR)DEMO_MENU_ID(DEMO_MENU_CMD_ELLIPSIS, DEMO_ELLIP_VAL_HORIZONTAL) },
    { NM_ITEM,  NM_BARLABEL, NULL, 0, 0, NULL },
    { NM_ITEM,  "Reset to defaults", NULL, 0, 0,
      (APTR)DEMO_MENU_ID(DEMO_MENU_CMD_RESET, 0) },
    { NM_END,   NULL, NULL, 0, 0, NULL }
};

/* Latest outer control box (window-relative); used for click-to-focus. */
static DemoGeom g_demo_geom;
/*
 * Persistent GTTX_Text buffer (GadTools borrows the pointer on updates).
 * g_demo_event_status_gad is set/cleared with the gadget chain.
 */
static char g_demo_event_text[DEMO_EVENT_TEXT_LEN] =
    "Last control event: none";
static struct Gadget *g_demo_event_status_gad = 0;
#ifdef RLV_ENABLE_BENCHMARKS
static BOOL g_demo_bench_mode = FALSE;
#endif

static VOID demo_paint(RLV_Control *control);
static VOID demo_paint_viewport(RLV_Control *control);
static VOID demo_sync_scroller(struct Window *win,
                               struct Gadget *scroller,
                               RLV_Control *control,
                               LONG *last_top);
static BOOL demo_apply_input(RLV_Control *control,
                             struct Window *win,
                             struct Gadget *scroller,
                             const RLV_InputEvent *inev,
                             LONG *last_top);
static VOID demo_update_status_text(struct Window *win, CONST_STRPTR text);
static VOID demo_init_default_settings(DemoSettings *out);
static BOOL demo_settings_equal(const DemoSettings *a,
                                const DemoSettings *b);
static VOID demo_copy_settings(DemoSettings *dst,
                               const DemoSettings *src);
static VOID demo_sync_menu_checks_from_pending(void);
static VOID demo_update_apply_enabled_state(struct Window *win);
static BOOL demo_apply_pending_settings(RLV_Control **control_io,
                                        RLV_BackendV36 *backend,
                                        struct Window *win,
                                        struct Gadget *scroller,
                                        const RLV_Pens *pens,
                                        const RLV_Column *columns,
                                        BOOL keyboard_off,
                                        LONG *last_top);
static BOOL demo_setup_menus(APTR vi);
static VOID demo_cleanup_menus(struct Window *win);
static BOOL demo_handle_menu_selection(ULONG menu_number,
                                       struct Window *win);
#ifdef RLV_ENABLE_BENCHMARKS
static VOID demo_bench_configure(RLV_Control *control,
                                 struct Window *win,
                                 struct Screen *screen);
static VOID demo_run_benchmarks(RLV_Control *control,
                                struct Window *win,
                                struct Gadget *scroller,
                                LONG *last_top);
#endif

static const RLV_Column g_columns[NUM_COLS] = {
    /* Disclosure first; Name wraps; Type/Date truncates; Description wraps;
     * Status uses WORD so Long-words policy is testable; On = checkbox.
     * Sorting builds retitle Type→Date and Status→Pos at install time.
     * NO_RESIZE locks disclosure and checkbox (resize builds). */
    { "",            2 * 8, RLV_CELL_ALIGN_CENTER, RLV_WRAP_NONE,
      (UWORD)(RLV_COL_TYPE_DISCLOSURE | RLV_COL_F_NO_RESIZE) },
    { "Name",        10 * 8, RLV_CELL_ALIGN_LEFT,   RLV_WRAP_WORD_OR_CHAR, 0 },
#if defined(RLV_ENABLE_SORTING) && (RLV_ENABLE_SORTING != 0)
    { "Date",        11 * 8, RLV_CELL_ALIGN_LEFT,   RLV_WRAP_NONE, 0 },
#else
    { "Type",         8 * 8, RLV_CELL_ALIGN_LEFT,   RLV_WRAP_NONE, 0 },
#endif
    { "Description", 18 * 8, RLV_CELL_ALIGN_LEFT,   RLV_WRAP_WORD_OR_CHAR, 0 },
    { "Status",       6 * 8, RLV_CELL_ALIGN_LEFT,   RLV_WRAP_WORD, 0 },
    { "On",           3 * 8, RLV_CELL_ALIGN_CENTER, RLV_WRAP_NONE,
      (UWORD)(RLV_COL_TYPE_CHECKBOX | RLV_COL_F_NO_RESIZE) }
};

static const char *g_row0[NUM_COLS] = {
    "", "Alpha", "Tool", "A compact cleanup utility", "Ready", ""
};
static const char *g_row1[NUM_COLS] = {
    "",
    "Beta Package With A Rather Long Name",
    "Library",
    "Shared runtime routines used by several Workbench tools",
    "Idle", ""
};
static const char *g_row2[NUM_COLS] = {
    "",
    "Gamma",
    "Tool",
    "Long description text that wraps across two or three lines when the "
    "Description column is narrow enough for word wrapping",
    "Busy", ""
};
static const char *g_row3[NUM_COLS] = {
    "", "-- Category --", "Heading", "Non-selectable section title row", "-", ""
};
static const char *g_row4[NUM_COLS] = {
    "", "Delta", "Data", "Configuration presets", "Ready", ""
};
static const char *g_row5[NUM_COLS] = {
    "",
    "Epsilon",
    "Tool",
    "Another tool entry with enough descriptive prose to occupy four "
    "wrapped display lines inside one logical row so the whole block "
    "stays a single geometric item with one row gap after it",
    "Done", ""
};
static const char *g_row6[NUM_COLS] = {
    "", "Zeta", "Library", "Support module", "Ready", ""
};
static const char *g_row7[NUM_COLS] = {
    "",
    "Eta Path/Example:Deep_Folder-Name",
    "PathTest",
    "PATH wrap is not enabled here; this Name column uses WORD_OR_CHAR",
    "Test", ""
};
static const char *g_row8[NUM_COLS] = {
    "",
    "Alpha", /* duplicate Name vs row 0 — distinct tag proves identity */
    "VeryLongUnbrokenTypeToken",
    "Same visible Name as row 0; identify via row tag, not text",
    "Truncated", ""
};

/*
 * App-owned authoritative checkbox store (column DEMO_CB_COL = On).
 * Disclosure interactivity lives in the same control_cells row vectors.
 * set_rows borrows these descriptors and copies into the control snapshot;
 * the control never writes back. Each RLV_Row.user_data holds a stable
 * numeric tag (DEMO_TAG_BASE + i). On CELL_CONTROL the demo syncs the
 * On Boolean by scanning for that tag (not by visible text). Disclosure
 * expand/collapse still syncs RLV_Row.flags via the event row index.
 */
#define DEMO_DISC_COL  0
#define DEMO_CB_COL    5
#define DEMO_TAG_BASE  1000UL
#define DEMO_CB_ON \
    (UBYTE)(RLV_CELL_F_VISIBLE | RLV_CELL_F_ENABLED \
            | RLV_CELL_F_INTERACTIVE)
#define DEMO_CB_DISPLAY \
    (UBYTE)(RLV_CELL_F_VISIBLE | RLV_CELL_F_ENABLED)
#define DEMO_CB_DISABLED \
    (UBYTE)(RLV_CELL_F_VISIBLE)
#define DEMO_DISC_ON \
    (UBYTE)(RLV_CELL_F_VISIBLE | RLV_CELL_F_ENABLED \
            | RLV_CELL_F_INTERACTIVE)

/* Initial templates (copied into the mutable store at demo_init_rows). */
static const RLV_Cell g_ctrl_init0[NUM_COLS] = {
    { DEMO_DISC_ON, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
    { DEMO_CB_ON, RLV_CELL_CHECKED }
};
static const RLV_Cell g_ctrl_init1[NUM_COLS] = {
    { DEMO_DISC_ON, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
    { DEMO_CB_ON, RLV_CELL_UNCHECKED }
};
static const RLV_Cell g_ctrl_init2[NUM_COLS] = {
    { DEMO_DISC_ON, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
    { DEMO_CB_ON, RLV_CELL_CHECKED }
};
static const RLV_Cell g_ctrl_init3[NUM_COLS] = {
    { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
    { 0, 0 } /* heading: no disclosure / checkbox */
};
static const RLV_Cell g_ctrl_init4[NUM_COLS] = {
    { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
    { DEMO_CB_DISPLAY, RLV_CELL_CHECKED } /* non-expandable; display-only cb */
};
static const RLV_Cell g_ctrl_init5[NUM_COLS] = {
    { DEMO_DISC_ON, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
    { DEMO_CB_ON, RLV_CELL_UNCHECKED }
};
static const RLV_Cell g_ctrl_init6[NUM_COLS] = {
    { DEMO_DISC_ON, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
    { DEMO_CB_DISABLED, RLV_CELL_CHECKED } /* ghosted checkbox */
};
static const RLV_Cell g_ctrl_init7[NUM_COLS] = {
    { DEMO_DISC_ON, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
    { DEMO_CB_ON, RLV_CELL_CHECKED }
};
static const RLV_Cell g_ctrl_init8[NUM_COLS] = {
    { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
    { DEMO_CB_ON, RLV_CELL_UNCHECKED } /* non-expandable */
};

static RLV_Cell g_demo_ctrl_store[DEMO_MAX_ROWS][NUM_COLS];
static RLV_Row g_rows[DEMO_MAX_ROWS];
static UWORD g_demo_row_count = 9;

static VOID demo_init_rows(void)
{
    UWORD i;
    static CONST_STRPTR *cells[DEMO_MAX_ROWS];
    static CONST_STRPTR row_name_buf[DEMO_MAX_ROWS][NUM_COLS];
    static const RLV_Cell *inits[9];

    cells[0] = (CONST_STRPTR *)g_row0;
    cells[1] = (CONST_STRPTR *)g_row1;
    cells[2] = (CONST_STRPTR *)g_row2;
    cells[3] = (CONST_STRPTR *)g_row3;
    cells[4] = (CONST_STRPTR *)g_row4;
    cells[5] = (CONST_STRPTR *)g_row5;
    cells[6] = (CONST_STRPTR *)g_row6;
    cells[7] = (CONST_STRPTR *)g_row7;
    cells[8] = (CONST_STRPTR *)g_row8;

    inits[0] = g_ctrl_init0;
    inits[1] = g_ctrl_init1;
    inits[2] = g_ctrl_init2;
    inits[3] = g_ctrl_init3;
    inits[4] = g_ctrl_init4;
    inits[5] = g_ctrl_init5;
    inits[6] = g_ctrl_init6;
    inits[7] = g_ctrl_init7;
    inits[8] = g_ctrl_init8;

#if defined(RLV_ENABLE_SORTING) && (RLV_ENABLE_SORTING != 0)
    /*
     * Put the fixed heading first so one contiguous data run sorts below
     * it. A mid-list barrier made Pos look "unsorted" globally (1,2,10
     * above the heading and 1,3,4… below) even though each run was correct.
     */
    cells[0] = (CONST_STRPTR *)g_row3;
    cells[1] = (CONST_STRPTR *)g_row0;
    cells[2] = (CONST_STRPTR *)g_row1;
    cells[3] = (CONST_STRPTR *)g_row2;
    cells[4] = (CONST_STRPTR *)g_row4;
    cells[5] = (CONST_STRPTR *)g_row5;
    cells[6] = (CONST_STRPTR *)g_row6;
    cells[7] = (CONST_STRPTR *)g_row7;
    cells[8] = (CONST_STRPTR *)g_row8;
    inits[0] = g_ctrl_init3;
    inits[1] = g_ctrl_init0;
    inits[2] = g_ctrl_init1;
    inits[3] = g_ctrl_init2;
    inits[4] = g_ctrl_init4;
    inits[5] = g_ctrl_init5;
    inits[6] = g_ctrl_init6;
    inits[7] = g_ctrl_init7;
    inits[8] = g_ctrl_init8;
#endif

    memset(g_demo_ctrl_store, 0, sizeof(g_demo_ctrl_store));
    for (i = 0; i < 9 && i < DEMO_MAX_ROWS; i++) {
        memcpy(g_demo_ctrl_store[i], inits[i], sizeof(g_demo_ctrl_store[i]));
    }

#ifdef RLV_ENABLE_BENCHMARKS
    if (g_demo_row_count > 9) {
        static char name_buf[DEMO_MAX_ROWS][64];
        static char desc_buf[DEMO_MAX_ROWS][192];
        static char type_buf[DEMO_MAX_ROWS][32];
        static char status_buf[DEMO_MAX_ROWS][24];
        UWORD template_row;

        for (i = 9; i < g_demo_row_count && i < DEMO_MAX_ROWS; i++) {
            template_row = (UWORD)((i - 4) % 5);
            if (template_row == 0) {
                strcpy(name_buf[i], "Benchmark Alpha");
                strcpy(type_buf[i], "Tool");
                strcpy(status_buf[i], "Ready");
            } else if (template_row == 1) {
                strcpy(name_buf[i], "Benchmark Beta Extended Package");
                strcpy(type_buf[i], "Library");
                strcpy(status_buf[i], "Idle");
            } else if (template_row == 2) {
                strcpy(name_buf[i], "Benchmark Gamma");
                strcpy(type_buf[i], "Data");
                strcpy(status_buf[i], "Busy");
            } else if (template_row == 3) {
                strcpy(name_buf[i], "Benchmark Delta");
                strcpy(type_buf[i], "Tool");
                strcpy(status_buf[i], "Done");
            } else {
                strcpy(name_buf[i], "Benchmark Epsilon Path:Work/Deep_Item");
                strcpy(type_buf[i], "PathTest");
                strcpy(status_buf[i], "Test");
            }
            sprintf(name_buf[i], "%s %u", name_buf[i], (unsigned)i);
            sprintf(desc_buf[i],
                    "Benchmark row %u uses a longer description so wrapped "
                    "layout, viewport motion, and selection redraws can be "
                    "measured repeatedly on a larger dataset.",
                    (unsigned)i);
            row_name_buf[i][0] = "";
            row_name_buf[i][1] = name_buf[i];
            row_name_buf[i][2] = type_buf[i];
            row_name_buf[i][3] = desc_buf[i];
            row_name_buf[i][4] = status_buf[i];
            row_name_buf[i][5] = "";
            cells[i] = row_name_buf[i];

            g_demo_ctrl_store[i][DEMO_DISC_COL].flags = DEMO_DISC_ON;
            g_demo_ctrl_store[i][DEMO_DISC_COL].value = 0;
            g_demo_ctrl_store[i][DEMO_CB_COL].flags = DEMO_CB_ON;
            g_demo_ctrl_store[i][DEMO_CB_COL].value = (UBYTE)((i & 1U)
                ? RLV_CELL_CHECKED
                : RLV_CELL_UNCHECKED);
        }
    }
#endif

    memset(g_rows, 0, sizeof(g_rows));
    for (i = 0; i < g_demo_row_count && i < DEMO_MAX_ROWS; i++) {
        g_rows[i].cells = cells[i];
        g_rows[i].control_cells = g_demo_ctrl_store[i];
        g_rows[i].flags = 0;
        /* Stable numeric tag; cast to APTR for the opaque row field. */
        g_rows[i].user_data = (APTR)(DEMO_TAG_BASE + (ULONG)i);
    }
    /* Non-selectable category/heading row (sort barrier when sorting on). */
#if defined(RLV_ENABLE_SORTING) && (RLV_ENABLE_SORTING != 0)
    if (g_demo_row_count > 0) {
        g_rows[0].flags = (UWORD)(RLV_ROW_NONSELECTABLE | RLV_ROW_SORT_FIXED);
    }
#else
    if (g_demo_row_count > 3) {
        g_rows[3].flags = (UWORD)(RLV_ROW_NONSELECTABLE | RLV_ROW_SORT_FIXED);
    }
#endif
    /* Expandable rows (heading / Delta / duplicate-Alpha stay plain). */
#if defined(RLV_ENABLE_SORTING) && (RLV_ENABLE_SORTING != 0)
    /* After heading-first reorder: data at 1=Alpha,2=Beta,3=Gamma,5=Eps… */
    if (g_demo_row_count > 1) {
        g_rows[1].flags = (UWORD)(g_rows[1].flags
                                  | RLV_ROW_EXPANDABLE
                                  | RLV_ROW_EXPANDED);
    }
    if (g_demo_row_count > 2) {
        g_rows[2].flags = (UWORD)(g_rows[2].flags
                                  | RLV_ROW_EXPANDABLE
                                  | RLV_ROW_EXPANDED);
    }
    if (g_demo_row_count > 3) {
        g_rows[3].flags = (UWORD)(g_rows[3].flags
                                  | RLV_ROW_EXPANDABLE
                                  | RLV_ROW_EXPANDED);
    }
    if (g_demo_row_count > 5) {
        g_rows[5].flags = (UWORD)(g_rows[5].flags
                                  | RLV_ROW_EXPANDABLE
                                  | RLV_ROW_EXPANDED);
    }
    if (g_demo_row_count > 6) {
        g_rows[6].flags = (UWORD)(g_rows[6].flags
                                  | RLV_ROW_EXPANDABLE
                                  | RLV_ROW_EXPANDED);
    }
    if (g_demo_row_count > 7) {
        g_rows[7].flags = (UWORD)(g_rows[7].flags
                                  | RLV_ROW_EXPANDABLE
                                  | RLV_ROW_EXPANDED);
    }
#else
    if (g_demo_row_count > 0) {
        g_rows[0].flags = (UWORD)(g_rows[0].flags
                                  | RLV_ROW_EXPANDABLE
                                  | RLV_ROW_EXPANDED);
    }
    if (g_demo_row_count > 1) {
        g_rows[1].flags = (UWORD)(g_rows[1].flags
                                  | RLV_ROW_EXPANDABLE
                                  | RLV_ROW_EXPANDED);
    }
    if (g_demo_row_count > 2) {
        g_rows[2].flags = (UWORD)(g_rows[2].flags
                                  | RLV_ROW_EXPANDABLE
                                  | RLV_ROW_EXPANDED);
    }
    if (g_demo_row_count > 5) {
        g_rows[5].flags = (UWORD)(g_rows[5].flags
                                  | RLV_ROW_EXPANDABLE
                                  | RLV_ROW_EXPANDED);
    }
    if (g_demo_row_count > 6) {
        g_rows[6].flags = (UWORD)(g_rows[6].flags
                                  | RLV_ROW_EXPANDABLE
                                  | RLV_ROW_EXPANDED);
    }
    if (g_demo_row_count > 7) {
        g_rows[7].flags = (UWORD)(g_rows[7].flags
                                  | RLV_ROW_EXPANDABLE
                                  | RLV_ROW_EXPANDED);
    }
#endif
#ifdef RLV_ENABLE_BENCHMARKS
    for (i = 9; i < g_demo_row_count && i < DEMO_MAX_ROWS; i++) {
        g_rows[i].flags = (UWORD)(g_rows[i].flags
                                  | RLV_ROW_EXPANDABLE
                                  | RLV_ROW_EXPANDED);
    }
#endif
}

#if defined(RLV_ENABLE_SORTING) && (RLV_ENABLE_SORTING != 0)
/*
 * App-owned date records for column 2 (Date). Indexed by source/attachment
 * row. Display strings are formatted separately — never parsed for sorting.
 * Spec.context points at this array for the life of the installed specs.
 */
typedef struct DemoSortRecord
{
    struct DateStamp stamp; /* ds_Days / ds_Minute / ds_Tick */
} DemoSortRecord;

/* Pos aligned to heading-first row order (index 0 = Category barrier). */
static const char *g_sort_pos[9] = {
    "-", "10", "2", "1", "5", "3", "7", "4", "1"
};

/*
 * Display labels (DD-Mon-YYYY). Chronological order differs from ASCII
 * lexical order of these strings. Sources 2 (Beta) and 6 (Zeta) share
 * the same DateStamp for equal-key stability checks.
 */
static const char *g_sort_date_text[9] = {
    "-",            /* Category barrier */
    "04-Aug-2026",  /* Alpha  src1 — newest */
    "17-Jan-2025",  /* Beta   src2 — equal with Zeta */
    "29-Feb-2024",  /* Gamma  src3 — leap day */
    "01-Dec-2025",  /* Delta  src4 */
    "03-Mar-2024",  /* Epsilon src5 */
    "17-Jan-2025",  /* Zeta   src6 — equal with Beta */
    "11-Nov-2023",  /* Eta    src7 — oldest */
    "15-Jun-2025"   /* Alpha  src8 — duplicate Name */
};

/* Precomputed Amiga epoch days (1978-01-01); minutes/ticks 0 unless noted. */
static const LONG g_sort_date_days[9] = {
    0L,
    17747L, /* 04-Aug-2026 */
    17183L, /* 17-Jan-2025 */
    16860L, /* 29-Feb-2024 */
    17501L, /* 01-Dec-2025 */
    16863L, /* 03-Mar-2024 */
    17183L, /* 17-Jan-2025 — equal key with Beta */
    16750L, /* 11-Nov-2023 */
    17332L  /* 15-Jun-2025 */
};

static DemoSortRecord g_demo_date_records[DEMO_MAX_ROWS];
static char g_demo_date_display[9][16];

static LONG demo_compare_dates(const RLV_Control *control,
                               ULONG source_a,
                               ULONG source_b,
                               UWORD column,
                               APTR context)
{
    const DemoSortRecord *records;
    const struct DateStamp *a;
    const struct DateStamp *b;

    (void)control;
    (void)column;
    if (context == 0) {
        return 0;
    }
    records = (const DemoSortRecord *)context;
    if (source_a >= (ULONG)DEMO_MAX_ROWS || source_b >= (ULONG)DEMO_MAX_ROWS) {
        return 0;
    }
    a = &records[source_a].stamp;
    b = &records[source_b].stamp;
    if (a->ds_Days < b->ds_Days) {
        return -1;
    }
    if (a->ds_Days > b->ds_Days) {
        return 1;
    }
    if (a->ds_Minute < b->ds_Minute) {
        return -1;
    }
    if (a->ds_Minute > b->ds_Minute) {
        return 1;
    }
    if (a->ds_Tick < b->ds_Tick) {
        return -1;
    }
    if (a->ds_Tick > b->ds_Tick) {
        return 1;
    }
    return 0;
}

/*
 * Name: case-insensitive text
 * Date: CUSTOM DateStamp via context (DEFAULT_DESC = recent-first)
 * Pos: unsigned numeric (1,2,10 not 1,10,2)
 * On: boolean checkbox snapshot
 * Description: ordinary display text (not a sort key)
 */
static const RLV_SortSpec g_demo_sort_specs[] = {
    { 1, (UWORD)RLV_SORT_TEXT_NOCASE, 0, 0, 0, 0 },
    { 2, (UWORD)RLV_SORT_CUSTOM, (UWORD)RLV_SORT_F_DEFAULT_DESC, 0,
      demo_compare_dates, (APTR)g_demo_date_records },
    { 4, (UWORD)RLV_SORT_UNSIGNED, 0, 0, 0, 0 },
    { 5, (UWORD)RLV_SORT_BOOLEAN, 0, 0, 0, 0 }
};

static VOID demo_apply_sort_data(RLV_Column *columns)
{
    UWORD i;
    CONST_STRPTR *cell_ptrs;

    if (columns != 0) {
        columns[2].title = "Date";
        columns[4].title = "Pos";
    }
    for (i = 0; i < 9 && i < g_demo_row_count; i++) {
        g_demo_date_records[i].stamp.ds_Days = g_sort_date_days[i];
        g_demo_date_records[i].stamp.ds_Minute = 0L;
        g_demo_date_records[i].stamp.ds_Tick = 0L;
        strcpy(g_demo_date_display[i], g_sort_date_text[i]);

        cell_ptrs = (CONST_STRPTR *)g_rows[i].cells;
        if (cell_ptrs != 0) {
            cell_ptrs[2] = g_demo_date_display[i];
            cell_ptrs[4] = g_sort_pos[i];
        }
    }
}

static BOOL demo_install_sort(RLV_Control *control, RLV_Column *columns)
{
    if (control == 0) {
        return FALSE;
    }
    demo_apply_sort_data(columns);
    /* Re-attach after Date/Pos cell mutation so wrap uses new strings. */
    if (!rlv_set_rows(control, g_rows, g_demo_row_count)) {
        return FALSE;
    }
    if (!rlv_set_sort_specs(control, g_demo_sort_specs,
                            (UWORD)(sizeof(g_demo_sort_specs)
                                    / sizeof(g_demo_sort_specs[0])))) {
        return FALSE;
    }
    return TRUE;
}
#endif /* RLV_ENABLE_SORTING */

#if defined(RLV_ENABLE_COLUMN_RESIZE) && (RLV_ENABLE_COLUMN_RESIZE != 0)
/*
 * Enable interactive resize after set_columns. Disclosure and On stay locked
 * via RLV_COL_F_NO_RESIZE. Raise Name/Date minima so clamping is visible.
 */
static BOOL demo_install_column_resize(RLV_Control *control, WORD font_w)
{
    WORD min_name;
    WORD min_mid;

    if (control == 0) {
        return FALSE;
    }
    rlv_set_column_resize_enabled(control, TRUE);
    min_name = (WORD)(4 * font_w);
    min_mid = (WORD)(6 * font_w);
    if (min_name < 16) {
        min_name = 16;
    }
    if (min_mid < 24) {
        min_mid = 24;
    }
    (void)rlv_set_column_min_width(control, 1, min_name); /* Name */
    (void)rlv_set_column_min_width(control, 2, min_mid);  /* Date/Type */
    (void)rlv_set_column_min_width(control, 3, min_mid);  /* Description */
    (void)rlv_set_column_min_width(control, 4, (WORD)(3 * font_w)); /* Status */
    return TRUE;
}

static VOID demo_sync_resize_report_mouse(struct Window *win,
                                          RLV_Control *control)
{
    BOOL active;

    if (win == 0) {
        return;
    }
    active = (control != 0 && rlv_column_resize_is_active(control))
             ? TRUE : FALSE;
    if (active && !g_demo_resize_report_mouse) {
        /* Prefer flag toggle — ReportMouse() calling convention varies. */
        win->Flags |= WFLG_REPORTMOUSE;
        g_demo_resize_report_mouse = TRUE;
        RLV_LOG("COLUMN_RESIZE ReportMouse ON");
    } else if (!active && g_demo_resize_report_mouse) {
        win->Flags &= (ULONG)~WFLG_REPORTMOUSE;
        g_demo_resize_report_mouse = FALSE;
        RLV_LOG("COLUMN_RESIZE ReportMouse OFF");
    }
}
#endif /* RLV_ENABLE_COLUMN_RESIZE */

/*
 * Sum of configured column pixel widths + divider strips.
 * Matches control layout (fixed widths; no proportional shrink).
 */
static WORD demo_fixed_content_width(const RLV_Column *cols,
                                     UWORD count)
{
    UWORD i;
    WORD sum;
    WORD w;

    sum = 0;
    if (cols == 0 || count == 0) {
        return 0;
    }
    for (i = 0; i < count; i++) {
        w = cols[i].width_pixels;
        if (w < 1) {
            w = 1;
        }
        sum = (WORD)(sum + w);
    }
    if (count > 1) {
        sum = (WORD)(sum + (WORD)((count - 1) * DEMO_CTRL_DIVIDER_W));
    }
    return sum;
}

static WORD demo_scroll_width(WORD font_w)
{
    WORD scroll_w;

    scroll_w = (WORD)(font_w + 8);
    if (scroll_w < 16) {
        scroll_w = 16;
    }
    return scroll_w;
}

/*
 * Outer control width needed for fixed columns: content + flat outline.
 * Viewport is inset by DEMO_CTRL_FRAME_W on each side.
 */
static WORD demo_min_ctrl_width(WORD fixed_content_w)
{
    WORD w;

    w = (WORD)(fixed_content_w + (2 * DEMO_CTRL_FRAME_W));
    if (w < 1) {
        w = 1;
    }
    return w;
}

static CONST_STRPTR demo_control_type_name(UWORD type)
{
    if (type == (UWORD)RLV_COL_TYPE_CHECKBOX) {
        return "CHECKBOX";
    }
    if (type == (UWORD)RLV_COL_TYPE_DISCLOSURE) {
        return "DISCLOSURE";
    }
    return "UNKNOWN";
}

static CONST_STRPTR demo_control_action_name(UWORD action)
{
    if (action == (UWORD)RLV_ACTION_VALUE_CHANGED) {
        return "CHANGED";
    }
    if (action == (UWORD)RLV_ACTION_PRESSED) {
        return "PRESSED";
    }
    if (action == (UWORD)RLV_ACTION_EXPANDED) {
        return "EXPANDED";
    }
    if (action == (UWORD)RLV_ACTION_COLLAPSED) {
        return "COLLAPSED";
    }
    return "NONE";
}

static CONST_STRPTR demo_activation_policy_name(UWORD policy)
{
    if (policy == (UWORD)RLV_CONTROL_ACTIVATE_KEEP_CURRENT) {
        return "KEEP_CURRENT";
    }
    return "SELECT_ROW";
}

static CONST_STRPTR demo_current_row_visual_name(UWORD visual)
{
    if (visual == (UWORD)RLV_CURRENT_ROW_VISUAL_MARKER) {
        return "MARKER";
    }
    if (visual == (UWORD)RLV_CURRENT_ROW_VISUAL_NONE) {
        return "NONE";
    }
    return "FULL";
}

static VOID demo_init_default_settings(DemoSettings *out)
{
    if (out == 0) {
        return;
    }
    out->divider_style = (UWORD)RLV_ROW_DIVIDER_DOTTED;
    out->padding_x = 1;
    out->padding_y = 2;
    out->row_gap = 1;
    out->row_display_mode = (UWORD)RLV_ROWS_COLLAPSIBLE;
    out->long_word_mode = (UWORD)RLV_LONG_WORD_CLIP;
    out->ellipsis_flags = (UWORD)RLV_ELLIPSIS_COLLAPSED_CONTENT;
    out->initial_expand = (UWORD)RLV_INITIAL_EXPAND_ALL_OPEN;
}

static BOOL demo_settings_equal(const DemoSettings *a,
                                const DemoSettings *b)
{
    if (a == 0 || b == 0) {
        return FALSE;
    }
    if (a->divider_style != b->divider_style) {
        return FALSE;
    }
    if (a->padding_x != b->padding_x) {
        return FALSE;
    }
    if (a->padding_y != b->padding_y) {
        return FALSE;
    }
    if (a->row_gap != b->row_gap) {
        return FALSE;
    }
    if (a->row_display_mode != b->row_display_mode) {
        return FALSE;
    }
    if (a->long_word_mode != b->long_word_mode) {
        return FALSE;
    }
    if (a->ellipsis_flags != b->ellipsis_flags) {
        return FALSE;
    }
    if (a->initial_expand != b->initial_expand) {
        return FALSE;
    }
    return TRUE;
}

static VOID demo_copy_settings(DemoSettings *dst,
                               const DemoSettings *src)
{
    if (dst == 0 || src == 0) {
        return;
    }
    dst->divider_style = src->divider_style;
    dst->padding_x = src->padding_x;
    dst->padding_y = src->padding_y;
    dst->row_gap = src->row_gap;
    dst->row_display_mode = src->row_display_mode;
    dst->long_word_mode = src->long_word_mode;
    dst->ellipsis_flags = src->ellipsis_flags;
    dst->initial_expand = src->initial_expand;
}

static BOOL demo_menu_item_matches_pending(ULONG item_id,
                                           const DemoSettings *pending)
{
    UWORD cmd;
    UWORD val;

    if (pending == 0) {
        return FALSE;
    }
    cmd = DEMO_MENU_CMD(item_id);
    val = DEMO_MENU_VAL(item_id);
    if (cmd == (UWORD)DEMO_MENU_CMD_DIVIDER) {
        return (BOOL)(val == pending->divider_style);
    }
    if (cmd == (UWORD)DEMO_MENU_CMD_XPAD) {
        return (BOOL)(val == pending->padding_x);
    }
    if (cmd == (UWORD)DEMO_MENU_CMD_YPAD) {
        return (BOOL)(val == pending->padding_y);
    }
    if (cmd == (UWORD)DEMO_MENU_CMD_GAP) {
        return (BOOL)(val == pending->row_gap);
    }
    if (cmd == (UWORD)DEMO_MENU_CMD_ROWDISP) {
        return (BOOL)(val == pending->row_display_mode);
    }
    if (cmd == (UWORD)DEMO_MENU_CMD_INITEXP) {
        return (BOOL)(val == pending->initial_expand);
    }
    if (cmd == (UWORD)DEMO_MENU_CMD_LONGWORD) {
        return (BOOL)(val == pending->long_word_mode);
    }
    if (cmd == (UWORD)DEMO_MENU_CMD_ELLIPSIS) {
        if (val == (UWORD)DEMO_ELLIP_VAL_COLLAPSED) {
            return (BOOL)((pending->ellipsis_flags
                           & RLV_ELLIPSIS_COLLAPSED_CONTENT) != 0);
        }
        if (val == (UWORD)DEMO_ELLIP_VAL_HORIZONTAL) {
            return (BOOL)((pending->ellipsis_flags
                           & RLV_ELLIPSIS_HORIZONTAL_CLIP) != 0);
        }
        return FALSE;
    }
    return FALSE;
}

static VOID demo_sync_menu_checks_from_pending(void)
{
    struct Menu *menu;
    struct MenuItem *item;
    struct MenuItem *sub;
    ULONG item_id;

    if (g_demo_menu_strip == 0) {
        return;
    }
    for (menu = g_demo_menu_strip; menu != 0; menu = menu->NextMenu) {
        for (item = menu->FirstItem; item != 0; item = item->NextItem) {
            for (sub = item->SubItem; sub != 0; sub = sub->NextItem) {
                if ((sub->Flags & CHECKIT) != 0) {
                    item_id = (ULONG)GTMENUITEM_USERDATA(sub);
                    if (demo_menu_item_matches_pending(item_id,
                                                       &g_demo_pending)) {
                        sub->Flags |= CHECKED;
                    } else {
                        sub->Flags &= (UWORD)~CHECKED;
                    }
                }
            }
            if ((item->Flags & CHECKIT) != 0) {
                item_id = (ULONG)GTMENUITEM_USERDATA(item);
                if (demo_menu_item_matches_pending(item_id,
                                                   &g_demo_pending)) {
                    item->Flags |= CHECKED;
                } else {
                    item->Flags &= (UWORD)~CHECKED;
                }
            }
        }
    }
}

/* Must ClearMenuStrip before mutating MenuItem flags on an attached strip. */
static VOID demo_resync_menu_strip(struct Window *win)
{
    if (win != 0 && g_demo_menu_strip != 0) {
        ClearMenuStrip(win);
    }
    demo_sync_menu_checks_from_pending();
    if (win != 0 && g_demo_menu_strip != 0) {
        SetMenuStrip(win, g_demo_menu_strip);
    }
}

static VOID demo_update_apply_enabled_state(struct Window *win)
{
    BOOL dirty;

    dirty = (BOOL)!demo_settings_equal(&g_demo_pending, &g_demo_applied);
    if (g_demo_apply_gad == 0 || win == 0) {
        return;
    }
    GT_SetGadgetAttrs(g_demo_apply_gad,
                      win,
                      NULL,
                      GA_Disabled, dirty ? FALSE : TRUE,
                      TAG_DONE);
}

static BOOL demo_setup_menus(APTR vi)
{
    if (vi == 0) {
        return FALSE;
    }
    if (g_demo_menu_strip != 0) {
        FreeMenus(g_demo_menu_strip);
        g_demo_menu_strip = 0;
    }
    g_demo_menu_strip = CreateMenus(g_demo_newmenu, TAG_END);
    if (g_demo_menu_strip == 0) {
        RLV_LOG("FAIL CreateMenus");
        return FALSE;
    }
    /* NewLook tag is V39; ignored on older gadtools, layout still succeeds. */
    if (!LayoutMenus(g_demo_menu_strip, vi,
                     GTMN_NewLookMenus, TRUE,
                     TAG_END)) {
        RLV_LOG("FAIL LayoutMenus");
        FreeMenus(g_demo_menu_strip);
        g_demo_menu_strip = 0;
        return FALSE;
    }
    demo_sync_menu_checks_from_pending();
    return TRUE;
}

static VOID demo_cleanup_menus(struct Window *win)
{
    if (win != 0 && g_demo_menu_strip != 0) {
        ClearMenuStrip(win);
    }
    if (g_demo_menu_strip != 0) {
        FreeMenus(g_demo_menu_strip);
        g_demo_menu_strip = 0;
    }
}

static BOOL demo_handle_menu_selection(ULONG menu_number,
                                       struct Window *win)
{
    struct MenuItem *menu_item;
    ULONG item_id;
    UWORD cmd;
    UWORD val;
    BOOL changed;
    BOOL dirty;

    changed = FALSE;
    while (menu_number != MENUNULL) {
        menu_item = ItemAddress(g_demo_menu_strip, menu_number);
        if (menu_item == 0) {
            break;
        }
        item_id = (ULONG)GTMENUITEM_USERDATA(menu_item);
        cmd = DEMO_MENU_CMD(item_id);
        val = DEMO_MENU_VAL(item_id);

        if (cmd == (UWORD)DEMO_MENU_CMD_DIVIDER) {
            if (val <= (UWORD)RLV_ROW_DIVIDER_DOTTED
                && g_demo_pending.divider_style != val) {
                g_demo_pending.divider_style = val;
                changed = TRUE;
            }
        } else if (cmd == (UWORD)DEMO_MENU_CMD_XPAD) {
            if (val <= 4 && g_demo_pending.padding_x != val) {
                g_demo_pending.padding_x = val;
                changed = TRUE;
            }
        } else if (cmd == (UWORD)DEMO_MENU_CMD_YPAD) {
            if (val <= 4 && g_demo_pending.padding_y != val) {
                g_demo_pending.padding_y = val;
                changed = TRUE;
            }
        } else if (cmd == (UWORD)DEMO_MENU_CMD_GAP) {
            if (val <= 4 && g_demo_pending.row_gap != val) {
                g_demo_pending.row_gap = val;
                changed = TRUE;
            }
        } else if (cmd == (UWORD)DEMO_MENU_CMD_ROWDISP) {
            if ((val == (UWORD)RLV_ROWS_COLLAPSIBLE
                 || val == (UWORD)RLV_ROWS_ALWAYS_EXPANDED
                 || val == (UWORD)RLV_ROWS_SINGLE_LINE)
                && g_demo_pending.row_display_mode != val) {
                g_demo_pending.row_display_mode = val;
                changed = TRUE;
            }
        } else if (cmd == (UWORD)DEMO_MENU_CMD_INITEXP) {
            if ((val == (UWORD)RLV_INITIAL_EXPAND_ALL_OPEN
                 || val == (UWORD)RLV_INITIAL_EXPAND_ALL_COLLAPSED)
                && g_demo_pending.initial_expand != val) {
                g_demo_pending.initial_expand = val;
                changed = TRUE;
            }
        } else if (cmd == (UWORD)DEMO_MENU_CMD_LONGWORD) {
            if ((val == (UWORD)RLV_LONG_WORD_CLIP
                 || val == (UWORD)RLV_LONG_WORD_WRAP)
                && g_demo_pending.long_word_mode != val) {
                g_demo_pending.long_word_mode = val;
                changed = TRUE;
            }
        } else if (cmd == (UWORD)DEMO_MENU_CMD_ELLIPSIS) {
            if (val == (UWORD)DEMO_ELLIP_VAL_COLLAPSED) {
                g_demo_pending.ellipsis_flags ^=
                    (UWORD)RLV_ELLIPSIS_COLLAPSED_CONTENT;
                changed = TRUE;
            } else if (val == (UWORD)DEMO_ELLIP_VAL_HORIZONTAL) {
                g_demo_pending.ellipsis_flags ^=
                    (UWORD)RLV_ELLIPSIS_HORIZONTAL_CLIP;
                changed = TRUE;
            }
        } else if (cmd == (UWORD)DEMO_MENU_CMD_RESET) {
            demo_init_default_settings(&g_demo_pending);
            changed = TRUE;
        }

        menu_number = menu_item->NextSelect;
    }

    if (changed) {
        demo_resync_menu_strip(win);
        demo_update_apply_enabled_state(win);
        dirty = (BOOL)!demo_settings_equal(&g_demo_pending, &g_demo_applied);
        if (dirty) {
            demo_update_status_text(win,
                                    "Settings changed - select Apply");
        }
    }
    return TRUE;
}

static VOID demo_update_status_text(struct Window *win, CONST_STRPTR text)
{
    if (win == 0 || text == 0) {
        return;
    }
    strncpy(g_demo_event_text, text, (size_t)(DEMO_EVENT_TEXT_LEN - 1));
    g_demo_event_text[DEMO_EVENT_TEXT_LEN - 1] = '\0';
    if (g_demo_event_status_gad != 0) {
        GT_SetGadgetAttrs(g_demo_event_status_gad,
                          win,
                          NULL,
                          GTTX_Text, (ULONG)g_demo_event_text,
                          TAG_DONE);
    }
}

static VOID demo_apply_policies(RLV_Control *control)
{
    if (control == 0) {
        return;
    }
    rlv_set_control_activation_policy(control, g_demo_activation_policy);
    rlv_set_current_row_visual(control, g_demo_current_row_visual);
}

static VOID demo_apply_display_settings(RLV_Control *control,
                                        const DemoSettings *settings)
{
    if (control == 0 || settings == 0) {
        return;
    }
    rlv_set_row_display_mode(control, settings->row_display_mode);
    rlv_set_long_word_mode(control, settings->long_word_mode);
    rlv_set_ellipsis_flags(control, settings->ellipsis_flags);
}

/*
 * Write RLV_ROW_EXPANDED on every expandable app row from the creation
 * policy (used when Start rows changes, or at init).
 */
static VOID demo_apply_initial_expand_to_rows(UWORD mode)
{
    ULONG i;
    BOOL want_open;

    want_open = (mode != (UWORD)RLV_INITIAL_EXPAND_ALL_COLLAPSED)
                ? TRUE : FALSE;
    for (i = 0; i < g_demo_row_count && i < (ULONG)DEMO_MAX_ROWS; i++) {
        if ((g_rows[i].flags & RLV_ROW_EXPANDABLE) == 0) {
            continue;
        }
        if (want_open) {
            g_rows[i].flags = (UWORD)(g_rows[i].flags | RLV_ROW_EXPANDED);
        } else {
            g_rows[i].flags = (UWORD)(g_rows[i].flags
                                      & (UWORD)~RLV_ROW_EXPANDED);
        }
    }
}

/* Copy live control expand bits into app-owned RLV_Row.flags. */
static VOID demo_sync_row_expand_flags_from_control(RLV_Control *control)
{
    ULONG i;

    if (control == 0) {
        return;
    }
    for (i = 0; i < g_demo_row_count && i < (ULONG)DEMO_MAX_ROWS; i++) {
        if ((g_rows[i].flags & RLV_ROW_EXPANDABLE) == 0) {
            continue;
        }
        if (rlv_is_row_expanded(control, (LONG)i)) {
            g_rows[i].flags = (UWORD)(g_rows[i].flags | RLV_ROW_EXPANDED);
        } else {
            g_rows[i].flags = (UWORD)(g_rows[i].flags
                                      & (UWORD)~RLV_ROW_EXPANDED);
        }
    }
}

/*
 * After a recreate that re-applied the creation policy, restore expand
 * bits from g_rows (interactive state) without emitting CELL_CONTROL.
 */
static VOID demo_restore_expand_from_row_flags(RLV_Control *control)
{
    ULONG i;
    BOOL want;
    BOOL have;

    if (control == 0) {
        return;
    }
    for (i = 0; i < g_demo_row_count && i < (ULONG)DEMO_MAX_ROWS; i++) {
        if ((g_rows[i].flags & RLV_ROW_EXPANDABLE) == 0) {
            continue;
        }
        want = ((g_rows[i].flags & RLV_ROW_EXPANDED) != 0) ? TRUE : FALSE;
        have = rlv_is_row_expanded(control, (LONG)i);
        if (want == have) {
            continue;
        }
        if (want) {
            (VOID)rlv_expand_row(control, (LONG)i);
        } else {
            (VOID)rlv_collapse_row(control, (LONG)i);
        }
    }
}

static VOID demo_cycle_activation_policy(struct Window *win,
                                         RLV_Control *control)
{
    if (g_demo_activation_policy
        == (UWORD)RLV_CONTROL_ACTIVATE_SELECT_ROW) {
        g_demo_activation_policy =
            (UWORD)RLV_CONTROL_ACTIVATE_KEEP_CURRENT;
    } else {
        g_demo_activation_policy =
            (UWORD)RLV_CONTROL_ACTIVATE_SELECT_ROW;
    }
    if (control != 0) {
        rlv_set_control_activation_policy(control,
                                          g_demo_activation_policy);
    }
    sprintf(g_demo_event_text, "Activation: %s (A toggles)",
            demo_activation_policy_name(g_demo_activation_policy));
    g_demo_event_text[DEMO_EVENT_TEXT_LEN - 1] = '\0';
    demo_update_status_text(win, g_demo_event_text);
    DEMO_PRINTF("%s\n", g_demo_event_text);
    DEMO_FFLUSH();
    RLV_LOGF("demo activation_policy=%u",
             (unsigned)g_demo_activation_policy);
}

static VOID demo_cycle_current_row_visual(struct Window *win,
                                          RLV_Control *control)
{
    if (g_demo_current_row_visual
        == (UWORD)RLV_CURRENT_ROW_VISUAL_FULL) {
        g_demo_current_row_visual =
            (UWORD)RLV_CURRENT_ROW_VISUAL_MARKER;
    } else if (g_demo_current_row_visual
               == (UWORD)RLV_CURRENT_ROW_VISUAL_MARKER) {
        g_demo_current_row_visual =
            (UWORD)RLV_CURRENT_ROW_VISUAL_NONE;
    } else {
        g_demo_current_row_visual =
            (UWORD)RLV_CURRENT_ROW_VISUAL_FULL;
    }
    if (control != 0) {
        rlv_set_current_row_visual(control, g_demo_current_row_visual);
        /* Presentation-only: repaint viewport, no layout rebuild. */
        rlv_render(control, RLV_RENDER_VIEWPORT_ONLY);
    }
    sprintf(g_demo_event_text,
            "Row visual: %s (V cycles; X Pad>=2 recommended for MARKER)",
            demo_current_row_visual_name(g_demo_current_row_visual));
    g_demo_event_text[DEMO_EVENT_TEXT_LEN - 1] = '\0';
    demo_update_status_text(win, g_demo_event_text);
    DEMO_PRINTF("%s\n", g_demo_event_text);
    DEMO_FFLUSH();
    RLV_LOGF("demo current_row_visual=%u",
             (unsigned)g_demo_current_row_visual);
}

/*
 * Format a row-related event into the persistent buffer and push via
 * GTTX_Text. Always includes logical row index and opaque tag.
 * GadTools borrows the pointer (V36+); do not free or reuse for other text.
 */
static VOID demo_update_event_status(struct Window *win,
                                     RLV_Control *control,
                                     const RLV_Event *ev)
{
    CONST_STRPTR type_name;
    CONST_STRPTR action_name;
    CONST_STRPTR row_name;
    ULONG tag;
#if defined(RLV_ENABLE_SORTING) && (RLV_ENABLE_SORTING != 0)
    LONG view;
    UWORD sort_col;
    UWORD sort_dir;
#endif

    if (win == 0 || ev == 0) {
        return;
    }

    tag = (ULONG)ev->row_user_data;
    type_name = demo_control_type_name(ev->control_type);
    action_name = demo_control_action_name(ev->control_action);
    row_name = "-";
    if (ev->row >= 0 && (UWORD)ev->row < g_demo_row_count
        && g_rows[ev->row].cells != 0
        && g_rows[ev->row].cells[1] != 0) {
        row_name = g_rows[ev->row].cells[1];
    }

#if defined(RLV_ENABLE_SORTING) && (RLV_ENABLE_SORTING != 0)
    view = (control != 0) ? rlv_view_row_of(control, ev->row) : -1;
    sort_col = 0;
    sort_dir = 0;
    (void)rlv_get_sort_state(control, &sort_col, &sort_dir);
#endif

    if (ev->type == (UWORD)RLV_EVENT_SELECTION_CHANGED) {
#if defined(RLV_ENABLE_SORTING) && (RLV_ENABLE_SORTING != 0)
        sprintf(g_demo_event_text,
                "Sel src=%ld view=%ld tag=%lu sort=%u/%s",
                (long)ev->row, (long)view, (unsigned long)tag,
                (unsigned)sort_col,
                (sort_dir == (UWORD)RLV_SORT_DESC) ? "DESC" : "ASC");
#else
        sprintf(g_demo_event_text,
                "Selected row %ld tag %lu",
                (long)ev->row, (unsigned long)tag);
#endif
    } else if (ev->type == (UWORD)RLV_EVENT_ACTIVATED) {
        sprintf(g_demo_event_text,
                "Activated row %ld tag %lu",
                (long)ev->row, (unsigned long)tag);
    } else if (ev->type == (UWORD)RLV_EVENT_SORT_CHANGED) {
#if defined(RLV_ENABLE_SORTING) && (RLV_ENABLE_SORTING != 0)
        sprintf(g_demo_event_text,
                "Sort col=%u %s src=%ld view=%ld tag=%lu",
                (unsigned)ev->column,
                (ev->value == (LONG)RLV_SORT_DESC) ? "DESC" : "ASC",
                (long)ev->row, (long)view, (unsigned long)tag);
#else
        sprintf(g_demo_event_text,
                "Sort changed col=%u",
                (unsigned)ev->column);
#endif
    } else if (ev->type == (UWORD)RLV_EVENT_COLUMN_RESIZED) {
        sprintf(g_demo_event_text,
                "Resize %u/%u %d+%d -> %d+%d %s",
                (unsigned)ev->resize_left,
                (unsigned)ev->resize_right,
                (int)ev->old_left_width,
                (int)ev->old_right_width,
                (int)ev->new_left_width,
                (int)ev->new_right_width,
                (ev->value == RLV_RESIZE_REPAINT_FULL) ? "full" : "reg");
    } else if (ev->type == (UWORD)RLV_EVENT_CELL_CONTROL) {
        if (ev->control_type == (UWORD)RLV_COL_TYPE_CHECKBOX) {
            sprintf(g_demo_event_text,
                    "Checkbox row %ld tag %lu value %u",
                    (long)ev->row, (unsigned long)tag,
                    (unsigned)ev->cell_value);
        } else if (strlen(row_name) > 16) {
            sprintf(g_demo_event_text,
                    "CELL row=%ld tag=%lu col=%u %s %s %u->%u",
                    (long)ev->row,
                    (unsigned long)tag,
                    (unsigned)ev->column,
                    type_name,
                    action_name,
                    (unsigned)ev->previous_value,
                    (unsigned)ev->cell_value);
        } else {
            sprintf(g_demo_event_text,
                    "CELL row=%ld (%s) tag=%lu %s %s %u->%u",
                    (long)ev->row,
                    row_name,
                    (unsigned long)tag,
                    type_name,
                    action_name,
                    (unsigned)ev->previous_value,
                    (unsigned)ev->cell_value);
        }
    } else {
        sprintf(g_demo_event_text,
                "Event type=%u row=%ld tag %lu",
                (unsigned)ev->type, (long)ev->row, (unsigned long)tag);
    }
    g_demo_event_text[DEMO_EVENT_TEXT_LEN - 1] = '\0';

    if (g_demo_event_status_gad != 0) {
        GT_SetGadgetAttrs(g_demo_event_status_gad,
                          win,
                          NULL,
                          GTTX_Text, (ULONG)g_demo_event_text,
                          TAG_DONE);
    }
}

/*
 * Locate the authoritative On Boolean by stable row tag. Returns NULL when
 * the tag is unknown or the row has no checkbox column.
 */
static UBYTE *demo_find_checkbox_by_tag(APTR tag)
{
    UWORD i;

    for (i = 0; i < g_demo_row_count && i < DEMO_MAX_ROWS; i++) {
        if (g_rows[i].user_data == tag
            && g_demo_ctrl_store[i][DEMO_CB_COL].flags != 0) {
            return &g_demo_ctrl_store[i][DEMO_CB_COL].value;
        }
    }
    return 0;
}

/*
 * Derive outer control, scroller, status field, and Apply strip from the
 * usable window interior. Control width is at least g_demo_min_ctrl_w
 * (fixed columns); spare horizontal space stays after the last column.
 * Status TEXT_KIND sits between the ListView and the Apply button.
 */
static VOID demo_compute_geom(struct Window *win,
                              WORD font_w,
                              WORD font_h,
                              DemoGeom *out)
{
    WORD inner_left;
    WORD inner_top;
    WORD inner_right;
    WORD inner_bottom;
    WORD scroll_w;
    WORD action_h;
    WORD status_h;
    WORD strip_h;
    WORD avail_w;
    WORD avail_h;
    WORD ctrl_w;
    WORD ctrl_h;
    WORD min_ctrl_w;

    if (out == 0) {
        return;
    }
    memset(out, 0, sizeof(*out));
    if (win == 0) {
        return;
    }

    scroll_w = demo_scroll_width(font_w);
    action_h = (WORD)(font_h + 6);
    status_h = (WORD)(font_h + 6);
    /* status + pad + Apply strip below the list. */
    strip_h = (WORD)(status_h + DEMO_PAD + action_h);
    min_ctrl_w = g_demo_min_ctrl_w;
    if (min_ctrl_w < 1) {
        min_ctrl_w = 1;
    }

    inner_left = (WORD)(win->BorderLeft + DEMO_PAD);
    inner_top = (WORD)(win->BorderTop + DEMO_PAD);
    inner_right = (WORD)(win->Width - win->BorderRight - DEMO_PAD - 1);
    inner_bottom = (WORD)(win->Height - win->BorderBottom - DEMO_PAD - 1);

    avail_w = (WORD)(inner_right - inner_left + 1);
    avail_h = (WORD)(inner_bottom - inner_top + 1);

    /* Status + Apply strip below the list; gaps DEMO_PAD. */
    avail_h = (WORD)(avail_h - strip_h - DEMO_PAD);
    avail_w = (WORD)(avail_w - scroll_w);

    ctrl_w = avail_w;
    ctrl_h = avail_h;
    if (ctrl_w < min_ctrl_w) {
        ctrl_w = min_ctrl_w;
    }
    if (ctrl_h < DEMO_MIN_CTRL_H) {
        ctrl_h = DEMO_MIN_CTRL_H;
    }

    /* Height only: keep non-inverted if the interior is still tiny. */
    if ((WORD)(inner_top + ctrl_h - 1)
        > (WORD)(inner_bottom - strip_h - DEMO_PAD)) {
        ctrl_h = (WORD)(inner_bottom - strip_h - DEMO_PAD - inner_top + 1);
        if (ctrl_h < 1) {
            ctrl_h = 1;
        }
    }

    out->ctrl_left = inner_left;
    out->ctrl_top = inner_top;
    out->ctrl_w = ctrl_w;
    out->ctrl_h = ctrl_h;
    out->scroll_left = (WORD)(inner_left + ctrl_w);
    out->scroll_top = inner_top;
    out->scroll_w = scroll_w;
    out->scroll_h = ctrl_h;

    out->status_left = inner_left;
    out->status_top = (WORD)(inner_top + ctrl_h + DEMO_PAD);
    out->status_w = (WORD)(ctrl_w + scroll_w);
    out->status_h = status_h;

    out->apply_left = inner_left;
    out->apply_top = (WORD)(out->status_top + status_h + DEMO_PAD);
    out->apply_w = (WORD)(font_w * 8);
    out->apply_h = action_h;
    if (out->apply_w < 56) {
        out->apply_w = 56;
    }

    out->bench_left =
        (WORD)(out->apply_left + out->apply_w + DEMO_PAD);
    out->bench_top = out->apply_top;
    out->bench_w = (WORD)(font_w * 16);
    out->bench_h = action_h;
    if (out->bench_w < 96) {
        out->bench_w = 96;
    }
}

static VOID demo_bounds_from_geom(const DemoGeom *geom,
                                  struct Rectangle *bounds)
{
    if (geom == 0 || bounds == 0) {
        return;
    }
    bounds->MinX = geom->ctrl_left;
    bounds->MinY = geom->ctrl_top;
    bounds->MaxX = (WORD)(geom->ctrl_left + geom->ctrl_w - 1);
    bounds->MaxY = (WORD)(geom->ctrl_top + geom->ctrl_h - 1);
}

/* V37-safe clear: RectFill with BACKGROUNDPEN (no EraseRect). */
static VOID demo_clear_window_interior(struct Window *win,
                                       struct DrawInfo *dri)
{
    struct RastPort *rp;
    UBYTE pen;

    if (win == 0 || win->RPort == 0) {
        return;
    }
    rp = win->RPort;
    pen = 0;
    if (dri != 0 && dri->dri_Pens != 0) {
        pen = (UBYTE)dri->dri_Pens[BACKGROUNDPEN];
    }
    SetAPen(rp, pen);
    RectFill(rp,
             win->BorderLeft,
             win->BorderTop,
             (WORD)(win->Width - win->BorderRight - 1),
             (WORD)(win->Height - win->BorderBottom - 1));
}

/*
 * GadTools caches Width/Height at CreateGadget time. Poking LeftEdge/Width
 * on an existing SCROLLER_KIND does not resize it (AI_AGENT_LAYOUT_GUIDE /
 * amiga_window_resize_template). Rebuild: FreeGadgets → CreateGadget →
 * AddGList. Caller must RemoveGList first when the list is attached.
 */
static VOID demo_destroy_gadgets(struct Gadget **glist_io,
                                 struct Gadget **scroller_io,
                                 struct Gadget **apply_io,
                                 struct Gadget **bench_io)
{
    if (glist_io != 0 && *glist_io != 0) {
        FreeGadgets(*glist_io);
        *glist_io = 0;
    }
    g_demo_event_status_gad = 0;
    g_demo_apply_gad = 0;
    if (scroller_io != 0) {
        *scroller_io = 0;
    }
    if (apply_io != 0) {
        *apply_io = 0;
    }
    if (bench_io != 0) {
        *bench_io = 0;
    }
}

static BOOL demo_create_gadgets(struct Gadget **glist_io,
                                struct Gadget **scroller_io,
                                struct Gadget **apply_io,
                                struct Gadget **bench_io,
                                APTR vi,
                                struct TextAttr *textattr,
                                WORD font_h,
                                const DemoGeom *geom)
{
    struct Gadget *gad;
    struct Gadget *scroller;
    struct Gadget *apply;
    struct Gadget *bench;
    struct NewGadget ng;
    BOOL apply_disabled;

    if (glist_io == 0 || scroller_io == 0 || apply_io == 0
        || bench_io == 0 || vi == 0 || geom == 0) {
        return FALSE;
    }

    *glist_io = 0;
    *scroller_io = 0;
    *apply_io = 0;
    *bench_io = 0;
    g_demo_event_status_gad = 0;
    g_demo_apply_gad = 0;

    gad = CreateContext(glist_io);
    if (gad == 0) {
        RLV_LOG("FAIL CreateContext");
        return FALSE;
    }

    memset(&ng, 0, sizeof(ng));
    ng.ng_LeftEdge = geom->scroll_left;
    ng.ng_TopEdge = geom->scroll_top;
    ng.ng_Width = geom->scroll_w;
    ng.ng_Height = geom->scroll_h;
    ng.ng_GadgetText = NULL;
    ng.ng_TextAttr = textattr;
    ng.ng_GadgetID = GID_SCROLL;
    ng.ng_Flags = 0;
    ng.ng_VisualInfo = vi;

    scroller = gad = CreateGadget(SCROLLER_KIND, gad, &ng,
                                  GTSC_Top, 0,
                                  GTSC_Total, 100,
                                  GTSC_Visible, 50,
                                  GTSC_Arrows, (ULONG)(font_h + 2),
                                  PGA_Freedom, LORIENT_VERT,
                                  TAG_END);
    if (gad == 0) {
        RLV_LOG("FAIL CreateGadget SCROLLER_KIND");
        demo_destroy_gadgets(glist_io, scroller_io, apply_io, bench_io);
        return FALSE;
    }

    /* Read-only status under the ListView (E3). GTTX_Text borrows buffer. */
    ng.ng_LeftEdge = geom->status_left;
    ng.ng_TopEdge = geom->status_top;
    ng.ng_Width = geom->status_w;
    ng.ng_Height = geom->status_h;
    ng.ng_GadgetText = NULL;
    ng.ng_GadgetID = GID_EVENT_STATUS;
    ng.ng_Flags = 0;
    gad = CreateGadget(TEXT_KIND, gad, &ng,
                       GTTX_Text, (ULONG)g_demo_event_text,
                       GTTX_Border, TRUE,
                       TAG_END);
    if (gad == 0) {
        RLV_LOG("FAIL CreateGadget EVENT_STATUS TEXT_KIND");
        demo_destroy_gadgets(glist_io, scroller_io, apply_io, bench_io);
        return FALSE;
    }
    g_demo_event_status_gad = gad;

    apply_disabled =
        (BOOL)demo_settings_equal(&g_demo_pending, &g_demo_applied);
    ng.ng_LeftEdge = geom->apply_left;
    ng.ng_TopEdge = geom->apply_top;
    ng.ng_Width = geom->apply_w;
    ng.ng_Height = geom->apply_h;
    ng.ng_GadgetText = "Apply";
    ng.ng_GadgetID = GID_APPLY;
    ng.ng_Flags = PLACETEXT_IN;
    if (apply_disabled) {
        ng.ng_Flags |= GFLG_DISABLED;
    }
    apply = gad = CreateGadget(BUTTON_KIND, gad, &ng, TAG_END);
    if (gad == 0) {
        RLV_LOG("FAIL CreateGadget APPLY BUTTON_KIND");
        demo_destroy_gadgets(glist_io, scroller_io, apply_io, bench_io);
        return FALSE;
    }
    g_demo_apply_gad = apply;

    bench = 0;
#ifdef RLV_ENABLE_BENCHMARKS
    if (g_demo_bench_mode) {
        ng.ng_LeftEdge = geom->bench_left;
        ng.ng_TopEdge = geom->bench_top;
        ng.ng_Width = geom->bench_w;
        ng.ng_Height = geom->bench_h;
        ng.ng_GadgetText = "Start Benchmark";
        ng.ng_GadgetID = GID_BENCH;
        ng.ng_Flags = PLACETEXT_IN;
        bench = gad = CreateGadget(BUTTON_KIND, gad, &ng, TAG_END);
        if (gad == 0) {
            RLV_LOG("FAIL CreateGadget BENCH BUTTON_KIND");
            demo_destroy_gadgets(glist_io, scroller_io, apply_io, bench_io);
            return FALSE;
        }
    }
#endif

    *scroller_io = scroller;
    *apply_io = apply;
    *bench_io = bench;
    RLV_LOGF("gadgets created scroller=%dx%d @%d,%d",
             (int)geom->scroll_w, (int)geom->scroll_h,
             (int)geom->scroll_left, (int)geom->scroll_top);
    return TRUE;
}

/*
 * Full resize transaction (template rebuild pattern):
 * clear stale pixels → FreeGadgets → CreateGadget at new boxes → AddGList
 * → control relayout → full repaint (never smart-scroll) → scroller sync.
 * Gadgets must already be detached (SIZEVERIFY / RemoveGList).
 */
static VOID demo_handle_newsize(struct Window *win,
                                struct Gadget **glist_io,
                                struct Gadget **scroller_io,
                                struct Gadget **apply_io,
                                struct Gadget **bench_io,
                                APTR vi,
                                struct TextAttr *textattr,
                                struct DrawInfo *dri,
                                RLV_Control *control,
                                WORD font_w,
                                WORD font_h,
                                LONG *last_top)
{
    DemoGeom geom;
    struct Rectangle bounds;

    RLV_LOG("RESIZE demo_handle_newsize begin");
    if (win == 0 || control == 0 || glist_io == 0
        || scroller_io == 0 || apply_io == 0 || bench_io == 0) {
        RLV_LOG("RESIZE fallback/failure reason=null_win_or_control");
        return;
    }

    /* Use final window dimensions from the NEWSIZE message path. */
    demo_compute_geom(win, font_w, font_h, &geom);
    demo_bounds_from_geom(&geom, &bounds);

    RLV_LOGF("RESIZE demo window=%d x %d ctrl=%d,%d %dx%d",
             (int)win->Width, (int)win->Height,
             (int)geom.ctrl_left, (int)geom.ctrl_top,
             (int)geom.ctrl_w, (int)geom.ctrl_h);

    demo_clear_window_interior(win, dri);

    demo_destroy_gadgets(glist_io, scroller_io, apply_io, bench_io);
    if (!demo_create_gadgets(glist_io, scroller_io, apply_io, bench_io,
                             vi, textattr, font_h, &geom)) {
        RLV_LOG("RESIZE fallback/failure reason=gadget_recreate");
        g_demo_gadgets_detached = TRUE;
        return;
    }

    AddGList(win, *glist_io, ~0, -1, NULL);
    g_demo_gadgets_detached = FALSE;
    RefreshGList(*glist_io, win, NULL, -1);

    rlv_set_bounds(control, &bounds);

    GT_RefreshWindow(win, NULL);

    RLV_LOG("RESIZE full repaint begin");
    /* Full control path — never rlv_render_scrolled on resize. */
    demo_paint(control);
    RLV_LOG("RESIZE full repaint end");

    RLV_LOG("RESIZE scroller sync begin");
    demo_sync_scroller(win, *scroller_io, control, last_top);
    RLV_LOG("RESIZE scroller sync end");
    RLV_LOG("RESIZE demo_handle_newsize end");
}

/*
 * TRUE only when Intuition defines IAddress as a Gadget*.
 * MOUSEMOVE is NOT included: with WFLG_REPORTMOUSE the pointer is
 * undefined / non-gadget and may be odd — dereferencing it causes
 * Address Error (#80000003) on 68000. Scroller FOLLOWMOUSE moves are
 * recognised by pointer identity against the known scroller gadget.
 */
static BOOL demo_idcmp_is_gadget_class(ULONG class)
{
    if (class == IDCMP_GADGETUP || class == IDCMP_GADGETDOWN) {
        return TRUE;
    }
    return FALSE;
}

/*
 * Resolve IAddress to a Gadget only when safe. For MOUSEMOVE, accept the
 * address only when it is exactly the known scroller (no field access on
 * unknown pointers).
 */
static struct Gadget *demo_gadget_from_imsg(ULONG class,
                                            APTR iaddr,
                                            struct Gadget *scroller)
{
    if (class == IDCMP_GADGETUP || class == IDCMP_GADGETDOWN) {
        if (iaddr == 0) {
            return 0;
        }
        /* 68000: odd addresses cannot hold a Gadget. */
        if ((((ULONG)iaddr) & 1UL) != 0) {
            return 0;
        }
        return (struct Gadget *)iaddr;
    }
    if (class == IDCMP_MOUSEMOVE
        && scroller != 0
        && iaddr == (APTR)scroller) {
        return scroller;
    }
    return 0;
}

#ifdef RLV_ENABLE_LOGGING
static VOID demo_log_idcmp(struct IntuiMessage *imsg)
{
    ULONG class;
    UWORD code;
    UWORD qual;
    WORD mx;
    WORD my;
    APTR iaddr;
    struct Gadget *g;

    if (imsg == 0) {
        return;
    }

    class = imsg->Class;
    if (class != IDCMP_REFRESHWINDOW
        && class != IDCMP_MOUSEBUTTONS
        && class != IDCMP_GADGETDOWN
        && class != IDCMP_GADGETUP
        && class != IDCMP_MOUSEMOVE
        && class != IDCMP_INTUITICKS
        && class != IDCMP_NEWSIZE
        && class != IDCMP_SIZEVERIFY
        && class != IDCMP_CLOSEWINDOW
        && class != IDCMP_RAWKEY) {
        return;
    }

    code = imsg->Code;
    qual = imsg->Qualifier;
    mx = imsg->MouseX;
    my = imsg->MouseY;
    iaddr = imsg->IAddress;

    RLV_LOGF("IDCMP class=0x%08lx code=%u qual=0x%04x mouse=%d,%d IAddress=%p",
             (unsigned long)class,
             (unsigned)code,
             (unsigned)qual,
             (int)mx,
             (int)my,
             (void *)iaddr);

    if (demo_idcmp_is_gadget_class(class)) {
        if (iaddr == 0) {
            RLV_LOG("INVARIANT gadget-class message with NULL IAddress");
            return;
        }
        if ((((ULONG)iaddr) & 1UL) != 0) {
            RLV_LOG("INVARIANT gadget IAddress odd — skip deref");
            return;
        }
        g = (struct Gadget *)iaddr;
        RLV_LOGF("IDCMP gadget GadgetID=%u", (unsigned)g->GadgetID);
        if (g->GadgetID != GID_SCROLL
            && g->GadgetID != GID_APPLY
            && g->GadgetID != GID_BENCH
            && g->GadgetID != GID_EVENT_STATUS) {
            RLV_LOGF("INVARIANT unexpected GadgetID=%u",
                     (unsigned)g->GadgetID);
        }
    } else if (class == IDCMP_MOUSEMOVE && iaddr != 0) {
        /* REPORTMOUSE leaves IAddress undefined — log only, never deref. */
        RLV_LOG("IDCMP MOUSEMOVE IAddress not treated as Gadget");
    }
}
#endif /* RLV_ENABLE_LOGGING */

static VOID demo_paint(RLV_Control *control)
{
    if (control == 0) {
        RLV_LOG("INVARIANT demo_paint control is NULL");
        return;
    }
    RLV_LOG("demo_paint begin");
    rlv_render(control, 0);
    RLV_LOG("demo_paint end");
}

/* Scroll/selection: redraw viewport only so the outer frame does not flash. */
static VOID demo_paint_viewport(RLV_Control *control)
{
    if (control == 0) {
        RLV_LOG("INVARIANT demo_paint_viewport control is NULL");
        return;
    }
    RLV_LOG("demo_paint_viewport begin");
    rlv_render(control, RLV_RENDER_VIEWPORT_ONLY);
    RLV_LOG("demo_paint_viewport end");
}

static VOID demo_sync_scroller(struct Window *win,
                               struct Gadget *scroller,
                               RLV_Control *control,
                               LONG *last_top)
{
    LONG content_h;
    LONG vp_h;
    LONG scroll_y;
    LONG total;
    LONG visible;
    LONG top;
    RLV_BENCH_DECLARE(bench_scroller);

    RLV_LOG("scroller re-sync begin");
    RLV_BENCH_BEGIN(RLV_BENCH_SCROLLER_UPDATE, bench_scroller);

    if (win == 0 || control == 0) {
        RLV_LOG("INVARIANT demo_sync_scroller missing win/control");
        RLV_LOG("scroller re-sync end");
        RLV_BENCH_END(RLV_BENCH_SCROLLER_UPDATE, bench_scroller);
        return;
    }
    if (scroller == 0) {
        RLV_LOG("INVARIANT demo_sync_scroller scroller is NULL");
        RLV_LOG("scroller re-sync end");
        RLV_BENCH_END(RLV_BENCH_SCROLLER_UPDATE, bench_scroller);
        return;
    }

    content_h = rlv_get_content_height(control);
    vp_h = rlv_get_viewport_height(control);
    scroll_y = rlv_get_scroll_y(control);

    total = content_h;
    if (total < 1) {
        total = 1;
    }
    visible = vp_h;
    if (visible < 1) {
        visible = 1;
    }
    if (visible > total) {
        visible = total;
    }
    top = scroll_y;
    if (top < 0) {
        top = 0;
    }

    RLV_LOGF("GT_SetGadgetAttrs begin Total=%ld Visible=%ld Top=%ld",
             (long)total, (long)visible, (long)top);
    GT_SetGadgetAttrs(scroller, win, NULL,
                      GTSC_Total, total,
                      GTSC_Visible, visible,
                      GTSC_Top, top,
                      TAG_END);
    RLV_BENCH_COUNT(RLV_BENCH_COUNTER_SCROLLBAR_UPDATES);
    RLV_LOG("GT_SetGadgetAttrs end");

    if (last_top != 0) {
        *last_top = top;
    }
    RLV_LOGF("scroller re-sync end last_top=%ld scroll_y=%ld",
             (long)(last_top != 0 ? *last_top : top),
             (long)scroll_y);
    RLV_BENCH_END(RLV_BENCH_SCROLLER_UPDATE, bench_scroller);
}

/*
 * True when (x,y) is inside the outer control rectangle (header + viewport).
 * Dual-control demos would test each instance; outside all preserves focus.
 */
static BOOL demo_point_in_control(const DemoGeom *geom, WORD x, WORD y)
{
    WORD right;
    WORD bottom;

    if (geom == 0 || geom->ctrl_w < 1 || geom->ctrl_h < 1) {
        return FALSE;
    }
    right = (WORD)(geom->ctrl_left + geom->ctrl_w - 1);
    bottom = (WORD)(geom->ctrl_top + geom->ctrl_h - 1);
    if (x < geom->ctrl_left || x > right) {
        return FALSE;
    }
    if (y < geom->ctrl_top || y > bottom) {
        return FALSE;
    }
    return TRUE;
}

/*
 * Demo-local RAWKEY → RLV_InputEvent. Ignores upstrokes. Control wins over
 * Shift when both are held with a cursor key. Space → RLV_INPUT_TOGGLE
 * (sole eligible checkbox on selected row; core decides eligibility).
 * Right/Left → expand/collapse current expandable row.
 */
static BOOL demo_translate_rawkey(UWORD code, UWORD qual, RLV_InputEvent *out)
{
    UWORD key;

    if (out == 0) {
        return FALSE;
    }
    if ((code & IECODE_UP_PREFIX) != 0) {
        return FALSE;
    }
    key = (UWORD)(code & ~IECODE_UP_PREFIX);

    memset(out, 0, sizeof(*out));

    if (key == DEMO_RAWKEY_RETURN) {
        out->type = (UWORD)RLV_INPUT_NAV_ACTIVATE;
        return TRUE;
    }
    if (key == DEMO_RAWKEY_SPACE) {
        out->type = (UWORD)RLV_INPUT_TOGGLE;
        return TRUE;
    }
    if (key == CURSORRIGHT) {
        out->type = (UWORD)RLV_INPUT_EXPAND_ROW;
        return TRUE;
    }
    if (key == CURSORLEFT) {
        out->type = (UWORD)RLV_INPUT_COLLAPSE_ROW;
        return TRUE;
    }
    if (key == CURSORUP || key == CURSORDOWN) {
        if ((qual & IEQUALIFIER_CONTROL) != 0) {
            out->type = (key == CURSORUP)
                ? (UWORD)RLV_INPUT_NAV_FIRST
                : (UWORD)RLV_INPUT_NAV_LAST;
        } else if ((qual & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) != 0) {
            out->type = (key == CURSORUP)
                ? (UWORD)RLV_INPUT_NAV_PAGE_UP
                : (UWORD)RLV_INPUT_NAV_PAGE_DOWN;
        } else {
            out->type = (key == CURSORUP)
                ? (UWORD)RLV_INPUT_NAV_PREV
                : (UWORD)RLV_INPUT_NAV_NEXT;
        }
        return TRUE;
    }
    return FALSE;
}

/*
 * Deterministic NAV/scroll workload for Phase 6 profiling. Uses the same
 * handle_input + paint path as interactive keys — no synthetic RAWKEY.
 */
static VOID demo_run_exercise(RLV_Control *control,
                              struct Window *win,
                              struct Gadget *scroller,
                              LONG *last_top)
{
    RLV_InputEvent inev;
    int i;

    if (control == 0) {
        return;
    }

    DEMO_PRINTF("EXERCISE: reset scroll_y=0 selected=-1, then fixed NAV/scroll sequence.\n");
    DEMO_FFLUSH();

    rlv_set_selected(control, -1);
    rlv_set_scroll_y(control, 0);
    demo_paint_viewport(control);
    demo_sync_scroller(win, scroller, control, last_top);

    memset(&inev, 0, sizeof(inev));
    for (i = 0; i < 50; i++) {
        inev.type = (UWORD)RLV_INPUT_NAV_NEXT;
        demo_apply_input(control, win, scroller, &inev, last_top);
    }
    for (i = 0; i < 25; i++) {
        inev.type = (UWORD)RLV_INPUT_NAV_PREV;
        demo_apply_input(control, win, scroller, &inev, last_top);
    }
    for (i = 0; i < 20; i++) {
        inev.type = (UWORD)RLV_INPUT_SCROLL_LINE_DOWN;
        demo_apply_input(control, win, scroller, &inev, last_top);
    }
    for (i = 0; i < 20; i++) {
        inev.type = (UWORD)RLV_INPUT_SCROLL_LINE_UP;
        demo_apply_input(control, win, scroller, &inev, last_top);
    }
    for (i = 0; i < 5; i++) {
        inev.type = (UWORD)RLV_INPUT_NAV_PAGE_DOWN;
        demo_apply_input(control, win, scroller, &inev, last_top);
    }
    for (i = 0; i < 5; i++) {
        inev.type = (UWORD)RLV_INPUT_NAV_PAGE_UP;
        demo_apply_input(control, win, scroller, &inev, last_top);
    }

    inev.type = (UWORD)RLV_INPUT_NAV_LAST;
    demo_apply_input(control, win, scroller, &inev, last_top);
    inev.type = (UWORD)RLV_INPUT_NAV_FIRST;
    demo_apply_input(control, win, scroller, &inev, last_top);

    /* Burst across the non-selectable heading (logical row 3). */
    for (i = 0; i < 6; i++) {
        inev.type = (UWORD)RLV_INPUT_NAV_NEXT;
        demo_apply_input(control, win, scroller, &inev, last_top);
    }
    for (i = 0; i < 4; i++) {
        inev.type = (UWORD)RLV_INPUT_NAV_PREV;
        demo_apply_input(control, win, scroller, &inev, last_top);
    }

    DEMO_PRINTF("EXERCISE complete (selected=%ld scroll_y=%ld). Window remains open.\n",
           (long)rlv_get_selected(control),
           (long)rlv_get_scroll_y(control));
    DEMO_FFLUSH();
}

#ifdef RLV_ENABLE_BENCHMARKS
static VOID demo_bench_reset(RLV_Control *control,
                             struct Window *win,
                             struct Gadget *scroller,
                             LONG *last_top,
                             LONG selected_row,
                             LONG scroll_y)
{
    if (control == 0) {
        return;
    }
    rlv_set_selected(control, selected_row);
    rlv_set_scroll_y(control, scroll_y);
    demo_paint_viewport(control);
    demo_sync_scroller(win, scroller, control, last_top);
}

static ULONG demo_bench_run_steps(RLV_Control *control,
                                  struct Window *win,
                                  struct Gadget *scroller,
                                  LONG *last_top,
                                  UWORD event_type,
                                  ULONG steps)
{
    RLV_InputEvent inev;
    ULONG i;
    ULONG completed;

    memset(&inev, 0, sizeof(inev));
    inev.type = event_type;
    completed = 0;
    for (i = 0; i < steps; i++) {
        if (!demo_apply_input(control, win, scroller, &inev, last_top)) {
            break;
        }
        completed++;
    }
    rlv_bench_test_note_steps(completed);
    return completed;
}

static VOID demo_bench_configure(RLV_Control *control,
                                 struct Window *win,
                                 struct Screen *screen)
{
    LONG vp_h;
    LONG line_h;
    ULONG visible_rows;
    ULONG feature_flags;

    if (control == 0 || win == 0 || screen == 0 || win->RPort == 0
        || win->RPort->Font == 0) {
        return;
    }

    vp_h = rlv_get_viewport_height(control);
    line_h = (LONG)win->RPort->TxHeight;
    if (line_h < 1) {
        line_h = 1;
    }
    visible_rows = (ULONG)(vp_h / line_h);
    feature_flags = RLV_BENCH_FEATURE_WRAP;
#if defined(RLV_ENABLE_SMART_SCROLL) && (RLV_ENABLE_SMART_SCROLL != 0)
    feature_flags |= RLV_BENCH_FEATURE_SMART_SCROLL;
#endif
    rlv_bench_set_target("rich-listview-demo-bench");
    rlv_bench_set_profile("rich-listview-demo-bench");
    rlv_bench_set_environment_note("Run on classic Amiga or WinUAE 68000.");
    rlv_bench_set_feature_flags(feature_flags);
    rlv_bench_set_dimensions(NUM_COLS,
                             g_demo_row_count,
                             g_demo_row_count,
                             visible_rows,
                             g_demo_geom.ctrl_w,
                             g_demo_geom.ctrl_h);
    rlv_bench_set_screen_info(screen->BitMap.Depth,
                              screen->Font->ta_Name,
                              screen->Font->ta_YSize,
                              win->RPort->TxWidth);
}

static VOID demo_run_benchmarks(RLV_Control *control,
                                struct Window *win,
                                struct Gadget *scroller,
                                LONG *last_top)
{
    RLV_InputEvent inev;
    ULONG i;
    RLV_BENCH_DECLARE(bench_nav_run);

    if (control == 0) {
        return;
    }

    /* Start each benchmark run with fresh report/debug artifacts. */
    (VOID)DeleteFile("PROGDIR:rlv_benchmark.txt");
    (VOID)DeleteFile("PROGDIR:rlv_benchmark_debug.log");

    rlv_bench_init();
    rlv_bench_debug_mark("demo_run_benchmarks enter");
    RLV_BENCH_BEGIN(RLV_BENCH_TOTAL_NAVIGATION_RUN, bench_nav_run);
    DEMO_PRINTF("BENCH: running deterministic benchmark suite.\n");
    DEMO_FFLUSH();

    demo_bench_configure(control, win, win->WScreen);
    rlv_bench_debug_mark("demo_run_benchmarks after configure");

    demo_bench_reset(control, win, scroller, last_top, -1, 0);
    for (i = 0; i < RLV_BENCH_WARMUP_STEPS; i++) {
        memset(&inev, 0, sizeof(inev));
        inev.type = (UWORD)RLV_INPUT_NAV_NEXT;
        (VOID)demo_apply_input(control, win, scroller, &inev, last_top);
    }

    demo_bench_reset(control, win, scroller, last_top, 0, 0);
    rlv_bench_test_begin(RLV_BENCH_TEST_SELECTION_ONLY_DOWN);
    (VOID)demo_bench_run_steps(control, win, scroller, last_top,
                               (UWORD)RLV_INPUT_NAV_NEXT, 6);
    rlv_bench_test_end();

    rlv_bench_test_begin(RLV_BENCH_TEST_SELECTION_ONLY_UP);
    (VOID)demo_bench_run_steps(control, win, scroller, last_top,
                               (UWORD)RLV_INPUT_NAV_PREV, 6);
    rlv_bench_test_end();

    demo_bench_reset(control, win, scroller, last_top, -1, 0);
    rlv_bench_test_begin(RLV_BENCH_TEST_STEADY_SCROLL_DOWN);
    (VOID)demo_bench_run_steps(control, win, scroller, last_top,
                               (UWORD)RLV_INPUT_SCROLL_LINE_DOWN,
                               RLV_BENCH_NAV_STEPS);
    rlv_bench_test_end();

    demo_bench_reset(control, win, scroller, last_top,
                     (LONG)(g_demo_row_count - 1), rlv_get_content_height(control));
    rlv_bench_test_begin(RLV_BENCH_TEST_STEADY_SCROLL_UP);
    (VOID)demo_bench_run_steps(control, win, scroller, last_top,
                               (UWORD)RLV_INPUT_SCROLL_LINE_UP,
                               RLV_BENCH_NAV_STEPS);
    rlv_bench_test_end();

    demo_bench_reset(control, win, scroller, last_top, 0, 0);
    rlv_bench_test_begin(RLV_BENCH_TEST_LARGE_MOVEMENT);
    (VOID)demo_bench_run_steps(control, win, scroller, last_top,
                               (UWORD)RLV_INPUT_NAV_PAGE_DOWN, 5);
    (VOID)demo_bench_run_steps(control, win, scroller, last_top,
                               (UWORD)RLV_INPUT_NAV_PAGE_UP, 5);
    rlv_bench_test_end();

    demo_bench_reset(control, win, scroller, last_top, -1, 0);
    rlv_bench_test_begin(RLV_BENCH_TEST_END_TO_END);
    (VOID)demo_bench_run_steps(control, win, scroller, last_top,
                               (UWORD)RLV_INPUT_NAV_LAST, 1);
    (VOID)demo_bench_run_steps(control, win, scroller, last_top,
                               (UWORD)RLV_INPUT_NAV_FIRST, 1);
    rlv_bench_test_end();

    rlv_bench_test_begin(RLV_BENCH_TEST_REDRAW_BASELINE);
    demo_paint(control);
    rlv_bench_test_note_steps(1);
    rlv_bench_test_end();

    rlv_bench_test_begin(RLV_BENCH_TEST_PREPARE_BASELINE);
    for (i = 0; i < RLV_BENCH_PREPARE_RUNS; i++) {
        struct Rectangle bounds;
        demo_bounds_from_geom(&g_demo_geom, &bounds);
        rlv_set_bounds(control, &bounds);
    }
    rlv_bench_test_note_steps(RLV_BENCH_PREPARE_RUNS);
    rlv_bench_test_end();

    RLV_BENCH_END(RLV_BENCH_TOTAL_NAVIGATION_RUN, bench_nav_run);
    (VOID)rlv_bench_write_report("PROGDIR:rlv_benchmark.txt");
    rlv_bench_debug_mark("demo_run_benchmarks after write_report");
    DEMO_PRINTF("BENCH complete. Report written to PROGDIR:rlv_benchmark.txt\n");
    DEMO_FFLUSH();
    rlv_bench_shutdown();
}
#endif

static BOOL demo_apply_input(RLV_Control *control,
                             struct Window *win,
                             struct Gadget *scroller,
                             const RLV_InputEvent *inev,
                             LONG *last_top)
{
    RLV_Event ev;
    BOOL handled;
    LONG scroll_before;
    LONG scroll_after;

    if (control == 0 || inev == 0) {
        RLV_LOG("INVARIANT demo_apply_input NULL control/event");
        return FALSE;
    }

    scroll_before = rlv_get_scroll_y(control);
    RLV_LOGF("rlv_handle_input begin type=%u x=%d y=%d value=%ld scroll_y=%ld",
             (unsigned)inev->type,
             (int)inev->x,
             (int)inev->y,
             (long)inev->value,
             (long)scroll_before);

    memset(&ev, 0, sizeof(ev));
    handled = rlv_handle_input(control, inev, &ev);
    scroll_after = rlv_get_scroll_y(control);

#if defined(RLV_ENABLE_COLUMN_RESIZE) && (RLV_ENABLE_COLUMN_RESIZE != 0)
    demo_sync_resize_report_mouse(win, control);
#endif

    RLV_LOGF("rlv_handle_input end handled=%d ev.type=%u ev.row=%ld ev.value=%ld scroll_y=%ld",
             (int)handled,
             (unsigned)ev.type,
             (long)ev.row,
             (long)ev.value,
             (long)scroll_after);

    if (!handled) {
        RLV_LOG("render skipped (handle_input FALSE)");
        return FALSE;
    }

    if (ev.type == (UWORD)RLV_EVENT_SELECTION_CHANGED) {
        DEMO_PRINTF("Selected row %ld tag %lu\n",
               (long)ev.row,
               (unsigned long)(ULONG)ev.row_user_data);
        DEMO_FFLUSH();
        demo_update_event_status(win, control, &ev);
    } else if (ev.type == (UWORD)RLV_EVENT_CELL_CONTROL) {
        DEMO_PRINTF("Cell control row %ld tag %lu col %u type=%u action=%u: %u -> %u\n",
               (long)ev.row,
               (unsigned long)(ULONG)ev.row_user_data,
               (unsigned)ev.column,
               (unsigned)ev.control_type,
               (unsigned)ev.control_action,
               (unsigned)ev.previous_value,
               (unsigned)ev.cell_value);
        DEMO_FFLUSH();
        demo_update_event_status(win, control, &ev);

        if (ev.control_type == (UWORD)RLV_COL_TYPE_DISCLOSURE) {
            /*
             * Sync app-owned RLV_ROW_EXPANDED from the event. Layout and
             * scroll already updated inside handle_input. Prefer a tail
             * paint from the toggled row when scroll is unchanged.
             */
            if (ev.row >= 0 && (UWORD)ev.row < g_demo_row_count) {
                if (ev.cell_value == (UBYTE)RLV_CELL_EXPANDED) {
                    g_rows[ev.row].flags = (UWORD)(g_rows[ev.row].flags
                                                   | RLV_ROW_EXPANDED);
                } else {
                    g_rows[ev.row].flags = (UWORD)(g_rows[ev.row].flags
                                                   & (UWORD)~RLV_ROW_EXPANDED);
                }
            }
            rlv_render_from_row(control, ev.row, scroll_before);
            demo_sync_scroller(win, scroller, control, last_top);
            return TRUE;
        }

        /*
         * Integrator pattern on checkbox CELL_CONTROL:
         *   1) Sync the app-owned authoritative Boolean by stable tag
         *      (control already mutated its internal snapshot only).
         *   2) Prefer rlv_render_cell_control using the event row index;
         *      escalate only when that reports ROW / VIEWPORT.
         */
        {
            UBYTE *store;

            store = demo_find_checkbox_by_tag(ev.row_user_data);
            if (store != 0) {
                *store = ev.cell_value;
            }
        }
        {
            UWORD repaint;

            repaint = rlv_render_cell_control(control, ev.row, ev.column);
            RLV_LOGF("CELL_CONTROL store sync + cell paint row=%ld col=%u "
                     "type=%u action=%u val=%u tag=%lu result=%u selected=%ld",
                     (long)ev.row, (unsigned)ev.column,
                     (unsigned)ev.control_type,
                     (unsigned)ev.control_action,
                     (unsigned)ev.cell_value,
                     (unsigned long)(ULONG)ev.row_user_data,
                     (unsigned)repaint,
                     (long)rlv_get_selected(control));
            if (repaint == (UWORD)RLV_CELL_REPAINT_ERROR) {
                rlv_render_logical_rows(control, ev.row, -1);
            }
            /* OK / NOT_VISIBLE / ROW / VIEWPORT handled by the API. */
        }
        demo_sync_scroller(win, scroller, control, last_top);
        return TRUE;
    } else if (ev.type == (UWORD)RLV_EVENT_ACTIVATED) {
        DEMO_PRINTF("Activated row %ld tag %lu\n",
               (long)ev.row,
               (unsigned long)(ULONG)ev.row_user_data);
        DEMO_FFLUSH();
        demo_update_event_status(win, control, &ev);
        RLV_LOG("ACTIVATED — no repaint");
        return TRUE;
    } else if (ev.type == (UWORD)RLV_EVENT_SORT_CHANGED) {
        DEMO_PRINTF("Sort changed col %u dir %ld src %ld tag %lu\n",
               (unsigned)ev.column,
               (long)ev.value,
               (long)ev.row,
               (unsigned long)(ULONG)ev.row_user_data);
        DEMO_FFLUSH();
        demo_update_event_status(win, control, &ev);
        rlv_render(control, 0);
        demo_sync_scroller(win, scroller, control, last_top);
        return TRUE;
    } else if (ev.type == (UWORD)RLV_EVENT_COLUMN_RESIZED) {
        DEMO_PRINTF("Column resize %u/%u %d+%d -> %d+%d (repaint=%ld)\n",
               (unsigned)ev.resize_left,
               (unsigned)ev.resize_right,
               (int)ev.old_left_width,
               (int)ev.old_right_width,
               (int)ev.new_left_width,
               (int)ev.new_right_width,
               (long)ev.value);
        DEMO_FFLUSH();
        demo_update_event_status(win, control, &ev);
#if defined(RLV_ENABLE_COLUMN_RESIZE) && (RLV_ENABLE_COLUMN_RESIZE != 0)
        demo_sync_resize_report_mouse(win, control);
#endif
        if (ev.value == RLV_RESIZE_REPAINT_REGIONAL
            && rlv_render_resized_columns(control,
                                          ev.resize_left,
                                          ev.resize_right)) {
            RLV_LOG("COLUMN_RESIZE regional paint ok");
        } else {
            RLV_LOG("COLUMN_RESIZE full paint");
            rlv_render(control, 0);
        }
        demo_sync_scroller(win, scroller, control, last_top);
        return TRUE;
    }

    RLV_LOG("render requested");
    /*
     * Selection without scroll: repaint only old/new logical rows.
     * Selection with make_visible scroll: full viewport — smart scroll would
     * keep the old highlight in shifted pixels and only partially paint the
     * new row inside the exposed band.
     * Scroll alone: smart scroll when eligible, else full viewport.
     * ACTIVATED: handled above (no visual change).
     */
    if (ev.type == (UWORD)RLV_EVENT_SELECTION_CHANGED
        && scroll_before == scroll_after) {
        RLV_BENCH_COUNT(RLV_BENCH_COUNTER_SELECTION_ONLY_REDRAWS);
        RLV_BENCH_COUNT(RLV_BENCH_COUNTER_PARTIAL_REDRAWS);
        RLV_LOGF("selection row paint previous=%ld new=%ld",
                 (long)ev.previous_row, (long)ev.row);
        rlv_render_logical_rows(control, ev.previous_row, ev.row);
    } else if (ev.type == (UWORD)RLV_EVENT_SELECTION_CHANGED
               && scroll_before != scroll_after) {
        RLV_BENCH_COUNT(RLV_BENCH_COUNTER_FULL_REDRAWS);
        RLV_BENCH_COUNT(RLV_BENCH_COUNTER_VIEWPORT_SCROLLS);
        RLV_LOGF("selection+scroll full viewport previous=%ld new=%ld old_scroll=%ld new_scroll=%ld",
                 (long)ev.previous_row, (long)ev.row,
                 (long)scroll_before, (long)scroll_after);
        demo_paint_viewport(control);
    } else if (scroll_before != scroll_after) {
        RLV_BENCH_COUNT(RLV_BENCH_COUNTER_VIEWPORT_SCROLLS);
        rlv_render_scrolled(control, scroll_before);
    } else {
        RLV_BENCH_COUNT(RLV_BENCH_COUNTER_FULL_REDRAWS);
        demo_paint_viewport(control);
    }
    demo_sync_scroller(win, scroller, control, last_top);
    return TRUE;
}

static VOID demo_handle_scroller(RLV_Control *control,
                                 struct Window *win,
                                 struct Gadget *scroller,
                                 LONG *last_top,
                                 ULONG msg_class,
                                 UWORD msg_code)
{
    LONG top;
    LONG delta;
    LONG prev_top;
    LONG scroll_y;
    RLV_InputEvent inev;

    RLV_LOG("scroller event received");
    RLV_LOGF("scroller message class=0x%08lx code=%u",
             (unsigned long)msg_class, (unsigned)msg_code);

    if (control == 0 || win == 0 || last_top == 0) {
        RLV_LOG("INVARIANT demo_handle_scroller missing control/win/last_top");
        RLV_LOG("scroller event complete");
        return;
    }
    if (scroller == 0) {
        RLV_LOG("INVARIANT demo_handle_scroller scroller is NULL");
        RLV_LOG("scroller event complete");
        return;
    }

    RLV_LOGF("scroller IAddress=%p GadgetID=%u",
             (void *)scroller, (unsigned)scroller->GadgetID);

    prev_top = *last_top;
    scroll_y = rlv_get_scroll_y(control);
    RLV_LOGF("scroller previous last_top=%ld control scroll_y=%ld",
             (long)prev_top, (long)scroll_y);

    /*
     * WB2.x / gadtools V37: use IntuiMessage->Code as GTSC_Top.
     * Autodocs: scroller MOUSEMOVE/GADGET* Code carries the new Top.
     * Do NOT call GT_GetGadgetAttrs here — it is V39-only and was observed
     * returning 0 / crashing (exception 80000004) on gadtools 37.x.
     */
    top = (LONG)msg_code;
    RLV_LOGF("scroller Top from message Code=%ld (no GT_GetGadgetAttrs)",
             (long)top);

    delta = top - *last_top;
    memset(&inev, 0, sizeof(inev));

    if (delta == 1) {
        inev.type = (UWORD)RLV_INPUT_SCROLL_LINE_DOWN;
        RLV_LOGF("scroller translate type=SCROLL_LINE_DOWN requested_top=%ld",
                 (long)top);
        demo_apply_input(control, win, scroller, &inev, last_top);
    } else if (delta == -1) {
        inev.type = (UWORD)RLV_INPUT_SCROLL_LINE_UP;
        RLV_LOGF("scroller translate type=SCROLL_LINE_UP requested_top=%ld",
                 (long)top);
        demo_apply_input(control, win, scroller, &inev, last_top);
    } else if (delta != 0) {
        inev.type = (UWORD)RLV_INPUT_SCROLL_POSITION;
        inev.value = top;
        RLV_LOGF("scroller translate type=SCROLL_POSITION value=%ld",
                 (long)top);
        demo_apply_input(control, win, scroller, &inev, last_top);
    } else {
        RLV_LOG("scroller delta=0 (no input)");
    }

    RLV_LOGF("scroller event complete scroll_y=%ld last_top=%ld",
             (long)rlv_get_scroll_y(control),
             (long)*last_top);
}

/*
 * Build a fully configured replacement before releasing the current control.
 * Appearance changes therefore either commit together or leave the old
 * control untouched.
 */
static BOOL demo_recreate_control(RLV_Control **control_io,
                                  RLV_BackendV36 *backend,
                                  struct TextFont *font,
                                  const RLV_Pens *pens,
                                  const RLV_Column *columns,
                                  const struct Rectangle *bounds,
                                  const DemoSettings *settings,
                                  BOOL keyboard_off)
{
    RLV_Config cfg;
    RLV_Control *old_control;
    RLV_Control *new_control;
    LONG selected;
    LONG scroll_y;
    BOOL keyboard_enabled;
    BOOL preserve_expand;

    if (control_io == 0 || backend == 0 || pens == 0
        || columns == 0 || bounds == 0 || settings == 0) {
        return FALSE;
    }

    old_control = *control_io;
    selected = -1;
    scroll_y = 0;
    keyboard_enabled = keyboard_off ? FALSE : TRUE;
    preserve_expand = FALSE;
    if (old_control != 0) {
        selected = rlv_get_selected(old_control);
        scroll_y = rlv_get_scroll_y(old_control);
        keyboard_enabled =
            rlv_get_keyboard_enabled(old_control);
        g_demo_activation_policy =
            rlv_get_control_activation_policy(old_control);
        g_demo_current_row_visual =
            rlv_get_current_row_visual(old_control);
        /*
         * Same Start-rows policy: keep interactive expand state in g_rows.
         * Changed policy: rewrite g_rows to all open/collapsed.
         */
        if (settings->initial_expand == g_demo_applied.initial_expand) {
            demo_sync_row_expand_flags_from_control(old_control);
            preserve_expand = TRUE;
        } else {
            demo_apply_initial_expand_to_rows(settings->initial_expand);
        }
    } else {
        demo_apply_initial_expand_to_rows(settings->initial_expand);
    }

    memset(&cfg, 0, sizeof(cfg));
    cfg.draw_ops = rlv_backend_v36_get_ops();
    cfg.draw_context = rlv_backend_v36_get_context(backend);
    cfg.font = font;
    cfg.cell_padding_x = settings->padding_x;
    cfg.cell_padding_y = settings->padding_y;
    cfg.row_gap = settings->row_gap;
    cfg.row_divider_style = settings->divider_style;
    cfg.initial_expand = settings->initial_expand;
    if (!keyboard_enabled) {
        cfg.flags = RLV_CFG_NO_KEYBOARD;
    }

    new_control = rlv_create(&cfg);
    if (new_control == 0) {
        return FALSE;
    }
    rlv_set_pens(new_control, pens);
    if (!rlv_set_columns(new_control, columns, NUM_COLS)
        || !rlv_set_rows(new_control, g_rows, g_demo_row_count)) {
        rlv_destroy(new_control);
        return FALSE;
    }
#if defined(RLV_ENABLE_SORTING) && (RLV_ENABLE_SORTING != 0)
    if (!demo_install_sort(new_control, (RLV_Column *)columns)) {
        rlv_destroy(new_control);
        return FALSE;
    }
#endif
#if defined(RLV_ENABLE_COLUMN_RESIZE) && (RLV_ENABLE_COLUMN_RESIZE != 0)
    if (!demo_install_column_resize(new_control,
                                    (WORD)(font != 0 ? font->tf_XSize : 8))) {
        rlv_destroy(new_control);
        return FALSE;
    }
#endif

    /* Display policies before set_bounds so the first layout is correct. */
    demo_apply_display_settings(new_control, settings);
    demo_apply_policies(new_control);
    if (preserve_expand) {
        demo_restore_expand_from_row_flags(new_control);
    }
    rlv_set_bounds(new_control, bounds);
    rlv_set_selected(new_control, selected);
    rlv_set_scroll_y(new_control, scroll_y);

    rlv_destroy(old_control);
    *control_io = new_control;
    return TRUE;
}

/*
 * Commit pending visual settings in one recreate + full paint + scroller
 * sync. Applied settings update only after a successful recreate.
 */
static BOOL demo_apply_pending_settings(RLV_Control **control_io,
                                        RLV_BackendV36 *backend,
                                        struct Window *win,
                                        struct Gadget *scroller,
                                        const RLV_Pens *pens,
                                        const RLV_Column *columns,
                                        BOOL keyboard_off,
                                        LONG *last_top)
{
    struct Rectangle bounds;

    if (control_io == 0 || win == 0 || pens == 0 || columns == 0) {
        return FALSE;
    }
    if (demo_settings_equal(&g_demo_pending, &g_demo_applied)) {
        demo_update_apply_enabled_state(win);
        return TRUE;
    }

    demo_bounds_from_geom(&g_demo_geom, &bounds);
    if (!demo_recreate_control(control_io, backend, win->RPort->Font,
                               pens, columns, &bounds, &g_demo_pending,
                               keyboard_off)) {
        RLV_LOG("FAIL Apply control recreation");
        return FALSE;
    }

    demo_copy_settings(&g_demo_applied, &g_demo_pending);
    demo_paint(*control_io);
    demo_sync_scroller(win, scroller, *control_io, last_top);
    demo_update_apply_enabled_state(win);
    demo_update_status_text(win, "Settings applied");
    RLV_LOGF("SETTINGS applied div=%u xpad=%u ypad=%u gap=%u "
             "rowdisp=%u longword=%u ellip=0x%x initexp=%u",
             (unsigned)g_demo_applied.divider_style,
             (unsigned)g_demo_applied.padding_x,
             (unsigned)g_demo_applied.padding_y,
             (unsigned)g_demo_applied.row_gap,
             (unsigned)g_demo_applied.row_display_mode,
             (unsigned)g_demo_applied.long_word_mode,
             (unsigned)g_demo_applied.ellipsis_flags,
             (unsigned)g_demo_applied.initial_expand);
    return TRUE;
}

int main(int argc, char **argv)
{
    struct Screen *screen = 0;
    struct Window *win = 0;
    struct Gadget *glist = 0;
    struct Gadget *scroller = 0;
    struct Gadget *apply_gad = 0;
    struct Gadget *bench_gad = 0;
    APTR vi = 0;
    struct DrawInfo *dri = 0;
    struct IntuiMessage *imsg;
    BOOL done = FALSE;
    LONG last_scroll_top = 0;
    ULONG idcmp_flags;
    int exit_code = 20;
    BOOL run_exercise = FALSE;
    BOOL keyboard_off = FALSE;
#ifdef RLV_ENABLE_BENCHMARKS
    BOOL run_bench = FALSE;
#endif

    RLV_BackendV36 *backend = 0;
    RLV_Control *control = 0;
    RLV_Control *active_control = 0; /* app-owned focus; not a global */
    RLV_Config cfg;
    RLV_Pens pens;
    RLV_Column columns[NUM_COLS];
    struct Rectangle bounds;
    RLV_InputEvent inev;
    DemoGeom geom;

    WORD font_w;
    WORD font_h;
    WORD border_top;
    WORD border_bottom;
    WORD border_left;
    WORD border_right;
    WORD init_w;
    WORD init_h;
    WORD min_w;
    WORD min_h;
    WORD action_w;
    WORD gadget_w;
    WORD i;
    if (argc > 1 && argv != 0) {
        for (i = 1; i < (WORD)argc; i++) {
            if (argv[i] == 0) {
                continue;
            }
            if (strcmp(argv[i], "EXERCISE") == 0
                || strcmp(argv[i], "exercise") == 0) {
                run_exercise = TRUE;
#ifdef RLV_ENABLE_BENCHMARKS
            } else if (strcmp(argv[i], "BENCH") == 0
                       || strcmp(argv[i], "bench") == 0) {
                run_bench = TRUE;
#endif
            } else if (strcmp(argv[i], "NOKEYBOARD") == 0
                       || strcmp(argv[i], "nokeyboard") == 0) {
                keyboard_off = TRUE;
            } else if (strcmp(argv[i], "KEEPCURRENT") == 0
                       || strcmp(argv[i], "keepcurrent") == 0) {
                g_demo_activation_policy =
                    (UWORD)RLV_CONTROL_ACTIVATE_KEEP_CURRENT;
            } else if (strcmp(argv[i], "MARKER") == 0
                       || strcmp(argv[i], "marker") == 0) {
                g_demo_current_row_visual =
                    (UWORD)RLV_CURRENT_ROW_VISUAL_MARKER;
            } else if (strcmp(argv[i], "NOVISUAL") == 0
                       || strcmp(argv[i], "novisual") == 0) {
                g_demo_current_row_visual =
                    (UWORD)RLV_CURRENT_ROW_VISUAL_NONE;
            }
        }
    }

#ifdef RLV_ENABLE_BENCHMARKS
    if (run_bench) {
        g_demo_row_count = 80;
        run_exercise = FALSE;
    } else {
        g_demo_row_count = 9;
    }
    g_demo_bench_mode = run_bench ? TRUE : FALSE;
#endif

    demo_init_default_settings(&g_demo_applied);
    demo_copy_settings(&g_demo_pending, &g_demo_applied);

    rlv_log_init();
    RLV_LOG("PROGRAM start");
    RLV_LOG("libraries opened via -lauto / runtime OpenLibrary as needed");
    if (run_exercise) {
        RLV_LOG("EXERCISE mode requested");
    }
    if (keyboard_off) {
        RLV_LOG("NOKEYBOARD: NAV_* disabled");
    }
    RLV_LOGF("startup activation_policy=%u visual=%u",
             (unsigned)g_demo_activation_policy,
             (unsigned)g_demo_current_row_visual);
    RLV_LOGF("startup settings div=%u xpad=%u ypad=%u gap=%u "
             "rowdisp=%u longword=%u ellip=0x%x",
             (unsigned)g_demo_applied.divider_style,
             (unsigned)g_demo_applied.padding_x,
             (unsigned)g_demo_applied.padding_y,
             (unsigned)g_demo_applied.row_gap,
             (unsigned)g_demo_applied.row_display_mode,
             (unsigned)g_demo_applied.long_word_mode,
             (unsigned)g_demo_applied.ellipsis_flags);

    screen = LockPubScreen(NULL);
    if (screen == 0) {
        RLV_LOG("FAIL LockPubScreen");
        rlv_log_shutdown();
        return 20;
    }

    font_w = screen->RastPort.TxWidth;
    font_h = screen->RastPort.TxHeight;
    /* RKM border estimates before OpenWindow (verified after open). */
    border_top = (WORD)(screen->WBorTop + screen->Font->ta_YSize + 1);
    border_bottom = (WORD)(screen->WBorBottom + 11); /* size gadget */
    border_left = (WORD)(screen->WBorLeft);
    border_right = (WORD)(screen->WBorRight + 11); /* size gadget */

    /* Scale column widths from actual font, not hard-coded Topaz.
     * Indices match g_columns: disclosure, Name, Type/Date, Description,
     * Status/Pos, On. Disclosure stays a narrow fixed glyph column. */
    for (i = 0; i < NUM_COLS; i++) {
        columns[i] = g_columns[i];
    }
    columns[0].width_pixels = (WORD)(2 * font_w);   /* disclosure +/- */
    columns[1].width_pixels = (WORD)(10 * font_w);  /* Name */
#if defined(RLV_ENABLE_SORTING) && (RLV_ENABLE_SORTING != 0)
    columns[2].width_pixels = (WORD)(11 * font_w);  /* Date DD-Mon-YYYY */
#else
    columns[2].width_pixels = (WORD)(8 * font_w);   /* Type */
#endif
    columns[3].width_pixels = (WORD)(18 * font_w);  /* Description */
    columns[4].width_pixels = (WORD)(6 * font_w);   /* Status / Pos */
    columns[5].width_pixels = (WORD)(3 * font_w);   /* On checkbox */

    /*
     * Fixed pixel column widths: window may not shrink below
     * borders + pad + frame + sum(widths) + dividers + scroller + pad.
     */
    g_demo_fixed_content_w = demo_fixed_content_width(columns, NUM_COLS);
    g_demo_min_ctrl_w = demo_min_ctrl_width(g_demo_fixed_content_w);

    /* Bottom strip needs Apply (+ optional Bench); columns still dominate. */
    gadget_w = (WORD)(font_w * 8);
    if (gadget_w < 56) {
        gadget_w = 56;
    }
    action_w = gadget_w;
#ifdef RLV_ENABLE_BENCHMARKS
    if (g_demo_bench_mode) {
        gadget_w = (WORD)(font_w * 16);
        if (gadget_w < 96) {
            gadget_w = 96;
        }
        action_w = (WORD)(action_w + DEMO_PAD + gadget_w);
    }
#endif
    if (g_demo_min_ctrl_w < action_w) {
        g_demo_min_ctrl_w = action_w;
    }

    /*
     * Initial outer window size from fixed column total + frame + scroller +
     * status/Apply strip. WA_MinWidth uses the same horizontal requirement.
     */
    init_w = (WORD)(border_left + DEMO_PAD + g_demo_min_ctrl_w
                    + demo_scroll_width(font_w) + DEMO_PAD + border_right);
    init_h = (WORD)(border_top + DEMO_PAD + ((font_h + 2) * 16)
                    + DEMO_PAD + (font_h + 6) + DEMO_PAD + (font_h + 6)
                    + DEMO_PAD + border_bottom);
    min_w = init_w; /* same formula: fixed columns must remain fully visible */
    min_h = (WORD)(border_top + DEMO_PAD + DEMO_MIN_CTRL_H
                   + DEMO_PAD + (font_h + 6) + DEMO_PAD + (font_h + 6)
                   + DEMO_PAD + border_bottom);

    vi = GetVisualInfo(screen, TAG_END);
    if (vi == 0) {
        RLV_LOG("FAIL GetVisualInfo");
        UnlockPubScreen(NULL, screen);
        rlv_log_shutdown();
        return 20;
    }

    dri = GetScreenDrawInfo(screen);
    if (dri == 0) {
        RLV_LOG("FAIL GetScreenDrawInfo");
        FreeVisualInfo(vi);
        UnlockPubScreen(NULL, screen);
        rlv_log_shutdown();
        return 20;
    }
    RLV_LOG("draw info acquired");

    if (!demo_setup_menus(vi)) {
        RLV_LOG("FAIL demo_setup_menus");
        FreeScreenDrawInfo(screen, dri);
        FreeVisualInfo(vi);
        UnlockPubScreen(NULL, screen);
        rlv_log_shutdown();
        return 20;
    }

    demo_init_rows();

    /*
     * Provisional gadget boxes from estimated borders; after OpenWindow
     * recreate with real Border* (GadTools caches size at CreateGadget).
     */
    memset(&geom, 0, sizeof(geom));
    geom.ctrl_left = (WORD)(border_left + DEMO_PAD);
    geom.ctrl_top = (WORD)(border_top + DEMO_PAD);
    geom.ctrl_w = g_demo_min_ctrl_w;
    geom.ctrl_h = (WORD)((font_h + 2) * 16);
    geom.scroll_w = demo_scroll_width(font_w);
    geom.scroll_left = (WORD)(geom.ctrl_left + geom.ctrl_w);
    geom.scroll_top = geom.ctrl_top;
    geom.scroll_h = geom.ctrl_h;
    geom.status_left = geom.ctrl_left;
    geom.status_top = (WORD)(geom.ctrl_top + geom.ctrl_h + DEMO_PAD);
    geom.status_w = (WORD)(geom.ctrl_w + geom.scroll_w);
    geom.status_h = (WORD)(font_h + 6);
    geom.apply_left = geom.ctrl_left;
    geom.apply_top = (WORD)(geom.status_top + geom.status_h + DEMO_PAD);
    geom.apply_w = (WORD)(font_w * 8);
    if (geom.apply_w < 56) {
        geom.apply_w = 56;
    }
    geom.apply_h = (WORD)(font_h + 6);
    geom.bench_left =
        (WORD)(geom.apply_left + geom.apply_w + DEMO_PAD);
    geom.bench_top = geom.apply_top;
    geom.bench_w = (WORD)(font_w * 16);
    if (geom.bench_w < 96) {
        geom.bench_w = 96;
    }
    geom.bench_h = geom.apply_h;

    if (!demo_create_gadgets(&glist, &scroller,
                             &apply_gad, &bench_gad,
                             vi, screen->Font, font_h, &geom)) {
        goto fail;
    }
    RLV_LOG("scroller created");

    idcmp_flags = IDCMP_CLOSEWINDOW | IDCMP_GADGETUP
                  | IDCMP_GADGETDOWN
                  | IDCMP_MOUSEBUTTONS
                  | IDCMP_MOUSEMOVE
                  | IDCMP_REFRESHWINDOW
                  | IDCMP_NEWSIZE
                  | IDCMP_SIZEVERIFY
                  | IDCMP_RAWKEY
                  | IDCMP_MENUPICK
                  | SCROLLERIDCMP
                  | DEMO_IDCMP_EXTRA;

    win = OpenWindowTags(NULL,
                         WA_Title, (ULONG)"RichListview (Phase E4)",
                         WA_Width, (ULONG)init_w,
                         WA_Height, (ULONG)init_h,
                         WA_MinWidth, (ULONG)min_w,
                         WA_MinHeight, (ULONG)min_h,
                         WA_MaxWidth, ~0UL,
                         WA_MaxHeight, ~0UL,
                         WA_IDCMP, idcmp_flags,
                         WA_Gadgets, (ULONG)glist,
                         WA_DragBar, TRUE,
                         WA_DepthGadget, TRUE,
                         WA_CloseGadget, TRUE,
                         WA_SizeGadget, TRUE,
                         WA_SizeBRight, TRUE,
                         WA_SizeBBottom, TRUE,
                         WA_Activate, TRUE,
                         WA_SimpleRefresh, TRUE,
                         WA_NewLookMenus, TRUE,
                         WA_PubScreen, (ULONG)screen,
                         TAG_END);
    if (win == 0) {
        RLV_LOG("FAIL OpenWindowTags");
        goto fail_gadgets;
    }
    RLV_LOG("window opened");

    SetMenuStrip(win, g_demo_menu_strip);

    /*
     * Recompute WA_MinWidth from actual borders (size gadget widens
     * BorderRight/Bottom vs pre-open estimates).
     */
    min_w = (WORD)(win->BorderLeft + DEMO_PAD + g_demo_min_ctrl_w
                   + demo_scroll_width(font_w) + DEMO_PAD
                   + win->BorderRight);
    min_h = (WORD)(win->BorderTop + DEMO_PAD + DEMO_MIN_CTRL_H
                   + DEMO_PAD + (font_h + 6) + DEMO_PAD + (font_h + 6)
                   + DEMO_PAD + win->BorderBottom);
    WindowLimits(win, min_w, min_h, (UWORD)~0, (UWORD)~0);
    if (win->Width < min_w) {
        SizeWindow(win, (WORD)(min_w - win->Width), 0);
    }
    if (win->Height < min_h) {
        SizeWindow(win, 0, (WORD)(min_h - win->Height));
    }

    /* Recreate gadgets from actual borders (template rebuild pattern). */
    demo_compute_geom(win, font_w, font_h, &geom);
    g_demo_geom = geom;
    RemoveGList(win, glist, -1);
    demo_destroy_gadgets(&glist, &scroller, &apply_gad, &bench_gad);
    if (!demo_create_gadgets(&glist, &scroller,
                             &apply_gad, &bench_gad,
                             vi, screen->Font, font_h, &geom)) {
        RLV_LOG("FAIL post-open gadget recreate");
        goto fail_window;
    }
    AddGList(win, glist, ~0, -1, NULL);
    RefreshGList(glist, win, NULL, -1);

    backend = rlv_backend_v36_create(win->RPort, win->RPort->Font);
    if (backend == 0) {
        RLV_LOG("FAIL rlv_backend_v36_create");
        goto fail_window;
    }

    memset(&cfg, 0, sizeof(cfg));
    cfg.draw_ops = rlv_backend_v36_get_ops();
    cfg.draw_context = rlv_backend_v36_get_context(backend);
    cfg.font = win->RPort->Font;
    cfg.cell_padding_x = g_demo_applied.padding_x;
    cfg.cell_padding_y = g_demo_applied.padding_y;
    cfg.row_gap = g_demo_applied.row_gap;
    cfg.row_divider_style = g_demo_applied.divider_style;
    cfg.initial_expand = g_demo_applied.initial_expand;
    cfg.flags = 0;

    control = rlv_create(&cfg);
    if (control == 0) {
        RLV_LOG("FAIL rlv_create");
        goto fail_backend;
    }
    RLV_LOG("control created");

    rlv_backend_v36_pens_from_drawinfo(dri, &pens);
    rlv_set_pens(control, &pens);

    if (!rlv_set_columns(control, columns, NUM_COLS)) {
        RLV_LOG("FAIL rlv_set_columns");
        goto fail_control;
    }
    if (!rlv_set_rows(control, g_rows, g_demo_row_count)) {
        RLV_LOG("FAIL rlv_set_rows");
        goto fail_control;
    }
#if defined(RLV_ENABLE_SORTING) && (RLV_ENABLE_SORTING != 0)
    if (!demo_install_sort(control, columns)) {
        RLV_LOG("FAIL demo_install_sort");
        goto fail_control;
    }
    DEMO_PRINTF("Sorting enabled: click Name/Date/Pos/On headers.\n");
    DEMO_PRINTF("Heading row is a fixed sort barrier at the top; data below sorts as one run.\n");
    DEMO_PRINTF("Date sorts by DateStamp via CUSTOM context (not display text).\n");
    DEMO_PRINTF("Pos is numeric; first Date click is DESC (recent first).\n");
#ifdef RLV_ENABLE_LOGGING
    DEMO_PRINTF("Logging build: sort map dumped to PROGDIR:rlv.log after each sort.\n");
#endif
#endif
#if defined(RLV_ENABLE_COLUMN_RESIZE) && (RLV_ENABLE_COLUMN_RESIZE != 0)
    if (!demo_install_column_resize(control, font_w)) {
        RLV_LOG("FAIL demo_install_column_resize");
        goto fail_control;
    }
    DEMO_PRINTF("Column resize: drag header dividers (not disclosure/On).\n");
    DEMO_PRINTF("Right button cancels a drag; R resets column widths.\n");
    DEMO_PRINTF("MOUSEMOVE is reported while dragging (including outside the control).\n");
#endif

    demo_bounds_from_geom(&geom, &bounds);
    demo_apply_display_settings(control, &g_demo_applied);
    demo_apply_policies(control);
    rlv_set_bounds(control, &bounds);
    /* Single-control demo: first instance is initially active. */
    active_control = control;
    if (keyboard_off) {
        rlv_set_keyboard_enabled(control, FALSE);
    }

    GT_RefreshWindow(win, NULL);
    demo_paint(control);
    demo_sync_scroller(win, scroller, control, &last_scroll_top);

    DEMO_PRINTF("Phase 5.5 custom control: keyboard nav + resize + scroll sync.\n");
    DEMO_PRINTF("Cursor Up/Down = prev/next; Shift+cursor = page; Ctrl+cursor = first/last.\n");
    DEMO_PRINTF("Right/Left = expand/collapse current expandable row; C = Collapse All.\n");
    DEMO_PRINTF("Return activates selection; Space toggles sole checkbox. Click control for focus.\n");
    DEMO_PRINTF("A = toggle activation policy (SELECT_ROW / KEEP_CURRENT).\n");
    DEMO_PRINTF("V = cycle current-row visual (FULL / MARKER / NONE).\n");
    DEMO_PRINTF("Settings menu selects pending divider/padding/gap/row-display/"
           "long-word/ellipsis; Apply commits.\n");
    DEMO_PRINTF("Row 3 (-- Category --) is non-selectable; Delta/row8 Alpha have empty disclosure cells.\n");
    DEMO_PRINTF("Rows 0 and 8 both show Name Alpha with distinct tags 1000 and 1008.\n");
    DEMO_PRINTF("Activation=%s  Visual=%s\n",
           demo_activation_policy_name(g_demo_activation_policy),
           demo_current_row_visual_name(g_demo_current_row_visual));
    if (keyboard_off) {
        DEMO_PRINTF("NOKEYBOARD: keyboard NAV_* disabled (mouse/scroller still work).\n");
    } else {
        DEMO_PRINTF("Keyboard enabled (default). Pass NOKEYBOARD to disable.\n");
    }
    if (run_exercise) {
        DEMO_PRINTF("CLI EXERCISE: running deterministic sequence after first paint.\n");
    }
#ifdef RLV_ENABLE_BENCHMARKS
    if (run_bench) {
        DEMO_PRINTF("CLI BENCH: benchmark armed. Move windows, then click Start Benchmark.\n");
    }
#endif
    DEMO_FFLUSH();

    if (run_exercise) {
        demo_run_exercise(control, win, scroller, &last_scroll_top);
    }
    RLV_LOG("event loop entered");
    while (!done) {
        Wait(1UL << win->UserPort->mp_SigBit);
        while ((imsg = GT_GetIMsg(win->UserPort)) != 0) {
            ULONG class;
            UWORD code;
            UWORD qual;
            WORD mx;
            WORD my;
            APTR iaddr;
            struct Gadget *g;

            class = imsg->Class;
            code = imsg->Code;
            qual = imsg->Qualifier;
            mx = imsg->MouseX;
            my = imsg->MouseY;
            iaddr = imsg->IAddress;
            g = demo_gadget_from_imsg(class, iaddr, scroller);

#ifdef RLV_ENABLE_LOGGING
            demo_log_idcmp(imsg);
#endif

            /*
             * SIZEVERIFY: Intuition waits for the reply. Detach gadgets
             * before replying so they are not drawn mid-drag.
             */
            if (class == IDCMP_SIZEVERIFY) {
                RLV_LOG("SIZEVERIFY detach gadgets");
                if (glist != 0 && !g_demo_gadgets_detached) {
                    RemoveGList(win, glist, -1);
                    g_demo_gadgets_detached = TRUE;
                }
                GT_ReplyIMsg(imsg);
                continue;
            }

            GT_ReplyIMsg(imsg);

            if (class == IDCMP_CLOSEWINDOW) {
                done = TRUE;
            } else if (class == IDCMP_MENUPICK) {
                (VOID)demo_handle_menu_selection((ULONG)code, win);
            } else if (class == IDCMP_REFRESHWINDOW) {
                RLV_LOG("refresh message received");
                if (g_demo_refresh_depth != 0) {
                    RLV_LOGF("INVARIANT refresh depth unbalanced before begin=%ld",
                             (long)g_demo_refresh_depth);
                }
                RLV_LOG("GT_BeginRefresh begin");
                GT_BeginRefresh(win);
                g_demo_refresh_depth++;
                RLV_LOG("GT_BeginRefresh end");
                demo_paint(control);
                RLV_LOG("GT_EndRefresh begin");
                GT_EndRefresh(win, TRUE);
                g_demo_refresh_depth--;
                RLV_LOG("GT_EndRefresh end");
                if (g_demo_refresh_depth != 0) {
                    RLV_LOGF("INVARIANT refresh depth after end=%ld",
                             (long)g_demo_refresh_depth);
                }
            } else if (class == IDCMP_MOUSEBUTTONS) {
                if ((code & IECODE_UP_PREFIX) == 0
                    && (code & ~IECODE_UP_PREFIX) == IECODE_LBUTTON) {
                    if (demo_point_in_control(&g_demo_geom, mx, my)) {
                        active_control = control;
                        memset(&inev, 0, sizeof(inev));
                        inev.type = (UWORD)RLV_INPUT_SELECT_DOWN;
                        inev.x = mx;
                        inev.y = my;
                        RLV_LOGF("SELECT_DOWN mouse=%d,%d", (int)mx, (int)my);
                        demo_apply_input(active_control, win, scroller, &inev,
                                         &last_scroll_top);
                    }
                } else if ((code & IECODE_UP_PREFIX) != 0
                           && (code & ~IECODE_UP_PREFIX) == IECODE_LBUTTON) {
                    if (active_control != 0) {
                        memset(&inev, 0, sizeof(inev));
                        inev.type = (UWORD)RLV_INPUT_SELECT_UP;
                        inev.x = mx;
                        inev.y = my;
                        RLV_LOGF("SELECT_UP mouse=%d,%d", (int)mx, (int)my);
                        demo_apply_input(active_control, win, scroller, &inev,
                                         &last_scroll_top);
                    }
#if defined(RLV_ENABLE_COLUMN_RESIZE) && (RLV_ENABLE_COLUMN_RESIZE != 0)
                } else if ((code & ~IECODE_UP_PREFIX) == IECODE_RBUTTON) {
                    /* Cancel an in-progress column-resize drag. */
                    if (active_control != 0
                        && rlv_column_resize_is_active(active_control)) {
                        memset(&inev, 0, sizeof(inev));
                        inev.type = (UWORD)RLV_INPUT_CANCEL;
                        inev.x = mx;
                        inev.y = my;
                        demo_apply_input(active_control, win, scroller, &inev,
                                         &last_scroll_top);
                    }
#endif
                }
            } else if (class == IDCMP_RAWKEY) {
                UWORD key;

                key = (UWORD)(code & ~IECODE_UP_PREFIX);
                if ((code & IECODE_UP_PREFIX) == 0
                    && active_control != 0
                    && key == DEMO_RAWKEY_A) {
                    demo_cycle_activation_policy(win, active_control);
                } else if ((code & IECODE_UP_PREFIX) == 0
                           && active_control != 0
                           && key == DEMO_RAWKEY_V) {
                    demo_cycle_current_row_visual(win, active_control);
                } else if ((code & IECODE_UP_PREFIX) == 0
                           && active_control != 0
                           && key == DEMO_RAWKEY_C) {
                    UWORD ri;

                    rlv_collapse_all(active_control);
                    for (ri = 0; ri < g_demo_row_count; ri++) {
                        g_rows[ri].flags = (UWORD)(g_rows[ri].flags
                                                   & (UWORD)~RLV_ROW_EXPANDED);
                    }
                    demo_update_status_text(win, "Collapse All");
                    demo_paint_viewport(active_control);
                    demo_sync_scroller(win, scroller, active_control,
                                      &last_scroll_top);
                    RLV_LOG("COLLAPSE_ALL via key C");
#if defined(RLV_ENABLE_COLUMN_RESIZE) && (RLV_ENABLE_COLUMN_RESIZE != 0)
                } else if ((code & IECODE_UP_PREFIX) == 0
                           && active_control != 0
                           && key == DEMO_RAWKEY_R) {
                    if (rlv_reset_column_widths(active_control)) {
                        struct Rectangle reset_bounds;

                        demo_bounds_from_geom(&g_demo_geom, &reset_bounds);
                        rlv_set_bounds(active_control, &reset_bounds);
                        demo_update_status_text(win, "Column widths reset");
                        demo_paint(active_control);
                        demo_sync_scroller(win, scroller, active_control,
                                          &last_scroll_top);
                        RLV_LOG("COLUMN_RESIZE reset via key R");
                    }
#endif
                } else if (active_control != 0
                           && rlv_get_keyboard_enabled(active_control)
                           && demo_translate_rawkey(code, qual, &inev)) {
                    RLV_LOGF("RAWKEY code=0x%04x qual=0x%04x -> input type=%u",
                             (unsigned)code, (unsigned)qual,
                             (unsigned)inev.type);
                    demo_apply_input(active_control, win, scroller, &inev,
                                     &last_scroll_top);
                }
            } else if (class == IDCMP_GADGETUP && g != 0) {
                if (g->GadgetID == GID_APPLY) {
                    if (demo_apply_pending_settings(
                            &control, backend, win, scroller,
                            &pens, columns, keyboard_off,
                            &last_scroll_top)) {
                        active_control = control;
                    }
#ifdef RLV_ENABLE_BENCHMARKS
                } else if (g->GadgetID == GID_BENCH) {
                    if (run_bench) {
                        demo_run_benchmarks(control, win, scroller,
                                            &last_scroll_top);
                    }
#endif
                } else if (g->GadgetID == GID_SCROLL) {
                    active_control = control;
                    demo_handle_scroller(control, win, scroller,
                                         &last_scroll_top, class, code);
                }
            } else if (class == IDCMP_GADGETDOWN
                       && g != 0 && g->GadgetID == GID_SCROLL) {
                active_control = control;
                demo_handle_scroller(control, win, scroller,
                                     &last_scroll_top, class, code);
            } else if (class == IDCMP_MOUSEMOVE) {
#if defined(RLV_ENABLE_COLUMN_RESIZE) && (RLV_ENABLE_COLUMN_RESIZE != 0)
                if (active_control != 0
                    && rlv_column_resize_is_active(active_control)) {
                    memset(&inev, 0, sizeof(inev));
                    inev.type = (UWORD)RLV_INPUT_POINTER_MOVE;
                    inev.x = mx;
                    inev.y = my;
                    demo_apply_input(active_control, win, scroller, &inev,
                                     &last_scroll_top);
                } else
#endif
                if (g != 0 && g->GadgetID == GID_SCROLL) {
                    active_control = control;
                    demo_handle_scroller(control, win, scroller,
                                         &last_scroll_top, class, code);
                }
            } else if (class == IDCMP_NEWSIZE) {
                if (!g_demo_gadgets_detached && glist != 0) {
                    RemoveGList(win, glist, -1);
                    g_demo_gadgets_detached = TRUE;
                }
                demo_handle_newsize(win, &glist, &scroller,
                                    &apply_gad, &bench_gad,
                                    vi, screen->Font, dri,
                                    control, font_w, font_h,
                                    &last_scroll_top);
                demo_compute_geom(win, font_w, font_h, &g_demo_geom);
            } else if (class == IDCMP_INTUITICKS) {
                /* Logged in demo_log_idcmp; no action. */
            }
        }
    }
    RLV_LOG("event loop exited");

    RLV_LOG("control destroyed");
    rlv_destroy(control);
    control = 0;
    rlv_backend_v36_destroy(backend);
    backend = 0;

    RLV_LOG("window closed");
    demo_cleanup_menus(win);
    CloseWindow(win);
    win = 0;
    FreeGadgets(glist);
    glist = 0;

    FreeScreenDrawInfo(screen, dri);
    dri = 0;
    FreeVisualInfo(vi);
    UnlockPubScreen(NULL, screen);
    RLV_LOG("libraries closed / screen unlocked");
    rlv_log_shutdown();
    return 0;

fail_control:
    RLV_LOG("cleanup fail_control");
    rlv_destroy(control);
    control = 0;
fail_backend:
    RLV_LOG("cleanup fail_backend");
    rlv_backend_v36_destroy(backend);
    backend = 0;
fail_window:
    RLV_LOG("cleanup fail_window");
    if (win != 0) {
        demo_cleanup_menus(win);
        CloseWindow(win);
        win = 0;
    }
fail_gadgets:
    RLV_LOG("cleanup fail_gadgets");
    if (glist != 0) {
        FreeGadgets(glist);
        glist = 0;
    }
fail:
    RLV_LOG("cleanup fail");
    demo_cleanup_menus(0);
    if (dri != 0) {
        FreeScreenDrawInfo(screen, dri);
    }
    if (vi != 0) {
        FreeVisualInfo(vi);
    }
    UnlockPubScreen(NULL, screen);
    rlv_log_shutdown();
    return exit_code;
}

