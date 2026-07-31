# RichListview — Control Activation Policy Implementation Report

**Date:** 2026-07-31  
**Scope:** Opt-in separation of embedded checkbox activation from the current/selected row, current-row visual styles, and local cell-control repaint.  
**Authority:** Current public headers, source, and Makefile override this report if they disagree.

---

## Summary of behaviour implemented

1. **Default unchanged.** `RLV_CONTROL_ACTIVATE_SELECT_ROW` + `RLV_CURRENT_ROW_VISUAL_FULL` reproduce the previous checkbox SELECT_DOWN selection path and full-row highlight.
2. **Opt-in independent activation.** `RLV_CONTROL_ACTIVATE_KEEP_CURRENT` arms/commits a checkbox without changing `selected_row`, without `make_visible`, and without emitting `SELECTION_CHANGED`. Body clicks still select normally.
3. **Current-row visuals.** `FULL` / `MARKER` / `NONE` control presentation only. Logical current row remains `rlv_get_selected`.
4. **Local repaint.** `rlv_render_cell_control(row, column)` paints a fully visible checkbox from the snapshot; escalates to logical-row or viewport paint when unsafe; returns a clear result code.
5. **Demo.** Keys `A` / `V` and CLI `KEEPCURRENT` / `MARKER` / `NOVISUAL`; CELL_CONTROL uses the narrow repaint path.

---

## Design note (Phase 1 audit)

### Where selection and checkbox activation were coupled

In `rlv_input.c` SELECT_DOWN, an interactive checkbox hit on a different selectable row always:

- armed the control;
- assigned `selected_row` to the checkbox row;
- called `rlv_make_visible`;
- possibly emitted `SELECTION_CHANGED`.

Row paint treated `selected_row == layout_index` as synonymous with full selected fill/text pens.

### Chosen public policy API

Setters/getters (same pattern as `rlv_set_keyboard_enabled`), not new `RLV_Config` fields, so positional zero-init of `RLV_Config` is undisturbed:

- `rlv_set/get_control_activation_policy`
- `rlv_set/get_current_row_visual`
- `rlv_render_cell_control` + `RLV_CellControlRepaintResult`

Defaults are enum value `0` matching historical behaviour.

### Lightweight focus marker

Narrow left-edge fill (prefer 2 px, clamp to `cell_padding_x` when 1, else 1 px) using `pens.selected_background`, spanning the full logical content height, clipped to the paint region / viewport. No layout gutter added. Recommendation: `cell_padding_x >= 2` so the bar sits in the text inset.

### Local repaint contract

| Result | Meaning |
|--------|---------|
| `RLV_CELL_REPAINT_OK` | Full control rect inside viewport; local fill + shared checkbox paint |
| `RLV_CELL_REPAINT_NOT_VISIBLE` | No intersection with viewport; nothing drawn |
| `RLV_CELL_REPAINT_ROW` | Partial visibility or clip failure → `rlv_render_logical_rows` |
| `RLV_CELL_REPAINT_VIEWPORT` | Layout invalid → full viewport paint |
| `RLV_CELL_REPAINT_ERROR` | Bad args / unresolved geometry; caller may row-paint |

No layout rebuild; no scroll/selection mutation; no event emit.

### Why backward compatible

Defaults are zero/legacy; existing apps need no source changes. Event numeric values and field order are unchanged. Policies do not invalidate wrap caches.

---

## Files changed

| Path | Role |
|------|------|
| `src/rich_listview/rich_listview.h` | Public enums, setters, `rlv_render_cell_control` |
| `src/rich_listview/rlv_internal.h` | Instance fields; `rlv_row_uses_selected_fill` |
| `src/rich_listview/rlv.c` | Create defaults; policy setters/getters |
| `src/rich_listview/rlv_input.c` | KEEP_CURRENT SELECT_DOWN path |
| `src/rich_listview/rlv_render.c` | Visual styles + marker; local cell repaint |
| `src/rich_listview/rlv_checkbox.c` | `rlv_row_uses_selected_fill` |
| `src/rich_listview/rlv_bench_internal.h` / `rlv_bench.c` | Control-only / fallback counters |
| `examples/rich_listview_demo/main.c` | Policies, A/V keys, narrow repaint |
| `examples/rich_listview_demo/README.md` | Integrator + demo guidance |
| `docs/RICHLISTVIEW_OVERVIEW.md` | Architecture sections 2.6–2.8 |
| `docs/historical/CLV_FUTURE_IMPROVEMENTS_WISHLIST.md` | Item 1 marked implemented (subset) |
| `tests/public_headers/rlv_public_core.c` | Touch new public enums |
| `docs/RICHLISTVIEW_CONTROL_ACTIVATION_POLICY_IMPLEMENTATION_REPORT.md` | This report |

No retained Commodore GadTools / AutoDocs sources were modified.

---

## Public API additions and defaults

```c
enum RLV_ControlActivationPolicy {
    RLV_CONTROL_ACTIVATE_SELECT_ROW = 0,   /* default */
    RLV_CONTROL_ACTIVATE_KEEP_CURRENT
};

enum RLV_CurrentRowVisual {
    RLV_CURRENT_ROW_VISUAL_FULL = 0,       /* default */
    RLV_CURRENT_ROW_VISUAL_MARKER,
    RLV_CURRENT_ROW_VISUAL_NONE
};

enum RLV_CellControlRepaintResult {
    RLV_CELL_REPAINT_OK = 0,
    RLV_CELL_REPAINT_NOT_VISIBLE,
    RLV_CELL_REPAINT_ROW,
    RLV_CELL_REPAINT_VIEWPORT,
    RLV_CELL_REPAINT_ERROR
};
```

`selected_row` continues to mean the current/navigation row (no disruptive rename).

---

## State model

Independently reasoned fields after this change:

| Field | Role |
|-------|------|
| `selected_row` | Current / navigation row |
| `current_row_visual` | FULL / MARKER / NONE |
| `cell_snapshot[].value` | Checkbox snapshot |
| `control_armed` / `armed_row` / `armed_column` | Transient mouse arm |
| `control_activation_policy` | SELECT_ROW / KEEP_CURRENT |
| `scroll_y` | Viewport scroll |

Full-row fill is no longer proof that a row is current (MARKER/NONE). Checkbox state is never used as current-row proof.

---

## Mouse and keyboard behaviour

### Mouse (SELECT_DOWN)

- **Checkbox + KEEP_CURRENT:** arm only; no selection/scroll/event.
- **Checkbox + SELECT_ROW:** prior behaviour (other row may select + make_visible).
- **Non-control body:** always normal selection path.
- **SELECT_UP:** unchanged verified commit → `CELL_CONTROL` only, or cancel.

### Keyboard

- NAV_* still moves `selected_row`.
- Space / `RLV_INPUT_TOGGLE`: toggles sole eligible checkbox on current row; does not change current row.
- Return / `NAV_ACTIVATE`: activation only; never toggles.

---

## Local repaint API and fallback contract

See table above. Shared `rlv_checkbox_resolve_rect` / `rlv_checkbox_paint` are reused. Background under the box uses selected fill pens only when `rlv_row_uses_selected_fill` is true (FULL visual + current row).

---

## Lightweight marker design

- Left edge of logical content rect (`viewport.MinX`).
- Width 2 px when padding allows; else `cell_padding_x` or 1 px.
- Pen: `selected_background`.
- Full logical content height; clipped; not drawn in header.
- Independent of checkbox state; no wrap/height change.

---

## Compatibility impact

- Existing apps: no required source changes.
- Event types/values and `RLV_Event` layout unchanged.
- `RLV_Config` field order unchanged.
- Policy changes do not call `rlv_layout_invalidate`.
- Internal policy fields are appended at the end of `RLV_Control`.
  An early mid-struct insertion plus Makefile rules that ignored header
  deps produced a mixed-object crash (`Software Failure #8000000B`) during
  layout rebuild; fixed by appending fields and adding header dependencies.

---

## Code-size / object-size impact (measured)

Production VBCC objects after this change (smart-scroll on):

| Object | Size (bytes) |
|--------|--------------|
| `rlv.o` | 4692 |
| `rlv_input.o` | 6492 |
| `rlv_render.o` | 9228 |
| `rlv_checkbox.o` | 3404 |

Linked demo binaries:

| Binary | Size (bytes) |
|--------|--------------|
| `bin/rich-listview-demo` | 47196 |
| `bin/rich-listview-demo-nosmart` | 45936 |
| `bin/rich-listview-demo-log` | 63360 |
| `bin/rich-listview-demo-bench` | 63992 |

Production link line does not include `rlv_log.o` or `rlv_bench.o`. ASCII scan of the production binary found no `rlv_log_init` / `rlv_bench_init` strings.

No pre-change baseline was captured in this session; sizes above are absolute post-change measurements only.

---

## Stack / heap impact

- No new heap allocation in input, hit-test, paint, regional repaint, or scroll paths.
- Local repaint uses stack `Rectangle` locals only (existing pattern).
- Instance cost: two `UWORD` fields on `RLV_Control` (policy + visual).
- No large automatic arrays; no recursion introduced.

---

## Tests and build targets run

| Target | Result |
|--------|--------|
| `make public-header-audit` | Success (exit 0) |
| `make rich-listview-demo` | Success (exit 0) |
| `make rich-listview-demo-nosmart` | Success (exit 0) |
| `make rich-listview-demo-log` | Success (exit 0) |
| `make rich-listview-demo-bench` | Success (exit 0) |

Compiler output showed no warnings for the rebuilt translation units.

There is no host-side unit-test harness with a fake `RLV_DrawOps` backend in this repository. Behavioural matrix cases were not executed as automated Amiga runtime tests.

---

## Runtime validation performed

**None on Amiga hardware or emulator in this agent environment.** Binaries were cross-compiled with VBCC `+aos68k` only. Compilation success is not equivalent to runtime validation.

Manual Amiga checks still recommended:

- Default mode: checkbox on another row still selects.
- `KEEPCURRENT`: toggle remote checkbox; current row stays; scroll unchanged.
- `A` / `V` visuals; Space vs Return.
- Fully visible checkbox → `control_only_redraws` in bench/log builds.
- Partial/off-screen checkbox → documented fallback / not-visible.
- Two controls with different policies (multi-instance app) for isolation.

---

## Anything not validated

- Real Amiga / UAE interactive behaviour.
- Pixel-correct marker on every Workbench depth/font combination.
- Benchmark counter deltas for control-only vs row paint on hardware.
- Multi-instance policy isolation at runtime.
- Reject-restore path beyond compile-time API presence.
- Smart-scroll exposed-band visual correctness under MARKER/NONE (code path shares `rlv_paint_row_content`, but not visually verified).

---

## Remaining limitations and sensible future extensions

- Only checkbox cell type today; `rlv_render_cell_control` is structured for later button/cycle types but does not implement them.
- Marker is a single left-edge bar; outline/arrow/first-column styles remain wishlist items.
- No second persistent “committed selection” index (out of scope).
- No permanent layout gutter; apps wanting a guaranteed inset should set `cell_padding_x >= 2`.
- Formal draw-ops recording tests would strengthen the behavioural matrix without requiring Amiga UI.

---

## Demo usage (quick)

```text
bin/rich-listview-demo
bin/rich-listview-demo KEEPCURRENT MARKER
```

| Key | Effect |
|-----|--------|
| A | Toggle activation policy |
| V | Cycle FULL → MARKER → NONE |
| Space | Toggle current row’s sole checkbox |
| Return | Activate row only |
