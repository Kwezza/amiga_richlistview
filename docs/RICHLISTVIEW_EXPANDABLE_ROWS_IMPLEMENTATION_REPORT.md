# RichListview — Expandable / Collapsible Rows Implementation Report

**Date:** 2026-07-31  
**Feature gate:** `RLV_ENABLE_EXPANDABLE_ROWS` (Makefile default `1`)  
**Build verified:** `make clean && make rich-listview-demo` (VBCC +aos68k, 68000) — exit 0, no warnings  
**Public-header audit:** `make public-header-audit` — exit 0  
**Runtime:** Compiled and linked; not run under emulation or on hardware in this session

---

## Summary of behaviour

Explicit per-row disclosure for classic Amiga RichListview:

- Rows marked `RLV_ROW_EXPANDABLE` may be collapsed (one compact display line) or expanded (full wrapped layout).
- A narrow `RLV_COL_TYPE_DISCLOSURE` column draws `+` (collapsed) or `-` (expanded) inside a compact outlined box; non-expandable rows leave the cell empty but keep column width for alignment.
- Mouse arm/commit on the disclosure control toggles only expansion state (same verified-click model as checkboxes). It does not select the row or toggle checkboxes.
- Keyboard: Right expands, Left collapses the current row; Up/Down never auto-expand.
- Multiple rows may remain expanded. `rlv_collapse_all` clears all expanded bits and rebuilds heights once.
- Programmatic expand/collapse/toggle/Collapse All do **not** emit `CELL_CONTROL` (same policy as `rlv_set_checkbox_value`).
- Mouse/keyboard transitions emit `RLV_EVENT_CELL_CONTROL` with `control_type = DISCLOSURE` and `RLV_ACTION_EXPANDED` / `RLV_ACTION_COLLAPSED`.

Expansion, checkbox, selection, and current-row visual states remain independent.

---

## Files added

| File | Role |
|------|------|
| `src/rich_listview/rlv_expand.c` | Owned expand snapshot, central `rlv_set_row_expanded`, public APIs, Collapse All, viewport anchor |
| `src/rich_listview/rlv_disclosure.c` | `+/-` resolve_rect + paint |
| `docs/RICHLISTVIEW_EXPANDABLE_ROWS_IMPLEMENTATION_REPORT.md` | This report |

## Files changed

| File | Change |
|------|--------|
| `src/rich_listview/rich_listview.h` | Row/column flags, actions, input types, public APIs, documentation |
| `src/rich_listview/rlv_internal.h` | `row_expand[]`, expand helpers, `rlv_layout_reheight_from` |
| `src/rich_listview/rlv.c` | Snapshot refresh/free; stubs when feature off |
| `src/rich_listview/rlv_layout.c` | Collapsed one-line height; `rlv_layout_reheight_from` |
| `src/rich_listview/rlv_render.c` | Disclosure paint; collapsed first-line text; disclosure → viewport in `render_cell_control` |
| `src/rich_listview/rlv_input.c` | Disclosure hit/arm/commit before checkbox; Left/Right |
| `Makefile` | Feature macro + conditional object lists |
| `examples/rich_listview_demo/main.c` | Disclosure column, mixed row flags, keys, event paint path |
| `examples/rich_listview_demo/README.md` | Integrator notes + keyboard |
| `docs/RICHLISTVIEW_OVERVIEW.md` | §2.9 + feature tables |
| `docs/historical/CLV_FUTURE_IMPROVEMENTS_WISHLIST.md` | Section 2 status |
| `docs/DevLog.md` | Session summary |

---

## Public API

### Row flags

| Flag | Meaning |
|------|---------|
| `RLV_ROW_EXPANDABLE` | Row may expand/collapse (application-explicit; not inferred from wrap) |
| `RLV_ROW_EXPANDED` | Initial / authoritative expanded bit (with EXPANDABLE) |

### Column type

| Type | Meaning |
|------|---------|
| `RLV_COL_TYPE_DISCLOSURE` (`0x0002`) | Narrow +/- cell control |

### Cell values (event payload)

| Macro | Value |
|-------|-------|
| `RLV_CELL_COLLAPSED` | 0 |
| `RLV_CELL_EXPANDED` | 1 |

### Actions

| Action | Meaning |
|--------|---------|
| `RLV_ACTION_EXPANDED` | Disclosure opened |
| `RLV_ACTION_COLLAPSED` | Disclosure closed |

### Input types

| Type | Demo key |
|------|----------|
| `RLV_INPUT_EXPAND_ROW` | Cursor Right |
| `RLV_INPUT_COLLAPSE_ROW` | Cursor Left |

### Operations

```c
BOOL rlv_expand_row(RLV_Control *c, LONG row);
BOOL rlv_collapse_row(RLV_Control *c, LONG row);
BOOL rlv_toggle_row(RLV_Control *c, LONG row);
VOID rlv_collapse_all(RLV_Control *c);
BOOL rlv_is_row_expandable(const RLV_Control *c, LONG row);
BOOL rlv_is_row_expanded(const RLV_Control *c, LONG row);
```

**Semantics:** same-state expand/collapse → TRUE no-op; non-expandable expand → FALSE; invalid row → FALSE; Collapse All rebuilds once. No `CELL_CONTROL` from these APIs.

Stable row identity remains `RLV_Row.user_data` (borrowed); events still carry `row_user_data` at commit. Logical row index is used for API arguments (no separate tag table in this codebase).

---

## Private state

Owned `UBYTE *row_expand` (length `row_count`), bits:

- `RLV_ROWEXP_EXPANDABLE` (0x01)
- `RLV_ROWEXP_EXPANDED` (0x02)

Copied from `RLV_Row.flags` on `set_rows` / `set_columns`. Mutated by expand APIs and disclosure input. Does not write through borrowed `RLV_Row` memory — the application syncs flags from events or after programmatic calls.

Central path: `rlv_set_row_expanded(c, row, expanded, source_flags, result)`.

Source flags: `RLV_EXPAND_SRC_API` | `MOUSE` | `KEY` | `BULK`.

---

## Event design

Reuses `RLV_EVENT_CELL_CONTROL`:

| Field | Disclosure |
|-------|------------|
| `control_type` | `RLV_COL_TYPE_DISCLOSURE` |
| `control_action` | `EXPANDED` or `COLLAPSED` |
| `previous_value` / `cell_value` | `COLLAPSED` / `EXPANDED` |
| `column` | First disclosure column index |
| `value` | Current `scroll_y` after anchoring |
| `row_user_data` | Borrowed from `RLV_Row.user_data` |

No parallel event system. Programmatic APIs do not impersonate user input.

---

## Compact and expanded layout

- Wrap cache always prepared for full text (unchanged).
- **Collapsed expandable:** `maximum_line_count = 1`; paint draws only the first fragment of each text cell; disclosure + checkbox still drawn in the first-line band.
- **Expanded / non-expandable:** existing full wrap height and paint.
- Not mere clipping of a tall row: content height is computed as one line + padding.
- No ellipsis marker in this first implementation (optional future).

---

## Display-map rebuild strategy

- Full wrap rebuild still happens on `set_bounds` / `set_rows` / columns / padding (existing path).
- Expand/collapse uses `rlv_layout_reheight_from(from_row)`:
  - rows before `from_row` keep `top_y`;
  - recompute heights and `top_y` from `from_row` onward;
  - update `content_height`;
  - wrap fragments untouched.
- Collapse All mutates all bits then one `reheight_from(0)`.

---

## Viewport anchoring

1. Record screen Y of the toggled row’s first line:  
   `viewport.MinY + layout_rows[row].top_y - scroll_y`
2. Change expand state and reheight from that row.
3. Set `scroll_y = row.top_y - (anchor_y - viewport.MinY)`, then clamp via `rlv_set_scroll_y`.

Fallbacks: invalid layout → full rebuild; clamp handles content shorter than viewport, bottom expansion, and max-scroll cases. No automatic `make_visible` of the entire tall row beyond anchoring (matches “scroll only as needed” for this first cut).

---

## Redraw strategy

| Change | Recommended paint |
|--------|-------------------|
| Disclosure `CELL_CONTROL` | Full viewport (demo does this) — height shift invalidates the tail |
| `rlv_render_cell_control` on disclosure | Escalates to `RLV_CELL_REPAINT_VIEWPORT` |
| Checkbox `CELL_CONTROL` | Unchanged local / escalate path |
| Programmatic expand APIs | Caller full viewport + scroller sync |

Incremental “row + viewport tail” regional paint is not yet a dedicated public helper; full viewport is the safe path used by the demo. Existing regional/smart-scroll paths remain for selection and pure scroll.

---

## Mouse and keyboard

**Mouse (order):** disclosure hit → arm only (no selection) → SELECT_UP commit → `rlv_set_row_expanded(..., SRC_MOUSE)` → event. Roll-off cancel clears arm. Hit pad: +1 px around visible box, clamped to column text inset.

**Keyboard:** `EXPAND_ROW` / `COLLAPSE_ROW` on selected row through the same central path (`SRC_KEY`). Inapplicable states → no event. Space still toggles checkbox only.

---

## Ownership and allocation

| Object | Owner |
|--------|-------|
| `RLV_Row.flags` expand bits | Application (authoritative) |
| `row_expand[]` | Control (snapshot) |
| Wrap frags | Control (borrowed string pointers) |
| Disclosure glyphs | Drawn each paint; no image allocation |

Per-toggle: no heap allocation. Failure during reheight restores prior expand bit.

---

## Memory and code-size impact

Measured after clean build (`RLV_ENABLE_EXPANDABLE_ROWS=1`):

| Item | Size (bytes) |
|------|----------------|
| `bin/rich-listview-demo` | 52832 |
| `rlv_expand.o` | 3444 |
| `rlv_disclosure.o` | 2500 |
| Combined feature objects | ~5944 |

| Cost | Amount |
|------|--------|
| Per-row expand snapshot | 1 byte (`UBYTE`) |
| Per-instance | pointer + count fields on `RLV_Control` (~8 bytes on 68k) when feature on |

When `RLV_ENABLE_EXPANDABLE_ROWS=0`: objects omitted from the link; public APIs stub to FALSE/no-op in `rlv.c`; layout/render/input expand paths compile out.

Benchmarks (toggle cost / Collapse All timings) were **not** run in this session — no emulator pass.

---

## Tests performed

| Area | Result |
|------|--------|
| Compile / link (default feature on) | PASS |
| Public header audit | PASS |
| Emulator / hardware behavioural suite | Not run this session |
| Visual / hit-test / keyboard matrix from the task | Manual checklist for integrator (demo keys printed at startup) |

Recommended manual checks on A500-class profile: middle/last-row toggle, Collapse All, disclosure vs checkbox independence, Right/Left no-ops, viewport anchor on tall Epsilon, resize with mixed expanded states.

---

## Compiler warnings / errors

None observed on the clean VBCC build.

---

## Compatibility / regression risks

- Demo column count increased 5 → 6 (disclosure first); integrators copying the old column layout must add the column explicitly.
- New enum values appended to `RLV_InputType` and `RLV_CellControlAction` (source-compatible if apps switch on known values only).
- `RLV_Control` gains trailing fields only when the feature macro is on — keep object trees isolated (already Makefile practice).
- Checkbox, selection, smart scroll, and wrap paths unchanged when disclosure is unused.

---

## Known limitations / future extensions

- No Expand All (intentionally omitted).
- No single-expanded-row policy.
- No automatic expansion on navigation.
- No ellipsis continuation marker on collapsed text.
- No dedicated “paint from row N to viewport bottom” public helper (viewport full paint used).
- Detail fields beyond first wrapped line not modelled.
- Tree / child-node hierarchy out of scope.

---

## Decision notes (Phase 1)

No public ABI blocker: additive flags on existing `RLV_Row.flags`, new column type in the low nibble, new APIs, append-only enums. Multiple expanded rows supported without architectural conflict.
