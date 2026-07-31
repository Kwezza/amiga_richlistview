# CLV Custom Control — Phase 1 Architecture Audit

**Status:** Complete  
**Date:** 2026-07-23  
**Branch:** `experiment/clv-custom-control`  
**Living roadmap:** [CLV_CUSTOM_CONTROL_DESIGN_AND_IMPLEMENTATION_PLAN.md](CLV_CUSTOM_CONTROL_DESIGN_AND_IMPLEMENTATION_PLAN.md)  
**Scope:** Architecture and reusable-code audit only. No `src/custom_listview_control/` implementation.

---

## 1. Executive findings (Phase 1 questions)

### Q1. Which existing modules can be reused without GadTools ListView semantics?

| Reusable now (include / link) | Notes |
|-------------------------------|-------|
| `clv_platform.h` | Amiga target policy only |
| `clv_platform.c` + `clv_platform_internal.h` | Neutral malloc/free/strdup |
| `clv_compiler.h`, `clv_sdk_compat.h` | Toolchain / SDK fallbacks |
| `clv_types.h` (`CLV_CellAlign`, `CLV_PixelColumn`) | Low-level; no ListView hooks |
| `clv_log.h` | Header-only; optional |

Divider-width helpers such as `clv_renderer_line_style_width()` / `clv_renderer_apply_divider_reserve()` are **conceptually** reusable but live in the GadTools renderer TU and take ListView-relative coordinates. Phase 2 must reimplement equivalent reserve logic inside the control (do not link `clv_renderer_columns.o` into the control core).

### Q2. Which structures reuse, adapt, or avoid?

| Structure | Verdict |
|-----------|---------|
| `CLV_PixelColumn`, `CLV_CellAlign` | **Reuse** (include `clv_types.h`) |
| `CLV_LineStyle` enum values (NONE/SINGLE/RECESSED/RAISED) | **Adapt** — redefine or share a small control-local enum with the same numeric meaning; do not pull `clv_renderer.h` for this alone |
| `CLV_Pens` / semantic pen roles (design doc) | **New** in control headers; v36 backend fills from `DrawInfo` |
| `CLV_DrawOps` | **New** — core draw boundary |
| `CLV_RowLayout` | **New** — variable-height logical geometry |
| `CLV_PreparedList`, `CLV_RenderNode`, `CLV_RenderFragment`, `CLV_RenderMap` | **Avoid** as control primary model (physical GadTools lines) |
| `CLV_SelectionState` / `clv_handle_selection` | **Avoid** (`GTLV_Selected` adapter) |
| `CLV_Renderer` / Hook | **Avoid** (`GTLV_CallBack` / `LV_DRAW`) |
| ASCII justified `struct List` / `ln_Name` formatters | **Avoid** for control viewport |

### Q3. Can the current renderer draw arbitrary row rectangles?

**No.** A new thin control renderer is required.

Evidence:

- Draw entry is `LV_DRAW` with `struct LVDrawMsg` (`clv_renderer_core.c`); bounds come from GadTools per physical item.
- Prepared wrap expands one logical row into **multiple** `CLV_RenderNode`s (fixed item height).
- Header/separator are **physical list rows** (`CLV_MAP_HEADER`), not a fixed rectangle above a viewport.
- Optional ops take `struct RastPort *` directly, not a neutral draw-ops table.

Do **not** call `clv_listview_*` prepare/draw against custom bounds. Optional shared extraction is deferred until Phase 6 proves the architecture.

### Q4. Where should text measurement and wrapping live?

| Concern | Location |
|---------|----------|
| Font metrics (`tf_YSize`, baseline) | Backend → `CLV_FontMetrics` on control |
| `TextFit` / `TextLength` / `TextExtent` | Backend `CLV_DrawOps` (or measurement ops on the same context) |
| Pixel wrap algorithm | Control-owned prepare path (Phase 3). **Copy/adapt** ideas from `clv_char_wrap_cell_pixel()` in `clv_pixel_wrap.c`; do **not** move or rewrite v1 wrap until Phase 6 |
| Layout cache | `CLV_RowLayout[]` owned by control; rebuild on bounds/font/columns/rows/gap change |
| Draw time | No heap allocation; draw cached fragments only |

Phase 2 uses fixed one-line row heights (no wrap). Phase 3 adds wrap + variable height.

### Q5. Smallest viable public experimental API?

See [§5 Proposed API](#5-proposed-publicprivate-api). Control-specific names only (`clv_control_*` / `CLV_Control*`). Do not extend or repurpose v1 public headers.

Phase 2 minimum surface: `create` / `destroy` / `set_columns` / `set_rows` / `set_bounds` / `render`. Selection and input APIs may exist as stubs or be added in Phase 4; preferred: declare full signatures now, implement selection/scroll handlers in Phase 4.

### Q6. Ownership model?

See [§4 Ownership table](#4-ownershiplifetime-table). Summary:

- Caller **borrows** column titles and row cell strings.
- Control **owns** layout cache and prepared wrap fragments.
- Font is **borrowed** (never closed by control in v1 experiment).
- Backend owns draw context + scroller gadget; core holds ops pointer + `APTR`.

### Q7. Build / example comparison targets?

| Purpose | Target |
|---------|--------|
| Functional demo (Phase 2+) | `examples/custom_control_demo/` → e.g. `bin/custom-control-demo` |
| Size baseline (Phase 2 / 6) | `bin/size-draw-basic` (27928 bytes, VBCC `-O2 -size -final`, 2026-07-22) |
| Visual reference (fixed columns) | `examples/05_draw_basic/` / `bin/draw-basic` |
| Visual reference (wrap problem) | `examples/06_draw_wrapped/` (later phases) |
| Size discipline | Same compiler/flags as [CLV_SIZE_REPORT.md](CLV_SIZE_REPORT.md); do not compare dissimilar shells |

Root CLV v1 profiles remain unchanged. New demo gets its own Makefile rules; does not use `CLV_DRAW_BASIC_LIBS`.

### Q8. What APIs are safe on Workbench 2.x?

Safe for the v36 backend (Kickstart 2.04+ / library V36+):

- Graphics: `RectFill`, `Move`/`Draw`, `Text`, `TextLength`, `TextFit`, `TextExtent`, `SetAPen`/`SetBPen`/`SetDrMd`, RastPort clip (`rp_Layer` / region APIs as used by templates)
- Intuition: window RastPort, `DrawInfo` pens (`TEXTPEN`, `BACKGROUNDPEN`, `FILLPEN`, `SHINEPEN`, `SHADOWPEN`, …)
- GadTools: `SCROLLER_KIND` (or proportional gadget) for scrollbar only — **not** `LISTVIEW_KIND` for the viewport

Avoid in core: `GTLV_*`, `LV_DRAW`, Reaction/MUI, AmigaOS4-only APIs. Prefer runtime library-version checks over compile-time OS guesses (`clv_platform.h` policy).

### Q9. Abstractions now for a future WB 1.3 backend?

Required now (interfaces only; no v34 code):

- `CLV_DrawOps` (+ measurement) with opaque `APTR` context
- `CLV_Pens` semantic roles (not raw `DrawInfo` in core)
- Neutral `CLV_InputEvent` (no `IntuiMessage` in layout/render)
- Backend translation of scroller ↔ `scroll_y`
- No GadTools ListView types in `CLV_Control` state

Defer: `clv_backend_amiga_v34.*` implementation.

### Q10. What remains deferred?

Everything in design plan §3.2, plus until later phases:

- Word wrapping / variable height (Phase 3)
- Selection, hit-testing, scrollbar interaction (Phase 4)
- Resize/refresh hardening, WB2.04 validation (Phase 5)
- Optimisation, size comparison conclusion, V2 merge recommendation (Phase 6)
- Moving/deduplicating v1 renderer code into shared modules
- Icons, styles, details rows, sorting, clickable headers

---

## 2. Reuse matrix

| Module / symbol | Verdict | Link into control? | Notes |
|-----------------|---------|--------------------|-------|
| `clv_types.h` | **Reuse** | Header only | `CLV_PixelColumn`, `CLV_CellAlign` |
| `clv_platform.h` | **Reuse** | Header only | Assert Amiga |
| `clv_platform.c` | **Reuse** | Yes | Alloc |
| `clv_compiler.h` / `clv_sdk_compat.h` | **Reuse** | Header only | As needed |
| `clv_log.h` | **Reuse** | Header only | Optional diagnostics |
| `clv_path.c` | **Defer** | No (Phase 2–4) | Path wrap mode later |
| `clv_ascii_formatter.c` | **Avoid** | No | `GTLV_Labels` lists |
| `clv_ascii_columns.c` | **Avoid** (core) | No | Char columns + `CLV_LISTVIEW_SCROLLBAR_BORDER` bridge; demo may compute widths manually |
| `clv_columns.c` / `clv_sort.c` / `clv_cell_tracking.c` | **Avoid** | No | ASCII / sort / click-detect |
| `clv_char_wrap.c` | **Avoid** | No | Character wrap + physical nodes |
| `clv_renderer_core.c` | **Avoid** | No | `LV_DRAW` hook + prepare |
| `clv_renderer_columns.c` | **Adapt ideas** | No | Reimplement divider reserve / 3D lines in control render |
| `clv_renderer_ops.c` / `clv_bind_*.c` | **Mirror pattern** | No | Control may later use its own ops bind table for icons/styles |
| `clv_renderer_setup.c` | **Adapt ideas** | No | Clip/intersect/geometry helpers — rewrite against viewport rects |
| `clv_pixel_wrap.c` | **Adapt later** | No until Phase 3 evidence | Algorithm reference; TU tied to `CLV_LvTempFrag` + renderer ops |
| `clv_prepared_display_map.c` | **Avoid** | No | Physical↔logical maps |
| `clv_selection.c` | **Avoid** | No | `GTLV_Selected` |
| `clv_icons.c` / `clv_styles.c` | **Defer** | No | Optional; Phase 5+ if needed |
| `clv_details*.c` | **Avoid** | No | GadTools prepared details |
| `clv_config.h` / `CLV_HAS_*` | **Do not reuse** for control | Separate feature flags if needed later | v1 profile system |

---

## 3. Structure verdicts (detail)

### 3.1 Keep using

- **`CLV_PixelColumn`:** left/right/text_left/text_right/alignment/flags — suitable for control column geometry once coordinates are relative to the control content origin (not `lvdm_Bounds.MinX`). Document that control columns use the same field meanings with control-local origin.
- **`CLV_CellAlign`:** reuse enum values.

### 3.2 Do not use as primary model

- **`CLV_RenderNode`:** one physical GadTools line; wrap = multiple nodes.
- **`CLV_RenderMap` / display maps:** map physical selection; obsolete for logical-row selection.
- **`CLV_PreparedList`:** owns Exec list of physical nodes for `GTLV_Labels`.
- **Header-as-row:** `header_logical_index` / `CLV_MAP_HEADER` — replaced by fixed `header_bounds`.

### 3.3 Patterns to mirror (not call)

- Prepare-time measure, draw-time cache (no alloc in paint).
- Omit-at-link optional features via an ops table (icons/styles/wrap) when those features are added to the control.
- Two-pen recessed/raised dividers using shine/shadow pens.
- Explicit ownership and detach-before-free discipline (adapted: no `GTLV_Labels`; free layout before destroy; backend frees scroller after window teardown order from templates).

---

## 4. Ownership/lifetime table

| Object | Allocator | Owner | Valid until | Free / close |
|--------|-----------|-------|-------------|--------------|
| `CLV_Control` | `clv_control_create` | Caller | Destroy | `clv_control_destroy` |
| Column descriptors (`CLV_ControlColumn[]`) | Caller | Caller (borrowed) | Next `set_columns` or destroy | Caller |
| Column title strings | Caller | Caller (borrowed) | Same | Caller |
| Row descriptors (`CLV_ControlRow[]`) | Caller | Caller (borrowed) | Next `set_rows` or destroy | Caller |
| Cell text strings | Caller | Caller (borrowed) | Same | Caller |
| `CLV_RowLayout[]` | Control | Control | Relayout or destroy | Control |
| Wrap fragment caches | Control | Control | Relayout or destroy | Control |
| `struct TextFont *` | Caller / screen | Caller (borrowed) | Control destroy or explicit font change API | **Never** closed by control |
| `CLV_Pens` values | Backend fill | Stored in control | Pen refresh / destroy | N/A (values) |
| `CLV_DrawOps` table | Static / backend | Backend | Control lifetime | N/A |
| Draw context (`APTR`) | Backend | Backend | Destroy | Backend |
| Scroller gadget | Backend / app | Backend or app (document) | Window close | Per template cleanup |
| Window / VisualInfo / DrawInfo | Application | Application | App teardown | Templates |

**Cleanup order (control experiment):**

1. Stop delivering input/render to the control.
2. Destroy control (frees owned layout/fragments).
3. Dispose scroller / gadgets / window per `templates/AI_AGENT_GETTING_STARTED.md`.
4. Free VisualInfo, DrawInfo, screen locks, ports.
5. Backend draw context freed with or before control destroy (document pairing in Phase 2).

Partial create failure must free any owned allocations already made.

---

## 5. Proposed public/private API

Canonical signatures for Phase 2+. Experimental; not the final CLV v2 API.

### 5.1 Public header — `clv_control.h`

```c
/* Experimental custom-drawn ListView control (not GadTools LISTVIEW_KIND). */

typedef struct CLV_Control CLV_Control;

typedef struct CLV_ControlConfig
{
    const struct CLV_DrawOps *draw_ops; /* required */
    APTR draw_context;                  /* opaque; owned by backend */
    struct TextFont *font;              /* borrowed; NULL = use ops default font */
    UWORD row_gap;                      /* pixels between logical rows */
    UWORD flags;                        /* reserved; 0 for Phase 2 */
} CLV_ControlConfig;

typedef struct CLV_ControlColumn
{
    CONST_STRPTR title;   /* borrowed */
    WORD width_pixels;    /* content width; dividers laid out separately */
    UWORD alignment;      /* CLV_CellAlign */
    UWORD wrap_mode;      /* unused until Phase 3; 0 = none */
    UWORD flags;
} CLV_ControlColumn;

typedef struct CLV_ControlRow
{
    CONST_STRPTR *cells;  /* borrowed; length == column_count */
    UWORD flags;          /* e.g. non-selectable bit for Phase 4 */
    APTR user_data;       /* optional; borrowed */
} CLV_ControlRow;

CLV_Control *clv_control_create(const CLV_ControlConfig *cfg);
VOID         clv_control_destroy(CLV_Control *control);

BOOL clv_control_set_columns(CLV_Control *c,
                             const CLV_ControlColumn *cols,
                             UWORD count);
BOOL clv_control_set_rows(CLV_Control *c,
                          const CLV_ControlRow *rows,
                          ULONG count);
VOID clv_control_set_row_gap(CLV_Control *c, UWORD pixels);
VOID clv_control_set_bounds(CLV_Control *c, const struct Rectangle *bounds);
VOID clv_control_set_pens(CLV_Control *c, const struct CLV_Pens *pens);
VOID clv_control_set_selected(CLV_Control *c, LONG logical_row);
VOID clv_control_make_visible(CLV_Control *c, LONG logical_row);

VOID clv_control_render(CLV_Control *c, ULONG flags);
BOOL clv_control_handle_input(CLV_Control *c,
                              const struct CLV_InputEvent *event,
                              struct CLV_Event *result);

/* Scroll accessors for backend ↔ scrollbar sync (Phase 4; may stub in Phase 2). */
LONG clv_control_get_scroll_y(const CLV_Control *c);
VOID clv_control_set_scroll_y(CLV_Control *c, LONG scroll_y);
LONG clv_control_get_content_height(const CLV_Control *c);
```

Phase 2 implements create/destroy, set_columns/rows/bounds/pens, render (fixed-height rows). Input/selection/scroll may be no-ops until Phase 4.

### 5.2 Private headers

| Header | Contents |
|--------|----------|
| `clv_control_internal.h` | `struct CLV_Control`, `CLV_RowLayout`, layout helpers |
| `clv_control_draw.h` | `CLV_DrawOps`, `CLV_Pens`, `CLV_FontMetrics` |
| `clv_control_platform.h` | Control-local platform notes; may include `clv_platform.h` |

### 5.3 Draw ops (finalize for Phase 2)

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
    UWORD (*line_height)(APTR ctx);   /* font Y size */
    UWORD (*baseline)(APTR ctx);
};
```

### 5.4 Backend — `clv_backend_amiga_v36`

Responsibilities:

- Implement `CLV_DrawOps` against a window RastPort.
- Map `DrawInfo` → `CLV_Pens`.
- Translate IDCMP → `CLV_InputEvent` (Phase 4).
- Own or drive GadTools `SCROLLER_KIND` ↔ `scroll_y` (Phase 4).
- Must **not** create `LISTVIEW_KIND` for the content viewport.

---

## 6. Source tree and build design

### 6.1 Final tree for Phase 2

```text
src/custom_listview_control/
├── clv_control.h
├── clv_control.c
├── clv_control_layout.c
├── clv_control_render.c
├── clv_control_input.c
├── clv_control_scroll.c
├── clv_control_draw.h
├── clv_control_platform.h
├── clv_control_internal.h
└── backends/
    ├── clv_backend_amiga_v36.h
    └── clv_backend_amiga_v36.c

examples/custom_control_demo/
├── main.c
├── Makefile          # or rules in root Makefile — Phase 2 chooses one style
└── README.md
```

Existing `src/custom_listview/` remains untouched.

### 6.2 Link set (Phase 2 demo)

Expected objects:

- All new `custom_listview_control/*.o` used by the demo (input/scroll may be thin stubs)
- `clv_backend_amiga_v36.o`
- `clv_platform.o` (alloc)
- **Not** `clv_renderer_*.o`, `clv_selection.o`, `clv_pixel_wrap.o`, ASCII formatters

Include path: `src/` so `#include "custom_listview/clv_types.h"` and `#include "custom_listview_control/clv_control.h"` both work.

### 6.3 Size recording

Each phase that produces a binary records:

| Field | Value |
|-------|-------|
| New demo bytes | TBD Phase 2 |
| Comparable baseline | `size-draw-basic` = 27928 (report date 2026-07-22) |
| Compiler / flags | Match `docs/CLV_SIZE_REPORT.md` |
| Linked modules | List explicitly |

---

## 7. WB 2.x API safety notes

- Target Kickstart 2.04+ / gadtools V36+ for the first backend.
- Use DrawInfo pens; do not hard-code palette indices.
- Prefer borrowed screen/window font over opening Topaz.
- Full viewport redraw first; no `ScrollRasterBF` until Phase 6.
- Templates: follow window/VisualInfo/cleanup order even without ListView labels.
- `CLV_LISTVIEW_SCROLLBAR_BORDER` (36) is a v1 ListView heuristic — control layout must reserve scrollbar width from actual scroller gadget geometry, not blindly reuse that constant for content width.

---

## 8. Backend boundary checklist (§12.1)

| Forbidden in control core | Where it belongs |
|---------------------------|------------------|
| GadTools `LISTVIEW_KIND` | Nowhere in new control |
| `GTLV_*` tags / `GTLV_CallBack` | Nowhere |
| GadTools physical-row selection | Nowhere |
| `DrawInfo *` inside renderer TUs | v36 backend only |
| Raw `IntuiMessage` in layout/render | v36 backend → `CLV_InputEvent` |
| GadTools scroller structs in `CLV_Control` | Backend; core stores `scroll_y` pixels only |

| Allowed supporting use | Notes |
|------------------------|-------|
| GadTools `SCROLLER_KIND` | Backend Phase 4 |
| Graphics.library via draw ops | Backend implements ops |
| `clv_types.h` | Shared low-level types |

---

## 9. Explicit deferrals

- Workbench 1.3 backend (`clv_backend_amiga_v34`)
- Formal BOOPSI custom gadget class
- Column sorting / pressed header buttons
- Row separator styles (solid/dotted) beyond clear `row_gap`
- Horizontal scrolling, multi-select, inline edit, DnD, column resize/reorder, tree view
- Animated smooth scrolling / `ScrollRaster` optimisation
- Moving v1 code into shared modules
- Icons, text styles, details/semantic builders in the control (until explicitly scheduled)

---

## 10. Risks and open questions for Phase 2

Non-blocking; Phase 2 may resolve during implementation:

| Item | Guidance |
|------|----------|
| Exact clip API (`push_clip`/`pop_clip` vs single clip rect) | Verify against templates/AutoDocs; keep ops table |
| Whether Phase 2 stubs `handle_input` or omits the symbol until Phase 4 | Prefer declare + stub returning FALSE |
| Demo Makefile: root vs local | Prefer root Makefile target `custom-control-demo` for consistency with other examples; local Makefile optional wrapper |
| Sharing `CLV_LineStyle` without including `clv_renderer.h` | Control-local typedef with same values, or tiny shared header later — **do not** add dependency on `clv_renderer.h` |

**No remaining ambiguity** on: core state shape, ownership, backend boundary, initial public API, source tree, or reuse vs avoid decisions for Phase 2.

---

## 11. Instructions for Phase 2 (summary)

1. Create the source tree in §6.1.
2. Implement v36 backend draw ops + pen fill from DrawInfo.
3. Implement control create/destroy, bounds → `header_bounds` + `viewport_bounds`, column layout with 2-pen dividers.
4. Draw fixed header (raised cells) + fixed-height sample rows (no wrap).
5. Full viewport redraw; no selection/scroll interaction required.
6. Ship `examples/custom_control_demo` with README (modules linked/excluded, build command).
7. Record executable size vs `size-draw-basic`.
8. Leave CLV v1 and existing examples untouched.
9. Update the living design plan Phase 2 section + completion record when done.
