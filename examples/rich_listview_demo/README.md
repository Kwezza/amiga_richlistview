# RichListview Demo (Phase E4)

## Features demonstrated

- Experimental `RLV_Control` with a compact fixed header
- Title-cell dark boxes with `SHINEPEN` top/left highlights
- Body cells retain dark-right/shine-left vertical edges
- Configurable data-row dividers: none, solid, or one-on/one-off dotted
- Shared title/body horizontal and vertical cell padding
- Cycle gadgets select pending divider, X/Y padding, and row-gap values;
  `Go` transactionally recreates and repaints the control
- One-pixel `SHADOWPEN` outer outline
- Manually drawn viewport (no GadTools `LISTVIEW_KIND`)
- Variable-height logical rows with pixel-measured word wrapping
- Configurable `row_gap` applied once per logical row
- Whole logical-row selection highlight across every wrapped line
  (a configured nonzero gap remains excluded)
- Selection changes repaint only the old/new logical rows when `scroll_y`
  is unchanged (regional viewport painter)
- Small scroll changes use smart vertical scrolling (pixel shift + exposed
  band); large/unsafe scrolls fall back to full viewport paint
- Resizable window (`WA_SizeGadget` + `IDCMP_SIZEVERIFY` / `IDCMP_NEWSIZE`)
  with transactional `rlv_set_bounds`, wrap rebuild, scroll clamp,
  scroller re-sync, and full control repaint (never smart-scroll on resize)
- Fixed pixel column minima; last column grows to fill spare viewport width
  (flush next to scrollbar); `WA_MinWidth` / `WindowLimits` from column sum +
  dividers + frame + scroller so earlier columns never collapse on narrow resize
- Mouse hit-testing via demo IDCMP → `RLV_InputEvent` → `handle_input`
- Keyboard navigation via `IDCMP_RAWKEY` → demo translate → same `handle_input`
- App-owned `active_control` focus pointer (click-to-focus; outside click
  preserves; scroller events keep this control active)
- Non-selectable logical row (`RLV_ROW_NONSELECTABLE` category row)
- Experimental checkbox column (`On`) — authoritative product path for
  interactive checkboxes (see integrator section below)
- Expandable rows: narrow disclosure column (`+/-`), mixed
  `RLV_ROW_EXPANDABLE` / `RLV_ROW_EXPANDED` flags, Right/Left keyboard,
  `C` = Collapse All; status line reports disclosure `CELL_CONTROL`
- Read-only GadTools `TEXT_KIND` status field under the ListView showing
  the latest `RLV_EVENT_CELL_CONTROL` (`GT_SetGadgetAttrs` / `GTTX_Text`)
- Pixel `SCROLLER_KIND` synced to `scroll_y` (line / proportional; page via core)
- Deterministic `EXERCISE` CLI workload (neutral NAV/scroll ops; no RAWKEY inject)
- Semantic pens from `DrawInfo` via the v36 backend
- Optional crash-safe `PROGDIR:rlv.log` (logging build only)

> **Legacy note:** GadTools `clv_cellctl_*` under `src/custom_listview/`
> (profile `draw-cellctl-checkbox`, `examples/05_draw_cellctl_checkbox/`) is
> **legacy / non-authoritative**. New apps should use this custom-control
> demo and `rich_listview.h`. Do not extend the GadTools cellctl tree for
> product checkbox work.

## Checkbox cells — integrator pattern

Public header only: `rich_listview/rich_listview.h` (no private
headers required).

1. Mark a column with `RLV_COL_TYPE_CHECKBOX`.
2. Supply parallel `RLV_Cell` descriptors via
   `RLV_Row.control_cells` (length == column count; `NULL` = none).
3. Put the app-owned authoritative Boolean behind `user_data` (this demo
   points at `&store[row][On].value`).
4. Call `rlv_set_rows` — the control **copies** into an internal
   snapshot and does not write app memory by default.
5. Translate IDCMP: LMB down → `RLV_INPUT_SELECT_DOWN`; LMB up →
   `RLV_INPUT_SELECT_UP` (even if the pointer left the box); Space →
   `RLV_INPUT_TOGGLE`; Return → `RLV_INPUT_NAV_ACTIVATE`.
6. Handle events separately (at most one per `handle_input` call):
   - `RLV_EVENT_SELECTION_CHANGED` — regional or full paint as before
   - `RLV_EVENT_CELL_CONTROL` — checkbox: `control_type` CHECKBOX,
     `control_action` VALUE_CHANGED; sync store from `ev.cell_value` via
     `ev.row_user_data`, then prefer
     `rlv_render_cell_control(c, row, column)` (escalates to row/viewport
     when local restore is unsafe); this demo also formats the event into
     a persistent buffer and updates a bordered `TEXT_KIND` status gadget
     with `GT_SetGadgetAttrs(GTTX_Text)`
   - `RLV_EVENT_ACTIVATED` — Return only; never a toggle
7. Reject-restore / async: `rlv_set_checkbox_value` then the same narrow
   or regional repaint (does not write the app store).

## Control activation and current-row visuals

Defaults preserve historical behaviour (`SELECT_ROW` + `FULL` highlight).

| Key / CLI | Effect |
|-----------|--------|
| `A` | Toggle `SELECT_ROW` ↔ `KEEP_CURRENT` |
| `V` | Cycle `FULL` → `MARKER` → `NONE` |
| `KEEPCURRENT` | Start in independent checkbox mode |
| `MARKER` | Start with left-edge current-row marker |
| `NOVISUAL` | Start with no current-row decoration |

In `KEEP_CURRENT` mode:

1. Select a row with the mouse or keyboard (current row stays put).
2. Click a checkbox on a **different** row.
3. The checkbox toggles and the status line reports that row/column.
4. The current/selected row does **not** move; scroll does not jump.

`MARKER` draws a narrow left-edge bar with the selected-background pen
across the logical row height (including wraps). Prefer `X Pad: 2` or
higher so the marker sits in the text inset.

Changing either policy does **not** rebuild wrapping; `V` triggers a
viewport repaint only.

## Expandable rows — integrator pattern

Requires `RLV_ENABLE_EXPANDABLE_ROWS=1` (Makefile default).

1. Mark rows with `RLV_ROW_EXPANDABLE` and optionally `RLV_ROW_EXPANDED`.
2. Add a narrow `RLV_COL_TYPE_DISCLOSURE` column (this demo places it first).
3. On expandable rows set disclosure cell flags
   `VISIBLE|ENABLED|INTERACTIVE`; leave non-expandable rows empty so the
   column stays aligned without a `+/-` glyph.
4. Collapsed rows show one compact display line (first wrap fragment);
   expanded rows use the full wrapped layout. Multiple rows may stay
   expanded. Checkboxes remain usable in both states.
5. Mouse disclosure uses the same arm/commit model as checkboxes but
   **does not** change selection. Keyboard: Right expands, Left collapses
   the current row (no-ops when inapplicable). Up/Down never auto-expand.
6. On `CELL_CONTROL` with `control_type` DISCLOSURE and action
   `EXPANDED`/`COLLAPSED`: sync `RLV_ROW_EXPANDED` on the app row, then
   `rlv_render_from_row(c, row, scroll_before)`. That may block-copy
   content below the toggled row (smart scroll) or paint a viewport tail;
   it falls back to a full viewport paint when unsafe.
7. Programmatic `rlv_expand_row` / `rlv_collapse_row` / `rlv_toggle_row` /
   `rlv_collapse_all` do not emit events; sync app flags yourself and
   repaint. Demo key `C` calls Collapse All.

## Cell-control event notification (integrator contract)

`RLV_EVENT_CELL_CONTROL` is the single application-facing notification for
completed drawn cell actions. Delivery model:

```text
IDCMP → RLV_InputEvent → rlv_handle_input() → RLV_Event (stack)
```

- Synchronous fill into a caller-owned `RLV_Event`; no callbacks; no Exec
  messages; do not retain the event (or `row_user_data`) past the immediate
  handler.
- `control_type` reuses `RLV_COL_TYPE_*`; `control_action` is
  `RLV_ACTION_VALUE_CHANGED` or `RLV_ACTION_PRESSED`.
- Checkbox today: CHECKBOX + VALUE_CHANGED; `previous_value` / `cell_value`
  are the old/new snapshot Booleans.
- Future button: same event + PRESSED; `previous_value` / `cell_value` may
  both be zero (stateless).
- Future cycle: same event + VALUE_CHANGED; values carry old/new cycle
  indices (or packed values).
- `row_user_data` is a borrowed copy of `RLV_Row.user_data` at
  commit. Per-cell `control_user_data` is deferred (no cell slot yet).
- Demo formatting (`demo_control_type_name` / status `TEXT_KIND`) stays in
  the example — not linked into the control core.
- Event construction is centralised in the control (`rlv_fill_cell_event`);
  future cell types extend enums / fill arguments, not the delivery model.

Row kinds in this demo:

| Row | Name | Disclosure | Checkbox |
|-----|------|------------|----------|
| 0 Alpha | Expandable (start collapsed) | `+` | interactive checked |
| 1 Beta | Expandable (start expanded) | `-` | interactive unchecked |
| 2 Gamma | Expandable collapsed (tall when open) | `+` | interactive checked |
| 3 Category | Non-selectable heading | empty | none |
| 4 Delta | Not expandable | empty | display-only checked |
| 5 Epsilon | Expandable (start expanded, tall) | `-` | interactive unchecked |
| 6 Zeta | Expandable collapsed | `+` | disabled/ghosted |
| 7 Eta | Expandable collapsed | `+` | interactive checked |
| 8 Theta | Not expandable | empty | interactive unchecked |

## Keyboard mapping

| Key | Action |
|-----|--------|
| Cursor Up / Down | `NAV_PREV` / `NAV_NEXT` (skip non-selectable; no wrap) |
| Shift + Cursor Up / Down | `NAV_PAGE_UP` / `NAV_PAGE_DOWN` (selection-centric page) |
| Ctrl + Cursor Up / Down | `NAV_FIRST` / `NAV_LAST` (Ctrl wins over Shift) |
| Cursor Right / Left | `EXPAND_ROW` / `COLLAPSE_ROW` (current expandable row) |
| Return (`0x44`) | `NAV_ACTIVATE` when a selectable row is selected |
| Space (`0x40`) | `RLV_INPUT_TOGGLE` — sole eligible checkbox on selected row |
| A (`0x20`) | Toggle activation policy (`SELECT_ROW` / `KEEP_CURRENT`) |
| V (`0x34`) | Cycle current-row visual (`FULL` / `MARKER` / `NONE`) |
| C (`0x33`) | `rlv_collapse_all` + sync app row flags + viewport paint |
| Home / End / PgUp / PgDn | Not mapped (not on classic Amiga keyboards) |

Key repeat uses OS/Intuition repeated `IDCMP_RAWKEY` messages (no INTUITICKS
engine). Upstrokes are ignored.

### Dual-control focus routing (documented; demo is single-control)

```text
Demo state: ctrl[0], ctrl[1], active = ctrl[0]  /* or NULL until first click */

IDCMP_MOUSEBUTTONS (LMB down in a control outer box):
  → active = that control; SELECT_DOWN → that control
IDCMP_MOUSEBUTTONS (LMB up, active != NULL):
  → SELECT_UP → active (even if pointer left the box; cancels arm)
Outside all controls (down):
  → leave active unchanged

IDCMP_RAWKEY:
  → if active != NULL: translate → handle_input(active)

Scroller gadget for ctrl[i]:
  → active = ctrl[i]; scroll input → ctrl[i]
```

## Source / object modules required

- `src/rich_listview/rlv.c`
- `src/rich_listview/rlv_layout.c`
- `src/rich_listview/rlv_wrap.c`
- `src/rich_listview/rlv_render.c`
- `src/rich_listview/rlv_checkbox.c`
- `src/rich_listview/rlv_input.c`
- `src/rich_listview/rlv_scroll.c`
- `src/rich_listview/backends/rlv_backend_amiga_v36.c`
- `src/rich_listview/rlv_platform.c`

Logging build only:

- `src/rich_listview/rlv_log.c`

## Optional modules deliberately excluded

- All `clv_renderer_*.o` / bind objects
- `clv_selection.o`
- `clv_pixel_wrap.o` / `clv_char_wrap.o`
- ASCII formatters / columns / sort / cell tracking / details / icons / styles
- `clv_cellctl_*` / `clv_bind_cellctl.o` (legacy GadTools checkbox path)
- `rlv_log.o` (normal build)
- Smart-scroll code when built with `RLV_ENABLE_SMART_SCROLL=0`

## Build commands

From the repository root (VBCC `+aos68k`):

```text
make rich-listview-demo
make rich-listview-demo-log
make rich-listview-demo-bench
make rich-listview-demo-nosmart
```

| Target | Executable | Logging | Smart scroll |
|--------|------------|---------|--------------|
| `rich-listview-demo` | `bin/rich-listview-demo` | Off | On (default) |
| `rich-listview-demo-log` | `bin/rich-listview-demo-log` | On | On (default) |
| `rich-listview-demo-bench` | `bin/rich-listview-demo-bench` | Off | On (default) |
| `rich-listview-demo-nosmart` | `bin/rich-listview-demo-nosmart` | Off | Off (isolated objects) |

Log file (logging build): `PROGDIR:rlv.log`

### EXERCISE / NOKEYBOARD CLI

```text
rich-listview-demo EXERCISE
rich-listview-demo-bench BENCH
rich-listview-demo NOKEYBOARD
rich-listview-demo EXERCISE NOKEYBOARD
rich-listview-demo-bench BENCH NOKEYBOARD
```

- `EXERCISE` — after first paint, runs a fixed NAV/scroll sequence through
  `demo_apply_input` (same path as interactive keys), then leaves the window
  open. Does **not** synthesise `IDCMP_RAWKEY`. Start state: `scroll_y = 0`,
  `selected_row = -1`.
- `NOKEYBOARD` — calls `rlv_set_keyboard_enabled(control, FALSE)`.
  Cursor/Return NAV_* are ignored; mouse selection and scroller still work.
  Keyboard is **enabled by default**.
- `BENCH` — benchmark build only. Runs the scripted benchmark suite, then writes
  `PROGDIR:rlv_benchmark.txt` after all timed work is complete.

Validate a copied benchmark report from the repository root:

```text
powershell -NoProfile -ExecutionPolicy Bypass -File tools/validate_benchmark_report.ps1 -ReportPath bin/rlv_benchmark.txt
```

The validator checks all eight per-test blocks, expected step counts,
non-saturated elapsed times, and counter deltas. A successful Windows
cross-link proves the Amiga executable was built; only an Amiga/WinUAE run
validates `ReadEClock()` timings and the rendered workload.

Initial outer window size follows the normal demo open formula (borders +
pad + fixed columns + scroller + controls strip; height ≈
`(font_h+2)*16` for the control body).

API: `rlv_set_keyboard_enabled` / `get_keyboard_enabled`, or create
with `RLV_CFG_NO_KEYBOARD` in `RLV_Config.flags`.

Cell spacing and data-row divider API:

- `RLV_Config.cell_padding_x` insets title and body text horizontally.
  It changes available wrapping width.
- `RLV_Config.cell_padding_y` adds the same top/bottom inset to title
  and body cells. It changes header and logical-row heights.
- `RLV_Config.row_gap` adds pixels after each logical body row.
- Set `RLV_Config.row_divider_style` to
  `RLV_ROW_DIVIDER_NONE`, `RLV_ROW_DIVIDER_SOLID`, or
  `RLV_ROW_DIVIDER_DOTTED`.
- The demo starts with X padding `1`, Y padding `1`, row gap `0`, and a
  `SOLID` divider. Each numeric cycle offers `0` through `4` pixels.
- Every cycle stores `IntuiMessage.Code` as its pending selection. Pressing
  `Go` creates a fully configured replacement before destroying the old
  control, retaining selection, scroll position, and keyboard state.
- `rlv_set_cell_padding()` and `rlv_set_row_gap()` invalidate
  layout; callers must relayout and repaint. Corresponding getters expose
  the current values.
- `rlv_set_row_divider_style()` changes the live setting without
  relayout; repaint afterward. The title row is never affected.
- Use the window close gadget to leave the demo.

## Measured file size (Phase 5.5, 2026-07-27)

VBCC `+aos68k -O2 -size -final`, `-cpu=68000`.

| Target | Bytes | vs Phase 5 | Notes |
|--------|------:|-----------:|-------|
| `bin/rich-listview-demo` (smart on) | 33032 | +2556 | NAV_* + RAWKEY + EXERCISE |
| `bin/rich-listview-demo-nosmart` | 31788 | +2556 | `RLV_ENABLE_SMART_SCROLL=0` |
| `bin/rich-listview-demo-log` | 47148 | +2720 | Logger + instrumented sites |
| `bin/size-draw-basic` (v1 baseline) | 27852 | 0 | Unchanged |

Linked modules (normal): control objs (incl. wrap) + `rlv_backend_amiga_v36.o` + `rlv_platform.o` only.

### Configurable appearance-control sizes (2026-07-28)

VBCC `+aos68k -O2 -size -final`, `-cpu=68000`:

| Target | Bytes |
|--------|------:|
| `bin/rich-listview-demo` | 37036 |
| `bin/rich-listview-demo-log` | 51556 |
| `bin/rich-listview-demo-bench` | 53604 |

### Phase C2 skeleton sizes (2026-07-30)

VBCC `+aos68k -O2 -size -final`, `-cpu=68000`. Checkbox ABI + snapshot/setter
only (no interactive paint/toggle).

| Target | Bytes | vs 2026-07-28 |
|--------|------:|--------------:|
| `bin/rich-listview-demo` (smart on) | 37772 | +736 |
| `bin/rich-listview-demo` (`RLV_ENABLE_SMART_SCROLL=0`) | 36528 | — |
| `bin/rich-listview-demo-log` | 52348 | +792 |
| `bin/rich-listview-demo-bench` | 54340 | +736 |

### Phase C3 paint sizes (2026-07-30)

VBCC `+aos68k -O2 -size -final`, `-cpu=68000`. Checkbox first-line-band
geometry + snapshot paint (no mouse toggle yet).

| Target | Bytes | vs C2 |
|--------|------:|------:|
| `bin/rich-listview-demo` (smart on) | 40460 | +2688 |
| `bin/rich-listview-demo` (`RLV_ENABLE_SMART_SCROLL=0`) | 39216 | +2688 |
| `bin/rich-listview-demo-log` | 55028 | +2680 |
| `bin/rich-listview-demo-bench` | 57116 | +2776 |

### Phase C4 mouse-commit sizes (2026-07-30)

VBCC `+aos68k -O2 -size -final`, `-cpu=68000`. Verified SELECT_DOWN/UP
arm-and-commit + `CELL_TOGGLED` (minimal demo SELECT_UP / row refresh).

| Target | Bytes | vs C3 |
|--------|------:|------:|
| `bin/rich-listview-demo` (smart on) | 41768 | +1308 |
| `bin/rich-listview-demo` (`RLV_ENABLE_SMART_SCROLL=0`) | 41100 | +1884 |
| `bin/rich-listview-demo-log` | 56644 | +1616 |
| `bin/rich-listview-demo-bench` | 58524 | +1408 |

**Runtime (2026-07-30):** Amiga/emulator confirmed — `On` checkboxes are
clickable; verified down/up toggles and prints `Toggled row N col 4: …`
(0↔1), including re-toggle of the same cell; selection events continue
to print alongside.

### Phase C5 selection-vs-toggle (2026-07-30)

Policy lock (§D.11): at most one `RLV_EventType` per `handle_input`.
Other-row checkbox SELECT_DOWN may emit `SELECTION_CHANGED` and arm;
same-row checkbox SELECT_DOWN arms only; SELECT_UP emits `CELL_CONTROL`
only (or nothing on cancel).

VBCC `+aos68k -O2 -size -final`, `-cpu=68000`:

| Target | Bytes | vs C4 |
|--------|------:|------:|
| `bin/rich-listview-demo` (smart on) | 41804 | +36 |
| `bin/rich-listview-demo` (`RLV_ENABLE_SMART_SCROLL=0`) | 40560 | −540 |
| `bin/rich-listview-demo-log` | 56880 | +236 |
| `bin/rich-listview-demo-bench` | 58588 | +64 |

Manual checklist (code-path locked; Amiga runtime optional):

- [ ] Other-row checkbox: selection event on down, toggle on up
- [ ] Same-row checkbox: no selection event on down; toggle on up
- [ ] Release outside armed box: no toggle
- [ ] Rapid other-row then same-row clicks: events stay separate

### Phase C6 smart-scroll / wrap / resize (2026-07-30)

Checkbox pixels stay correct under control-owned smart-scroll, wrap, and
relayout. Exposed-band expansion covers `cell_padding_y + line_height`
(first-line band). Arm cancels on `set_bounds` / `set_rows` / `set_columns`
/ layout invalidate. Selection + `make_visible` scroll still uses full
viewport paint (never `render_scrolled`).

VBCC `+aos68k -O2 -size -final`, `-cpu=68000`:

| Target | Bytes | vs C5 |
|--------|------:|------:|
| `bin/rich-listview-demo` (smart on) | 41836 | +32 |
| `bin/rich-listview-demo` (`RLV_ENABLE_SMART_SCROLL=0`) | 41152 | +592 |
| `bin/rich-listview-demo-log` | 56912 | +32 |
| `bin/rich-listview-demo-bench` | 58620 | +32 |

(Smart-off vs C5: C5’s 40560 looked low vs C4’s 41100 after a wipe; C6
41152 is within ~52 bytes of C4 nosmart.)

Build twin: `make rich-listview-demo` and
`make rich-listview-demo RLV_ENABLE_SMART_SCROLL=0`.

Manual checklist (code-path locked; Amiga/emulator when available):

- [ ] Line scroll: checkboxes move with blit; no ghost/stale ticks
- [ ] Jump / large scroll: full viewport fallback; marks match snapshot
- [ ] Selection that triggers `make_visible`: full viewport; no stale highlight or ticks
- [ ] Narrow resize rewrap: checkbox stays on first-line band (not mid-row)
- [ ] Arm then resize / `Go` relayout: arm cancelled; no spurious toggle
- [ ] Smart-scroll-off twin: same visual correctness (full viewport each scroll)
- [ ] Short scroll over checked rows: ticks stay complete (no half-tick /
      remnant sliver after smart-scroll band paint; 2026-07-30 diagonal soft-clip fix)

### Phase C7 Space toggle (2026-07-30)

Demo RAWKEY Space (`0x40`) → `RLV_INPUT_TOGGLE` (DC-006). Core toggles the
selected row when it has exactly one VISIBLE|ENABLED|INTERACTIVE checkbox
column; otherwise no event. Return / `NAV_ACTIVATE` still activates only.

VBCC `+aos68k -O2 -size -final`, `-cpu=68000`:

| Target | Bytes | vs C6 |
|--------|------:|------:|
| `bin/rich-listview-demo` (smart on) | 42332 | +496 |
| `bin/rich-listview-demo` (`RLV_ENABLE_SMART_SCROLL=0`) | 41072 | −80 |
| `bin/rich-listview-demo-log` | 57540 | +628 |
| `bin/rich-listview-demo-bench` | 59172 | +552 |

Manual checklist (code-path locked; Amiga/emulator when available):

- [ ] Select interactive single-checkbox row; Space toggles / prints `Toggled …`
- [ ] Space on display-only (Delta) or disabled (Zeta) row: no toggle
- [ ] Return still prints `Activated …` only (never toggles)
- [ ] Multi-column deferral: core requires sole eligible column (demo has one `On` col)

### Phase C8 demo + integration docs (2026-07-30)

Authoritative app-store sync: `user_data` points at the On-column `UBYTE`;
`CELL_CONTROL` writes `ev.cell_value` then `render_logical_rows`. Docs mark
GadTools `clv_cellctl_*` as legacy. Public `rich_listview.h` documents ownership
/ `set_rows` / experimental checkbox cells.

VBCC `+aos68k -O2 -size -final`, `-cpu=68000` (demo wiring only; formal
campaign is C9):

| Target | Bytes | notes |
|--------|------:|-------|
| `bin/rich-listview-demo` (smart on) | 43496 | includes prior half-tick fix |
| `bin/rich-listview-demo` (`RLV_ENABLE_SMART_SCROLL=0`) | 42812 | |

Emulator / Amiga integration checklist:

- [ ] Other-row checkbox: `Selected …` on down, `Toggled …` on up; store sync
- [ ] Same-row checkbox: no selection print on down; toggle on up
- [ ] Release outside armed box: no toggle; store unchanged
- [ ] Display-only (Delta) / disabled (Zeta): click does not toggle
- [ ] Space on interactive row: toggle + store sync; Space on Delta/Zeta: none
- [ ] Return: `Activated …` only
- [ ] After toggle, `Go` recreate keeps the synced Boolean (store → set_rows)
- [ ] Docs: integrator can follow without private headers; cellctl marked legacy

### Phase C9 size / regression (2026-07-30)

Formal campaign: wipe `build/custom_listview_control/**/*.o` (+ example
obj) between smart and nosmart links. Same flags as prior baselines
(VBCC `+aos68k -O2 -size -final`, `-cpu=68000`).

| Target | Bytes | Notes |
|--------|------:|-------|
| `bin/rich-listview-demo` (smart on) | 43496 | Matches C8 smart; no code delta |
| `bin/rich-listview-demo-nosmart` | 42236 | Full wipe; **corrects** C8 nosmart 42812 |
| Smart − nosmart | +1260 | Smart-scroll compile cost |
| `bin/rich-listview-demo-log` | 58736 | Logging tree |
| `bin/rich-listview-demo-bench` | 60244 | Benchmark tree |
| `rlv_checkbox.o` (smart) | 3264 | Object attribution |

Cumulative vs pre-C2 appearance demo (37036 smart, 2026-07-28):
**+6460** for the full checkbox feature path through C8 wiring.

**Optional omit:** none. Checkbox is **always linked** with the control
package (`RLV_CUSTOM_CONTROL_OBJS` includes `rlv_checkbox.o`).
No `RLV_ENABLE_CTRL_CHECKBOX` (or similar) Makefile flag in this phase.

#### Cumulative smart sizes (C2…C9)

| Phase | Smart bytes | Δ vs prior |
|-------|------------:|-----------:|
| Pre-C2 (appearance) | 37036 | — |
| C2 skeleton | 37772 | +736 |
| C3 paint | 40460 | +2688 |
| C4 mouse | 41768 | +1308 |
| C5 selection policy | 41804 | +36 |
| C6 scroll/wrap | 41836 | +32 |
| C7 Space | 42332 | +496 |
| C8 store docs / C9 formal | 43496 | +1164 |

#### Regression checklist (C9)

Code-path audit (host cross-link + source review). No unexplained
regressions. Amiga interactive re-pass optional where prior evidence exists.

| Area | Result | Evidence |
|------|--------|----------|
| Selection vs checkbox | PASS | §D.11 two-event rule in `rlv_input.c` SELECT_DOWN/UP; C5 locked |
| Ordinary smart scroll | PASS | Demo uses `render_scrolled` on scroll-only; exposed band expands `cell_padding_y + line_height` |
| Selection / make_visible scroll | PASS | Full viewport paint (never smart-scroll) when selection changes scroll |
| Wrap / resize | PASS | `set_bounds` cancels arm, rebuilds wrap; checkbox first-line band only |
| Mouse toggle | PASS | Arm/commit path intact; C4 Amiga/emulator runtime confirmed |
| Space / `RLV_INPUT_TOGGLE` | PASS | Sole eligible column; display-only/disabled defer; `NAV_ACTIVATE` untouched |
| Demo store sync | PASS | `CELL_CONTROL` → write `user_data` → `render_logical_rows` (C8; renamed E2) |

Build commands:

```text
# wipe build/custom_listview_control/*.o + backends + example obj, then:
make rich-listview-demo RLV_ENABLE_SMART_SCROLL=0
# copy to preserve twin, wipe again, then:
make rich-listview-demo
make rich-listview-demo-log
make rich-listview-demo-bench
```

### Phase C10 closure (2026-07-30)

Corrected interactive-checkbox roadmap **Complete** (master plan C0–C10).
Public ABI matches §D.11; checkbox paint/hit/input lives only under
`src/custom_listview_control/`; GadTools scrollbar remains demo-owned;
GadTools `clv_cellctl_*` stays **legacy / non-authoritative**.

**Deferred (not blocking):** Makefile omit for `rlv_checkbox.o`;
multi-column keyboard focus; indeterminate value; future cell types;
optional Amiga re-pass of unchecked interactive checklist boxes above.

See `docs/RLV_INTERACTIVE_CONTROL_CELLS_MASTER_PLAN.md` §D.10 / §F and
`docs/RLV_CONTROL_CELLS_DEVELOPER_LOG.md` (C10 entry).

### Phase E4 event-notification closure (2026-07-30)

Docs/ABI audit only (no product behaviour change). Public header documents
generic `RLV_EVENT_CELL_CONTROL`, VALUE_CHANGED / PRESSED, stateless value
semantics, borrowed `row_user_data`, deferred `control_user_data`, and
synchronous event lifetime. Demo README adds the integrator contract;
window title `RichListview (Phase E4)`.

| Target | Bytes | vs E3 |
|--------|------:|------:|
| `bin/rich-listview-demo` (smart on) | 44824 | 0 (comments/docs only) |

See `docs/RLV_CELL_CONTROL_EVENT_NOTIFICATION_PLAN.md` (E4 complete).

## Manual interaction checklist (Phase 4 exit criteria)

Validated on Workbench 2.x and Workbench 3.2 (`2026-07-24`):

- [x] Click any wrapped line of a multi-line row → that logical row selects as one block
- [x] Selection highlight covers all wrapped lines
- [x] Header stays fixed while content scrolls
- [x] Scroller arrows move by approximately one font line height
- [x] Scroller thumb drag stays synchronised with the viewport
- [x] Clicking `-- Category --` (row 3) does not change selection
- [ ] With a nonzero `row_gap`, clicking the gap does not change selection

## Incremental selection / smart scroll checklist

Validated on Workbench 2.x and Workbench 3.2 (`2026-07-24`):

- [x] Selecting a new row does not flash unrelated rows / header / frame
- [x] Tall↔short selection updates both old and new blocks correctly
- [x] Small arrow scrolls shift pixels; only the exposed band repaints
- [x] Large jumps fall back to full viewport paint
- [x] Selection that triggers `make_visible` uses full viewport paint (old
      highlight cleared; new wrapped row fully highlighted)
- [x] Ordinary scrolling still uses smart scroll when eligible
- [x] Header, outer frame, and scrollbar never shift
- [x] Scroll-down no longer leaves blank bands / letter fragments

## Resize / relayout checklist

Validated on Workbench 2.x and Workbench 3.2 (Phase 5 Task 1):

- [x] Widen / narrow / taller / shorter; rapid resize
- [x] Window will not shrink below fixed column + divider + frame + scroller min
- [x] Description (and other non-last columns) keep configured pixel widths
- [x] Widen: last column (Size) flushes to scrollbar; right-aligned values track new right
- [x] Horizontal resize above min: only last-column wrap/height may change
- [x] Vertical resize updates viewport + scroller; selection preserved
- [x] Scroller recreates and tracks control; Total/Visible/Top correct
- [x] No stale frame/header/divider pixels; full repaint only (no smart scroll)
- [x] Smart scroll works again after resize completes
- [x] WB2.x: no `80000004`; no text outside viewport; soft_only intact

## Appearance controls visual checklist

Requires a fresh Amiga/WinUAE run after the appearance-control change:

- [ ] Cycling any setting changes its displayed choice without repainting
- [ ] `Go` recreates the control and applies all four displayed choices
- [ ] X padding changes title/body horizontal inset and wrapping together
- [ ] Y padding changes title/body vertical inset and row/header height together
- [ ] Row gap adds only the requested space between logical body rows
- [ ] Selection, scroll position, and keyboard state survive `Go`
- [ ] Header/title rendering is identical in all three modes
- [ ] `NONE` draws no horizontal line between data rows
- [ ] `SOLID` draws one continuous dark line between data rows
- [ ] `DOTTED` draws a one-on/one-off dark line between data rows
- [ ] Row dividers stop one pixel inside the left/right body borders
- [ ] No divider is drawn after the final data row
- [ ] Column edges continue through row gaps and empty space to the bottom
- [ ] Extended column edges remain inside the outer black outline
- [ ] Body dark-right/shine-left vertical edges remain intact
- [ ] Outer control outline is one dark pixel on all four sides
- [ ] Demo rows have no extra vertical gap
- [ ] Selection, wrapped rows, scrolling, refresh, and resize remain correct

## Keyboard / EXERCISE checklist (Phase 5.5)

Host cross-link / size: validated `2026-07-27`. Amiga interactive items:

- [ ] Workbench 2.x: Cursor / Shift / Ctrl / Return map; no crash
- [ ] Workbench 3.2: same
- [ ] Key repeat (hold Up/Down) advances selection via OS RAWKEY repeat
- [ ] Non-selectable heading skipped by NAV_*; mouse still rejects
- [ ] Page / first / last clamp; no wrap at ends
- [ ] Return prints `Activated logical row N` when selectable selected
- [ ] Space on single interactive checkbox row toggles / prints `Toggled …`
- [ ] Space on display-only or disabled checkbox row does nothing
- [ ] Return still activates only (never toggles)
- [ ] Scroller knob tracks after keyboard nav that scrolls
- [ ] Selection + make_visible still uses full viewport paint
- [ ] Line scroll (scroller / EXERCISE) still smart-scrolls when eligible
- [ ] `rich-listview-demo EXERCISE` runs without RAWKEY injection; window stays open
- [ ] Click outside control preserves keyboard focus target

## WB2.x diagnostic checklist (logging build)

Preserve a separate `PROGDIR:rlv.log` after each isolated run:

- [x] Row click (selection miss on WB2.x)
- [x] Uncover control after another window obscured it (refresh)
- [x] Scroller arrow button
- [x] Scroller proportional knob / trough (stop after first crash)
- [x] Selection delta paint: balanced clip push/pop in the log
- [x] Smart scroll: `SMART_SCROLL` done vs `fallback full viewport`
- [x] Selection + scroll: log shows `selection+scroll full viewport`
- [x] Resize: `RESIZE begin` … full repaint / scroller sync (no smart scroll)

## Phase 5.5 closure notes

See `docs/RLV_CUSTOM_CONTROL_DESIGN_AND_IMPLEMENTATION_PLAN.md` — **Phase 5.5
Completion Record** and [keyboard nav plan](../../docs/RLV_CUSTOM_CONTROL_KEYBOARD_NAVIGATION_PLAN.md).
