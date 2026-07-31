# CLV Custom-Drawn ListView Control

**Branch:** `experiment/clv-custom-control`  
**Status:** Phase 5.5 complete — keyboard navigation (`NAV_*`), optional
enable/disable (default on), Return activation, app-owned focus pointer,
and deterministic `EXERCISE` CLI.
**Phase 6 next** (optimisation / size comparison / V2 recommendation only).  
**Document role:** Living design, implementation roadmap, and phase handoff record  
**Primary target:** Classic AmigaOS / Workbench 2.x and 3.x  
**Future target:** Workbench 1.3 through a separate low-level backend  
**Phase 1 audit:** [CLV_CUSTOM_CONTROL_PHASE1_AUDIT.md](CLV_CUSTOM_CONTROL_PHASE1_AUDIT.md)  
**Keyboard nav plan:** [CLV_CUSTOM_CONTROL_KEYBOARD_NAVIGATION_PLAN.md](CLV_CUSTOM_CONTROL_KEYBOARD_NAVIGATION_PLAN.md) (implemented in Phase 5.5)  

> **Interactive checkbox cells (experimental):** **Complete** on this package
> through master-plan phases C0–C10
> (`CLV_CTRL_COL_TYPE_CHECKBOX`, `CLV_EVENT_CELL_TOGGLED`, app-owned store).
> Design authority and phase status live in
> [CLV_INTERACTIVE_CONTROL_CELLS_MASTER_PLAN.md](CLV_INTERACTIVE_CONTROL_CELLS_MASTER_PLAN.md)
> (§A–§G). Integrator demo: `examples/custom_control_demo/`.
> GadTools `clv_cellctl_*` is **legacy / non-authoritative** — do not treat
> it as the product path for new checkbox work.

---

## 1. Purpose

This document defines the architecture and staged implementation plan for a new custom-drawn ListView control.

The new control is intended to replace the current dependence on the GadTools `LISTVIEW_KIND` for drawn list content while preserving the useful, reusable parts of the existing CLV codebase.

The immediate motivation is to solve two structural limitations of the native GadTools ListView:

1. A word-wrapped logical row is currently represented by several physical ListView rows, allowing each wrapped line to be selected separately.
2. A column title row placed inside the ListView scrolls with the content and cannot remain permanently visible.

The new control will therefore use a manually drawn viewport with a fixed header and variable-height logical rows.

---

## 2. Core Design Goal

The central design rule is:

> A logical row may occupy any number of rendered text lines, but selection, spacing, hit-testing, decoration, and ownership apply to the logical row as one item.

The control must present:

- a static, three-dimensional column header;
- a manually rendered scrolling viewport;
- variable-height logical rows;
- full-row highlighting across every wrapped line;
- configurable spacing between logical rows;
- vertical column separators rendered with light and dark pens;
- scrolling in approximately one text-line increments;
- support for the font active on the target screen or supplied by the application;
- a backend boundary that leaves room for a later Workbench 1.3 implementation.

---

## 3. Scope

### 3.1 Required for the first complete experimental version

- Fixed, non-scrolling header.
- Compact fixed header using the screen's semantic pens.
- Column titles aligned with content columns.
- Dark cell boxes with shine-pen highlights on their top and left edges.
- Variable-height logical rows.
- Pixel-measured word wrapping.
- Single logical-row selection.
- Whole-row highlighting regardless of wrapped height.
- Configurable inter-row gap.
- Vertical scrolling.
- Scroll steps of one current-font text line where practical.
- Proportional scrollbar integration.
- Mouse click hit-testing.
- Refresh and resize support.
- Use of current or explicitly supplied screen font.
- Separate example executable.
- Existing CLV v1 backend remains intact during development.

### 3.2 Deferred

- Workbench 1.3 backend implementation.
- Column sorting.
- Clickable/pressed header buttons.
- Row separator styles such as dotted or solid lines.
- Horizontal scrolling.
- Multi-selection.
- Inline editing.
- Drag-and-drop rows.
- Column resizing or reordering.
- Tree-view behaviour.
- Formal BOOPSI custom gadget class.
- Animated smooth scrolling.

### 3.3 Explicit non-goals

The first implementation must not attempt to recreate every feature of MUI NList, ReAction ListBrowser, or a modern desktop table control.

The project should remain small, Amiga-oriented, and suitable for 68000-class systems.

---

## 4. Design Principles

### 4.1 Logical rows are authoritative

A row in the application data model remains one row even when one or more columns wrap over several display lines.

No physical wrapped fragment may become independently selectable.

### 4.2 The header is not content

The title/header region is a separate fixed rectangle above the scrolling viewport. It must never be inserted into the row list.

### 4.3 Layout is measured in pixels

Variable-height rows and arbitrary row gaps require pixel-based layout and scroll positions.

The control may expose line-based scrolling operations, but the stored scroll position should be a pixel offset.

### 4.4 Drawing and input remain separable

The core control logic must not be tightly coupled to GadTools ListView behaviour, `GTLV_*` tags, draw callbacks, or physical ListView row selection.

### 4.5 GadTools may be used as a supporting component

For Workbench 2.x/3.x, a GadTools scroller may be used initially. The new viewport itself must remain manually drawn.

### 4.6 Workbench 1.3 compatibility is prepared architecturally, not implemented prematurely

The core must avoid hard dependencies on GadTools and `DrawInfo`, but no Workbench 1.3 backend is required in the first implementation.

### 4.7 Size remains measurable

Every phase that adds runtime functionality must record executable size and, where practical, object size compared with the current drawn ListView demo.

---

## 5. Proposed Repository Layout

**Phase 1 finalized** for Phase 2 creation (unchanged paths):

```text
src/
├── custom_listview/
│   └── existing CLV v1 implementation
│
└── custom_listview_control/
    ├── clv_control.h
    ├── clv_control.c
    ├── clv_control_layout.c
    ├── clv_control_wrap.c
    ├── clv_control_render.c
    ├── clv_control_input.c
    ├── clv_control_scroll.c
    ├── clv_control_draw.h
    ├── clv_control_platform.h
    ├── clv_control_internal.h
    │
    └── backends/
        ├── clv_backend_amiga_v36.h
        └── clv_backend_amiga_v36.c

examples/
└── custom_control_demo/
    ├── main.c
    ├── Makefile
    └── README.md
```

A later Workbench 1.3 backend may add:

```text
src/custom_listview_control/backends/
├── clv_backend_amiga_v34.h
└── clv_backend_amiga_v34.c
```

File count and split may be adjusted after implementation evidence, but responsibilities must remain separated.

---

## 6. High-Level Architecture

```text
Application rows
      │
      ▼
Column and row model
      │
      ▼
Text measurement and wrapping
      │
      ▼
Variable-height row layout
      │
      ├── hit testing
      ├── make-visible logic
      └── total content height
      │
      ▼
Control state
      │
      ├── selected logical row
      ├── scroll_y
      ├── header rectangle
      ├── viewport rectangle
      └── semantic pens and font metrics
      │
      ▼
Manual renderer
      │
      ▼
Amiga backend
      ├── RastPort access
      ├── clipping
      ├── IDCMP translation
      └── proportional scrollbar
```

---

## 7. Core Data Model

**Phase 1 finalized.** Canonical API and ownership live in
[CLV_CUSTOM_CONTROL_PHASE1_AUDIT.md](CLV_CUSTOM_CONTROL_PHASE1_AUDIT.md).
The control does **not** reuse `CLV_PreparedList` / `CLV_RenderNode` /
physical display maps. It may include `clv_types.h` for `CLV_PixelColumn`
and `CLV_CellAlign`.

### 7.1 Control state

```c
struct CLV_Control
{
    struct Rectangle bounds;
    struct Rectangle header_bounds;
    struct Rectangle viewport_bounds;

    const struct CLV_ControlColumn *columns; /* borrowed */
    UWORD column_count;

    const struct CLV_ControlRow *rows; /* borrowed */
    ULONG row_count;

    struct CLV_RowLayout *layout_rows; /* owned */
    LONG content_height;
    LONG scroll_y;
    LONG selected_row;

    UWORD cell_padding_x;
    UWORD cell_padding_y;
    UWORD row_gap;
    UWORD row_divider_style; /* CLV_ControlRowDividerStyle */
    UWORD line_height;
    UWORD header_height;

    struct CLV_Pens pens;
    struct CLV_FontMetrics font_metrics;

    const struct CLV_DrawOps *draw_ops;
    APTR draw_context;
};
```

### 7.2 Logical row layout

Each logical row should have cached geometry:

```c
struct CLV_RowLayout
{
    ULONG logical_index;
    LONG top_y;
    UWORD content_height;
    UWORD total_height;
    UWORD maximum_line_count;
    UWORD flags;
};
```

Where:

```text
total_height = content_height + row_gap
content_height = (maximum_line_count × line_height) + (2 × cell_padding_y)
header_height = line_height + (2 × cell_padding_y)
```

`cell_padding_x` is subtracted from both sides of each title/body text
rectangle before text fitting, alignment, and wrapping. `cell_padding_y` is
shared by title and body cells so the header and logical rows retain matching
vertical text insets.

Additional per-cell wrapping data may be stored separately or referenced through an existing prepared-row abstraction.

### 7.3 Columns

Each column requires at minimum:

- title;
- pixel width;
- alignment;
- wrapping policy;
- cell text accessor or prepared cell data;
- optional icon/style metadata;
- header and content rectangles.

Cell-edge width must be included in layout calculations and must never overlap
cell text.

---

## 8. Rendering Design

### 8.1 Header

The header is drawn independently of the viewport.

Default appearance:

- flat face using the semantic background pen;
- text using the configured text pen;
- horizontal and vertical text insets from `cell_padding_x` and
  `cell_padding_y`;
- dark box drawn first around every title cell;
- shine-pen top and left edges drawn over the dark box;
- divider positions identical to content divider positions.

Drawing the shine edge last makes title highlights continuous across the top
and leaves each dark right edge beginning one pixel below the cell top.

### 8.2 Column dividers

Adjacent cells use a dark right edge followed by the next cell's shine left
edge. Header title boxes retain their top/bottom treatment; body cells retain
only the vertical dark-right/shine-left edges. Body column edges continue
through row gaps and unused viewport space to the bottom of the control, but
remain inside the one-pixel outer outline.

#### 8.2.1 Shared cell padding

`CLV_ControlConfig.cell_padding_x` and
`CLV_ControlConfig.cell_padding_y` are public pixel values shared by title
and body cells:

- `cell_padding_x` controls the left/right text inset and therefore affects
  text fitting and wrapping width;
- `cell_padding_y` controls the top/bottom text inset and therefore affects
  header height and every logical-row content height;
- zero is a valid compact setting;
- changing either value requires layout reconstruction and a full repaint.

`clv_control_set_cell_padding()` changes both live values and invalidates
layout. `clv_control_get_cell_padding_x()` and
`clv_control_get_cell_padding_y()` return the current values.

### 8.3 Logical rows

Each visible logical row is rendered as one rectangle, regardless of wrap count.

Rendering sequence:

1. Determine visible clipped portion of the row.
2. Fill row background.
3. If selected, fill the entire logical row content rectangle with selected background.
4. Draw body-cell vertical edges.
5. Draw every wrapped cell fragment using selected or normal text pens.
6. Draw the configured horizontal row divider when this is not the final row.
7. Clear the configured row-gap area.

### 8.4 Row gap

`CLV_ControlConfig.row_gap` is the number of pixels between logical rows.

It is applied once after the complete logical row, not after each wrapped line.

Initial default behaviour:

- gap is rendered using normal background;
- selection highlight excludes the gap;
- hit-testing within the gap should resolve consistently, preferably to no row unless usability testing shows that assigning it to the preceding row is better.

The gap region must be kept explicit so later separator styles can use it.
`clv_control_set_row_gap()` changes the live value and invalidates layout;
`clv_control_get_row_gap()` returns the current value. A full render rebuilds
the invalidated row geometry.

### 8.5 Data-row divider styles

The public style values are:

```c
typedef enum CLV_ControlRowDividerStyle
{
    CLV_CTRL_ROW_DIVIDER_NONE = 0,
    CLV_CTRL_ROW_DIVIDER_SOLID,
    CLV_CTRL_ROW_DIVIDER_DOTTED
} CLV_ControlRowDividerStyle;
```

`CLV_ControlConfig.row_divider_style` selects `NONE`, `SOLID`, or `DOTTED`.
The divider is drawn at the bottom of each logical data row except the final
row. `DOTTED` is a one-on/one-off semantic separator-pen line. Header/title
rendering is independent and never changes with this setting. Divider
endpoints are inset one pixel from the left shine edge and right dark edge so
the horizontal line does not overwrite either body border.

`clv_control_set_row_divider_style()` changes the live style without relayout;
the caller repaints afterward. `clv_control_get_row_divider_style()` returns
the current normalized style.

---

## 9. Font Design

The control must not assume Topaz 8 or a monospaced font.

The Workbench 2.x/3.x backend should use either:

- an explicitly supplied `struct TextFont *`; or
- the font already active in the target window/screen RastPort.

All layout must derive from the actual font:

- `tf_YSize` or equivalent line height;
- `tf_Baseline`;
- `TextLength`, `TextExtent`, or `TextFit` for pixel widths;
- per-string measurement for proportional fonts.

Ownership must be explicit:

- borrowed fonts are never closed by the control;
- fonts opened by the control must be closed by the control;
- the first implementation should prefer borrowing an existing active font.

The rendering core should receive neutral font metrics and text-measurement operations rather than opening fonts itself.

---

## 10. Scrolling Design

### 10.1 Stored position

The authoritative scroll position is:

```c
LONG scroll_y;
```

This is a pixel offset into the variable-height content.

### 10.2 Scroll increments

Default operations:

- line up/down: current font line height;
- page up/down: viewport height minus one line;
- scrollbar drag: proportional pixel position;
- make selected visible: minimal pixel adjustment;
- optional future row-boundary alignment.

This gives text-line scrolling without requiring animated smooth scrolling.

### 10.3 Scrollbar

The Workbench 2.x/3.x backend may initially use `SCROLLER_KIND` or a lower-level proportional gadget.

The core must not directly call GadTools APIs. The backend translates between scroller values and `scroll_y`.

### 10.4 Rendering optimisation

The first correct implementation may redraw the full viewport on scroll.

`ScrollRasterBF()` or partial exposed-region redraw should be treated as a later optimisation only after correctness and refresh behaviour are proven.

---

## 11. Input and Selection Design

### 11.1 Neutral input events

Raw `IntuiMessage` handling should remain in the backend.

The core should receive translated events such as:

```c
enum CLV_InputType
{
    CLV_INPUT_SELECT_DOWN,
    CLV_INPUT_SELECT_UP,
    CLV_INPUT_POINTER_MOVE,
    CLV_INPUT_SCROLL_LINE_UP,
    CLV_INPUT_SCROLL_LINE_DOWN,
    CLV_INPUT_SCROLL_PAGE_UP,
    CLV_INPUT_SCROLL_PAGE_DOWN,
    CLV_INPUT_SCROLL_POSITION
};
```

### 11.2 Hit testing

Mouse Y is converted to content Y:

```text
content_y = scroll_y + mouse_y - viewport_top
```

The row layout cache resolves that pixel to one logical row.

Clicks anywhere inside a wrapped row select the same logical row.

### 11.3 Selection redraw

When selection changes, the implementation may initially redraw the full viewport.

A later optimisation may redraw only the old and new logical-row rectangles.

### 11.4 Non-selectable rows

The architecture should retain support for non-selectable logical rows, even though the title is no longer represented as a row.

This remains useful for category headings, separators, and informational rows.

---

## 12. Backend Boundary and Future Workbench 1.3 Support

The new control must leave a clean boundary for a later Workbench 1.3 backend.

### 12.1 Core must not depend on

- GadTools `LISTVIEW_KIND`;
- `GTLV_*` tags;
- `GTLV_CallBack`;
- GadTools physical-row selection;
- `DrawInfo` being available inside the renderer;
- raw `IntuiMessage` structures inside layout/render code;
- GadTools scroller structures inside control state.

### 12.2 Drawing operations

The renderer should use a small operations table or equivalent boundary:

```c
struct CLV_DrawOps
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
};
```

**Phase 1 finalized.** Drawing responsibilities remain isolated in the v36
backend. Minor ABI tweaks during Phase 2 are allowed if documented in the
Phase 2 completion record.

### 12.3 Semantic pens

The renderer should consume semantic roles:

```c
struct CLV_Pens
{
    UWORD text;
    UWORD background;
    UWORD selected_text;
    UWORD selected_background;
    UWORD shine;
    UWORD shadow;
    UWORD separator;
};
```

The Workbench 2.x/3.x backend may derive these from `DrawInfo`. A future Workbench 1.3 backend may obtain them from application configuration or screen conventions.

### 12.4 Future backend responsibilities

A Workbench 1.3 backend would later provide:

- RastPort drawing operations;
- font metrics and text measurement;
- clipping appropriate to the available OS APIs;
- basic Intuition proportional gadget handling or manual scrollbar handling;
- raw IDCMP translation;
- semantic pen selection.

No Workbench 1.3 code is required in the current branch unless a later phase explicitly adds it.

---

## 13. Public API Direction

**Phase 1 finalized.** Experimental control-specific names only; not the final
CLV v2 API. Full signatures, ownership comments, and private header list are in
[CLV_CUSTOM_CONTROL_PHASE1_AUDIT.md §5](CLV_CUSTOM_CONTROL_PHASE1_AUDIT.md).

```c
typedef struct CLV_ControlConfig
{
    const struct CLV_DrawOps *draw_ops; /* required */
    APTR draw_context;                  /* opaque; owned by backend */
    struct TextFont *font;              /* borrowed; NULL = ops default */
    UWORD cell_padding_x;               /* title/body horizontal inset */
    UWORD cell_padding_y;               /* title/body vertical inset */
    UWORD row_gap;                      /* pixels between logical rows */
    UWORD row_divider_style;            /* CLV_ControlRowDividerStyle */
    UWORD flags;                        /* CLV_CTRL_CFG_* */
} CLV_ControlConfig;

CLV_Control *clv_control_create(const CLV_ControlConfig *cfg);
VOID         clv_control_destroy(CLV_Control *control);

BOOL clv_control_set_columns(CLV_Control *c,
                             const CLV_ControlColumn *cols, UWORD count);
BOOL clv_control_set_rows(CLV_Control *c,
                          const CLV_ControlRow *rows, ULONG count);
VOID clv_control_set_cell_padding(CLV_Control *c, UWORD x, UWORD y);
UWORD clv_control_get_cell_padding_x(const CLV_Control *c);
UWORD clv_control_get_cell_padding_y(const CLV_Control *c);
VOID clv_control_set_row_gap(CLV_Control *c, UWORD pixels);
UWORD clv_control_get_row_gap(const CLV_Control *c);
VOID clv_control_set_row_divider_style(CLV_Control *c, UWORD style);
UWORD clv_control_get_row_divider_style(const CLV_Control *c);
VOID clv_control_set_bounds(CLV_Control *c, const struct Rectangle *bounds);
VOID clv_control_set_pens(CLV_Control *c, const struct CLV_Pens *pens);
VOID clv_control_set_selected(CLV_Control *c, LONG logical_row);
VOID clv_control_make_visible(CLV_Control *c, LONG logical_row);

VOID clv_control_render(CLV_Control *c, ULONG flags);
BOOL clv_control_handle_input(CLV_Control *c,
                              const struct CLV_InputEvent *event,
                              struct CLV_Event *result);

LONG clv_control_get_scroll_y(const CLV_Control *c);
VOID clv_control_set_scroll_y(CLV_Control *c, LONG scroll_y);
LONG clv_control_get_content_height(const CLV_Control *c);
```

Ownership defaults: borrowed columns/rows/strings/font; control-owned layout
and wrap caches; backend-owned draw context and scroller. Do not include
`clv_renderer.h` or `clv_selection.h` from control core.

The demo initializes `cell_padding_x = 1`, `cell_padding_y = 1`,
`row_gap = 0`, and `row_divider_style = CLV_CTRL_ROW_DIVIDER_SOLID`.
Its GadTools cycles hold pending values (`0`–`4` pixels for padding/gap and
the three divider styles); pressing **Go** recreates the control with all
four selected settings.

---

## 14. Compatibility and Build Strategy

The experiment must coexist with the existing implementation.

Required rules:

- Existing CLV v1 source remains buildable.
- Existing examples remain buildable unless a phase explicitly documents a necessary shared change.
- The new control has a separate example target.
- No existing public header may be silently repurposed.
- Shared code may be reused only after identifying and documenting whether it is genuinely backend-neutral.
- Any movement of existing code must be deferred until the prototype proves the new architecture.

---

## 15. Testing Strategy

### 15.1 Functional test cases

The example must include at least:

- rows with one line;
- rows with two, three, and four wrapped lines;
- columns with different alignments;
- rows with icons where supported;
- non-selectable logical rows;
- first and last row selection;
- row gap values of 0, 1, 2, and a larger visible value;
- narrow and wide window sizes;
- fonts taller than eight pixels where available;
- proportional font testing where practical;
- scrolling to partially clipped rows;
- resizing while scrolled;
- empty list;
- list shorter than viewport;
- list much taller than viewport.

### 15.2 Platform test targets

At minimum:

- Workbench 3.x or equivalent emulator environment;
- Workbench 2.04 or equivalent emulator environment;
- 68000-compatible build profile.

### 15.3 Visual acceptance criteria

- Header does not move while content scrolls.
- Selected wrapped row appears as one coherent highlighted block.
- Header and content column boundaries remain aligned.
- Column dividers do not overwrite text.
- No drawing enters the scrollbar region.
- Row gap occurs only between logical rows.
- Resize does not leave stale pixels.
- Font change does not assume fixed-width characters.

### 15.4 Size and complexity tracking

Each major phase should record:

- executable size of the new demo;
- executable size of the comparable existing drawn demo;
- new control object sizes where available;
- source file count;
- significant allocations per control;
- any existing modules that could eventually be removed in a V2-only build.

---

## 16. Implementation Phases

The implementation is intentionally divided into several independently planned phases.

Each phase must begin with investigation and a written implementation plan before code changes are made.

Each phase must update this document before completion.

---

# Phase Status Summary

| Phase | Name | Status | Completion commit | Notes |
|------:|------|--------|-------------------|-------|
| 1 | Architecture and reusable-code audit | **Complete** | pending commit | See [CLV_CUSTOM_CONTROL_PHASE1_AUDIT.md](CLV_CUSTOM_CONTROL_PHASE1_AUDIT.md) |
| 2 | Static header and viewport rendering skeleton | **Complete** | pending commit | Fixed header + fixed-height rows |
| 3 | Variable-height wrapping and logical-row layout | **Complete** | pending commit | Pixel wrap + variable `CLV_RowLayout` |
| 4 | Selection, hit-testing, and line-step scrolling | **Complete** | pending commit | Logical-row select + pixel scroll sync |
| 5 | Backend hardening, resize/refresh, WB2.x validation | **Complete** | `629ddf3` | Ownership/failure/backend/multi-instance audited; WB2.x+WB3.2 matrices consolidated; 68000 build + final sizes recorded |
| 5.5 | Keyboard navigation and deterministic exercise API | **Complete** | pending commit | `NAV_*` + enable/disable (default on) + `ACTIVATED` + `get_selected`; demo RAWKEY/focus/`EXERCISE`/`NOKEYBOARD`; sizes recorded |
| 6 | Optimisation, size comparison, and V2 recommendation | **NEXT** | — | After 5.5; compare architecture; recommend V2 direction (do not migrate yet) |

**Current phase:** Phase 6 — Optimisation, size comparison, and V2 recommendation
(Phase 5.5 complete; keyboard + EXERCISE available for profiling)

---

## Phase 1 — Architecture and Reusable-Code Audit

### Objective

Investigate the current CLV implementation and produce the exact implementation architecture for the new control without breaking or modifying the existing backend unnecessarily.

### Status

**Complete** (2026-07-23). Full findings:
[CLV_CUSTOM_CONTROL_PHASE1_AUDIT.md](CLV_CUSTOM_CONTROL_PHASE1_AUDIT.md).

### Findings summary

1. **Reuse without GadTools ListView semantics:** `clv_types.h` (`CLV_PixelColumn`, `CLV_CellAlign`), `clv_platform` alloc, `clv_compiler.h` / `clv_sdk_compat.h`, optional `clv_log.h`.
2. **Avoid as primary model:** `CLV_PreparedList`, `CLV_RenderNode`, display maps, `clv_selection.*`, `LV_DRAW` / `CLV_Renderer` hook, ASCII `GTLV_Labels` formatters.
3. **New thin renderer required:** current draw path is bound to fixed-height physical items and `LVDrawMsg`; do not invoke it against arbitrary rectangles.
4. **Wrapping:** control-owned layout (`CLV_RowLayout`) in Phase 3; copy/adapt TextFit algorithm from `clv_pixel_wrap.c` without moving v1 code until Phase 6.
5. **API:** experimental `clv_control_*` surface finalized in the audit (§5); borrowed rows/columns/font; control-owned layout caches; backend draw ops + pens.
6. **Source tree:** `src/custom_listview_control/` + `examples/custom_control_demo/` as in §5 of this document (confirmed).
7. **Size baseline:** `bin/size-draw-basic` (27928 bytes per `CLV_SIZE_REPORT.md`); visual reference `examples/05_draw_basic/`.
8. **WB2.x:** Graphics text APIs + DrawInfo pens in backend; GadTools scroller only (not ListView); no BOOPSI gadget class.
9. **WB1.3:** preserve `CLV_DrawOps` / `CLV_Pens` / neutral input; defer v34 backend code.
10. **Deferred:** wrap, selection, scroll interaction, icons/styles, sorting, v1 code moves — per phase plan and audit §9.

### Exit criteria

- [x] No implementation ambiguity remains around core state, ownership, backend boundary, and initial API.
- [x] Existing CLV public structures are confirmed as either reusable or unsuitable.
- [x] Phase 2 has a precise implementation target.
- [x] This document marks Phase 1 complete and Phase 2 next.

### Phase 1 Completion Record

**Status:** Complete  
**Completion commit:** pending commit  
**Date:** `2026-07-23`

#### Implemented

- Repository-grounded architecture audit answering all ten Phase 1 questions.
- Reuse matrix, ownership/lifetime table, finalized experimental API, source tree, and build/size comparison design.
- Living design document updated (§7, §12.2, §13, status table, handoff).

#### Files added or changed

- `docs/CLV_CUSTOM_CONTROL_PHASE1_AUDIT.md` — full Phase 1 audit report
- `docs/CLV_CUSTOM_CONTROL_DESIGN_AND_IMPLEMENTATION_PLAN.md` — status, findings, finalized API/state, Phase 2 handoff

#### Design decisions

- Decision: New thin control renderer; do not call v1 `LV_DRAW` prepare/draw for the experiment.
- Reason: Renderer is coupled to GadTools physical rows and fixed item height.
- Consequence: Phase 2 implements `CLV_DrawOps` + control render from scratch; optional shared extraction only after Phase 6 evidence.

- Decision: Do not move or rewrite `clv_pixel_wrap.c` in early phases.
- Reason: Algorithm is reusable but the TU depends on renderer internals/`CLV_LvTempFrag`.
- Consequence: Phase 3 copies/adapts wrap into control modules; v1 remains intact.

- Decision: Detailed matrices live in a separate audit doc; this file remains the roadmap/handoff.
- Reason: Keeps the living plan readable while preserving full audit evidence.
- Consequence: Phase 2+ agents must read both documents.

#### Tests performed

- Command: N/A (documentation-only phase; no compile probe required)
- Environment: Repository inspection on development host
- Result: Architecture locked for Phase 2

#### Size measurements

| Target | Before | After | Delta |
|--------|-------:|------:|------:|
| N/A (no binary) | — | — | — |

Baseline for later phases: `size-draw-basic` = 27928 bytes (`docs/CLV_SIZE_REPORT.md`, 2026-07-22).

#### Known limitations

- Clip-ops ABI may need a small Phase 2 adjustment after verifying Amiga clipping APIs.
- `CLV_LineStyle` should not be obtained by including `clv_renderer.h`; use a control-local equivalent.

#### Deviations from this design

- None material. Draw ops gained `text_fit`, `line_height`, and `baseline` entries for measurement without `DrawInfo` in core.

#### Instructions for Phase 2

- Create `src/custom_listview_control/` tree and `examples/custom_control_demo/` as specified in the Phase 1 audit §6.
- Implement v36 backend draw ops + `CLV_Pens` from DrawInfo; control create/destroy; bounds → header + viewport; 3D header cells; two-pen dividers; fixed-height sample rows; full redraw.
- Link `clv_platform.o` + control objects only — do not link `clv_renderer_*.o` / `clv_selection.o` / wrap / ASCII formatters.
- Follow `templates/AI_AGENT_GETTING_STARTED.md` for window lifecycle.
- Record demo size vs `size-draw-basic`.
- Do **not** implement wrapping, selection, or scrollbar interaction (Phases 3–4).
- Update this document’s Phase 2 section and status table when done.

---

## Phase 2 — Static Header and Viewport Rendering Skeleton

### Objective

Create the smallest manually drawn control that proves the fixed-header and scrolling-viewport geometry without implementing full variable-height wrapped rows.

### Required functionality

- Experimental control create/destroy lifecycle.
- Separate fixed header and content viewport rectangles.
- Workbench-style 3D header cells.
- Two-pen vertical column dividers.
- Font and semantic pen setup.
- Rendering through the backend/draw-ops boundary.
- Simple fixed-height sample rows.
- Full viewport redraw.
- Separate demo executable.
- Existing CLV v1 remains unaffected.

### Deliberately deferred

- Word wrapping.
- Variable row heights.
- Selection.
- Scrolling interaction beyond any minimum needed to prove geometry.
- Partial redraw optimisation.

### Required investigation before implementation

The Phase 2 agent must read
[CLV_CUSTOM_CONTROL_PHASE1_AUDIT.md](CLV_CUSTOM_CONTROL_PHASE1_AUDIT.md)
and verify the Phase 1 architecture against actual Amiga rendering and clipping
APIs, then write its own implementation plan before coding.

### Deliverables

- New control skeleton.
- WB2.x/3.x backend implementation sufficient for drawing.
- Static header demo.
- Build target and size record.
- Screenshots or raster dumps if the repository supports them.
- Updated Phase 2 section and status table.

### Exit criteria

- Header remains visually separate from content.
- Header and content column boundaries align exactly.
- No drawing enters the scrollbar/reserved region.
- Font metrics are not hard-coded.
- Phase 3 can add wrapping without replacing the control skeleton.

### Phase 2 Completion Record

**Status:** Complete  
**Completion commit:** pending commit  
**Date:** `2026-07-23`

#### Implemented

- `src/custom_listview_control/` tree with public API, draw ops, internal state, layout, render, and input/scroll stubs.
- V36 backend: `CLV_DrawOps` against window RastPort, `InstallClipRegion` push/pop, DrawInfo → `CLV_Pens`.
- Originally delivered with raised header cells and recessed two-pixel
  dividers; the current renderer uses highlighted title boxes, vertical
  dark/shine body edges, shared configurable title/body cell padding,
  configurable row gaps and horizontal data-row dividers, and a one-pixel
  dark outer outline.
- Non-interactive `SCROLLER_KIND` in the demo so content bounds exclude the scrollbar.
- Root Makefile target `custom-control-demo` → `bin/custom-control-demo`.
- Existing CLV v1 examples untouched (`draw-basic` still builds).

#### Files added or changed

- `src/custom_listview_control/clv_control.h` — experimental public API
- `src/custom_listview_control/clv_control_draw.h` — `CLV_DrawOps`, `CLV_Pens`, line-style enum
- `src/custom_listview_control/clv_control_internal.h` — `CLV_Control`, `CLV_RowLayout`
- `src/custom_listview_control/clv_control_platform.h` — platform include shim
- `src/custom_listview_control/clv_control.c` — create/destroy/setters/render entry
- `src/custom_listview_control/clv_control_layout.c` — header/viewport/columns/fixed rows
- `src/custom_listview_control/clv_control_render.c` — full redraw
- `src/custom_listview_control/clv_control_input.c` — selection/input stubs
- `src/custom_listview_control/clv_control_scroll.c` — scroll accessors with clamp
- `src/custom_listview_control/backends/clv_backend_amiga_v36.h`
- `src/custom_listview_control/backends/clv_backend_amiga_v36.c`
- `examples/custom_control_demo/main.c`
- `examples/custom_control_demo/README.md`
- `Makefile` — `CLV_CUSTOM_CONTROL_*` objects and `custom-control-demo` target
- `docs/CLV_CUSTOM_CONTROL_DESIGN_AND_IMPLEMENTATION_PLAN.md` — this record

#### Design decisions

- Decision: Demo creates a non-interactive `SCROLLER_KIND` for geometry; core receives bounds that already exclude it.
- Reason: Audit forbids reusing `CLV_LISTVIEW_SCROLLBAR_BORDER` for content width; real gadget geometry is authoritative.
- Consequence: Phase 4 wires scroller ↔ `scroll_y` without changing layout origin rules.

- Decision: Control-local `CLV_ControlLineStyle` in `clv_control_draw.h`; do not include `clv_renderer.h`.
- Reason: Avoid pulling GadTools renderer types into the experiment.
- Consequence: Numeric values match v1; shared header deferred until Phase 6.

- Decision: Phase 2 truncates cell text via binary search on `text_width`; `text_fit` ops entry remains NULL until Phase 3.
- Reason: Sufficient for one-line rows without requiring TextFit yet.
- Consequence: Phase 3 should implement `text_fit` in the v36 backend and use it for wrap.

- Decision: Full API declared now; input/selection/scroll interaction stubbed.
- Reason: Stable surface for later phases; matches Phase 1 audit preference.
- Consequence: Phase 4 fills in behaviour without public header churn.

#### Tests performed

- Command: `make custom-control-demo` and `make draw-basic`
- Environment: Host cross-compile with VBCC `+aos68k -O2 -size -final` (Windows development host)
- Result: Both targets linked successfully. Runtime/visual validation on WB2.x/3.x **not** performed in this phase (deferred to Phase 5).

#### Size measurements

| Target | Before | After | Delta |
|--------|-------:|------:|------:|
| `bin/custom-control-demo` | — | 13580 | new |
| `bin/size-draw-basic` (current rebuild) | 27928 (doc 2026-07-22) | 27768 | −160 (unrelated v1 drift) |
| `bin/draw-basic` (windowed example) | — | 32900 | reference only |

Compiler/flags: VBCC `vc`, `CFLAGS=+aos68k -c99 -cpu=68000 -O2 -size`, `LDFLAGS=+aos68k -cpu=68000 -O2 -size -final -lamiga -lauto`.

Linked control modules: `clv_control*.o`, `clv_backend_amiga_v36.o`, `clv_platform.o`.

Object sizes (approx.): control 1792, layout 2024, render 3704, input 440, scroll 544, backend 2880.

#### Known limitations

- No word wrapping; every logical row is one font line tall.
- Selection is stored but not drawn; input handling returns FALSE.
- Scroller does not move `scroll_y` (Phase 4).
- Nested clip push not supported (single clip slot in v36 backend).
- `FILLTEXTPEN` used for selected_text role; unused in Phase 2 drawing.
- Visual acceptance (header alignment, divider pens, refresh) needs emulator confirmation.

#### Deviations from this design

- None material. Clip ops follow the v1 `InstallClipRegion` pattern as planned.
- Screenshots/raster dumps not added (repository has no established screenshot pipeline for this experiment).

#### Instructions for Phase 3

- Keep the Phase 2 skeleton; extend layout/render only.
- Implement pixel wrap by **copying/adapting** the algorithm from `clv_pixel_wrap.c` / `clv_char_wrap_cell_pixel()` into control-owned prepare code. Do **not** move or rewrite v1 wrap modules; do **not** link `clv_pixel_wrap.o`.
- Implement v36 `text_fit` ops entry (TextFit) and use it from wrap prepare.
- Populate `CLV_RowLayout` with `maximum_line_count`, variable `content_height`, and `total_height = content_height + row_gap`.
- Store per-cell wrap fragments owned by the control; draw-time must not allocate.
- Relayout on bounds/font/columns/rows/row_gap change; clip partially visible rows.
- Extend `examples/custom_control_demo` with multi-line wrap samples and row-gap demonstration.
- Still no selection, hit-testing, or scroller sync (Phase 4).
- Record size delta vs Phase 2 demo and update this document’s Phase 3 section + handoff.


## Phase 3 — Variable-Height Wrapping and Logical-Row Layout

### Objective

Implement the defining data/layout behaviour: one logical row may contain multiple wrapped text lines and has one cached variable-height rectangle.

### Required functionality

- Pixel-measured wrapping for configured columns.
- Layout cache for logical rows.
- Maximum wrapped line count per row.
- Variable row heights.
- Configurable `row_gap` applied once per logical row.
- Correct clipping of partially visible rows.
- Rendering all wrapped fragments within one row rectangle.
- Empty-list and short-list behaviour.
- Relayout on width, font, column, data, or row-gap change.

### Deliberately deferred

- Selection interaction.
- Full scrollbar behaviour.
- Optimised relayout or partial redraw.
- Separator styles other than a clear gap.

### Required investigation before implementation

The Phase 3 agent must inspect wrapping guidance in
[CLV_CUSTOM_CONTROL_PHASE1_AUDIT.md](CLV_CUSTOM_CONTROL_PHASE1_AUDIT.md)
(Q4 / reuse matrix for `clv_pixel_wrap.c`) and decide whether to copy/adapt or
thin-wrap that algorithm. It must document the decision before coding. Do not
move v1 wrap modules until Phase 6 recommends it.

### Deliverables

- Variable-height layout implementation.
- Wrapping test data in the demo.
- Row-gap configuration demonstration.
- Layout invariants documented.
- Memory/allocation observations.
- Updated Phase 3 section and status table.

### Exit criteria

- A wrapped logical row renders as one geometric item.
- Row gap occurs only after the complete logical row.
- Header remains fixed.
- Resizing or changing column widths causes correct relayout.
- Phase 4 can add selection using row rectangles without physical-row maps.

### Phase 3 Completion Record

**Status:** Complete  
**Completion commit:** pending commit  
**Date:** `2026-07-23`

#### Implemented

- Control-local wrap modes (`CLV_CTRL_WRAP_*`) matching v1 numeric values.
- V36 backend `text_fit` via Graphics `TextFit`, wired into `CLV_DrawOps`.
- New `clv_control_wrap.c`: pixel wrap prepare copied/adapted from
  `clv_pixel_wrap.c` without icons, styles, or continuation guides; fragment
  text pointers borrow into application cell strings.
- `CLV_RowLayout` driven by max fragment count per logical row:
  `content_height = maximum_line_count × line_height`,
  `total_height = content_height + row_gap`.
- Control-owned row-major `cell_wraps` cache; draw-time uses cached fragments
  only (no heap allocation while rendering).
- Demo extended with multi-line wrap samples, mix of wrap/truncate columns,
  and visible `row_gap = 4`.

#### Layout invariants

- One logical row occupies one content rectangle of height
  `maximum_line_count × line_height`.
- `row_gap` is applied once after the content rectangle, never between wrapped
  lines; gap is background-only (selection will exclude it in Phase 4).
- Header remains a separate fixed rectangle above the viewport.
- Wrap fragment text borrows cell strings; caches are freed/rebuilt on
  columns/rows/bounds/row_gap change.
- Cap: `CLV_CTRL_MAX_FRAGS_PER_CELL` (32).

#### Files added or changed

- `src/custom_listview_control/clv_control.h` — `CLV_CTRL_WRAP_*`; Phase 3 comment
- `src/custom_listview_control/clv_control_internal.h` — frag/cell-wrap types,
  wrap cache on `CLV_Control`, wrap/free decls
- `src/custom_listview_control/clv_control_wrap.c` — **new** wrap prepare
- `src/custom_listview_control/clv_control_layout.c` — variable-height rows + wrap
- `src/custom_listview_control/clv_control_render.c` — multi-line fragment draw
- `src/custom_listview_control/clv_control.c` — free wrap caches on destroy
- `src/custom_listview_control/backends/clv_backend_amiga_v36.c` — `text_fit`
- `examples/custom_control_demo/main.c` — wrap samples, `row_gap = 4`
- `examples/custom_control_demo/README.md` — Phase 3 features/size
- `Makefile` — `clv_control_wrap.o` in `CLV_CUSTOM_CONTROL_OBJS`
- `docs/CLV_CUSTOM_CONTROL_DESIGN_AND_IMPLEMENTATION_PLAN.md` — this record

#### Design decisions

- Decision: Copy/adapt wrap into `clv_control_wrap.c`; do not link
  `clv_pixel_wrap.o` or include `clv_renderer.h`.
- Reason: Phase 1 Q4 / Phase 2 handoff; keep v1 intact until Phase 6.
- Consequence: Small algorithm duplication; control stays omit-at-link clean.

- Decision: Fragment text is pointer+length into borrowed cell strings (no
  per-fragment `strdup`).
- Reason: Matches ownership model; fewer allocations on classic Amiga.
- Consequence: Caller must keep row cell strings valid until next
  `set_rows` / destroy; relayout invalidates caches.

- Decision: No icons, styles, or continuation-guide indent in Phase 3 wrap.
- Reason: Deferred optional features; keep the first variable-height path thin.
- Consequence: Continuations start at `text_left` (left) or under first-line X
  (non-left); guides remain a later addition if needed.

- Decision: Cap wrap at 32 fragments per cell.
- Reason: Bound memory and worst-case narrow-column loops.
- Consequence: Extremely long strings in very narrow columns truncate at the
  fragment cap (remaining text not shown).

#### Tests performed

- Command: `make custom-control-demo` and `make draw-basic` /
  `make size-draw-basic`
- Environment: Host cross-compile with VBCC `+aos68k -O2 -size -final`
  (Windows development host)
- Result: All targets linked successfully. Runtime/visual validation on
  WB2.x/3.x **not** performed in this phase (deferred to Phase 5).

#### Size measurements

| Target | Before | After | Delta |
|--------|-------:|------:|------:|
| `bin/custom-control-demo` | 13580 (Phase 2) | 17296 | +3716 |
| `bin/size-draw-basic` | 27768 (Phase 2 rebuild) | 27852 | +84 (unrelated v1 drift) |
| `bin/draw-basic` | 32900 | 32900 | 0 |

Compiler/flags: VBCC `vc`, `CFLAGS=+aos68k -c99 -cpu=68000 -O2 -size`,
`LDFLAGS=+aos68k -cpu=68000 -O2 -size -final -lamiga -lauto`.

Linked control modules: `clv_control*.o` (incl. wrap), `clv_backend_amiga_v36.o`,
`clv_platform.o`.

Object sizes (approx.): control 1844, layout 2520, wrap 3832, render 4024,
input 440, scroll 544, backend 3100.

#### Known limitations

- Selection still stored but not drawn; input handling returns FALSE.
- Scroller does not move `scroll_y` (Phase 4).
- No continuation dotted guides, icons, or styles.
- Fragment cap may drop tail text on pathological narrow wraps.
- Visual acceptance (wrap quality, gap, header alignment) needs emulator
  confirmation in Phase 5.

#### Deviations from this design

- None material. Wrap is text-only as planned; PATH mode is implemented in
  break-finding but the demo uses WORD_OR_CHAR / NONE only.

#### Instructions for Phase 4

- Keep Phase 3 layout/wrap caches; do not change physical-row maps (there are
  none).
- Implement mouse hit-test: `content_y = scroll_y + mouse_y - viewport_top`
  → resolve via `CLV_RowLayout` rectangles; clicks anywhere in a wrapped row
  select that logical row.
- Draw whole-row selection highlight across `content_height` only (exclude
  `row_gap`).
- Honour non-selectable row flags; reject and restore prior selection.
- Wire `SCROLLER_KIND` ↔ `scroll_y` (line/page/proportional); implement
  `make_visible` with minimal pixel adjustment.
- Translate IDCMP in the demo/backend into `CLV_InputEvent`; fill
  `clv_control_handle_input` / `clv_control_set_selected`.
- Still no keyboard nav, drag-autoscroll, header buttons, or ScrollRaster
  unless trivial and documented.
- Record size delta vs Phase 3 demo and update this document’s Phase 4
  section + handoff.


## Phase 4 — Selection, Hit-Testing, and Line-Step Scrolling

### Session updates (2026-07-23) — Phase 5 hardening built on Phase 4

Phase 4 exit criteria were already met on WB3.x. The work below was done in
the following Phase 5 session to diagnose and fix WB2.x failures and related
visual regressions. It is recorded here (at the head of Phase 4) so later
agents can see how Phase 4 behaviour was preserved while refresh/scroll/clip
were hardened. Full diagnostic infrastructure detail remains under Phase 5.

#### Problems observed on WB2.x (intuition/gadtools 37)

| Symptom | Root cause (log-driven) | Fix |
|---------|-------------------------|-----|
| Row click appeared not to select | Often obscured by other bugs; selection path itself was instrumented and kept | Keep Phase 4 hit-test/selection; verify after scroller/refresh fixes |
| Scroller crash `80000004` / no scroll | Demo used `GT_GetGadgetAttrs` (V39+) to read `GTSC_Top`; on V37 Code already holds Top, call was unsafe | `demo_handle_scroller`: `top = (LONG)msg_code`; no `GT_GetGadgetAttrs` |
| Uncover left blank bands | `InstallClipRegion` during `BeginRefresh` / `LAYERUPDATING` fights damage ClipRects | Do **not** call `InstallClipRegion` while `LAYERUPDATING` |
| After scroll-under-CLI then uncover: text/selection drawn outside list box | Skipping all clip left damage pixels outside the viewport paintable | Soft (software) viewport clip on fill/line/text during `LAYERUPDATING` |
| Re-installing viewport `ClipRegion` on refresh (even without NULL-clear) | Restored blank uncover bands | Abandoned; soft clip only on refresh path |

#### Related visual / demo hardening

- **Outer frame:** one-pixel dark outline (`CLV_CTRL_FRAME_WIDTH = 1`);
  layout insets header/viewport/columns and drawing remains in
  `clv_control_render.c`.
- **Border flash on scroll:** full paint was rewriting the frame every scroll;
  added `CLV_RENDER_VIEWPORT_ONLY`; demo uses viewport-only paint for
  scroll/selection and full paint for startup/refresh.
- **Cell frames:** title boxes keep dark frames with shine top/left highlights;
  body cells keep vertical dark-right/shine-left edges and use a separately
  configured horizontal row divider.
- **Public appearance settings:** `cell_padding_x`, `cell_padding_y`,
  `row_gap`, and `row_divider_style` are create-time configuration fields.
  Padding and gap setters invalidate layout; divider-style changes require
  repaint only.
- **Demo appearance controls:** GadTools cycles hold pending values for all
  four settings. **Go** recreates the control while retaining selection,
  pixel scroll position, and keyboard-enable state.
- **Optional logger:** crash-safe `PROGDIR:clv_control.log` via
  `clv_control_log.*` and `make custom-control-demo-log` (normal
  `make custom-control-demo` stays logging-free). First infrastructure
  commit: `51e56ae`. Enabled the WB2.x diagnosis that produced the fixes
  above.

#### Clip policy (v36 backend) — why both paths exist

1. **Normal paint:** hardware `InstallClipRegion` to the viewport (plus soft
   clip as belt-and-suspenders).
2. **`LAYERUPDATING` (refresh):** no `InstallClipRegion`; soft clip only so
   damage ClipRects stay intact **and** scrolled glyphs cannot paint into
   damaged pixels outside the list viewport.
3. Soft text clip uses font ascent/descent (`tf_Baseline` / `tf_YSize`), not
   baseline alone; render also skips fragments whose baseline is outside the
   viewport.
4. **Soft-only text strictness (2026-07-24):** on the `LAYERUPDATING` path
   (`soft_only`), reject any glyph whose box extends above/below the soft
   rect. The looser “wholly outside” reject remains for hardware+soft so
   partially visible edge lines still draw under `InstallClipRegion`. See
   **Header text bleed (soft-only `Text()` overhang)** below.

#### Header text bleed (soft-only `Text()` overhang) — bug and fix

**Date:** `2026-07-24`  
**Severity:** Intermittent visual corruption (header), WB2.x first observed;
same path exists on WB3.x.  
**Status:** Fixed and validated on Workbench 2.x and Workbench 3.2.

##### Symptom

After covering the custom-control window with other windows (e.g. CLI),
scrolling the list while covered, then uncovering and scrolling again, wrapped
viewport text (and JAM2 selection back-pen) could appear inside the fixed
header — most obvious in the Description column (e.g. Gamma’s
“text that wraps…” overwriting the “Description” title). Ordinary scrolling
without cover/uncover rarely showed it. `CLV_RENDER_VIEWPORT_ONLY` updates
after the leak left the header unrepaired.

##### Root cause (log-driven)

- Clip push/pop stayed balanced; no `logical-fallback` / invariant clip
  failures in `PROGDIR:clv_control.log`.
- Uncover used `GT_BeginRefresh` → soft-only clip
  (`v36 push_clip result=1 soft (LAYERUPDATING+viewport)`).
- Soft refreshes while `scroll_y` left a wrapped row straddling the viewport
  top (e.g. `scroll_y=5`, later `27` / `59` / `77`) allowed `Text()` for
  fragments whose **baseline was inside** the soft rect but whose **glyph
  ascent extended above** `viewport_bounds.MinY` into the header.
- Soft text policy previously rejected only glyphs **wholly outside** the soft
  rect. `Text()` cannot partially clip; JAM2 therefore painted cell backgrounds
  and ink into damaged header pixels.
- `RectFill` was already correctly intersected; hardware+soft normal paints
  masked the overhang via `InstallClipRegion`. Soft-only refresh did not.

##### Fix

In `backends/clv_backend_amiga_v36.c`:

- Clip state flag `soft_only` is set only on the `LAYERUPDATING` path.
- Soft-only: reject `Text()` if `glyph_top < soft.MinY` or
  `glyph_bottom > soft.MaxY` (strict box containment).
- Hardware+soft: keep the looser wholly-outside reject so partially visible
  edge lines still draw and HW clip trims them.

##### Validation

- WB2.x: cover → scroll (row straddling top) → uncover → scroll to top —
  header stays clean; no Description bleed.
- WB3.2: same scenario — works correctly.
- Trade-off: during soft-only refresh, edge lines whose glyphs would overhang
  the viewport are skipped entirely (fills/dividers still soft-clipped).
  Normal non-refresh painting unchanged.

##### Files

- `src/custom_listview_control/backends/clv_backend_amiga_v36.c`
- this document (clip policy + this record)

#### Key files touched

- `backends/clv_backend_amiga_v36.c` — soft clip; `LAYERUPDATING` push/pop;
  `soft_only` strict text reject
- `clv_control_render.c` — bevel/frame, viewport-only flag, fragment Y skip
- `clv_control.h` / layout — frame bevel constant and insets
- `examples/custom_control_demo/main.c` — scroller Code→Top; paint modes;
  logging IDCMP
- `clv_control_log.h` / `.c`, `Makefile` — optional diagnostic build

#### Validation status (this session)

- WB2.x: soft-clip refresh path confirmed working (blank uncover fixed; no
  text outside list after scroll-under-cover).
- WB2.x / WB3.2 (2026-07-24): header bleed from soft-only `Text()` overhang
  fixed and retested (see record above).
- Phase 5 **not** complete: formal resize/relayout, multi-instance audit, and
  full WB2/WB3 checklist documentation still open.

### Objective

Make the control interactive and prove the two main user-facing requirements: whole-row selection and useful scrolling through variable-height content.

### Required functionality

- Mouse hit-testing from viewport coordinates to logical row.
- Whole-row highlight across every wrapped line.
- Configurable handling of clicks inside row gaps.
- Single selection.
- Non-selectable logical rows.
- Minimal make-visible behaviour.
- Pixel scroll state.
- Line up/down operations using current font line height.
- Page up/down operations.
- Proportional scrollbar synchronisation.
- Scroll clamping.
- Selection events returned to the application.

### Deliberately deferred

- Keyboard navigation unless trivial and well-contained.
- Drag selection/autoscroll.
- Double-click handling.
- Pressed header buttons and sorting.
- ScrollRaster optimisation.

### Required investigation before implementation

The Phase 4 agent must inspect the backend event loop and current demo interaction patterns, then create a precise message-to-neutral-event translation plan.

### Deliverables

- Input translation layer.
- Logical-row hit-testing.
- Whole-row selection rendering.
- Functional line/page/proportional scrolling.
- Interaction test cases.
- Updated size record.
- Updated Phase 4 section and status table.

### Exit criteria

- [x] Clicking any wrapped line selects the same logical row.
- [x] The entire row highlights as one block.
- [x] Header never scrolls.
- [x] Line scrolling advances by one current-font line height.
- [x] Scrollbar position and viewport stay synchronised.
- [x] Non-selectable rows cannot become selected.

### Phase 4 Completion Record

**Status:** Complete  
**Completion commit:** pending commit  
**Date:** `2026-07-23`

#### Implemented

- `CLV_CTRL_ROW_NONSELECTABLE` and `CLV_EventType`
  (`NONE` / `SELECTION_CHANGED` / `SCROLL_CHANGED`).
- Hit-test: `content_y = scroll_y + y - viewport_top` via `CLV_RowLayout`;
  gap clicks and header/outside → no row (`-1`).
- `clv_control_set_selected` rejects non-selectable / OOR (keeps prior);
  `-1` clears.
- `clv_control_make_visible` minimal pixel adjust using content rectangle.
- `clv_control_handle_input` for select-down and line/page/position scroll;
  returns TRUE iff `result->type != CLV_EVENT_NONE`.
- Whole-row selection fill/text pens across `content_height` only
  (gap stays normal background).
- `clv_control_get_viewport_height` for pixel scroller sync.
- Demo: `IDCMP_MOUSEBUTTONS` → `CLV_INPUT_SELECT_DOWN`; pixel
  `SCROLLER_KIND` (`GTSC_*` in content pixels); arrow `|delta|==1` →
  line step; non-selectable category row.

#### Files added or changed

- `src/custom_listview_control/clv_control.h` — row flag, event types,
  viewport getter
- `src/custom_listview_control/clv_control_internal.h` — hit-test decl
- `src/custom_listview_control/clv_control_input.c` — hit/select/make-visible/
  handle_input
- `src/custom_listview_control/clv_control_scroll.c` — viewport height
- `src/custom_listview_control/clv_control_render.c` — selection pens
- `examples/custom_control_demo/main.c` — Phase 4 interaction
- `examples/custom_control_demo/README.md` — features, size, checklist
- `docs/CLV_CUSTOM_CONTROL_DESIGN_AND_IMPLEMENTATION_PLAN.md` — this record

#### Design decisions

- Decision: Gap clicks select nothing.
- Reason: Design §8.4 preference; keeps gap free for future separators.
- Consequence: Users must click content pixels to select.

- Decision: Pixel `GTSC_Total` / `Visible` / `Top`; demo maps arrow
  `|delta|==1` to `SCROLL_LINE_*`.
- Reason: Variable-height content needs pixel proportional thumb; GadTools
  arrows still step by one scroller unit.
- Consequence: IDCMP translation stays in the demo; core remains GadTools-free.

- Decision: Full redraw on selection/scroll change.
- Reason: Correctness first; partial redraw / ScrollRaster deferred to Phase 5.
- Consequence: Acceptable for experimental demo; measure before optimising.

- Decision: `handle_input` TRUE iff a non-NONE `CLV_Event` is produced.
- Reason: Clear contract for apps (paint/sync only when something changed).
- Consequence: Re-clicking the already-selected fully visible row yields FALSE.

#### Tests performed

- Command: `make custom-control-demo` and `make size-draw-basic`
- Environment: Host cross-compile with VBCC `+aos68k -O2 -size -final`
  (Windows development host)
- Result: Both targets linked successfully. Runtime/visual validation on
  WB2.x/3.x **not** performed in this phase (deferred to Phase 5).
- Manual interaction checklist recorded in the demo README.

#### Size measurements

| Target | Before | After | Delta |
|--------|-------:|------:|------:|
| `bin/custom-control-demo` | 17296 (Phase 3) | 23580 | +6284 |
| `bin/size-draw-basic` | 27852 (Phase 3) | 27852 | 0 |

Compiler/flags: VBCC `vc`, `CFLAGS=+aos68k -c99 -cpu=68000 -O2 -size`,
`LDFLAGS=+aos68k -cpu=68000 -O2 -size -final -lamiga -lauto`.

Linked control modules: `clv_control*.o` (incl. wrap), `clv_backend_amiga_v36.o`,
`clv_platform.o`.

Object sizes (approx.): control 1844, layout 2520, wrap 3832, render 4188,
input 2244, scroll 704, backend 3100.

#### Known limitations

- No keyboard navigation, drag-autoscroll, or double-click.
- Full viewport redraw only (no ScrollRaster / dirty-row paint).
- Resize/relayout of window bounds not exercised (fixed demo geometry).
- Runtime WB2.x/3.x visual confirmation still pending (Phase 5).
- Scroller trough “page” behaviour depends on GadTools Top jumps, not an
  explicit `SCROLL_PAGE_*` from the demo (page ops exist in the core API).

#### Deviations from this design

- Added public `clv_control_get_viewport_height` (not listed in the Phase 1
  API sketch) so the demo can sync pixel `GTSC_Visible` without including
  internal headers.

#### Instructions for Phase 5

- Keep Phase 4 selection/scroll semantics; harden lifecycle and refresh.
- Validate SimpleRefresh / damage redraw; clear stale pixels at scroll edges.
- Plan resize: `set_bounds` + wrap rebuild + scroller re-sync while preserving
  `scroll_y` / selection where possible.
- Run and document WB2.x and WB3.x visual checks against the Phase 4 exit
  criteria and demo README checklist.
- Audit create/failure/destroy and font/pen ownership; note multi-instance.
- Optional only with evidence: partial selection redraw, ScrollRaster,
  basic keyboard line nav — do not break the GadTools-free core boundary.
- Record size delta vs Phase 4 demo and update this document’s Phase 5
  section + handoff.

---

## Phase 5 — Backend Hardening, Resize/Refresh, and WB2.x Validation

### Objective

Turn the working prototype into a reliable experimental control suitable for repeated use and comparative testing.

### Required functionality

- Correct refresh handling.
- Correct resize and relayout handling.
- Stable clipping and stale-pixel clearing.
- Disabled/read-only presentation if required by the Phase 1 API.
- Font ownership validation.
- Robust create/failure/destroy paths.
- Multiple control instances if feasible.
- Workbench 2.04 runtime validation.
- Workbench 3.x runtime validation.
- 68000 build validation.
- Documentation of backend assumptions.

### Optional if evidence supports it

- Partial redraw of old/new selection.
- ScrollRaster-based vertical scroll optimisation.
- Basic keyboard up/down navigation.

These optimisations must not compromise correctness or future backend separation.

### Required investigation before implementation

The Phase 5 agent must first review known refresh, layer, clipping, and scroller behaviour on the supported OS versions, then create a hardening plan.

### Deliverables

- Hardened control and backend.
- Runtime test report for WB2.x and WB3.x.
- Failure-path and ownership audit.
- Known limitations list.
- Updated Phase 5 section and status table.

### Exit criteria

- No known resource leaks in normal and failure paths.
- Refresh and resize are visually correct.
- Runtime behaviour is demonstrated on both primary OS generations.
- Remaining issues are documented rather than hidden.
- Known issues are documented with severity and workaround.
- Phase 6 can measure and judge the architecture fairly.

### Phase 5 Diagnostic Infrastructure Record

**Status:** Implemented — logger in place; WB2.x scroller + refresh/clip fixes
validated; see Phase 5 Completion Record for final closure  
**Completion commit (logger step):** `51e56ae`  
**Date:** `2026-07-23`

#### Purpose

Add compile-time optional, crash-safe runtime logging so WB2.x failures
(selection miss, refresh non-redraw, scroller exception `80000004`) can be
traced without changing control behaviour.

#### Files added or changed

- `src/custom_listview_control/clv_control_log.h` — macros / decls
- `src/custom_listview_control/clv_control_log.c` — PROGDIR open/write/close
- `src/custom_listview_control/clv_control.c` — render/create invariants
- `src/custom_listview_control/clv_control_input.c` — selection/hit-test
- `src/custom_listview_control/clv_control_render.c` — render/clip pairing
- `src/custom_listview_control/clv_control_scroll.c` — scroll clamp invariants
- `src/custom_listview_control/backends/clv_backend_amiga_v36.c` — push/pop clip
- `examples/custom_control_demo/main.c` — lifecycle, IDCMP, refresh, scroller
- `examples/custom_control_demo/README.md` — build commands and sizes
- `Makefile` — `custom-control-demo-log` isolated object tree
- this document

#### Build option / target

| Build | Command | Logger linked? |
|-------|---------|----------------|
| Normal | `make custom-control-demo` | No |
| Diagnostic | `make custom-control-demo-log` | Yes (`clv_control_log.o`) |

Diagnostic objects compile under `build/custom_listview_control_log/` with
`-DCLV_ENABLE_LOGGING` so normal objects are never contaminated.

#### Log path

`PROGDIR:clv_control.log`

Each line: open → seek end → write → close. Sequence number + `YYYY-MM-DD HH:MM:SS`.

#### Instrumentation points

- Program lifecycle (start, create/fail, event loop, destroy, end)
- IDCMP Class/Code/Qualifier/Mouse/IAddress; GadgetID only after class check
- Refresh: message → `GT_BeginRefresh` → paint → `GT_EndRefresh` (depth check)
- Selection: mouse, content_y, hit, flags, old/new selection, render skip/request
- Scroller: before/after `GT_GetGadgetAttrs` / `GT_SetGadgetAttrs`, translate,
  `handle_input`, resulting `scroll_y`, re-sync
- Render: `clv_control_render` / `render_full` / header / viewport / clip push result / pop
- v36 `push_clip` / `pop_clip` outcomes
- Lightweight INVARIANT logs (NULL pointers, bad viewport, scroll clamp, etc.)

Logging builds also request `IDCMP_INTUITICKS` and `IDCMP_NEWSIZE` (normal build
IDCMP set unchanged).

#### Size measurements

| Target | Bytes | Notes |
|--------|------:|-------|
| `bin/custom-control-demo` (Phase 4 baseline) | 23580 | Prior phase |
| `bin/custom-control-demo` (this step) | 23744 | +164; class-safe gadget IAddress + refresh depth; no logger.o |
| `bin/custom-control-demo-log` | 32768 | Includes `clv_control_log.o` + instrumented call sites |
| `bin/size-draw-basic` | 27852 | Unchanged |

Compiler/flags: VBCC `vc`, `CFLAGS=+aos68k -c99 -cpu=68000 -O2 -size`,
`LDFLAGS=+aos68k -cpu=68000 -O2 -size -final -lamiga -lauto`.

#### Tests performed

- Host cross-link: `make custom-control-demo`, `make custom-control-demo-log`,
  `make size-draw-basic` — all succeeded.
- Confirmed normal link line omits `clv_control_log.o`; logging link includes it.
- Runtime WB3.2 / WB2.x log collection **not** performed in this step.

#### Known limitations

- Open/write/close per line is slow (intentional crash safety).
- `IDCMP_INTUITICKS` in the logging build can flood the log.
- Distinct from v1 `src/custom_listview/clv_log.h` printf stub (shares only the
  `CLV_ENABLE_LOGGING` enable symbol name).
- No speculative WB2.x compatibility fix in this step.

#### Exact next debugging steps

1. ~~Run `bin/custom-control-demo-log` on Workbench 2.x.~~ Done.
2. ~~Collect traces for row click / uncover refresh / scroller `80000004`.~~
   Done — scroller: use IDCMP `Code` as `GTSC_Top` (no V39 `GT_GetGadgetAttrs`).
3. ~~Refresh blank bands / text-outside-list.~~ Done — soft viewport clip
   during `LAYERUPDATING`; never `InstallClipRegion` on that path.
4. ~~Remaining: resize/`set_bounds` + wrap rebuild + scroller re-sync; ownership /
   multi-instance audit; formal WB2.x and WB3.x checklist write-up.~~ Done —
   see Phase 5 Completion Record.

Phase 5 is **complete** (see Completion Record).

### Phase 5 incremental-render preparation

**Status:** Implemented (host cross-link validated; Amiga runtime checklist below)  
**Date:** `2026-07-24`  
**Scope:** Preparatory optimisation only — regional viewport paint + partial
selection repaint. No smart/pixel-shift scrolling.

#### Implementation summary

- One internal painter,
  `clv_control_paint_viewport_area(control, screen_area)`, takes a
  window-relative rectangle, intersects it with `viewport_bounds`, clears that
  band to the normal background, installs the existing dual clip policy on the
  intersection, paints intersecting logical rows (content + gap), and restores
  body vertical edges plus configured row dividers. Header and outer frame are
  never touched.
- Full viewport paint is
  `clv_control_paint_viewport_area(c, &c->viewport_bounds)` via
  `clv_control_render_viewport` / the viewport stage of
  `clv_control_render_full`.
- `clv_control_get_row_paint_area` returns the visible content rectangle for a
  logical row (gap excluded; viewport-intersected).
- Selection without a `scroll_y` change uses
  `clv_control_render_logical_rows(old, new)`. Ordinary scrolling and
  selection-triggered `make_visible` scroll still use full viewport paint.
- `CLV_Event.previous_row` carries the prior selection on
  `CLV_EVENT_SELECTION_CHANGED`.

#### New internal rendering flow

```text
clv_control_render(flags)
  ├── full → frame + header + paint_viewport_area(viewport_bounds) + frame
  └── VIEWPORT_ONLY → paint_viewport_area(viewport_bounds)

clv_control_render_logical_rows(row_a, row_b)
  └── for each visible row paint area:
        paint_viewport_area(row_content ∩ viewport)
        (clip/geometry failure → render_viewport fallback)
```

#### Files changed

- `src/custom_listview_control/clv_control_render.c` — region painter, row
  geometry helpers, logical-row render entry
- `src/custom_listview_control/clv_control_internal.h` — internal decls
- `src/custom_listview_control/clv_control.h` — `previous_row`,
  `clv_control_render_logical_rows`
- `src/custom_listview_control/clv_control_input.c` — fill `previous_row`
- `examples/custom_control_demo/main.c` — selection uses logical-row paint
- `examples/custom_control_demo/README.md` — sizes / checklist
- this document

#### New identifiers (clean-room)

| Identifier | Kind | Purpose | Original to CLV control |
|------------|------|---------|-------------------------|
| `clv_control_paint_viewport_area` | function | Paint arbitrary viewport sub-rectangle | Yes |
| `clv_control_get_row_paint_area` | function | Visible content rect for a logical row | Yes |
| `clv_control_render_logical_rows` | function | Public old/new row repaint helper | Yes |
| `clv_ctrl_intersect_rects` | static fn | Rectangle intersection | Yes |
| `clv_ctrl_row_content_screen_rect` | static fn | Unclipped content screen rect | Yes |
| `clv_ctrl_row_gap_screen_rect` | static fn | Gap band screen rect | Yes |
| `clv_ctrl_paint_row_content` | static fn | Selection fill + wrap fragments | Yes |
| `clv_ctrl_paint_row_gap` | static fn | Gap background restore | Yes |
| `CLV_Event.previous_row` | field | Prior selection for delta paint | Yes |
| `paint` / `screen_area` / `area_a` / `area_b` | locals | Region geometry | Yes |

No private names from any original system ListView implementation were reused.
Public AmigaOS API names (`Rectangle`, `InstallClipRegion`, etc.) unchanged.

#### Clipping decisions

- Dual policy preserved exactly:
  1. Normal: hardware `InstallClipRegion` on the **paint intersection** plus
     soft clip.
  2. `LAYERUPDATING`: no `InstallClipRegion`; soft clip only on the same
     intersection.
- Soft clip rectangle is `requested ∩ viewport`, not always the full viewport.
- Single clip slot / push-pop pairing unchanged; failed push returns FALSE and
  selection paint falls back to full viewport.
- Fragment baseline rejection against the **viewport** retained (Phase 5).
- Soft-only (`soft_only`) text reject is strict box containment — see
  **Header text bleed** under Phase 4 session updates. Validated WB2.x + WB3.2.

#### Selection repaint behaviour

- Demo: on `SELECTION_CHANGED` with unchanged `scroll_y`, call
  `clv_control_render_logical_rows(previous_row, row)`.
- Re-click same fully visible row still yields `handle_input` FALSE (no paint).
- Non-selectable / gap / miss: no event, no paint.
- `make_visible` that moves `scroll_y`: full viewport paint (no scroll blit).

#### Full-redraw fallback conditions

Use full viewport paint when:

- ordinary scrolling (`SCROLL_CHANGED`);
- selection changes `scroll_y` via `make_visible`;
- layout/bounds/font/columns/rows/row_gap changes (existing invalidate + full
  render paths);
- `paint_viewport_area` clip setup fails;
- neither old nor new row has a visible paint area (no-op; caller may still
  full-paint on scroll).

#### Size measurements

| Target | Before | After | Delta |
|--------|-------:|------:|------:|
| `bin/custom-control-demo` | 24896 | 25764 | +868 |
| `bin/custom-control-demo-log` | 34240 | 35808 | +1568 |
| `bin/size-draw-basic` | 27852 | 27852 | 0 |
| `clv_control_render.o` (normal) | 4972 | 6540 | +1568 |

Compiler/flags unchanged: VBCC `+aos68k -O2 -size -final`.

#### Tests performed

- Host: `make custom-control-demo`, `make custom-control-demo-log`,
  `make size-draw-basic` — linked successfully.
- Confirmed no `ScrollRaster` / `InstallLayerHook` / `LAYERS_NOBACKFILL` in
  custom-control sources or this change.
- Amiga runtime after soft-only text fix (`2026-07-24`):

**WB3.x / WB2.x visual checklist**

- [x] Initial full paint correct
- [x] Select unwrapped / wrapped rows; tall↔short selection; clear via reselect
      patterns; non-selectable ignored
- [x] Only old/new rows visibly update; header/frame do not flash
- [x] Column dividers continuous; gaps clean; partial edge rows correct
- [x] Ordinary scrolling still full-viewport and correct
- [x] Uncover refresh: no blank bands; no text outside viewport (WB2.x dual clip)
- [x] Cover → scroll (straddle top) → uncover → scroll to top: header stays
      clean (soft-only `Text()` overhang fix; WB2.x and WB3.2)
- [ ] Logging build: balanced clip push/pop around selection paints
      (optional re-check)

#### Known limitations

- Ordinary scrolling uses smart pixel-shift when eligible (see **Phase 5
  smart vertical scrolling**); otherwise full viewport.
- Resize/`set_bounds` formal audit and Phase 5 exit criteria still open.
- Soft-only refresh skips edge lines whose glyphs would overhang the viewport
  (by design of the strict text reject).

#### Exact handoff for later smart scrolling

~~Gate met.~~ Implemented under **Phase 5 smart vertical scrolling** below.

### Phase 5 smart vertical scrolling

**Status:** Implemented and validated on Workbench 2.x and Workbench 3.2  
**Date:** `2026-07-24`  
**Default:** `CLV_ENABLE_SMART_SCROLL=1` for custom-control demo builds.  
**Disable:** `make clean` then `make custom-control-demo CLV_ENABLE_SMART_SCROLL=0`
(or keep a measured `bin/custom-control-demo-nosmart` copy).

Phase 5 completion is recorded under **Phase 5 Completion Record**.

#### Implementation plan (decisions)

| Question | Decision |
|----------|----------|
| Where is old `scroll_y` captured? | Demo `demo_apply_input` records `scroll_before` before `handle_input` |
| Where is new `scroll_y` committed? | Core `clv_control_set_scroll_y` inside `handle_input` (unchanged) |
| Who decides smart vs full? | Core geometric eligibility + backend layer safety; demo only calls `clv_control_render_scrolled` |
| Which op shifts pixels? | Optional `CLV_DrawOps.move_viewport_pixels` (v36 impl) |
| Result reporting | `CLV_ViewportMoveResult`: `UNUSED` / `DONE` / `REPAINT` |
| Exposed band | Inclusive window-relative rect inside `viewport_bounds`, height `abs(delta)` |
| Clip policy | Never run while `LAYERUPDATING`; refuse if backend clip slot active; do not change soft_only refresh |
| Damage detection | `LAYERREFRESH` before → refuse; after V39+ → `REPAINT`; V37 vacated damage expected → `DONE` then `finish_viewport_move` |
| Refuse when | delta 0 / `abs(delta) >= vp_h` / bad viewport / null layer/RP / clip active / updating / pre-damaged / no op / compiled out |
| WB2.x refresh fixes | Untouched: refresh still full `demo_paint` under `GT_BeginRefresh`; no `InstallClipRegion` on `LAYERUPDATING`; soft_only strict text unchanged |
| Logging | `SMART_SCROLL …` lines distinguish request / reject / backend / damage / exposed / regional / fallback |

#### State ordering (why safe)

1. Record `previous_scroll_y`.
2. Commit new `scroll_y` via existing input path.
3. `clv_control_render_scrolled(c, previous_scroll_y)`.
4. If eligible: backend shifts pixels in `viewport_bounds` only
   (`dy = new_scroll_y - old_scroll_y`; positive content delta → pixels move up).
5. Regional-paint the exposed band using **already committed** `scroll_y`.
6. Optional `finish_viewport_move` acknowledges V37 simple-refresh vacated damage.
7. Else full `clv_control_render_viewport`.
8. Demo syncs scroller after paint (success or fallback).

Existing pixels still represent the old scroll until the shift; the painter
always uses the new scroll. No rollback of `scroll_y` is required.

#### Backend responsibilities

- Own `ScrollRasterBF` (gfx ≥ 39) or `ScrollRaster` (V36/V37),
  `LockLayerInfo` / `UnlockLayerInfo`, `InstallLayerHook` /
  `LAYERS_NOBACKFILL`, `BeginUpdate` / `EndUpdate` for damage ack.
- Shift **only** the authoritative `control->viewport_bounds` rectangle
  (never header, frame, or scrollbar).
- Every early exit restores hook / unlock before returning.
- Do not call `InstallClipRegion` during this path; do not enter
  `GT_BeginRefresh` from the backend.

#### Files changed

- `clv_control_draw.h` — `CLV_ViewportMoveResult`; optional draw-ops
- `backends/clv_backend_amiga_v36.c` — move + finish; soft_only (prior fix)
- `clv_control_render.c` — eligibility, exposed band, `render_scrolled`
- `clv_control.h` / `clv_control_internal.h` — public/internal decls
- `examples/custom_control_demo/main.c` — scroll path uses `render_scrolled`
- `Makefile` — `CLV_ENABLE_SMART_SCROLL`, `custom-control-demo-nosmart`
- this document; demo README

#### Clean-room identifier audit

| New identifier | Kind | Purpose | Confirmed original to CLV control |
|----------------|------|---------|-----------------------------------|
| `CLV_ViewportMoveResult` | enum | Pixel-move outcome | Yes |
| `CLV_VIEWPORT_MOVE_UNUSED` | enum value | Not attempted / unavailable | Yes |
| `CLV_VIEWPORT_MOVE_DONE` | enum value | Shift succeeded | Yes |
| `CLV_VIEWPORT_MOVE_REPAINT` | enum value | Caller must full-repaint viewport | Yes |
| `CLV_DrawOps.move_viewport_pixels` | ops field | Optional backend pixel shift | Yes |
| `CLV_DrawOps.finish_viewport_move` | ops field | Ack simple-refresh damage after band paint | Yes |
| `clv_v36_move_viewport_pixels` | static fn | V36+ shift implementation | Yes |
| `clv_v36_finish_viewport_move` | static fn | `BeginUpdate`/`EndUpdate` damage ack | Yes |
| `clv_control_render_scrolled` | function | Post-scroll smart or full paint | Yes |
| `clv_ctrl_smart_scroll_eligible` | static fn | Core geometric eligibility | Yes |
| `clv_ctrl_exposed_band_after_shift` | static fn | Exposed strip geometry | Yes |
| `clv_ctrl_expand_exposed_for_glyphs` | static fn | Grow paint rect for full Text cells | Yes |
| `CLV_ENABLE_SMART_SCROLL` | compile flag | Omit smart-scroll code when 0 | Yes |

No private names from another ListView implementation were reused. No copied
comments. Public AmigaOS API names (`ScrollRaster`, `ScrollRasterBF`,
`InstallLayerHook`, `LAYERS_NOBACKFILL`, `LockLayerInfo`, `LAYERREFRESH`,
`LAYERUPDATING`, `BeginUpdate`, `EndUpdate`) overlap by necessity only.

#### Size measurements

| Target | Before (incremental) | After (smart on) | Delta |
|--------|---------------------:|-----------------:|------:|
| `bin/custom-control-demo` | 25764 | 26996 | +1232 |
| `bin/custom-control-demo` smart off | — | 26052 | +288 vs pre-API |
| `bin/custom-control-demo-log` | 35808 | 39128 | +3320 |
| `bin/size-draw-basic` | 27852 | 27852 | 0 |
| `clv_control_render.o` (smart on) | 6540 | 7612 | +1072 |
| `clv_backend_amiga_v36.o` (smart on) | — | 4808 | — |

Compiler/flags unchanged: VBCC `+aos68k -O2 -size -final`.

#### Tests performed

- Host: `make custom-control-demo`, `make custom-control-demo-log`,
  `make size-draw-basic`, smart-off rebuild — all linked.
- Normal logger-free build remains logger-free.
- Amiga runtime (`2026-07-24`): validated on **Workbench 2.x** and
  **Workbench 3.2** (see checklist; follow-up bug fixes below also
  revalidated on both).

**WB3.2 / WB2.x smart-scroll checklist (Amiga session)**

- [x] One-line arrow scroll up/down; repeated slow arrows
- [x] Proportional knob drag; trough/page; large jump → full fallback
- [x] Top/bottom limits; partially visible rows; row gaps; empty below last
- [x] Selected wrapped row entering/leaving viewport
- [x] Cover → uncover → scroll; scroll under partial overlap → refresh
- [x] Selection repaint after smart scroll; header/frame never shift
- [x] WB2.x: no `80000004`; scroller still uses message Code; no
      `GT_GetGadgetAttrs` for Top; soft_only header bleed still fixed
- [x] Logging: `SMART_SCROLL` blit success vs `fallback full viewport`
- [x] Scroll-down glyph expand (no blank bands / letter fragments)
- [x] Selection + `make_visible`: full viewport; old highlight cleared;
      new wrapped row fully highlighted (WB2.x and WB3.2)
- [ ] 7 MHz qualitative: less blanking than full-viewport scroll
      (optional; not separately recorded)

#### Known limitations

- Page jumps with `abs(delta) >= viewport height` always full-repaint.
- Overlap damage on gfx ≥ 39 after shift forces full-viewport fallback.
- Resize/relayout uses full repaint only (see **Phase 5 resize and relayout
  hardening**); smart scroll resumes after resize completes.
- Scroller/Quit: GadTools caches size at `CreateGadget` — resize uses the
  template rebuild path (`RemoveGList` → `FreeGadgets` → `CreateGadget` →
  `AddGList`), not field poking.
- 7 MHz qualitative timing comparison optional / not separately recorded.

#### Bug fix — scroll-down glyph clip (2026-07-24)

**Symptom (WB2.x):** scrolling up OK; scrolling down left blank bands and
letter fragments (tops missing). Log showed successful blit + bottom-band
regional paint (`exposed` at viewport bottom).

**Cause:** vacated-strip HW clip + all-or-nothing `Text()` chopped glyph
ascent that sat above the strip. Chopped stubs then shifted upward on later
down-scrolls.

**Fix:** after computing the vacated strip, expand the regional paint rect
by one `line_height` against the scroll direction (`clv_ctrl_expand_exposed_for_glyphs`),
clamped to the viewport, so straddling lines redraw complete cells.

**Validation:** Fixed and confirmed on Workbench 2.x and Workbench 3.2.

#### Bug fix — selection + make_visible highlight (2026-07-24)

**Symptom:** After scrolling, select a short row then a tall wrapped row that
triggers `make_visible`. Viewport snaps correctly, but the previous row stays
highlighted and the new row is only partly highlighted (often the bottom
band).

**Cause:** Demo treated any `scroll_y` change as smart scroll. Pixel shift kept
the old selection colours in retained pixels; the exposed-band paint only
redraws the vacated strip, so the new logical row was only partially filled
with the selected pens. Log: `ev.type=1` with `old_scroll≠new_scroll` then
`SMART_SCROLL` and no `selection row paint`.

**Fix:** When `SELECTION_CHANGED` and `scroll_y` both change, paint the full
viewport (`CLV_RENDER_VIEWPORT_ONLY`). Smart scroll remains for pure scroll
events. Documented on `clv_control_render_scrolled`.

**Validation:** Fixed and confirmed on Workbench 2.x and Workbench 3.2
(`2026-07-24`).

#### Exact next handoff

1. ~~Continue remaining Phase 5 work: resize/`set_bounds`.~~ Done — see
   **Phase 5 resize and relayout hardening** below.
2. Phase 5 closure audit: ownership, failure paths, multi-instance,
   formal WB2.x/WB3.2 acceptance matrix, final sizes, Phase 5 completion.
3. Optional: record 7 MHz qualitative smart-scroll vs full-viewport feel.
4. Preserve dual clip + soft_only strict text; do not reintroduce
   `GT_GetGadgetAttrs` for V37 scroller Top.

### Phase 5 resize and relayout hardening

**Status:** Implemented (host cross-link validated; Amiga runtime checklist below)  
**Date:** `2026-07-24`  
**Scope:** Resizable demo window + transactional `clv_control_set_bounds`
relayout. No Phase 6 work. No ownership/multi-instance closure audit except
failure-safe restore of prior layout caches.

#### Implementation plan (answered)

| Question | Decision |
|----------|----------|
| Resize event | `IDCMP_SIZEVERIFY` detaches gadgets; `IDCMP_NEWSIZE` uses final `Window` size |
| Bounds derivation | Demo: borders + `DEMO_PAD` + scroller width + Quit strip → outer control rect. Control owns frame/header/viewport/column insets |
| Scroller geometry | Template rebuild: `RemoveGList` → `FreeGadgets` → `CreateGadget` at new boxes → `AddGList` + `RefreshGList`. GadTools caches size at create time — field poking does not resize `SCROLLER_KIND`. Values via `GT_SetGadgetAttrs` |
| Column widths | **Fixed minima** via `width_pixels` (never shrink). Last column **grows** to fill leftover viewport width up to `MaxX` (flush next to scrollbar). Window `WA_MinWidth` / `WindowLimits` keep the viewport ≥ sum(configured widths) + dividers + frame |
| Wrap rebuild | `clv_control_wrap_prepare` against new text widths; no draw-time wrap |
| Row layout rebuild | Heights from `(max fragment count × line height) + (2 × cell_padding_y) + row_gap` |
| Allocation failure | Snapshot owned caches; restore prior layout + prior outer bounds; log |
| Scroll | Preserve prior `scroll_y`, then clamp to new max |
| Selection | Keep if in range and selectable; else clear; then `make_visible` if selected |
| Selected visibility | Yes — minimal `make_visible` after clamp |
| Scroller sync | `GTSC_Total` / `Visible` / `Top` from control state (V37-safe) |
| Paint mode | Full `clv_control_render(0)` only — never `render_scrolled` |
| Stale pixels | `RectFill` interior with `BACKGROUNDPEN` before re-add/paint (no V39 `EraseRect`) |
| Smart-scroll bypass | Resize path never calls smart scroll; geometry change invalidates old pixels |
| WB2.x refresh | Unchanged: soft-only during `LAYERUPDATING`; no `InstallClipRegion` |

#### Resize event flow

```text
IDCMP_SIZEVERIFY
  └── RemoveGList(glist)  (reply before Intuition finishes size)

IDCMP_NEWSIZE  (final Width/Height)
  ├── RemoveGList if still attached
  ├── clear window interior (BACKGROUNDPEN RectFill)
  ├── FreeGadgets + CreateGadget(scroller, Quit) at new boxes
  ├── AddGList + RefreshGList
  ├── clv_control_set_bounds(outer rect)   /* transaction */
  ├── GT_RefreshWindow
  ├── clv_control_render(0)               /* full; no smart scroll */
  └── demo_sync_scroller (Total/Visible/Top)
```

#### Relayout transaction (`clv_control_set_bounds`)

1. Normalize proposed rectangle (no inverted Min/Max).
2. If bounds unchanged and `layout_valid`, return.
3. Snapshot owned caches + header/viewport metrics; detach pointers without free.
4. Apply new outer `bounds`; split header/viewport; rebuild columns/wraps/rows.
5. On failure: free partial new state; restore snapshot + prior outer bounds.
6. On success: free snapshot; clamp `scroll_y`; preserve/clear selection;
   `make_visible` if a selection remains.
7. Caller full-repaints (demo). Smart scroll is not used.

**Limitation:** First layout (no prior valid caches) is non-transactional
(`free` stale + fresh rebuild). Column/row/gap setters still only invalidate;
the next `set_bounds`/`render` rebuilds.

#### Minimum geometry

| Constant / value | Role |
|------------------|------|
| `DEMO_PAD` (8) | Interior padding |
| `DEMO_CTRL_FRAME_W` (1) | Matches the one-pixel control outline (both sides) |
| `DEMO_CTRL_DIVIDER_W` (1) | Matches one-pixel control column separators |
| `g_demo_fixed_content_w` | `sum(width_pixels) + (n-1)*divider` |
| `g_demo_min_ctrl_w` | `fixed_content_w + 2*frame` (outer control) |
| `WA_MinWidth` / `WindowLimits` | `BorderLeft + pad + min_ctrl_w + scroll_w + pad + BorderRight` |
| `DEMO_MIN_CTRL_H` (48) | Minimum outer control height |

**Fixed-pixel column policy:** configured `width_pixels` are minima for every
column. Columns `0 .. n-2` stay exactly at that width. The **last column
grows** to absorb leftover viewport width (`right = viewport MaxX`) so the
table flushes next to the scrollbar; it never shrinks below `width_pixels`.
Wrap / alignment / header text for the last column reflow on widen because
they already use `col_geom` text bounds. Other columns’ wrap is unchanged
on horizontal resize above the minimum.

Degenerate tiny **height** still clamps non-inverted viewport rectangles
inside the control. Width collapse is prevented by Intuition minimum size.

#### Smart-scroll bypass

Resize always uses the full control render path. Do not call
`clv_control_render_scrolled` from the resize handler. After resize completes,
ordinary scroller events may smart-scroll again.

#### Clean-room identifier audit

| New identifier | Kind | Purpose | Original to CLV control |
|----------------|------|---------|-------------------------|
| `CLV_CtrlLayoutSnapshot` | struct | Transactional cache snapshot | Yes |
| `clv_ctrl_layout_take_snapshot` | static fn | Detach owned caches into snapshot | Yes |
| `clv_ctrl_layout_restore_snapshot` | static fn | Restore prior caches after failure | Yes |
| `clv_ctrl_layout_discard_snapshot` | static fn | Free replaced prior caches | Yes |
| `clv_control_layout_free_owned` | static fn | Free all owned layout heap | Yes |
| `clv_control_layout_rebuild_fresh` | static fn | Non-transactional rebuild | Yes |
| `clv_ctrl_finish_column_text` | static fn | Recompute text_left/right insets | Yes |
| `clv_ctrl_normalize_bounds` | static fn | Fix inverted outer rectangles | Yes |
| `clv_ctrl_preserve_selection_after_relayout` | static fn | Keep/clear logical selection | Yes |
| `DemoGeom` | demo struct | Outer control + scroller + Quit boxes | Yes |
| `DEMO_PAD` | demo macro | Interior padding | Yes |
| `DEMO_CTRL_FRAME_W` / `DEMO_CTRL_DIVIDER_W` | demo macros | Match control outline/separator | Yes |
| `DEMO_MIN_CTRL_H` | demo macro | Minimum control height | Yes |
| `g_demo_fixed_content_w` / `g_demo_min_ctrl_w` | demo statics | Fixed column sum / min control width | Yes |
| `demo_fixed_content_width` | demo fn | Sum column + divider pixels | Yes |
| `demo_scroll_width` / `demo_min_ctrl_width` | demo fn | Scroller / outer control mins | Yes |
| `g_demo_gadgets_detached` | demo flag | SIZEVERIFY detach state | Yes |
| `demo_compute_geom` | demo fn | Bounds from window interior | Yes |
| `demo_bounds_from_geom` | demo fn | `Rectangle` from `DemoGeom` | Yes |
| `demo_clear_window_interior` | demo fn | Stale-pixel clear (V37 RectFill) | Yes |
| `demo_create_gadgets` | demo fn | CreateContext + scroller + Quit | Yes |
| `demo_destroy_gadgets` | demo fn | FreeGadgets + clear pointers | Yes |
| `demo_handle_newsize` | demo fn | Resize transaction orchestration | Yes |

No private names, comments, or control-flow copies from another ListView
implementation were introduced. Public AmigaOS APIs only.

#### Files changed

- `src/custom_listview_control/clv_control_layout.c` — transactional rebuild;
  fixed column minima + grow-only last-column fill
- `src/custom_listview_control/clv_control.c` — hardened `set_bounds`
- `src/custom_listview_control/clv_control_internal.h` — `layout_rebuild` → `BOOL`
- `src/custom_listview_control/clv_control.h` — `set_bounds` contract; `width_pixels`
  as fixed minimum (last column may grow)
- `examples/custom_control_demo/main.c` — size gadget, SIZEVERIFY/NEWSIZE,
  gadget recreate, `WA_MinWidth`/`WindowLimits` from fixed column sum
- `examples/custom_control_demo/README.md` — sizes / checklist
- this document

#### Size measurements

| Target | Before (smart-scroll record) | After | Delta |
|--------|-----------------------------:|------:|------:|
| `bin/custom-control-demo` (smart on) | 26996 | 29872 | +2876 |
| `bin/custom-control-demo` (smart off) | 26052 | 28628 | +2576 |
| `bin/custom-control-demo-log` | 39128 | 43576 | +4448 |
| `bin/size-draw-basic` | 27852 | 27852 | 0 |

Compiler/flags unchanged: VBCC `+aos68k -O2 -size -final`, 68000.

#### Host / cross-build tests

- `make custom-control-demo` — linked
- `make custom-control-demo-log` — linked
- `make clean && make custom-control-demo CLV_ENABLE_SMART_SCROLL=0` — linked
- `make size-draw-basic` — unchanged
- No `GT_GetGadgetAttrs` on V37 scroller Top path
- No `EraseRect` / V39-only clear in resize path

#### Runtime checklist (Workbench 3.2)

Validated on Amiga (Phase 5 Task 1 operator session; consolidated in
Phase 5 Completion Record):

- [x] Widen / narrow slowly; taller / shorter
- [x] Window refuses to shrink below fixed-column minimum
- [x] Description column never collapses; other fixed columns stable
- [x] Widen above min: last column (Size) flushes to scrollbar; no grey gap
- [x] Right-aligned Size values track the new right edge
- [x] Horizontal resize above min: only last-column wrap/height may change
- [x] Rapid repeated resize
- [x] Resize at top / middle / bottom scroll
- [x] Wrapped and unwrapped selection; vertical resize + scroller OK
- [x] After smart scroll; after cover/uncover
- [x] No stale frame/header/divider/selection pixels
- [x] Scroller thumb + clamping correct; smart scroll works again after resize

#### Runtime checklist (Workbench 2.x)

Same as WB3.2, plus:

- [x] No exception `80000004`
- [x] No blank bands after uncover; no text outside viewport
- [x] No header text bleed; soft_only strict containment intact
- [x] No V39 scroller query; no clip imbalance
- [x] Scroller responsive after repeated resize

#### Follow-up — fixed pixel column widths (`2026-07-24`)

**Problem:** Narrow resize clipped columns to the viewport, collapsing
Description to a few pixels and producing extremely tall wrapped rows.

**Fix:**
- Control layout uses configured `width_pixels` as fixed minima (no clip-to-
  viewport shrink of earlier columns).
- Demo `WA_MinWidth` / post-open `WindowLimits` =
  `BorderLeft + DEMO_PAD + (sum(widths)+dividers+2*frame) + scroll_w + DEMO_PAD + BorderRight`.

**Tests:** Host rebuild of `custom-control-demo`. Amiga WB2.x / WB3.2:
confirm the window will not shrink below the minimum; Description width
stable; vertical resize + scroller still correct.

#### Follow-up — last-column fill (`2026-07-27`)

**Problem:** When the viewport was wider than the configured column sum,
spare grey space remained after the Size column (abrupt right edge next to
the scrollbar).

**Fix:** Grow-only last-column absorb — after placing all columns at their
configured minima, extend the final column’s `right` to `viewport MaxX` and
recompute text insets. Wrap, right/center alignment, and header titles for
that column follow automatically via existing `col_geom` consumers. Earlier
columns stay fixed; min-width policy unchanged.

**Tests:** Host rebuild of `custom-control-demo`. Amiga: widen → Size flush
to scroller; right-aligned sizes on new right; Description width unchanged;
narrow to min still blocks collapse.

#### Known limitations

- SIZEVERIFY without a following NEWSIZE leaves gadgets detached until the
  next NEWSIZE (rare cancel path).
- GadTools scroller must be recreated on resize (cached create-time size);
  field update alone is insufficient (confirmed).
- Proportional redistribution of *all* columns / percentage widths /
  hiding columns / horizontal scrolling are intentionally out of scope;
  only the last column grows into leftover viewport width.

#### Exact next handoff

~~Phase 5 closure audit.~~ Done — see **Phase 5 Completion Record** below.

### Phase 5 Completion Record

**Status:** Complete  
**Completion commit:** `629ddf3`  
**Date:** `2026-07-27`

#### Final status

Phase 5 exit criteria are met. The experimental custom control is hardened
enough for fair Phase 6 architecture/size comparison. Remaining gaps are
documented design limitations, not open correctness blockers.

#### Implemented / hardened areas

- Optional crash-safe diagnostic logger (`CLV_ENABLE_LOGGING`)
- Workbench 2.x scroller Top via `IntuiMessage.Code` (no V39 query)
- Dual clipping: HW `InstallClipRegion` + soft; soft-only under `LAYERUPDATING`
- Soft-only strict text containment (header bleed fix)
- Regional viewport painting + partial selection repaint
- Smart vertical scrolling with safe layer-hook / LockLayerInfo restore
- Resize/relayout: transactional `set_bounds`, wrap rebuild, scroll clamp,
  scroller recreate/resync, full repaint (never smart-scroll on resize)
- Fixed pixel column minima; grow-only last column; minimum window width
- Soft-clip activation on HW-clip allocation/install failure (closure fix)

#### Files changed (closure audit + soft-clip fix)

- `src/custom_listview_control/backends/clv_backend_amiga_v36.c` —
  `clv_v36_activate_soft_only`; NewRegion/OrRect/AndRegion/InstallClipRegion
  and no-layer fallbacks activate soft_only (never report success with clip
  inactive)
- `src/custom_listview_control/clv_control_layout.c` / `clv_control.h` —
  last-column grow (Task 1 follow-up, included in closure baseline)
- `examples/custom_control_demo/README.md` — final sizes / checklists
- this document — ownership, matrices, limitations, Phase 6 handoff

#### Soft-clip failure-path fix (closure)

| Field | Detail |
|-------|--------|
| Root cause | `push_clip` returned TRUE after `NewRegion`/`OrRectRegion`/`AndRegionRegion`/`InstallClipRegion` failure (or no-layer) without setting `soft_active`, so paints continued unclipped |
| Fix | Shared `clv_v36_activate_soft_only`; all HW-clip failure and no-layer paths activate soft_only |
| Regression | Host rebuild of normal + logging demos; soft_only behaviour matches already-validated `LAYERUPDATING` path |
| Size impact | `clv_backend_amiga_v36.o` 4808 → 4920 (+112); normal demo +88 vs pre-fix last-column baseline |

#### Design decisions (closure)

- Decision: Mark Phase 5 Complete with multi-instance **static audit** evidence
  (no dual-control demo).
- Reason: Per-instance control + backend state; only shared mutables are
  optional logger sequence globals.
- Consequence: Dual-control runtime remains an accepted untested scenario,
  not a claimed feature demo.

- Decision: Do not begin Phase 6 optimisation or V2 recommendation here.
- Reason: Closure scope only.
- Consequence: Phase 6 agent receives precise size/OS/CPU baselines below.

#### Ownership and lifetime table

| Resource | Owner | Created by | Released by | Borrowed/owned | Failure behaviour |
|----------|-------|------------|-------------|----------------|-------------------|
| `CLV_Control` | Control | `clv_control_create` | `clv_control_destroy` | Owned | NULL on malloc fail; never exposed |
| columns array | Application | App | App | Borrowed | Setter rejects null+count; invalidate only |
| rows array | Application | App | App | Borrowed | Same |
| cell strings | Application | App | App | Borrowed (via rows) | Fragments point into cells; never freed by control |
| `layout_rows` | Control | layout rebuild | destroy / free_owned / snapshot discard | Owned | Transaction restores prior; fresh path frees partial |
| wrap / cell-frag cache | Control | `wrap_prepare` | `layout_free_wraps` / destroy | Owned | Partial wrap frees new wraps; snapshot keeps old |
| `col_geom` / `divider_x` | Control | layout columns | free_owned / destroy | Owned | Column malloc fail frees partial |
| font pointer | Application / RP | App / window | App (never Closed by control) | Borrowed | Backend `SetFont` only |
| DrawInfo pens | Copied into control | `set_pens` / `pens_from_drawinfo` | N/A (by value) | Owned copy of values | — |
| draw context / backend | Application | `clv_backend_v36_create` | `clv_backend_v36_destroy` | Owned by app; pointed by control | Destroy pops clip then frees |
| RastPort / Window / Layer | Intuition | OpenWindow | CloseWindow | Borrowed | Backend stores RP pointer |
| clip region state | Backend instance | `push_clip` | `pop_clip` / destroy | Owned (Region) | Soft-only fallback; pop restores old region |
| software clip state | Backend instance | `push_clip` | `pop_clip` | Owned fields | Cleared on pop |
| logger state | Process (optional) | `clv_log_init` | `clv_log_shutdown` | Global seq/active | Open/write/close per line; no stuck handle |
| scroller gadget / glist | Demo | CreateContext/CreateGadget | FreeGadgets | Owned by demo | Recreated on resize; fail paths FreeGadgets |
| VisualInfo / DrawInfo | Demo | GetVisualInfo / GetScreenDrawInfo | FreeVisualInfo / FreeScreenDrawInfo | Owned by demo | Fail paths release |
| resize scratch | Stack / demo locals | `demo_handle_newsize` | scope end | Transient | — |
| smart-scroll temps | Backend stack | `move_viewport_pixels` | unlock/hook restore before return | Transient | Early exits always unlock/restore hook |

#### Borrowed-data contract

- Columns, rows, cell strings, font: **borrowed**; control never frees them.
- Caller must keep them valid while attached and across any paint/input call.
- Caches invalidate on `set_columns` / `set_rows` /
  `set_cell_padding` / `set_row_gap` (`layout_valid=FALSE`).
- Rebuild runs on next `set_bounds` or `render` when invalid.
- Successful transactional `set_bounds` rebuilds wrap + row layout immediately.
- Pens are copied by value into the control.

#### Create / failure / destroy findings

| Path | Result |
|------|--------|
| `clv_control_create` | Single malloc; memset; no partial public object |
| `clv_control_destroy` | NULL-safe; frees all owned caches; does **not** destroy backend |
| `set_columns` / `set_rows` | Validate; replace pointers; invalidate (no alloc) |
| `set_bounds` fail | Restores prior outer bounds; transactional restore of caches when prior layout was valid; fresh first-layout fail → clean invalid |
| wrap/layout prepare fail | Frees partial new state; prior snapshot restored when applicable |
| Backend destroy | `pop_clip` then free; safe if clip inactive |
| Demo fail_* labels | Control → backend → window → gadgets → dri/vi/screen → log_shutdown |
| Destroy idempotency | NULL destroy is safe; double-destroy of same pointer is not (caller must null) |
| Logger | No persistent DOS handle between lines |

No known leaks on normal success paths. Partial create never returns a live control.

#### Backend state-safety findings

| Area | Verdict |
|------|---------|
| HW clip push/pop | Single slot; nested push auto-pops prior; destroy pops |
| Soft / soft_only | Active for LAYERUPDATING and HW-clip failure fallbacks |
| Failed clip push returning TRUE | Now always activates soft_only (fixed) |
| Refresh | Never `InstallClipRegion` while `LAYERUPDATING` |
| Smart scroll | Refuses if clip active / LAYERUPDATING / pre-damaged; LockLayerInfo always unlocked; layer hook always restored; finish_viewport_move acknowledges V37 damage |
| Smart during resize/refresh | Not used — resize/full refresh use full paint |
| Scroller Top (V37) | Message `Code` only; no `GT_GetGadgetAttrs` |

#### Multiple-instance result

| Item | Result |
|------|--------|
| Preferred dual-control demo | Not implemented (acceptable: static audit) |
| Per-instance state | Bounds, layout, wraps, selection, scroll, pens, metrics, draw_context, clip, smart-scroll locals, scroller (demo) — all instance-local |
| Mutable globals in `clv_control_*.c` | None |
| Mutable globals in backend | None (const `g_clv_v36_ops` only) |
| Optional logger globals | `g_clv_log_active`, `g_clv_log_seq` — process-wide; do not affect correctness |
| Shared RastPort caution | Two controls painting the same RP must not nest paints across instances (single clip slot per backend). Sequential paints are safe |
| Severity of dual-control untested | Low — architecture supports it; runtime not exercised |

#### Formal WB2.x validation matrix

| Category | Case | Result |
|----------|------|--------|
| Creation | Initial render; empty/short/tall lists; fixed header; fixed pixel columns; min width | Pass |
| Selection | Unwrapped/wrapped; tall↔short; non-selectable; gap; partial row; make_visible | Pass |
| Scrolling | Arrows; slow repeat; proportional; trough/page; large-jump fallback; clamp; smart on/off; selection enter/leave | Pass |
| Refresh | Cover/uncover; scroll while covered; uncover after scroll; no blank bands / outside text / header bleed / frame corruption | Pass |
| Resize | Wider/narrower; taller/shorter; min width; scrolled; wrapped selection; after smart scroll; scroller resync; no stale pixels | Pass (Task 1) |
| Shutdown | Normal close; open/close stability | Pass |
| Scroller API | Code→Top; no `80000004`; no V39 Top query | Pass |

#### Formal WB3.2 validation matrix

Same matrix as WB2.x: **Pass**. Differences: gfx ≥ 39 uses `ScrollRasterBF` + `LAYERS_NOBACKFILL`; V37 uses `ScrollRaster` + `finish_viewport_move` damage ack. Soft-only refresh path still required and validated on both.

#### 68000 build validation

| Item | Value |
|------|-------|
| Compiler | VBCC `vc` |
| CPU flags | `-cpu=68000` (CFLAGS and LDFLAGS) |
| Optimisation | `-O2 -size` |
| Target | `+aos68k` |
| Link | `-final -lamiga -lauto` |
| 68020+ | Not enabled |
| Runtime libs | intuition / gadtools / graphics / layers — V36+ as documented |
| Smart-scroll path | gfx ver ≥ 39 → ScrollRasterBF; else ScrollRaster |
| Host cross-link | `make custom-control-demo`, `-log`, smart-off, `size-draw-basic` — success |
| 68000 emulator run of *this* closure build | Not re-run in Task 2; prior Phase 5 Amiga sessions used the same 68000 profile |

#### Final size measurements

| Target | Bytes | Notes |
|--------|------:|-------|
| `bin/custom-control-demo` (smart on) | 30476 | Final Phase 5 normal |
| `bin/custom-control-demo-nosmart` | 29232 | `CLV_ENABLE_SMART_SCROLL=0` |
| `bin/custom-control-demo-log` | 44428 | Logger + instrumented sites |
| `bin/size-draw-basic` (v1) | 27852 | Unchanged |

| Object (smart on) | Bytes |
|-------------------|------:|
| `clv_control.o` | 2448 |
| `clv_control_layout.o` | 3772 |
| `clv_control_wrap.o` | 3832 |
| `clv_control_render.o` | 7808 |
| `clv_control_input.o` | 2256 |
| `clv_control_scroll.o` | 704 |
| `clv_backend_amiga_v36.o` | 4920 |

| Baseline comparison | Bytes / delta |
|---------------------|---------------|
| Phase 4 demo | 23580 |
| Incremental-render demo | 25764 |
| Smart-scroll demo (pre-resize) | 26996 |
| Resize hardening (pre last-col/soft fix) | 29872 |
| Final normal | 30476 (+5496 vs Phase 4; +3480 vs smart-scroll-only) |
| Smart-scroll cost (final on − off) | 30476 − 29232 = **+1244** |
| Logger cost (log − normal) | 44428 − 30476 = **+13952** |
| v1 `size-draw-basic` | 27852 (custom normal is +2624) |

Source files (control tree): 7 core `.c` + 1 backend `.c` + optional `clv_control_log.c`.

Typical allocations per live control after successful layout (demo shape):
1 control + 1 backend + `col_geom` + `divider_x` + `layout_rows` +
`cell_wraps` array + one frag block per non-empty wrapped cell.

#### Known limitations register

| Limitation | Severity | Affected | Workaround | Deferred to | Class |
|------------|----------|----------|------------|-------------|-------|
| Soft-only skips partially overhanging edge glyphs | Low | WB2.x refresh especially | None needed (correctness) | — | Accepted design |
| Large/page jumps full-repaint | Low | All | Smart scroll for small deltas | Phase 6 opt | Accepted design |
| Selection + make_visible full viewport | Low | All | Correct highlight | Phase 6 opt | Accepted design |
| No horizontal scrolling | Medium | Narrow content wider than min | Min window width | Future | Deferred feature |
| Fixed pixel columns (last grows only) | Low | Widen | Configure widths | Future | Accepted design |
| Min horizontal window size | Low | Demo | WindowLimits | — | Accepted design |
| No WB 1.3 backend | Medium | KS 1.3 | Out of scope | Separate project | Deferred feature |
| No keyboard navigation | — | — | Delivered in Phase 5.5 | — | Resolved — see Phase 5.5 Completion Record |
| Further keyboard polish (Home/End, dual-control runtime demo) | Low | All | Cursor/Shift/Ctrl/Return | Future | Deferred polish |
| No multi-selection | Low | All | — | Future | Deferred feature |
| Fragment cap 32/cell | Low | Extreme wrap | Cap truncates | Future | Accepted design |
| Dual-control runtime untested | Low | Multi-instance apps | Sequential paint; static audit OK | Optional demo | Untested behaviour |
| SIZEVERIFY without NEWSIZE leaves gadgets detached | Low | Rare cancel | Next NEWSIZE | — | Accepted design |
| First layout non-transactional | Low | First set_bounds OOM | Leaves clean invalid | — | Accepted design |
| GadTools scroller recreate on resize | Low | Demo | Template rebuild | — | Accepted design |
| 7 MHz qualitative smart vs full not separately timed | Info | Low-end CPU | Optional | Phase 6 notes | Untested behaviour |

#### Deviations from this design

- Partial selection + smart scroll delivered in Phase 5 (were optional).
- Dual-control interactive demo not built; multi-instance supported by audit only.
- Last-column grow-only fill added (still fixed minima for other columns).

#### Clean-room identifier audit (closure)

| New identifier | Kind | Purpose | Original to CLV control |
|----------------|------|---------|-------------------------|
| `clv_v36_activate_soft_only` | static fn | Soft-only clip activation helper | Yes |

No private names from another ListView implementation. No copied comments.
Public AmigaOS API names overlap by necessity only.

#### Exit criteria evaluation

| Criterion | Met? |
|-----------|------|
| No known leaks on normal paths | Yes |
| Failure paths leave valid or clean state | Yes |
| Refresh correct | Yes (WB2.x + WB3.2) |
| Resize correct | Yes (WB2.x + WB3.2) |
| WB2.x runtime matrix complete | Yes |
| WB3.2 runtime matrix complete | Yes |
| 68000 build validated | Yes (profile + host link) |
| Remaining issues documented | Yes |
| Multiple-instance result known | Yes (static audit) |
| Phase 6 can compare fairly | Yes |

#### Instructions for Phase 5.5 (inserted before Phase 6)

~~Keyboard navigation was deferred at Phase 5 close.~~ **Done** — see
**Phase 5.5 Completion Record**. Phase 6 is **NEXT**.

**Phase 5 baselines remain the pre-keyboard reference** in the Phase 6
table. Phase 5.5 post-keyboard sizes are recorded in the Completion Record.

---

## Phase 5.5 — Keyboard Navigation and Deterministic Exercise API

**Status:** **Complete** (implementation + host/VBCC size; Amiga interactive
checklist in demo README)  
**Plan:** [CLV_CUSTOM_CONTROL_KEYBOARD_NAVIGATION_PLAN.md](CLV_CUSTOM_CONTROL_KEYBOARD_NAVIGATION_PLAN.md)  
**Date:** `2026-07-27`

### Objective

Add classic-Amiga keyboard selection and page/first/last navigation to the
experimental custom control, plus a neutral deterministic exercise path that
Phase 6 can use for profiling—without synthesising raw-key messages or
changing the CLV v1 public API.

### Scope (delivered)

- Extended experimental `CLV_InputType` with `NAV_*` and `CLV_EVENT_ACTIVATED`.
- Navigation in `clv_control_handle_input`, sharing selectability and
  `make_visible` with mouse selection; selection-centric page step =
  `max(vp_h - line_h, line_h)`.
- Optional keyboard enable/disable on the control (**enabled by default**):
  `clv_control_set_keyboard_enabled` / `get_keyboard_enabled`, or create with
  `CLV_CTRL_CFG_NO_KEYBOARD`. When disabled, `CLV_INPUT_NAV_*` is ignored;
  mouse select and scroll inputs are unchanged.
- Demo: `IDCMP_RAWKEY`; translate with `CURSORUP` / `CURSORDOWN` / Return
  `0x44`; application-owned `active_control` (no globals); Return activate;
  Space unused; `EXERCISE` CLI calling the same `handle_input` path;
  `NOKEYBOARD` CLI to disable NAV_* at startup.
- Reused Phase 5 partial / smart / full paint rules and scroller sync.
- Size deltas vs Phase 5 baselines recorded below.

### Non-goals (unchanged)

- Phase 6 general optimisation or V2 recommendation work
- CLV v1 API or v1 example changes
- Home / End / Page Up / Page Down keys (not on classic Amiga keyboards)
- Space activation; `IDCMP_INTUITICKS` repeat engine
- Icons, sorting, horizontal scrolling, multi-selection, inline editing, DnD
- Workbench 1.3 backend; synthetic RAWKEY injection
- Global mutable active-control state; `clv_control_navigate()` parallel API

### Phase 5.5 Completion Record

**Status:** Complete (host cross-link + size); Amiga WB interactive checklist
pending in demo README  
**Completion commit:** pending commit  
**Date:** `2026-07-27`

#### Implemented

- `CLV_INPUT_NAV_PREV` / `NEXT` / `PAGE_UP` / `PAGE_DOWN` / `FIRST` / `LAST` /
  `ACTIVATE` on the experimental control API
- `CLV_EVENT_ACTIVATED`; `clv_control_get_selected`
- Keyboard NAV_* enable/disable (**default enabled**):
  `clv_control_set_keyboard_enabled` / `clv_control_get_keyboard_enabled`;
  create-time `CLV_CTRL_CFG_NO_KEYBOARD` in `CLV_ControlConfig.flags`
- Internal `clv_ctrl_find_selectable` / page-target helpers in
  `clv_control_input.c`
- Demo RAWKEY translate, click-to-focus `active_control`, Return activate
  print (no repaint), `EXERCISE` CLI sequence, `NOKEYBOARD` CLI

#### Files added or changed

- `src/custom_listview_control/clv_control.h` — NAV_* / ACTIVATED /
  get_selected / keyboard enable API + `CLV_CTRL_CFG_NO_KEYBOARD`
- `src/custom_listview_control/clv_control_internal.h` — `keyboard_enabled`
- `src/custom_listview_control/clv_control.c` — default-on create; honour
  `CLV_CTRL_CFG_NO_KEYBOARD`
- `src/custom_listview_control/clv_control_input.c` — navigation handlers;
  NAV_* gated when keyboard disabled
- `examples/custom_control_demo/main.c` — RAWKEY, focus, EXERCISE,
  NOKEYBOARD, paint policy
- `examples/custom_control_demo/README.md` — keys, EXERCISE, NOKEYBOARD,
  sizes, checklist
- This document — Phase 5.5 completion + Phase 6 handoff
- `docs/CLV_CUSTOM_CONTROL_KEYBOARD_NAVIGATION_PLAN.md` — status → implemented

#### Design decisions

- Decision: Extend `CLV_InputEvent` / `handle_input`; no `clv_control_navigate()`.
- Reason: Exercise mode and keyboard share one authoritative path.
- Consequence: Phase 6 can drive `EXERCISE` without synthesising RAWKEY.

- Decision: Raw-key translation stays in the demo.
- Reason: Matches Phase 4 IDCMP-in-demo architecture; core stays IntuiMessage-free.
- Consequence: Other apps map their own keys to `CLV_INPUT_NAV_*`.

- Decision: App-owned `active_control` pointer; not stored on `CLV_Control`.
- Reason: Focus is window/application policy; no hidden globals.
- Consequence: Single-control demo implements the pattern; dual-control
  routing is documented for a future optional smoke demo.

- Decision: Keyboard NAV_* is optional and **enabled by default**.
- Reason: Apps that only need mouse/scroller must be able to omit keyboard
  behaviour without a parallel navigate API.
- Consequence: `set_keyboard_enabled(FALSE)` or `CLV_CTRL_CFG_NO_KEYBOARD`
  ignores all `CLV_INPUT_NAV_*`; mouse and scroll inputs unchanged. Demo
  exposes `NOKEYBOARD` CLI.
#### Tests performed

- Command: `make clean && make custom-control-demo` (and log / smart-off)
- Environment: Host VBCC `+aos68k -cpu=68000 -O2 -size -final`
- Result: Linked successfully; sizes below
- Amiga WB2.x / 3.2 keyboard matrix: see demo README checklist (manual)

#### Size measurements

| Target | Phase 5 | Phase 5.5 | Delta |
|--------|--------:|----------:|------:|
| `custom-control-demo` (smart on) | 30476 | 33032 | +2556 |
| `custom-control-demo-nosmart` | 29232 | 31788 | +2556 |
| `custom-control-demo-log` | 44428 | 47148 | +2720 |
| `size-draw-basic` (v1) | 27852 | 27852 | 0 |

#### Known limitations

- Amiga interactive keyboard / EXERCISE validation not run on this agent host
  (checklist left open in demo README).
- Dual-control runtime demo still optional (focus pattern implemented;
  single control only).
- Home / End / PgUp / PgDn and Space activation remain out of scope.

#### Deviations from the keyboard plan

- Focus hit-test uses the outer control rectangle (header + viewport) from
  demo geometry rather than a new public viewport getter — still transfers
  focus on control clicks including heading/gap.
- Additive after the plan: optional keyboard enable/disable (default on)
  via `clv_control_set_keyboard_enabled` / `CLV_CTRL_CFG_NO_KEYBOARD` and
  demo `NOKEYBOARD`. Not in the original plan; does not change default
  behaviour.

#### Instructions for Phase 6

- Baselines include keyboard/exercise code size (table below).
- Drive `custom-control-demo EXERCISE` for optional timing (smart on/off).
- Do **not** re-implement keyboard; further polish only if evidence-backed
  and tiny.
- Still no application migration; still no v1 removal.

### Phase 6 handoff (after 5.5)

- Baselines include keyboard/exercise code size.
- Phase 6 may drive `EXERCISE` for timing (smart on/off).
- Still no application migration; still no v1 removal.
- Further keyboard polish is out of Phase 6 unless evidence-backed and tiny.

---

## Phase 6 — Optimisation, Size Comparison, and V2 Recommendation

**Prerequisite:** Phase 5.5 complete. **Status:** **NEXT**.

### Objective

Measure the mature experiment against the existing GadTools-backed drawn ListView and determine the correct long-term direction.

### Required analysis

Compare:

- executable size;
- CLV object size;
- source complexity;
- module count;
- allocation count and ownership complexity;
- rendering correctness;
- wrapped-row selection quality;
- fixed-header behaviour;
- WB2.x support;
- maintainability;
- future Workbench 1.3 portability;
- functionality still missing from the existing backend.

### Optimisation work

Only evidence-backed optimisations should be implemented, such as:

- removing duplicate temporary structures;
- consolidating tiny modules where clearer;
- eliminating avoidable allocations;
- reducing redraw regions;
- optional ScrollRaster use;
- separating optional features into omit-at-link modules.

### Required final recommendation

The phase must recommend one of:

1. Abandon the custom control experiment.
2. Keep it as an optional second backend.
3. Adopt it as the preferred CLV v2 direction while retaining v1.
4. Replace the existing backend after a separate migration project.

### Deliverables

- Final comparison report.
- Before/after size table.
- Architecture decision record.
- Recommended next branch or release plan.
- Updated Phase 6 section and final document status.

### Exit criteria

- The experiment has an evidence-based conclusion.
- No migration is performed implicitly.
- Any future Workbench 1.3 work is scoped as a separate project using the established backend boundary.

### Instructions for Phase 6 (after Phase 5.5)

Perform **only**:

1. Optimisation candidates with evidence.
2. Size / complexity comparison against v1.
3. Architecture assessment.
4. One of the four V2 recommendations from this section.
5. Optional: drive the Phase 5.5 `EXERCISE` path for timing (smart on/off).

Do **not**: migrate apps, remove v1, add sorting/icons/WB1.3 backend, or
weaken WB2.x clip/scroller fixes. Do **not** re-implement keyboard (that is
Phase 5.5). Further keyboard polish only if evidence-backed and tiny.

**Baselines for Phase 6:**

| Item | Value |
|------|-------|
| Phase 5.5 custom-control normal (post-keyboard) | **33032** bytes |
| Phase 5.5 smart-off | **31788** bytes |
| Phase 5.5 logging | **47148** bytes |
| Phase 5 final (pre-keyboard reference) | 30476 / 29232 / 44428 |
| v1 comparison (`size-draw-basic`) | **27852** bytes |
| Object modules | `clv_control*.o` (6) + `clv_backend_amiga_v36.o` + `clv_platform.o` (+ optional log) |
| OS | Workbench 2.x (V37) and 3.2 (Phase 5 validated; Phase 5.5 keyboard checklist in demo README) |
| CPU | 68000 (`-cpu=68000`); smart path selects ScrollRasterBF when gfx ≥ 39 |
| Proven features | Fixed header; wrap; logical selection; regional paint; smart scroll; resize/relayout; dual clip; V37 scroller Code→Top; keyboard NAV_* (default on; optional disable) + EXERCISE |
| Exercise API | `custom-control-demo EXERCISE` (neutral `handle_input`; no RAWKEY inject); `NOKEYBOARD` disables NAV_* |
| Known limitations | See Phase 5 register (keyboard resolved in 5.5); dual-control runtime still optional |

---

## 17. Phase Workflow and Agent Handoff Rules

Every phase agent must follow this sequence:

1. Read this entire document.
2. Read the previous phase’s completion notes and referenced reports.
3. Inspect the repository and relevant existing code.
4. Write a phase-specific investigation and implementation plan.
5. Implement only the current phase scope.
6. Build and test the affected targets.
7. Record failures, compromises, and unresolved issues honestly.
8. Update this document before finishing.
9. Mark the current phase complete only when exit criteria are met.
10. Mark the next phase as `NEXT`.

### Required document updates per phase

Each phase must add or update:

- status in the Phase Status Summary;
- completion commit hash;
- implementation summary;
- files added/changed;
- design decisions made;
- tests run and results;
- executable/object size measurements;
- known limitations;
- deviations from the original design;
- exact instructions for the next phase.

### Forbidden handoff behaviour

A phase agent must not:

- silently change the overall architecture;
- implement large parts of later phases without documenting them;
- remove the existing CLV backend;
- declare Workbench 1.3 support without runtime evidence;
- hide build failures;
- leave the current phase status ambiguous;
- rely on unstated private knowledge for the next agent.

---

## 18. Phase Completion Record Template

Each completed phase should append a record using this form:

```markdown
### Phase N Completion Record

**Status:** Complete / Partial / Blocked  
**Completion commit:** `<hash>`  
**Date:** `YYYY-MM-DD`

#### Implemented

- ...

#### Files added or changed

- `path/file.c` — purpose

#### Design decisions

- Decision:
- Reason:
- Consequence:

#### Tests performed

- Command:
- Environment:
- Result:

#### Size measurements

| Target | Before | After | Delta |
|--------|-------:|------:|------:|
| ... | ... | ... | ... |

#### Known limitations

- ...

#### Deviations from this design

- None, or explain each deviation.

#### Instructions for Phase N+1

- ...
```

---

## 19. Initial Risks

| Risk | Mitigation |
|------|------------|
| Rebuilding too much of GadTools | Continue using a standard scroller initially |
| Duplicating the current renderer | Phase 1: new thin renderer; adapt ideas only; no v1 draw-path link |
| Over-engineering for Workbench 1.3 | Preserve interfaces only; defer backend code |
| Increased executable size | Track size each phase and retain omit-at-link modules |
| Refresh/layer bugs | Full redraw first; optimise only after hardening |
| Font assumptions | Derive all metrics and widths from actual font APIs |
| Excessive allocations | Cache row layout and measure allocation count |
| API churn | Keep API experimental and control-specific |
| Agent phase drift | Mandatory status and handoff updates in this document |

---

## 20. Current Handoff

**Current phase:** Phase 6 — Optimisation, size comparison, and V2
recommendation  
**Next agent’s job:** Perform Phase 6 only — evidence-backed optimisation
candidates, size/complexity comparison vs v1, architecture assessment, and
one of the four V2 recommendations. Optional: drive `EXERCISE` for timing
(smart on/off). Do **not** migrate applications, remove v1, add
sorting/icons/WB1.3 backend, or re-implement keyboard.  
**Implementation permission:** Optimisation and analysis only within Phase 6
scope. Preserve dual clip + soft_only strict text; do not reintroduce
`InstallClipRegion` during `LAYERUPDATING` or `GT_GetGadgetAttrs` for V37
scroller Top.  
**Branch:** `experiment/clv-custom-control`  
**Required reading:** this document (**Phase 5 Completion Record**, **Phase
5.5 Completion Record**, Phase 6 section),
[keyboard nav plan](CLV_CUSTOM_CONTROL_KEYBOARD_NAVIGATION_PLAN.md),
and `src/custom_listview_control/`.  
**Existing skeleton:** Hardened Phase 5 control + Phase 5.5 keyboard NAV_*
(default enabled; `set_keyboard_enabled` / `CLV_CTRL_CFG_NO_KEYBOARD` /
demo `NOKEYBOARD`) / `EXERCISE` / app-owned focus; mouse select +
line/page/position scroll via neutral `CLV_InputEvent`.  
**Size baseline (Phase 5.5):** `bin/custom-control-demo` = **33032** bytes;
`bin/custom-control-demo-nosmart` = **31788**;
`bin/custom-control-demo-log` = **47148**; comparable v1
`bin/size-draw-basic` = **27852**. Pre-keyboard Phase 5 reference: 30476 /
29232 / 44428.
