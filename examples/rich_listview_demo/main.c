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
 *     Booleans (user_data); on CELL_CONTROL sync the store then
 *     rlv_render_cell_control (row fallback when needed).
 *   - Read-only TEXT_KIND status field under the ListView showing the
 *     latest RLV_EVENT_CELL_CONTROL (GT_SetGadgetAttrs / GTTX_Text).
 *     Policy changes also update this status line.
 *
 * Required modules: rlv_*.o (incl. wrap + checkbox), rlv_backend_amiga_v36.o,
 *                   rlv_platform.o
 * Deliberately excluded: clv_renderer_*.o, clv_selection.o, clv_pixel_wrap.o,
 *                        ASCII formatters, clv_cellctl_* (legacy GadTools path)
 *
 * Optional logging build (make rich-listview-demo-log) also links
 * rlv_log.o and writes PROGDIR:rlv.log.
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

#include <stdio.h>
#include <string.h>

long __stack = 80000L;

#define GID_SCROLL 1
#define GID_GO     2
#define GID_BENCH  3
#define GID_DIVIDER_STYLE 4
#define GID_PADDING_X     5
#define GID_PADDING_Y     6
#define GID_ROW_GAP       7
#define GID_EVENT_STATUS  8
#define NUM_COLS   5
#define DEMO_EVENT_TEXT_LEN 160
#ifdef RLV_ENABLE_BENCHMARKS
#define DEMO_MAX_ROWS 96
#define RLV_BENCH_NAV_STEPS 50
#define RLV_BENCH_PREPARE_RUNS 3
#define RLV_BENCH_WARMUP_STEPS 5
#else
#define DEMO_MAX_ROWS 9
#endif

/* Classic Return / Space raw keys (not OS3.2-only names). */
#define DEMO_RAWKEY_RETURN  0x44
#define DEMO_RAWKEY_SPACE   0x40
#define DEMO_RAWKEY_A       0x20
#define DEMO_RAWKEY_V       0x34

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
static BOOL g_demo_gadgets_detached = FALSE;
static UWORD g_demo_divider_style = (UWORD)RLV_ROW_DIVIDER_SOLID;
static UWORD g_demo_padding_x = 1;
static UWORD g_demo_padding_y = 1;
static UWORD g_demo_row_gap = 0;
static UWORD g_demo_activation_policy =
    (UWORD)RLV_CONTROL_ACTIVATE_SELECT_ROW;
static UWORD g_demo_current_row_visual =
    (UWORD)RLV_CURRENT_ROW_VISUAL_FULL;
static STRPTR g_demo_divider_labels[] = {
    "Div: None", "Div: Solid", "Div: Dotted", NULL
};
static STRPTR g_demo_padding_x_labels[] = {
    "X Pad: 0", "X Pad: 1", "X Pad: 2", "X Pad: 3", "X Pad: 4", NULL
};
static STRPTR g_demo_padding_y_labels[] = {
    "Y Pad: 0", "Y Pad: 1", "Y Pad: 2", "Y Pad: 3", "Y Pad: 4", NULL
};
static STRPTR g_demo_row_gap_labels[] = {
    "Gap: 0", "Gap: 1", "Gap: 2", "Gap: 3", "Gap: 4", NULL
};
/* Fixed column+divider content width (pixels); set once after font scale. */
static WORD g_demo_fixed_content_w = 0;
static WORD g_demo_min_ctrl_w = 0;

/*
 * Cached layout metrics derived from the window interior.
 * Control owns frame/header/viewport insets; demo only places the outer
 * control rectangle, scroller, and controls strip.
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
    WORD go_left;
    WORD go_top;
    WORD go_w;
    WORD go_h;
    WORD cycle_left;
    WORD cycle_top;
    WORD cycle_w;
    WORD cycle_h;
    WORD padding_x_left;
    WORD padding_x_top;
    WORD padding_x_w;
    WORD padding_x_h;
    WORD padding_y_left;
    WORD padding_y_top;
    WORD padding_y_w;
    WORD padding_y_h;
    WORD row_gap_left;
    WORD row_gap_top;
    WORD row_gap_w;
    WORD row_gap_h;
    WORD bench_left;
    WORD bench_top;
    WORD bench_w;
    WORD bench_h;
} DemoGeom;

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
    /* Name wraps; Type truncates (NONE); Description wraps heavily.
     * On = experimental checkbox column (mouse + Space). */
    { "Name",        10 * 8, RLV_CELL_ALIGN_LEFT,   RLV_WRAP_WORD_OR_CHAR, 0 },
    { "Type",         8 * 8, RLV_CELL_ALIGN_LEFT,   RLV_WRAP_NONE, 0 },
    { "Description", 18 * 8, RLV_CELL_ALIGN_LEFT,   RLV_WRAP_WORD_OR_CHAR, 0 },
    { "Status",       6 * 8, RLV_CELL_ALIGN_LEFT,   RLV_WRAP_NONE, 0 },
    { "On",           3 * 8, RLV_CELL_ALIGN_CENTER, RLV_WRAP_NONE,
      RLV_COL_TYPE_CHECKBOX }
};

static const char *g_row0[NUM_COLS] = {
    "Alpha", "Tool", "A compact cleanup utility", "Ready", ""
};
static const char *g_row1[NUM_COLS] = {
    "Beta Package With A Rather Long Name",
    "Library",
    "Shared runtime routines used by several Workbench tools",
    "Idle", ""
};
static const char *g_row2[NUM_COLS] = {
    "Gamma",
    "Tool",
    "Long description text that wraps across two or three lines when the "
    "Description column is narrow enough for word wrapping",
    "Busy", ""
};
static const char *g_row3[NUM_COLS] = {
    "-- Category --", "Heading", "Non-selectable section title row", "-", ""
};
static const char *g_row4[NUM_COLS] = {
    "Delta", "Data", "Configuration presets", "Ready", ""
};
static const char *g_row5[NUM_COLS] = {
    "Epsilon",
    "Tool",
    "Another tool entry with enough descriptive prose to occupy four "
    "wrapped display lines inside one logical row so the whole block "
    "stays a single geometric item with one row gap after it",
    "Done", ""
};
static const char *g_row6[NUM_COLS] = {
    "Zeta", "Library", "Support module", "Ready", ""
};
static const char *g_row7[NUM_COLS] = {
    "Eta Path/Example:Deep_Folder-Name",
    "PathTest",
    "PATH wrap is not enabled here; this Name column uses WORD_OR_CHAR",
    "Test", ""
};
static const char *g_row8[NUM_COLS] = {
    "Theta",
    "VeryLongUnbrokenTypeToken",
    "Short",
    "Trunc", ""
};

/*
 * App-owned authoritative checkbox store (column DEMO_CB_COL = On).
 * set_rows borrows these descriptors and copies into the control snapshot;
 * the control never writes back. On CELL_CONTROL the demo syncs the store
 * via row user_data (pointer to the On-cell value UBYTE), then repaints.
 */
#define DEMO_CB_COL  4
#define DEMO_CB_ON \
    (UBYTE)(RLV_CELL_F_VISIBLE | RLV_CELL_F_ENABLED \
            | RLV_CELL_F_INTERACTIVE)
#define DEMO_CB_DISPLAY \
    (UBYTE)(RLV_CELL_F_VISIBLE | RLV_CELL_F_ENABLED)
#define DEMO_CB_DISABLED \
    (UBYTE)(RLV_CELL_F_VISIBLE)

/* Initial templates (copied into the mutable store at demo_init_rows). */
static const RLV_Cell g_ctrl_init0[NUM_COLS] = {
    { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
    { DEMO_CB_ON, RLV_CELL_CHECKED }
};
static const RLV_Cell g_ctrl_init1[NUM_COLS] = {
    { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
    { DEMO_CB_ON, RLV_CELL_UNCHECKED }
};
static const RLV_Cell g_ctrl_init2[NUM_COLS] = {
    { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
    { DEMO_CB_ON, RLV_CELL_CHECKED }
};
static const RLV_Cell g_ctrl_init3[NUM_COLS] = {
    { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
    { 0, 0 } /* heading: no visible checkbox */
};
static const RLV_Cell g_ctrl_init4[NUM_COLS] = {
    { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
    { DEMO_CB_DISPLAY, RLV_CELL_CHECKED } /* display-only */
};
static const RLV_Cell g_ctrl_init5[NUM_COLS] = {
    { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
    { DEMO_CB_ON, RLV_CELL_UNCHECKED }
};
static const RLV_Cell g_ctrl_init6[NUM_COLS] = {
    { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
    { DEMO_CB_DISABLED, RLV_CELL_CHECKED } /* ghosted */
};
static const RLV_Cell g_ctrl_init7[NUM_COLS] = {
    { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
    { DEMO_CB_ON, RLV_CELL_CHECKED }
};
static const RLV_Cell g_ctrl_init8[NUM_COLS] = {
    { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
    { DEMO_CB_ON, RLV_CELL_UNCHECKED }
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
            row_name_buf[i][0] = name_buf[i];
            row_name_buf[i][1] = type_buf[i];
            row_name_buf[i][2] = desc_buf[i];
            row_name_buf[i][3] = status_buf[i];
            row_name_buf[i][4] = "";
            cells[i] = row_name_buf[i];

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
        /* Point user_data at the authoritative On Boolean when present. */
        if (g_demo_ctrl_store[i][DEMO_CB_COL].flags != 0) {
            g_rows[i].user_data =
                (APTR)&g_demo_ctrl_store[i][DEMO_CB_COL].value;
        } else {
            g_rows[i].user_data = NULL;
        }
    }
    /* Non-selectable category/heading row for Phase 4 rejection demo. */
    if (g_demo_row_count > 3) {
        g_rows[3].flags = RLV_ROW_NONSELECTABLE;
    }
}

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
    printf("%s\n", g_demo_event_text);
    fflush(stdout);
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
    printf("%s\n", g_demo_event_text);
    fflush(stdout);
    RLV_LOGF("demo current_row_visual=%u",
             (unsigned)g_demo_current_row_visual);
}

/*
 * Format CELL_CONTROL into the persistent buffer and push via GTTX_Text.
 * GadTools borrows the pointer (V36+); do not free or reuse for other text.
 */
static VOID demo_update_event_status(struct Window *win, const RLV_Event *ev)
{
    CONST_STRPTR type_name;
    CONST_STRPTR action_name;
    CONST_STRPTR row_name;
    CONST_STRPTR col_name;

    if (win == 0 || ev == 0) {
        return;
    }

    type_name = demo_control_type_name(ev->control_type);
    action_name = demo_control_action_name(ev->control_action);
    row_name = "-";
    col_name = "-";
    if (ev->row >= 0 && (UWORD)ev->row < g_demo_row_count
        && g_rows[ev->row].cells != 0
        && g_rows[ev->row].cells[0] != 0) {
        row_name = g_rows[ev->row].cells[0];
    }
    if (ev->column < (UWORD)NUM_COLS && g_columns[ev->column].title != 0) {
        col_name = g_columns[ev->column].title;
    }

    if (strlen(row_name) > 20) {
        /* Compact when the Name cell is long (wrap-demo rows). */
        sprintf(g_demo_event_text,
                "CELL row=%ld col=%u (%s) %s %s %u -> %u",
                (long)ev->row,
                (unsigned)ev->column,
                col_name,
                type_name,
                action_name,
                (unsigned)ev->previous_value,
                (unsigned)ev->cell_value);
    } else {
        sprintf(g_demo_event_text,
                "CELL: row=%ld (%s) col=%u (%s) %s %s %u -> %u",
                (long)ev->row,
                row_name,
                (unsigned)ev->column,
                col_name,
                type_name,
                action_name,
                (unsigned)ev->previous_value,
                (unsigned)ev->cell_value);
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
 * Derive outer control, scroller, status field, and action strip from the
 * usable window interior. Control width is at least g_demo_min_ctrl_w
 * (fixed columns); spare horizontal space stays after the last column.
 * Status TEXT_KIND sits between the ListView and the Go/cycle strip.
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
    /* status + pad + action strip below the list. */
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

    /* Status + controls strip below the list; gaps DEMO_PAD. */
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

    out->go_left = inner_left;
    out->go_top = (WORD)(out->status_top + status_h + DEMO_PAD);
    out->go_w = (WORD)(font_w * 6);
    out->go_h = action_h;
    if (out->go_w < 40) {
        out->go_w = 40;
    }

    out->cycle_left = (WORD)(out->go_left + out->go_w + DEMO_PAD);
    out->cycle_top = out->go_top;
    out->cycle_w = (WORD)(font_w * 14);
    out->cycle_h = action_h;
    if (out->cycle_w < 88) {
        out->cycle_w = 88;
    }

    out->padding_x_left =
        (WORD)(out->cycle_left + out->cycle_w + DEMO_PAD);
    out->padding_x_top = out->go_top;
    out->padding_x_w = (WORD)(font_w * 10);
    out->padding_x_h = action_h;
    if (out->padding_x_w < 72) {
        out->padding_x_w = 72;
    }

    out->padding_y_left =
        (WORD)(out->padding_x_left + out->padding_x_w + DEMO_PAD);
    out->padding_y_top = out->go_top;
    out->padding_y_w = (WORD)(font_w * 10);
    out->padding_y_h = action_h;
    if (out->padding_y_w < 72) {
        out->padding_y_w = 72;
    }

    out->row_gap_left =
        (WORD)(out->padding_y_left + out->padding_y_w + DEMO_PAD);
    out->row_gap_top = out->go_top;
    out->row_gap_w = (WORD)(font_w * 8);
    out->row_gap_h = action_h;
    if (out->row_gap_w < 64) {
        out->row_gap_w = 64;
    }

    out->bench_left =
        (WORD)(out->row_gap_left + out->row_gap_w + DEMO_PAD);
    out->bench_top = out->go_top;
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
                                 struct Gadget **go_io,
                                 struct Gadget **cycle_io,
                                 struct Gadget **padding_x_io,
                                 struct Gadget **padding_y_io,
                                 struct Gadget **row_gap_io,
                                 struct Gadget **bench_io)
{
    if (glist_io != 0 && *glist_io != 0) {
        FreeGadgets(*glist_io);
        *glist_io = 0;
    }
    g_demo_event_status_gad = 0;
    if (scroller_io != 0) {
        *scroller_io = 0;
    }
    if (go_io != 0) {
        *go_io = 0;
    }
    if (cycle_io != 0) {
        *cycle_io = 0;
    }
    if (padding_x_io != 0) {
        *padding_x_io = 0;
    }
    if (padding_y_io != 0) {
        *padding_y_io = 0;
    }
    if (row_gap_io != 0) {
        *row_gap_io = 0;
    }
    if (bench_io != 0) {
        *bench_io = 0;
    }
}

static BOOL demo_create_gadgets(struct Gadget **glist_io,
                                struct Gadget **scroller_io,
                                struct Gadget **go_io,
                                struct Gadget **cycle_io,
                                struct Gadget **padding_x_io,
                                struct Gadget **padding_y_io,
                                struct Gadget **row_gap_io,
                                struct Gadget **bench_io,
                                APTR vi,
                                struct TextAttr *textattr,
                                WORD font_h,
                                const DemoGeom *geom)
{
    struct Gadget *gad;
    struct Gadget *scroller;
    struct Gadget *go;
    struct Gadget *cycle;
    struct Gadget *padding_x;
    struct Gadget *padding_y;
    struct Gadget *row_gap;
    struct Gadget *bench;
    struct NewGadget ng;

    if (glist_io == 0 || scroller_io == 0 || go_io == 0 || cycle_io == 0
        || padding_x_io == 0 || padding_y_io == 0 || row_gap_io == 0
        || bench_io == 0 || vi == 0 || geom == 0) {
        return FALSE;
    }

    *glist_io = 0;
    *scroller_io = 0;
    *go_io = 0;
    *cycle_io = 0;
    *padding_x_io = 0;
    *padding_y_io = 0;
    *row_gap_io = 0;
    *bench_io = 0;
    g_demo_event_status_gad = 0;

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
        demo_destroy_gadgets(glist_io, scroller_io, go_io, cycle_io,
                             padding_x_io, padding_y_io, row_gap_io, bench_io);
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
        demo_destroy_gadgets(glist_io, scroller_io, go_io, cycle_io,
                             padding_x_io, padding_y_io, row_gap_io, bench_io);
        return FALSE;
    }
    g_demo_event_status_gad = gad;

    ng.ng_LeftEdge = geom->go_left;
    ng.ng_TopEdge = geom->go_top;
    ng.ng_Width = geom->go_w;
    ng.ng_Height = geom->go_h;
    ng.ng_GadgetText = "Go";
    ng.ng_GadgetID = GID_GO;
    ng.ng_Flags = PLACETEXT_IN;
    go = gad = CreateGadget(BUTTON_KIND, gad, &ng, TAG_END);
    if (gad == 0) {
        RLV_LOG("FAIL CreateGadget GO BUTTON_KIND");
        demo_destroy_gadgets(glist_io, scroller_io, go_io, cycle_io,
                             padding_x_io, padding_y_io, row_gap_io, bench_io);
        return FALSE;
    }

    ng.ng_LeftEdge = geom->cycle_left;
    ng.ng_TopEdge = geom->cycle_top;
    ng.ng_Width = geom->cycle_w;
    ng.ng_Height = geom->cycle_h;
    ng.ng_GadgetText = NULL;
    ng.ng_GadgetID = GID_DIVIDER_STYLE;
    ng.ng_Flags = 0;
    cycle = gad = CreateGadget(CYCLE_KIND, gad, &ng,
                               GTCY_Labels, (ULONG)g_demo_divider_labels,
                               GTCY_Active, (ULONG)g_demo_divider_style,
                               TAG_END);
    if (gad == 0) {
        RLV_LOG("FAIL CreateGadget DIVIDER CYCLE_KIND");
        demo_destroy_gadgets(glist_io, scroller_io, go_io, cycle_io,
                             padding_x_io, padding_y_io, row_gap_io, bench_io);
        return FALSE;
    }

    ng.ng_LeftEdge = geom->padding_x_left;
    ng.ng_TopEdge = geom->padding_x_top;
    ng.ng_Width = geom->padding_x_w;
    ng.ng_Height = geom->padding_x_h;
    ng.ng_GadgetID = GID_PADDING_X;
    padding_x = gad = CreateGadget(CYCLE_KIND, gad, &ng,
                                   GTCY_Labels,
                                   (ULONG)g_demo_padding_x_labels,
                                   GTCY_Active, (ULONG)g_demo_padding_x,
                                   TAG_END);
    if (gad == 0) {
        RLV_LOG("FAIL CreateGadget PADDING_X CYCLE_KIND");
        demo_destroy_gadgets(glist_io, scroller_io, go_io, cycle_io,
                             padding_x_io, padding_y_io, row_gap_io, bench_io);
        return FALSE;
    }

    ng.ng_LeftEdge = geom->padding_y_left;
    ng.ng_TopEdge = geom->padding_y_top;
    ng.ng_Width = geom->padding_y_w;
    ng.ng_Height = geom->padding_y_h;
    ng.ng_GadgetID = GID_PADDING_Y;
    padding_y = gad = CreateGadget(CYCLE_KIND, gad, &ng,
                                   GTCY_Labels,
                                   (ULONG)g_demo_padding_y_labels,
                                   GTCY_Active, (ULONG)g_demo_padding_y,
                                   TAG_END);
    if (gad == 0) {
        RLV_LOG("FAIL CreateGadget PADDING_Y CYCLE_KIND");
        demo_destroy_gadgets(glist_io, scroller_io, go_io, cycle_io,
                             padding_x_io, padding_y_io, row_gap_io, bench_io);
        return FALSE;
    }

    ng.ng_LeftEdge = geom->row_gap_left;
    ng.ng_TopEdge = geom->row_gap_top;
    ng.ng_Width = geom->row_gap_w;
    ng.ng_Height = geom->row_gap_h;
    ng.ng_GadgetID = GID_ROW_GAP;
    row_gap = gad = CreateGadget(CYCLE_KIND, gad, &ng,
                                 GTCY_Labels, (ULONG)g_demo_row_gap_labels,
                                 GTCY_Active, (ULONG)g_demo_row_gap,
                                 TAG_END);
    if (gad == 0) {
        RLV_LOG("FAIL CreateGadget ROW_GAP CYCLE_KIND");
        demo_destroy_gadgets(glist_io, scroller_io, go_io, cycle_io,
                             padding_x_io, padding_y_io, row_gap_io, bench_io);
        return FALSE;
    }

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
            demo_destroy_gadgets(glist_io, scroller_io,
                                 go_io, cycle_io, padding_x_io, padding_y_io,
                                 row_gap_io, bench_io);
            return FALSE;
        }
    }
#endif

    *scroller_io = scroller;
    *go_io = go;
    *cycle_io = cycle;
    *padding_x_io = padding_x;
    *padding_y_io = padding_y;
    *row_gap_io = row_gap;
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
                                struct Gadget **go_io,
                                struct Gadget **cycle_io,
                                struct Gadget **padding_x_io,
                                struct Gadget **padding_y_io,
                                struct Gadget **row_gap_io,
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
        || scroller_io == 0 || go_io == 0 || cycle_io == 0
        || padding_x_io == 0 || padding_y_io == 0 || row_gap_io == 0
        || bench_io == 0) {
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

    demo_destroy_gadgets(glist_io, scroller_io, go_io, cycle_io,
                         padding_x_io, padding_y_io, row_gap_io, bench_io);
    if (!demo_create_gadgets(glist_io, scroller_io, go_io, cycle_io,
                             padding_x_io, padding_y_io, row_gap_io, bench_io,
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

static BOOL demo_idcmp_is_gadget_class(ULONG class)
{
    if (class == IDCMP_GADGETUP || class == IDCMP_GADGETDOWN) {
        return TRUE;
    }
    if (class == IDCMP_MOUSEMOVE) {
        return TRUE; /* SCROLLERIDCMP may deliver gadget IAddress */
    }
    return FALSE;
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

    if (demo_idcmp_is_gadget_class(class) && iaddr != 0) {
        g = (struct Gadget *)iaddr;
        RLV_LOGF("IDCMP gadget GadgetID=%u", (unsigned)g->GadgetID);
        if (g->GadgetID != GID_SCROLL
            && g->GadgetID != GID_GO
            && g->GadgetID != GID_DIVIDER_STYLE
            && g->GadgetID != GID_PADDING_X
            && g->GadgetID != GID_PADDING_Y
            && g->GadgetID != GID_ROW_GAP
            && g->GadgetID != GID_BENCH
            && g->GadgetID != GID_EVENT_STATUS) {
            RLV_LOGF("INVARIANT unexpected GadgetID=%u",
                     (unsigned)g->GadgetID);
        }
    } else if (demo_idcmp_is_gadget_class(class) && iaddr == 0) {
        RLV_LOG("INVARIANT gadget-class message with NULL IAddress");
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

    printf("EXERCISE: reset scroll_y=0 selected=-1, then fixed NAV/scroll sequence.\n");
    fflush(stdout);

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

    printf("EXERCISE complete (selected=%ld scroll_y=%ld). Window remains open.\n",
           (long)rlv_get_selected(control),
           (long)rlv_get_scroll_y(control));
    fflush(stdout);
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
    printf("BENCH: running deterministic benchmark suite.\n");
    fflush(stdout);

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
    printf("BENCH complete. Report written to PROGDIR:rlv_benchmark.txt\n");
    fflush(stdout);
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
        printf("Selected logical row %ld\n", (long)ev.row);
        fflush(stdout);
    } else if (ev.type == (UWORD)RLV_EVENT_CELL_CONTROL) {
        printf("Cell control row %ld col %u type=%u action=%u: %u -> %u\n",
               (long)ev.row,
               (unsigned)ev.column,
               (unsigned)ev.control_type,
               (unsigned)ev.control_action,
               (unsigned)ev.previous_value,
               (unsigned)ev.cell_value);
        fflush(stdout);
        demo_update_event_status(win, &ev);
        /*
         * Integrator pattern on CELL_CONTROL:
         *   1) Sync the app-owned authoritative Boolean via row_user_data
         *      (control already mutated its internal snapshot only).
         *   2) Prefer rlv_render_cell_control; escalate only when that
         *      reports ROW / VIEWPORT. Selection (if any) already arrived
         *      on a prior SELECT_DOWN call under SELECT_ROW policy.
         */
        if (ev.row_user_data != NULL) {
            *(UBYTE *)ev.row_user_data = ev.cell_value;
        }
        {
            UWORD repaint;

            repaint = rlv_render_cell_control(control, ev.row, ev.column);
            RLV_LOGF("CELL_CONTROL store sync + cell paint row=%ld col=%u "
                     "type=%u action=%u val=%u result=%u selected=%ld",
                     (long)ev.row, (unsigned)ev.column,
                     (unsigned)ev.control_type,
                     (unsigned)ev.control_action,
                     (unsigned)ev.cell_value,
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
        printf("Activated logical row %ld\n", (long)ev.row);
        fflush(stdout);
        RLV_LOG("ACTIVATED — no repaint");
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
                                  BOOL keyboard_off)
{
    RLV_Config cfg;
    RLV_Control *old_control;
    RLV_Control *new_control;
    LONG selected;
    LONG scroll_y;
    BOOL keyboard_enabled;

    if (control_io == 0 || backend == 0 || pens == 0
        || columns == 0 || bounds == 0) {
        return FALSE;
    }

    old_control = *control_io;
    selected = -1;
    scroll_y = 0;
    keyboard_enabled = keyboard_off ? FALSE : TRUE;
    if (old_control != 0) {
        selected = rlv_get_selected(old_control);
        scroll_y = rlv_get_scroll_y(old_control);
        keyboard_enabled =
            rlv_get_keyboard_enabled(old_control);
        g_demo_activation_policy =
            rlv_get_control_activation_policy(old_control);
        g_demo_current_row_visual =
            rlv_get_current_row_visual(old_control);
    }

    memset(&cfg, 0, sizeof(cfg));
    cfg.draw_ops = rlv_backend_v36_get_ops();
    cfg.draw_context = rlv_backend_v36_get_context(backend);
    cfg.font = font;
    cfg.cell_padding_x = g_demo_padding_x;
    cfg.cell_padding_y = g_demo_padding_y;
    cfg.row_gap = g_demo_row_gap;
    cfg.row_divider_style = g_demo_divider_style;
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

    rlv_set_bounds(new_control, bounds);
    demo_apply_policies(new_control);
    rlv_set_selected(new_control, selected);
    rlv_set_scroll_y(new_control, scroll_y);

    rlv_destroy(old_control);
    *control_io = new_control;
    return TRUE;
}

int main(int argc, char **argv)
{
    struct Screen *screen = 0;
    struct Window *win = 0;
    struct Gadget *glist = 0;
    struct Gadget *scroller = 0;
    struct Gadget *go_gad = 0;
    struct Gadget *divider_cycle_gad = 0;
    struct Gadget *padding_x_gad = 0;
    struct Gadget *padding_y_gad = 0;
    struct Gadget *row_gap_gad = 0;
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

    /* Scale column widths from actual font, not hard-coded Topaz. */
    for (i = 0; i < NUM_COLS; i++) {
        columns[i] = g_columns[i];
    }
    columns[0].width_pixels = (WORD)(10 * font_w);
    columns[1].width_pixels = (WORD)(8 * font_w);
    columns[2].width_pixels = (WORD)(18 * font_w);
    columns[3].width_pixels = (WORD)(6 * font_w);
    columns[4].width_pixels = (WORD)(3 * font_w);

    /*
     * Fixed pixel column widths: window may not shrink below
     * borders + pad + frame + sum(widths) + dividers + scroller + pad.
     */
    g_demo_fixed_content_w = demo_fixed_content_width(columns, NUM_COLS);
    g_demo_min_ctrl_w = demo_min_ctrl_width(g_demo_fixed_content_w);

    gadget_w = (WORD)(font_w * 6);
    if (gadget_w < 40) {
        gadget_w = 40;
    }
    action_w = gadget_w;
    gadget_w = (WORD)(font_w * 14);
    if (gadget_w < 88) {
        gadget_w = 88;
    }
    action_w = (WORD)(action_w + DEMO_PAD + gadget_w);
    gadget_w = (WORD)(font_w * 10);
    if (gadget_w < 72) {
        gadget_w = 72;
    }
    action_w = (WORD)(action_w + DEMO_PAD + gadget_w);
    action_w = (WORD)(action_w + DEMO_PAD + gadget_w);
    gadget_w = (WORD)(font_w * 8);
    if (gadget_w < 64) {
        gadget_w = 64;
    }
    action_w = (WORD)(action_w + DEMO_PAD + gadget_w);
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
     * controls strip. WA_MinWidth uses the same horizontal requirement.
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
    geom.go_left = geom.ctrl_left;
    geom.go_top = (WORD)(geom.status_top + geom.status_h + DEMO_PAD);
    geom.go_w = (WORD)(font_w * 6);
    if (geom.go_w < 40) {
        geom.go_w = 40;
    }
    geom.go_h = (WORD)(font_h + 6);
    geom.cycle_left = (WORD)(geom.go_left + geom.go_w + DEMO_PAD);
    geom.cycle_top = geom.go_top;
    geom.cycle_w = (WORD)(font_w * 14);
    if (geom.cycle_w < 88) {
        geom.cycle_w = 88;
    }
    geom.cycle_h = geom.go_h;
    geom.padding_x_left =
        (WORD)(geom.cycle_left + geom.cycle_w + DEMO_PAD);
    geom.padding_x_top = geom.go_top;
    geom.padding_x_w = (WORD)(font_w * 10);
    if (geom.padding_x_w < 72) {
        geom.padding_x_w = 72;
    }
    geom.padding_x_h = geom.go_h;
    geom.padding_y_left =
        (WORD)(geom.padding_x_left + geom.padding_x_w + DEMO_PAD);
    geom.padding_y_top = geom.go_top;
    geom.padding_y_w = (WORD)(font_w * 10);
    if (geom.padding_y_w < 72) {
        geom.padding_y_w = 72;
    }
    geom.padding_y_h = geom.go_h;
    geom.row_gap_left =
        (WORD)(geom.padding_y_left + geom.padding_y_w + DEMO_PAD);
    geom.row_gap_top = geom.go_top;
    geom.row_gap_w = (WORD)(font_w * 8);
    if (geom.row_gap_w < 64) {
        geom.row_gap_w = 64;
    }
    geom.row_gap_h = geom.go_h;
    geom.bench_left =
        (WORD)(geom.row_gap_left + geom.row_gap_w + DEMO_PAD);
    geom.bench_top = geom.go_top;
    geom.bench_w = (WORD)(font_w * 16);
    if (geom.bench_w < 96) {
        geom.bench_w = 96;
    }
    geom.bench_h = geom.go_h;

    if (!demo_create_gadgets(&glist, &scroller,
                             &go_gad, &divider_cycle_gad,
                             &padding_x_gad, &padding_y_gad, &row_gap_gad,
                             &bench_gad,
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
                         WA_PubScreen, (ULONG)screen,
                         TAG_END);
    if (win == 0) {
        RLV_LOG("FAIL OpenWindowTags");
        goto fail_gadgets;
    }
    RLV_LOG("window opened");

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
    demo_destroy_gadgets(&glist, &scroller,
                         &go_gad, &divider_cycle_gad,
                         &padding_x_gad, &padding_y_gad, &row_gap_gad,
                         &bench_gad);
    if (!demo_create_gadgets(&glist, &scroller,
                             &go_gad, &divider_cycle_gad,
                             &padding_x_gad, &padding_y_gad, &row_gap_gad,
                             &bench_gad,
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
    cfg.cell_padding_x = g_demo_padding_x;
    cfg.cell_padding_y = g_demo_padding_y;
    cfg.row_gap = g_demo_row_gap;
    cfg.row_divider_style = g_demo_divider_style;
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

    demo_bounds_from_geom(&geom, &bounds);
    rlv_set_bounds(control, &bounds);
    demo_apply_policies(control);
    /* Single-control demo: first instance is initially active. */
    active_control = control;
    if (keyboard_off) {
        rlv_set_keyboard_enabled(control, FALSE);
    }

    GT_RefreshWindow(win, NULL);
    demo_paint(control);
    demo_sync_scroller(win, scroller, control, &last_scroll_top);

    printf("Phase 5.5 custom control: keyboard nav + resize + scroll sync.\n");
    printf("Cursor Up/Down = prev/next; Shift+cursor = page; Ctrl+cursor = first/last.\n");
    printf("Return activates selection; Space toggles sole checkbox. Click control for focus.\n");
    printf("A = toggle activation policy (SELECT_ROW / KEEP_CURRENT).\n");
    printf("V = cycle current-row visual (FULL / MARKER / NONE).\n");
    printf("Row 3 (-- Category --) is non-selectable.\n");
    printf("Activation=%s  Visual=%s\n",
           demo_activation_policy_name(g_demo_activation_policy),
           demo_current_row_visual_name(g_demo_current_row_visual));
    if (keyboard_off) {
        printf("NOKEYBOARD: keyboard NAV_* disabled (mouse/scroller still work).\n");
    } else {
        printf("Keyboard enabled (default). Pass NOKEYBOARD to disable.\n");
    }
    if (run_exercise) {
        printf("CLI EXERCISE: running deterministic sequence after first paint.\n");
    }
#ifdef RLV_ENABLE_BENCHMARKS
    if (run_bench) {
        printf("CLI BENCH: benchmark armed. Move windows, then click Start Benchmark.\n");
    }
#endif
    fflush(stdout);

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
            g = 0;

#ifdef RLV_ENABLE_LOGGING
            demo_log_idcmp(imsg);
#endif

            if (demo_idcmp_is_gadget_class(class) && iaddr != 0) {
                g = (struct Gadget *)iaddr;
            }

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
                    /*
                     * Click-to-focus: pointer inside this control's outer
                     * box transfers active_control (even on gap / heading).
                     * Outside all controls: preserve active. Dual-control
                     * demos would test each instance here.
                     */
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
                    /*
                     * Verified-up commit / cancel. Deliver SELECT_UP to the
                     * active control even when the pointer left the box
                     * (release-outside cancels arm).
                     */
                    if (active_control != 0) {
                        memset(&inev, 0, sizeof(inev));
                        inev.type = (UWORD)RLV_INPUT_SELECT_UP;
                        inev.x = mx;
                        inev.y = my;
                        RLV_LOGF("SELECT_UP mouse=%d,%d", (int)mx, (int)my);
                        demo_apply_input(active_control, win, scroller, &inev,
                                         &last_scroll_top);
                    }
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
                if (g->GadgetID == GID_GO) {
                    demo_bounds_from_geom(&g_demo_geom, &bounds);
                    if (demo_recreate_control(
                            &control, backend, win->RPort->Font,
                            &pens, columns, &bounds, keyboard_off)) {
                        active_control = control;
                        demo_paint(control);
                        demo_sync_scroller(win, scroller, control,
                                           &last_scroll_top);
                    } else {
                        RLV_LOG("FAIL Go control recreation");
                    }
                } else if (g->GadgetID == GID_DIVIDER_STYLE) {
                    /*
                     * GadTools reports the new CYCLE_KIND selection in
                     * IntuiMessage.Code. Querying GTCY_Active here can return
                     * the previous value.
                     */
                    if (code <= (UWORD)RLV_ROW_DIVIDER_DOTTED) {
                        g_demo_divider_style = code;
                    }
                } else if (g->GadgetID == GID_PADDING_X) {
                    if (code <= 4) {
                        g_demo_padding_x = code;
                    }
                } else if (g->GadgetID == GID_PADDING_Y) {
                    if (code <= 4) {
                        g_demo_padding_y = code;
                    }
                } else if (g->GadgetID == GID_ROW_GAP) {
                    if (code <= 4) {
                        g_demo_row_gap = code;
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
            } else if ((class == IDCMP_GADGETDOWN || class == IDCMP_MOUSEMOVE)
                       && g != 0 && g->GadgetID == GID_SCROLL) {
                active_control = control;
                demo_handle_scroller(control, win, scroller,
                                     &last_scroll_top, class, code);
            } else if (class == IDCMP_NEWSIZE) {
                if (!g_demo_gadgets_detached && glist != 0) {
                    RemoveGList(win, glist, -1);
                    g_demo_gadgets_detached = TRUE;
                }
                demo_handle_newsize(win, &glist, &scroller,
                                    &go_gad, &divider_cycle_gad,
                                    &padding_x_gad, &padding_y_gad,
                                    &row_gap_gad, &bench_gad,
                                    vi, screen->Font, dri,
                                    control, font_w, font_h,
                                    &last_scroll_top);
                /* Refresh cached focus hit box after resize. */
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
