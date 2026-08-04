# RichListview — What It Is and How It Works

**Status:** Living architecture overview  
**Last verified:** 2026-07-31  
**Authority:** Current public headers, source tree, and Makefile override this document if they disagree. Module lists, feature defaults, target names, and API details will drift; treat this as orientation, not an immutable contract.

**Audience:** developers embedding or maintaining the control  
**Scope:** product behaviour, runtime architecture, and compile-time optional parts  
**Related:** [README](../README.md), [demo README](../examples/rich_listview_demo/README.md), [repository creation report](RICHLISTVIEW_REPOSITORY_CREATION_REPORT.md)

---

## 1. What this project is

RichListview is a **full custom ListView control** for classic AmigaOS (68k, Workbench 2.x / 3.x). It draws and owns its own list viewport instead of using GadTools `LISTVIEW_KIND`.

RichListview is currently an **application-managed custom control**, not an Intuition gadget class or BOOPSI class. It does not receive IDCMP messages automatically. The application translates relevant IDCMP input into `RLV_InputEvent` values and explicitly asks the control to render.

It exists because the native GadTools ListView cannot express two requirements cleanly:

1. **Logical rows that wrap** — one data row may occupy several display lines, but selection, hit-testing, and spacing must still treat that row as a single item.
2. **A fixed header** — column titles must stay visible while content scrolls; they must not be ordinary rows in the list.

Applications use RichListview when they need a reusable, manually drawn multi-column list with wrapping, selection, keyboard navigation, scrolling, and embedded cell controls (checkboxes today), without reimplementing those behaviours in each program.


### What it owns

| Responsibility | Notes |
|----------------|-------|
| Viewport rendering | Header, body, frame, dividers, selection highlight |
| Layout and wrapping | Pixel-measured column geometry and variable-height rows |
| Scrolling | Pixel `scroll_y`; line / page / proportional steps |
| Selection | Single logical-row selection (not per wrapped line) |
| Input | Neutral events; mouse hit-test and keyboard `NAV_*` |
| Embedded cell controls | Checkbox paint, arm/commit, Space toggle |
| Application events | Synchronous `RLV_Event` fill (no callbacks / Exec messages) |

### What it deliberately does not do

- It is **not** an Intuition gadget class or BOOPSI class; the application owns IDCMP translation and paint requests.
- It does **not** use GadTools `LISTVIEW_KIND`, `GTLV_Labels`, `GTLV_CallBack`, or `LVDrawMsg`.
- It is **not** the legacy GadTools draw-hook enhancer, ASCII label formatter, binder, or `clv_cellctl_*` path from the older `amiga_custom_listview` tree.
- A GadTools `SCROLLER_KIND` may still sit beside the control as a **companion scrollbar**; the demo owns that gadget and syncs it to `rlv_get_scroll_y` / `rlv_set_scroll_y`.

Public include:

```c
#include "rich_listview/rich_listview.h"
```

Optional Amiga V36 backend:

```c
#include "rich_listview/backends/rlv_backend_amiga_v36.h"
```

Types/macros use `RLV_*`; functions use `rlv_*`. `RLV_Control` and `RLV_BackendV36` are opaque.

---

## 2. How it works

### 2.1 Core design rule

> A logical row may occupy any number of rendered text lines, but selection, spacing, hit-testing, decoration, and ownership apply to the logical row as one item.

Layout distinguishes three layers:

1. **Logical rows** — application data (`RLV_Row` / cells).
2. **Wrapped display lines** — measured text fragments inside a logical row.
3. **Visible viewport slots** — what is currently painted inside the scrolling body.

Layout and wrapping rebuild when data, font, columns, or viewport width change — not on every paint.

### 2.2 Architecture layers

```text
Application / demo
  │  owns window, IDCMP, scroller gadget, authoritative row data
  │  translates IDCMP → RLV_InputEvent
  │  paints after events (control does not paint inside handle_input)
  ▼
RLV_Control  (src/rich_listview/)
  │  layout, wrap, render, input, scroll, checkbox snapshot
  ▼
RLV_DrawOps  (neutral draw boundary)
  ▼
Amiga V36 backend  (rlv_backend_amiga_v36.*)
  │  RastPort fills/lines/text, clip stack, optional ScrollRaster
  ▼
graphics / layers / intuition (via SDK)
```

Generic control code never calls Amiga drawing APIs directly. Pens are semantic values on the control; only the backend (or host setup) maps them from `DrawInfo`.

### 2.3 Modules

| Module | Role |
|--------|------|
| `rlv.c` | Create / destroy, setters, bounds, pens, selection accessors |
| `rlv_layout.c` | Column geometry, row heights, caches |
| `rlv_wrap.c` | Pixel word / path wrapping into line fragments |
| `rlv_render.c` | Full paint, regional row paint, smart-scroll paint |
| `rlv_input.c` | Hit-test, selection, `NAV_*`, checkbox arm/commit |
| `rlv_scroll.c` | `scroll_y` / content / viewport height accessors |
| `rlv_checkbox.c` | Checkbox geometry, snapshot paint, helpers |
| `rlv_expand.c` | Optional expandable-row state, API, reheight/anchor |
| `rlv_disclosure.c` | Optional +/- disclosure cell paint / resolve |
| `rlv_platform.c` | Allocator and platform helpers |
| `backends/rlv_backend_amiga_v36.c` | `RLV_DrawOps` implementation for classic Amiga |
| `rlv_log.c` | Optional logger — linked only in logging builds |
| `rlv_bench.c` | Optional benchmarks — linked only in bench builds |

### 2.4 Typical application loop

```text
1. Open window / screen; create Amiga V36 backend; fill RLV_Config
2. rlv_create(...) → rlv_set_columns / rlv_set_rows → rlv_set_bounds
3. rlv_render(control, 0)   /* full first paint */
4. On IDCMP:
     map mouse / keys / scroller → RLV_InputEvent
     rlv_handle_input(control, &in, &ev)
     switch (ev.type):
       SELECTION_CHANGED → regional or full repaint
       CELL_CONTROL      → update app store; rlv_render_cell_control
       ACTIVATED         → app action (Return)
       scroll change     → rlv_render_scrolled or full paint
5. On resize: rlv_set_bounds → full control repaint (never smart-scroll)
6. rlv_destroy / dispose backend on exit
```

Input path:

```text
IDCMP → RLV_InputEvent → rlv_handle_input() → RLV_Event (caller stack)
```

- Delivery is **synchronous** into a caller-owned `RLV_Event`.
- At most **one** event type per `handle_input` call.
- The control does **not** paint inside `handle_input`; the application chooses regional vs full vs smart-scroll paint afterward.
- Do not retain a pointer to the caller-owned `RLV_Event` after the handler returns. `row_user_data` remains application-owned and must remain valid for as long as the installed row data may reference it (it is a borrowed copy of `RLV_Row.user_data` at commit — same lifetime as the app row store).

### 2.5 Data ownership

- The application owns the authoritative row text and checkbox Booleans.
- `rlv_set_rows` **borrows** `RLV_Row` / cell pointers and **copies** control descriptors into an internal snapshot.
- By default the control does **not** write through borrowed application memory.
- On `RLV_EVENT_CELL_CONTROL`, the app updates its store (often via `row_user_data`), then prefers `rlv_render_cell_control`.
- `rlv_set_checkbox_value` updates the snapshot only (reject-restore / async), without a full `set_rows`.

### 2.6 Painting modes

| API | When to use |
|-----|-------------|
| `rlv_render` | Full control (header + viewport + frame), or viewport-only flag |
| `rlv_render_logical_rows` | Selection change with unchanged `scroll_y` |
| `rlv_render_cell_control` | Checkbox-only repaint after `CELL_CONTROL` when fully visible |
| `rlv_render_scrolled` | After a pure scroll change; may smart-scroll |

Smart scroll (when compiled in) tries a viewport pixel shift plus exposed-band repaint. Large or unsafe scrolls fall back to a full viewport paint. Selection that also moves `scroll_y` (for example `make_visible`) must use a full viewport paint — never smart scroll.

### 2.7 Embedded checkbox cells

1. Mark a column with `RLV_COL_TYPE_CHECKBOX`.
2. Supply parallel `RLV_Cell` descriptors on each row (`control_cells`).
3. Mouse: `SELECT_DOWN` may select and arm (default); `SELECT_UP` may commit `CELL_CONTROL`.
4. Opt-in `RLV_CONTROL_ACTIVATE_KEEP_CURRENT`: checkbox arm/commit leaves the current/selected row and scroll unchanged.
5. Space: `RLV_INPUT_TOGGLE` toggles the selected row when it has exactly one eligible checkbox.
6. Return / `NAV_ACTIVATE` activates the row only — it never toggles.
7. After `CELL_CONTROL`, prefer `rlv_render_cell_control` for a fully visible checkbox; it escalates to row/viewport paint when local restore is unsafe.
8. Current-row presentation is independent via `RLV_CurrentRowVisual` (`FULL` default, `MARKER`, `NONE`).

### 2.8 Control activation and current-row visuals

| Concept | API | Default |
|---------|-----|---------|
| Current / navigation row | `rlv_get_selected` / `rlv_set_selected` | unchanged meaning |
| Control activation policy | `rlv_set_control_activation_policy` | `SELECT_ROW` (legacy) |
| Current-row visual | `rlv_set_current_row_visual` | `FULL` (legacy highlight) |
| Local control repaint | `rlv_render_cell_control` | escalates when unsafe |

Changing either policy is presentation/input only — it does **not** rebuild wrapping or row heights. Marker mode draws a narrow left-edge bar with the selected-background pen; `cell_padding_x >= 2` is recommended so the marker sits in the text inset.

### 2.9 Expandable / collapsible rows (optional)

When `RLV_ENABLE_EXPANDABLE_ROWS` is enabled (Makefile default):

1. Mark rows with `RLV_ROW_EXPANDABLE` and optionally `RLV_ROW_EXPANDED`.
2. Add a narrow `RLV_COL_TYPE_DISCLOSURE` column; set cell flags
   `VISIBLE|ENABLED|INTERACTIVE` on expandable rows (empty otherwise).
3. Collapsed expandable rows use one compact display line (first wrap
   fragment); expanded rows use the full wrapped height.
4. Mouse: disclosure arm/commit emits `CELL_CONTROL` with
   `DISCLOSURE` + `EXPANDED`/`COLLAPSED` and does not select the row or
   toggle checkboxes.
5. Keyboard: Right expands, Left collapses the current row; Up/Down never
   auto-expand. Key `C` in the demo calls `rlv_collapse_all`.
6. Programmatic `rlv_expand_row` / `rlv_collapse_row` / `rlv_toggle_row` /
   `rlv_collapse_all` update layout/scroll but do not emit `CELL_CONTROL`.
7. After disclosure events or API calls, prefer `rlv_render_from_row`
   with the pre-toggle scroll. When smart scroll is enabled it may
   ScrollRaster rows below the toggle and repaint only the toggled row
   plus an exposed band; otherwise it paints a viewport tail. Falls back
   to a full viewport paint when scroll moved or the blit is unsafe.
   Multiple rows may remain expanded.

Expansion, checkbox, selection, and current-row visual states stay independent.

### 2.10 Row display, long words, and ellipsis

Public setters (defaults are zero / compatibility):

| API | Default | Notes |
|-----|---------|-------|
| `rlv_set_row_display_mode` | `RLV_ROWS_COLLAPSIBLE` | Also `ALWAYS_EXPANDED`, `SINGLE_LINE` |
| `rlv_set_long_word_mode` | `RLV_LONG_WORD_CLIP` | Applies to `RLV_WRAP_WORD` only |
| `rlv_set_ellipsis_flags` | `RLV_ELLIPSIS_COLLAPSED_CONTENT` | also `HORIZONTAL_CLIP`; `NONE` clears |

**Row display**

- **Collapsible** — historical expandable-row behaviour (disclosure UI when multi-line).
- **Always expanded** — every row uses its full natural wrapped height; no `+/-`, no disclosure events, no collapsed-content ellipsis. Per-row expand bits are retained.
- **Single line** — one text line + padding/gap; wrap cache kept but height ignores extra fragments; no disclosure UI.

**Initial expand** (`RLV_Config.initial_expand`, default `ALL_OPEN`)

- Applied on the first `rlv_set_rows` after `rlv_create` (creation / recreate).
- Later `set_rows` calls honor per-row `RLV_ROW_EXPANDED` so interactive state can be preserved.
- Demo **Settings → Start rows**: All open (default) / All collapsed.

Changing row-display or long-word mode invalidates layout; call `rlv_set_bounds` (or recreate) then full-repaint — never smart-scroll.

**Long words** (`RLV_WRAP_WORD`)

- **Clip** — an indivisible overlong word does not create another wrap line; the visible prefix is fitted (Status `Truncated` → clean clipped prefix). Marks horizontal clipping metadata.
- **Wrap** — measured character fallback after normal breakpoints fail.

Explicit `RLV_WRAP_WORD_OR_CHAR` and `RLV_WRAP_PATH` ignore the control-level long-word mode. `RLV_WRAP_NONE` always clips.

**Ellipsis** — three compact hand-drawn dots (`fill_rect` at x, x+2, x+4), not `"..."` text:

- **Collapsed content** — final visible line of a text cell that has later wrap lines hidden by collapse (Collapsible + collapsed only).
- **Horizontal clip** — text cell whose wrap stopped with undisplayed source on the same line (independent of collapse).

## 3. How the compile system selects what you pay for

Classic Amiga binaries are size-sensitive. RichListview keeps optional cost out of normal builds in three ways: **explicit object lists**, **compile-time feature macros**, and **isolated object trees** so differently configured objects never mix.

### 3.1 Explicit source lists (no wildcards)

The Makefile lists every object. Nothing is pulled in by a recursive `*.c` rule.

**Always linked for a normal demo:**

```text
rlv.o
rlv_layout.o
rlv_wrap.o
rlv_render.o
rlv_checkbox.o
rlv_input.o
rlv_scroll.o
backends/rlv_backend_amiga_v36.o
rlv_platform.o
```

When `RLV_ENABLE_EXPANDABLE_ROWS=1` (default), also:

```text
rlv_expand.o
rlv_disclosure.o
```

**Linked only when that variant needs them:**

| Object | When linked |
|--------|-------------|
| `rlv_log.o` | `rich-listview-demo-log` only |
| `rlv_bench.o` | `rich-listview-demo-bench` only |

Legacy GadTools enhancer objects, ASCII formatters, binders, selection adapters, and `clv_cellctl_*` are **not part of this repository** and are never linked.

### 3.2 Feature macros

| Macro | Default | Effect |
|-------|---------|--------|
| `RLV_PLATFORM_AMIGA=1` | always (Makefile) | Platform assert / Amiga path |
| `RLV_ENABLE_SMART_SCROLL` | `1` | Include smart-scroll paint and backend `ScrollRaster` helpers |
| `RLV_ENABLE_EXPANDABLE_ROWS` | `1` | Link expand/disclosure modules; compact collapsed rows |
| `RLV_ENABLE_LOGGING` | off | Logger APIs become real; macros expand to writes |
| `RLV_ENABLE_BENCHMARKS` | off | Benchmark instrumentation and `rlv_bench.c` |

#### Smart scroll (`RLV_ENABLE_SMART_SCROLL`)

When set to `0`, `#if` blocks in `rlv_render.c` and the V36 backend omit the pixel-shift / exposed-band path. Scroll paint always does a full viewport redraw. That shrinks code size for apps that do not want the optimisation.

#### Logging (`RLV_ENABLE_LOGGING`)

- Call sites may remain in shared sources (`RLV_LOG` / `RLV_LOGF`).
- When the macro is **undefined**, those macros compile to empty `do { } while (0)` no-ops — no format work, no I/O.
- When **defined**, `rlv_log.c` is compiled and linked; output goes to `PROGDIR:rlv.log` (open/seek/write/close per line so a crash still leaves useful data).

#### Benchmarks (`RLV_ENABLE_BENCHMARKS`)

Similar gating: bench code and demo `BENCH` CLI exist only in the bench variant. Normal builds do not link `rlv_bench.o`.

### 3.3 Isolated object trees

Different flag combinations must not share the same `.o` files. The Makefile therefore builds into separate directories:

| Tree | Flags | Target |
|------|-------|--------|
| `build/rich_listview/` | smart on (default), no log/bench | `bin/rich-listview-demo` |
| `build/rich_listview_log/` | smart on + `RLV_ENABLE_LOGGING` | `bin/rich-listview-demo-log` |
| `build/rich_listview_bench/` | smart on + `RLV_ENABLE_BENCHMARKS` | `bin/rich-listview-demo-bench` |
| `build/rich_listview_nosmart/` | `RLV_ENABLE_SMART_SCROLL=0` | `bin/rich-listview-demo-nosmart` |

That isolation is what makes “only use the parts you need” reliable: turning logging or smart scroll off is not a link-order trick; the unused code is never compiled into those objects.

### 3.4 Build targets at a glance

```text
make rich-listview-demo          # production-shaped: smart on, no log/bench
make rich-listview-demo-log      # diagnostics → PROGDIR:rlv.log
make rich-listview-demo-bench    # timed suite → PROGDIR:rlv_benchmark.txt
make rich-listview-demo-nosmart  # size twin without smart scroll
```

For a smart-scroll-off binary, use the dedicated target only:

```text
make rich-listview-demo-nosmart
```

Overriding `RLV_ENABLE_SMART_SCROLL` on the ordinary `rich-listview-demo` target can reuse objects previously compiled with smart scrolling enabled unless the tree is cleaned first. Prefer `-nosmart`; it keeps objects in an isolated directory and matches the build discipline in §3.3.

### 3.5 What “optional” means today

| Feature | Optional at compile time? | Notes |
|---------|---------------------------|-------|
| Core control + layout + wrap + input + scroll | No | Always required |
| Amiga V36 backend | Required for Amiga demos | Only backend shipped today |
| Checkbox module | No (always linked) | No `RLV_ENABLE_CTRL_CHECKBOX` omit flag yet |
| Expandable rows | Yes | `RLV_ENABLE_EXPANDABLE_ROWS=0` omits modules |
| Smart scroll | Yes | Prefer `rich-listview-demo-nosmart` |
| Diagnostic logging | Yes | `-log` target; macros no-op otherwise |
| Benchmarks | Yes | `-bench` target only |

Apps that embed RichListview should copy the same pattern: list the core objects explicitly, add `rlv_log.o` / `rlv_bench.o` only for diagnostic binaries, and keep differently `#define`d builds in separate object directories.

### 3.6 Public vs private headers

Applications should include only:

- `rich_listview/rich_listview.h`
- optionally `rich_listview/backends/rlv_backend_amiga_v36.h`

Do **not** include `rlv_internal.h`, `rlv_platform_internal.h`, `rlv_bench_internal.h`, or `rlv_log.h` from product application code. The logger header is for diagnostic builds that opt into logging; it is not part of the stable public umbrella API. `make public-header-audit` compiles small tests that prove the public headers stand alone.

---

## 4. Target constraints (why the design looks this way)

- **CPU:** 68000-compatible (`-cpu=68000`).
- **Compiler:** VBCC `+aos68k`, conservative C89-style source.
- **Libraries:** Amiga SDK (`-lamiga -lauto`); no POSIX, no AmigaOS 4 / MorphOS / AROS / MUI / ReAction dependency.
- **Resources:** small stack and heap; no large automatic arrays; no heap allocation in hot paint/scroll/input paths.
- **Refresh:** balanced GadTools refresh where used; clip and RastPort state restored on every return; damage-aware behaviour during `LAYERUPDATING`.

These constraints drive the modular sources, the draw-ops boundary, and the compile-out of logging / benchmarks / smart scroll.

---

## 5. Where to go next

| Need | Document / location |
|------|---------------------|
| Quick clone / build | [README](../README.md) |
| Integrator demo, checkbox contract, checklists | [examples/rich_listview_demo/README.md](../examples/rich_listview_demo/README.md) |
| Deep design history and phase records | `docs/CLV_CUSTOM_CONTROL_DESIGN_AND_IMPLEMENTATION_PLAN.md` |
| Extraction / namespace history | `RICHLISTVIEW_REPOSITORY_CREATION_REPORT.md`, `RICHLISTVIEW_NAMESPACE_MIGRATION_REPORT.md` |
| Agent / contributor rules | [agents.md](../agents.md) |
