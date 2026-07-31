# CLV Interactive Control Cells — Master Design and Implementation Plan

**Status:** Phase C10 **Complete** — corrected checkbox feature closed under
`src/custom_listview_control/`; GadTools `clv_cellctl_*` remains legacy /
non-authoritative  
**Primary first control:** Drawn checkbox/tickbox cell
**Authoritative target (corrected 2026-07-30):** `src/custom_listview_control/`
(experimental custom-drawn ListView; owns paint, selection, scroll, hit-test)
**Not the target:** `src/custom_listview/` GadTools `LISTVIEW_KIND` / `LV_DRAW` /
`GTLV_*` / `GT_RefreshWindow` path
**Working method:** One corrected phase (C2…C10) per AI-agent session
**Master record:** This document must be updated at the end of every phase
**Developer log:** `docs/CLV_CONTROL_CELLS_DEVELOPER_LOG.md`
**Related package plan:** `docs/CLV_CUSTOM_CONTROL_DESIGN_AND_IMPLEMENTATION_PLAN.md`
**Agent protocol:** §G (live). Do **not** follow LEGACY §11.

---

> ## SCOPE CORRECTION (2026-07-30) — READ BEFORE ANY WORK
>
> **Phases 0–5 of this document targeted the wrong component.**
>
> They implemented interactive checkbox cells against
> `src/custom_listview/` (GadTools `LISTVIEW_KIND`, `GTLV_CallBack` /
> `LV_DRAW`, `GTLV_Top`, `GT_RefreshWindow`, `clv_cellctl_*`).
>
> **The intended ListView is `src/custom_listview_control/`.** That package
> manually owns row rendering, columns, selection, scrolling, variable-height
> rows, hit testing, and redraw. A standard GadTools scrollbar gadget is the
> only significant OS control involved.
>
> ### Rules for all future agents
>
> 1. **Do not continue Phase 6** (or any later phase) of the old GadTools plan.
> 2. **Do not mechanically port** `clv_cellctl_*` into `custom_listview_control/`.
> 3. Treat all Phases 0–5 completion records, DC-001…003 *as applied to
>    GadTools*, Makefile `CLV_ENABLE_CELLCTL*`, `examples/05_draw_cellctl_checkbox/`,
>    and `src/custom_listview/clv_cellctl_*` as **legacy / mis-scoped work**.
>    Leave that code in the tree; do not delete it in this correction.
> 4. New checkbox work belongs only under `src/custom_listview_control/`
>    (plus its demo under `examples/custom_control_demo/`), following the
>    **replacement plan** in §A–§D below.
> 5. Reuse only **architecture-neutral** concepts: compact checkbox artwork
>    metrics/styles, verified mouse-down / mouse-up arm-and-commit semantics,
>    optional link omission, app-owned authoritative Boolean store.
> 6. Work **one corrected phase only** (C2…C10). **C10 is Complete** — the
>    corrected checkbox roadmap is closed. Do not reopen C2–C10 unless a
>    Design Change Record authorises new work; do not start legacy Phase 6.
> 7. Prefer keyword hits in §A–§G over any “Confirmed” / “Accepted” /
>    “sole target” text in the LEGACY ARCHIVE.
> 8. **Do not read the LEGACY ARCHIVE** unless the phase reading set
>    explicitly requires a named historical fact. Default: stop before the
>    LEGACY ARCHIVE banner.
>
> The inverted “Authoritative target” block that previously forbade
> `src/custom_listview_control/` is **revoked**. See Design Change **DC-004**.

---

## A. Corrected purpose

Add optional **interactive control cells** (checkbox first) to the
**custom-drawn ListView control** under `src/custom_listview_control/`.

Goals that remain valid:

- compact custom-drawn checkbox (not a per-cell `CHECKBOX_KIND` gadget);
- clear path for future cell controls (cycle, compact button, …);
- optional / measurable size cost;
- correct behaviour under **this** package’s smart-scroll, variable-height
  wrap, selection, and regional redraw.

Goals that are **invalid** for this feature (GadTools-specific):

- integrating via `GTLV_CallBack` / `LV_DRAW`;
- refreshing via `GTLV_Top` / `GT_RefreshWindow` as the primary invalidate path;
- physical-row / wrapped-subline / `CLV_PreparedList` display-map models;
- treating GadTools as the owner of row blit/smart-scroll for checkbox pixels.

---

## B. Architecture audit — `src/custom_listview_control/` (2026-07-30)

Audit performed 2026-07-30 from package sources, public header, demo, and
`docs/CLV_CUSTOM_CONTROL_*`. At audit time no interactive checkbox existed
here; **C2–C10 implemented** the checkbox path under this package (see
§D.0). Historical present-tense notes in this section were updated at C10
where they contradicted shipped reality.

### B.1 Package role

| Item | Fact |
|------|------|
| Path | `src/custom_listview_control/` |
| Role | Experimental custom-drawn ListView viewport |
| Status | Custom-control plan Phase 5.5 complete (keyboard `NAV_*`) |
| OS gadget | Optional GadTools **scroller only** (owned by app/demo, not core) |
| Not used | `LISTVIEW_KIND`, `GTLV_Labels`, `LV_DRAW`, v1 `clv_renderer_*` |

### B.2 Public API (apps include)

| Header | Role |
|--------|------|
| `clv_control.h` | Create/destroy, columns/rows, selection, input, render |
| `clv_control_draw.h` | `CLV_DrawOps`, pens, font metrics, viewport-move result |
| `backends/clv_backend_amiga_v36.h` | WB2/3 RastPort backend |

Key types: opaque `CLV_Control`; `CLV_ControlConfig`; `CLV_ControlColumn`;
`CLV_ControlRow`; `CLV_InputEvent` / `CLV_InputType`; `CLV_Event` /
`CLV_EventType`.

### B.3 Row model

- **Authoritative unit = logical row** (never independently selectable wrap
  sublines).
- App supplies borrowed `CLV_ControlRow[]` (`cells[]`, `flags`, `user_data`).
- Control owns `CLV_RowLayout[]` (`top_y`, `content_height`, `total_height`,
  `maximum_line_count`) and wrap caches (`CLV_ControlCellWrap` /
  `CLV_ControlFrag`).
- Variable height from wrap line count + padding + `row_gap`.
- Header is **not** a row: fixed `header_bounds` above `viewport_bounds`.
- Special flag today: `CLV_CTRL_ROW_NONSELECTABLE` only.

### B.4 Column model

```c
typedef struct CLV_ControlColumn {
    CONST_STRPTR title;   /* borrowed */
    WORD width_pixels;
    UWORD alignment;      /* CLV_CellAlign */
    UWORD wrap_mode;      /* CLV_ControlWrapMode */
    UWORD flags;          /* reserved; unused today */
} CLV_ControlColumn;
```

Cell content is **text-only** (`CONST_STRPTR *cells`). No cell type, checked
state, or interactive payload. `flags` is the natural column-typing hook.

### B.5 Selection state

- Owned by control: `selected_row` (`LONG`, −1 = none).
- Single logical-row selection; multi-select deferred.
- Mouse (outside checkbox box): `CLV_INPUT_SELECT_DOWN` →
  `clv_control_hit_test` → selectable → set selection →
  `clv_control_make_visible`.
- Mouse (interactive checkbox box, C5 / §D.11):
  - other selectable row → arm + `SELECTION_CHANGED` when selection
    actually changes;
  - already selected row → arm only (no `SELECTION_CHANGED`);
  - nonselectable row → arm only (no selection event).
- Keyboard: `NAV_*` updates the same field; `NAV_ACTIVATE` →
  `CLV_EVENT_ACTIVATED` (no selection change). `CLV_INPUT_TOGGLE`
  (Space) does not change selection; toggles the selected row’s sole
  eligible checkbox when present.
- Gap band after row content: hit-test returns −1.
- Highlight covers content band; **excludes** `row_gap`.

### B.6 Viewport / coordinates / scrollbar

| Rectangle | Role |
|-----------|------|
| `bounds` | Outer control (includes 1px frame) |
| `header_bounds` | Fixed titles |
| `viewport_bounds` | Scrolling body |

Hit formula: `content_y = scroll_y + mouse_y - viewport_bounds.MinY`
(window-relative mouse).

Scrollbar: **outside** core. Demo maps GadTools `SCROLLER_KIND` ↔
`CLV_INPUT_SCROLL_*` / `SCROLL_POSITION`. Core exposes `scroll_y`,
`content_height`, `viewport_height` only.

### B.7 Smart-scroll path (control-owned)

Compile flag `CLV_ENABLE_SMART_SCROLL` (default on).

1. App: `clv_control_render_scrolled(c, previous_scroll_y)`
2. Eligibility → `CLV_DrawOps.move_viewport_pixels` (`ScrollRasterBF` /
   `ScrollRaster` in `clv_backend_amiga_v36.c`)
3. Exposed band → `clv_control_paint_viewport_area`
4. Else full `clv_control_render_viewport`

**Never smart-scroll** across resize or selection+`make_visible` that changed
`scroll_y` (demo uses full viewport paint). Checkbox pixels move with the
viewport blit; newly exposed rows paint from the owned snapshot via
`paint_viewport_area` (first-line-band geometry). Exposed-band expansion
covers `cell_padding_y + line_height` so straddling checkbox artwork is not
half-blit / half-clipped (C6).

### B.8 Input dispatch

Core is **IntuiMessage-free**. Demo translates IDCMP → `CLV_InputEvent`.

| Input | Core behaviour today |
|-------|----------------------|
| `CLV_INPUT_SELECT_DOWN` | Row hit-test + selection; may arm checkbox |
| `CLV_INPUT_SELECT_UP` | Commit armed checkbox → `CELL_TOGGLED`, or cancel |
| `CLV_INPUT_POINTER_MOVE` | Defined; no-op |
| `CLV_INPUT_SCROLL_*` | Adjust `scroll_y` |
| `CLV_INPUT_NAV_*` | Keyboard selection |
| `CLV_INPUT_NAV_ACTIVATE` | `CLV_EVENT_ACTIVATED` only (never toggles) |
| `CLV_INPUT_TOGGLE` | Space: sole eligible checkbox on selected row → `CELL_TOGGLED` |

Hit-test API: `clv_control_hit_test` (internal) — logical row; checkbox
box hit uses `clv_ctrl_checkbox_resolve_rect` inside `handle_input` (C4).

### B.9 Invalidation / redraw

| API | Use |
|-----|-----|
| `clv_control_render(c, 0)` | Full frame + header + viewport |
| `clv_control_render(…, CLV_RENDER_VIEWPORT_ONLY)` | Viewport only |
| `clv_control_render_logical_rows(c, a, b)` | Up to two logical rows (selection) |
| `clv_control_render_scrolled(c, prev)` | Post-scroll smart or full |

Layout invalidate on set_columns/rows/padding/gap; rebuild on bounds/render/
input when `!layout_valid`. Invalidate also clears verified-click arm (C6).
**No per-cell invalidate API** — closest is regional row paint.

### B.10 Public event mechanism

Synchronous `CLV_Event` from `clv_control_handle_input` (not callbacks).
**At most one `CLV_EventType` per successful `handle_input` filling** —
selection and toggle are never a compound event.

| Type | Meaning |
|------|---------|
| `CLV_EVENT_NONE` | No change |
| `CLV_EVENT_SELECTION_CHANGED` | `row`, `previous_row`, `value`=scroll_y |
| `CLV_EVENT_SCROLL_CHANGED` | Scroll moved |
| `CLV_EVENT_ACTIVATED` | `NAV_ACTIVATE` only |
| `CLV_EVENT_CELL_TOGGLED` | Verified SELECT_UP commit or `CLV_INPUT_TOGGLE` (Space) |

`CLV_Event` also carries `column`, `row_user_data`, `previous_value`,
`cell_value` for `CELL_TOGGLED` (§D.11).

App pattern on `CELL_TOGGLED`: update the authoritative Boolean store,
then `clv_control_render_logical_rows(c, row, -1)` (full demo wiring C8).

### B.11 Safe integration points (corrected)

| Stage | Where | Notes |
|-------|-------|-------|
| Model | `clv_control.h` — type mask + `CLV_ControlCell` / `control_cells` | Per §D.11; C2 adds ABI |
| Snapshot | Internal copy on `set_rows` + `clv_control_set_checkbox_value` | Never write borrowed app memory by default |
| Layout | `clv_control_layout.c` / wrap | First-line-band geom in C3 |
| Draw | `clv_ctrl_paint_row_content` (`clv_control_render.c`) | Via `CLV_DrawOps`; C3 |
| Hit-test | Extend `clv_control_hit_test` / helpers in `clv_control_input.c` | Column + control box; C4 done |
| Input | `handle_input` SELECT_DOWN / SELECT_UP / TOGGLE | Arm private; C4–C7 |
| Event | `CLV_EVENT_CELL_TOGGLED` + extended `CLV_Event` | §D.11; C4 emits |
| Redraw | `clv_control_render_logical_rows` | No new cell-invalidate API |
| Keyboard | **Space** → `CLV_INPUT_TOGGLE` sole checkbox; `NAV_ACTIVATE` unchanged | C7 done |
| Demo | `examples/custom_control_demo/` | IDCMP + paint policy in app; C8 |

### B.12 Forbidden transplants from mis-scoped GadTools cellctl

Do **not** copy: `GTLV_*`, `LV_DRAW`, `CLV_Renderer` / `clv_bind_cellctl`,
physical-row/subline maps, `CLV_PreparedList` label detach, “paint only on
physical subline 0”, `GTLV_Top` nudge refresh, or the assumption that GadTools
owns ScrollRaster for checkbox pixels.

---

## C. What may be reused (architecture-neutral only)

From the mis-scoped GadTools work and general design, **concepts only**
(reimplement inside `custom_listview_control/`):

| Concept | Reuse as |
|---------|----------|
| Compact box size defaults (~9×9), min/max clamp, pad | Appearance defaults |
| Plain / 3D frame; tick / cross / fill marks | Artwork policy |
| Selected-row pen choice; disabled dither/ghost | Visual variants |
| Verified button-up: arm on down inside control; commit on up still inside; cancel if left | Input semantics (`SELECT_DOWN`/`SELECT_UP` already exist) |
| App-owned authoritative Boolean; control may cache/snapshot for paint | Ownership |
| Optional omission / measurable size delta | Build discipline |
| Disabled / display-only / interactive flags | Behaviour matrix |

Do **not** reuse: file layout under `src/custom_listview/clv_cellctl_*`,
`CLV_ENABLE_CELLCTL*`, GadTools example `05_draw_cellctl_checkbox`, host tests
that assume that ABI, or DC-001’s “must not live under custom_listview_control”.

---

## D. Replacement implementation plan (custom_listview_control)

**Gate:** Corrected phases **C0–C10 are Complete** (2026-07-30). Further
checkbox work needs a new Design Change / phase card. Do not mechanically
port `clv_cellctl_*`.

**Naming note:** This package already uses `CLV_Control` / `clv_control_*` /
`CLV_CTRL_*`. New symbols must extend that family. Do **not** introduce a
second `clv_cellctl_*` tree here.

### D.0 Corrected phase status

| Phase | Title | Status |
|------:|-------|--------|
| — | Legacy GadTools Phases 0–5 (`clv_cellctl_*`) | **Frozen / mis-scoped** — retain code; do not extend |
| C0 | Corrective audit + plan | **Complete** |
| C1 | Design lock for control architecture | **Complete** (2026-07-30) — see §D.1 / §D.11 |
| C2 | Optional build / data-model skeleton | **Complete** (2026-07-30) — see §D.2 |
| C3 | Checkbox geometry + paint in control draw path | **Complete** (2026-07-30) — see §D.3 |
| C4 | Cell hit-test + verified SELECT_DOWN/UP commit | **Complete** (2026-07-30) — runtime-verified on Amiga/emulator; see §D.4 |
| C5 | Selection vs checkbox interaction + events | **Complete** (2026-07-30) — see §D.5 |
| C6 | Smart-scroll, wrap, resize, regional redraw | **Complete** (2026-07-30) — see §D.6 |
| C7 | Keyboard toggle (Space; NAV_ACTIVATE unchanged) | **Complete** (2026-07-30) — see §D.7 |
| C8 | Demo + documentation | **Complete** (2026-07-30) — see §D.8 |
| C9 | Size / regression validation | **Complete** (2026-07-30) — see §D.9 |
| C10 | Closure audit | **Complete** (2026-07-30) — see §D.10 |

### D.1 Phase C1 — Design lock (complete)

C1 answers (locked 2026-07-30 after review). Full sketches: **§D.11**.

| # | Topic | Locked decision |
|---|-------|-----------------|
| 1 | Column typing | `CLV_ControlColumn.flags` with a **reserved type mask** (see §D.11) |
| 2 | Per-cell state | Optional parallel `CLV_ControlCell` via `CLV_ControlRow.control_cells`; `NULL` = no interactive cells |
| 3 | Snapshot ownership | App owns authoritative Boolean; control **copies** into an internal snapshot on `set_rows` / setter; commit mutates **snapshot only**; never write borrowed app memory by default |
| 4 | Setter | Public `clv_control_set_checkbox_value(control, row, column, value)` for accept / reject-restore / async update without full rebuild |
| 5 | Event shape | Extend `CLV_Event` for `CLV_EVENT_CELL_TOGGLED` with `column`, `row_user_data`, `previous_value`, `cell_value` (plus existing `row`) |
| 6 | Selection + toggle | Two separate events across verified click: SELECT_DOWN may emit `SELECTION_CHANGED`; SELECT_UP may emit `CELL_TOGGLED` — no compound event |
| 7 | Wrap placement | Vertically centre checkbox in the **first content text-line band** (including normal top cell padding) — not mid-row of a tall wrap |
| 8 | Keyboard | Space toggles sole checkbox column of selected row; `NAV_ACTIVATE` keeps row-activation meaning; defer keyboard toggle if multiple checkbox columns |
| 9 | Arm state | Private inside `CLV_Control` (no public input-state object) |
| 10 | Redraw API | Use existing `clv_control_render_logical_rows`; no cell-specific invalidate API in the first implementation |
| 11 | Legacy GadTools tree | Frozen demos/profiles remain non-authoritative; do not extend |

**Exit criteria met:** Design answers locked; DC-005 records the C1 package.
C2 is authorised for skeleton implementation.

### D.2 Phase C2 — Skeleton

#### Agent reading set (C2 only — do not read LEGACY ARCHIVE)

1. SCOPE CORRECTION + rules at top of this document
2. §B.2–B.4, §B.10–B.11
3. §D.0, §D.2 (this card), **§D.11** (locked ABI)
4. §G (session protocol), §F (status)
5. Latest entry in `docs/CLV_CONTROL_CELLS_DEVELOPER_LOG.md`
6. Source: `clv_control.h`, `clv_control_internal.h`, `clv_control.c`
   (`create` / `destroy` / `set_rows`), `clv_control_input.c` (zero new
   event fields only)
7. Demo compile touch: `examples/custom_control_demo/main.c`
   (`control_cells = NULL` only)

#### Objective

Add the C1 public/private ABI and an internal checkbox snapshot so later
phases have types and storage. **No** checkbox paint, hit-test, arm/commit,
or Space toggle.

#### Required work

- In `clv_control.h`: column type mask; cell flag/value constants;
  `CLV_ControlCell`; `control_cells` on `CLV_ControlRow`;
  `CLV_EVENT_CELL_TOGGLED`; extend `CLV_Event`; declare
  `clv_control_set_checkbox_value`.
- Lock cell flag names in C2:
  `CLV_CTRL_CELL_F_VISIBLE`, `CLV_CTRL_CELL_F_ENABLED`,
  `CLV_CTRL_CELL_F_INTERACTIVE`; values `UNCHECKED` / `CHECKED`.
- Internal: snapshot storage on `CLV_Control`; copy from `control_cells` in
  `set_rows`; free on replace/destroy; private arm fields present but unused
  for behaviour.
- Implement setter: update snapshot only; return FALSE on bad row/column/type/
  value; **do not paint**.
- Existing selection/scroll/NAV paths: zero new `CLV_Event` fields; behaviour
  unchanged. `SELECT_UP` remains a no-op for toggles.
- Demo: initialize `control_cells` to NULL; still builds and runs as before.
- Record executable size baseline for `custom-control-demo`.

#### Out of scope (C2)

- Drawing checkboxes; geometry resolve; hit-test column/box.
- Arming, `CELL_TOGGLED` emission, SELECT_UP commit.
- Demo IDCMP SELECT_UP / Space wiring for toggles.
- Optional Makefile omit flag (defer sizing strategy nuances to C9).
- Any change under `src/custom_listview/clv_cellctl_*`.

#### Validation

- `make custom-control-demo` (and usual custom-control variants) link cleanly.
- Manual: demo still selects/scrolls; no checkbox UI claimed.
- Header comments must not claim working interactive checkboxes.

#### Deliverables

- Updated headers + snapshot/setter implementation.
- Demo still compilable.
- Size baseline in completion record / developer log.
- Master plan §D.0 / §F + developer-log entry.

#### Exit criteria

- ABI matches §D.11 (plus named cell flags).
- Snapshot copy/free correct on `set_rows` / destroy.
- Setter works on snapshot without painting.
- No behavioural checkbox interaction.
- Docs updated; phase marked Complete.

#### Phase completion record

**Status:** Complete (2026-07-30)

**Implemented:**
- Public ABI in `clv_control.h`: column type mask (`TEXT` / `CHECKBOX`);
  `CLV_ControlCell` + `CLV_CTRL_CELL_F_*` / `UNCHECKED` / `CHECKED`;
  `CLV_ControlRow.control_cells`; `CLV_EVENT_CELL_TOGGLED`; extended
  `CLV_Event`; `clv_control_set_checkbox_value`.
- Internal owned `cell_snapshot` (row-major) copied on `set_rows` /
  `set_columns`; freed on replace/destroy. Private arm fields present,
  cleared on snapshot rebuild; unused for behaviour.
- Setter updates snapshot value only (no paint); rejects bad
  row/column/type/value or missing snapshot.
- `clv_ctrl_clear_event` zeros new event fields; selection/scroll/NAV
  paths unchanged; `SELECT_UP` still a no-op for toggles.
- Demo sets `control_cells = NULL`; header comments do not claim working
  interactive checkboxes.

**Commands / results:**
- `make custom-control-demo` — link OK; `bin/custom-control-demo` **37772** bytes
  (VBCC `+aos68k -O2 -size -final`, `-cpu=68000`, smart scroll on)
- `make custom-control-demo CLV_ENABLE_SMART_SCROLL=0` — **36528** bytes
- `make custom-control-demo-log` — **52348** bytes
- `make custom-control-demo-bench` — **54340** bytes
- Parallel VBCC builds can collide on shared temp `.asm`; sequential rebuild
  used for the sizes above.

**Not done (deferred):** paint, hit-test, arm/commit, Space toggle, demo
IDCMP SELECT_UP / Space wiring, optional omit flag (C9).

**Next:** C3 may begin — paint from snapshot inside first-line-band geometry;
still no mouse commit required.

#### Next phase starting conditions

C3 may begin when C2 is Complete: paint from snapshot inside first-line-band
geometry; still no mouse commit required.

---

### D.3 Phase C3 — Checkbox paint

#### Agent reading set

1. SCOPE CORRECTION
2. §B.6–B.7, §B.9, §B.11
3. §C (artwork **concepts** only — do not copy GadTools `clv_cellctl_*` files)
4. §D.0, §D.3, §D.11 (geometry + appearance)
5. §G, §F
6. Latest developer-log (must show C2 Complete)
7. Source: `clv_control_render.c` (`clv_ctrl_paint_row_content`),
   `clv_control_draw.h` / backend ops, layout `top_y` / line height

#### Objective

Draw checked/unchecked (and disabled/selected variants) from the control
snapshot inside the first content line band. No input commit yet.

#### Required work

- Resolve checkbox rectangle: column bounds + first text-line band + pad;
  alignment per column.
- Paint via `CLV_DrawOps`; respect clip and selection pens.
- Appearance defaults (~9×9, plain/tick) as architecture-neutral policy;
  shrink/clamp as needed for short rows.
- Read values from **snapshot**, not live borrowed `control_cells`.
- Optional visual rows in demo, or document “paint path only; matrix in C8”.

#### Out of scope

- Hit-test / arm / SELECT_UP / `CELL_TOGGLED`.
- Smart-scroll-specific tests (C6); keyboard (C7).

#### Validation

- Build custom-control demo (or focused binary).
- Emulator/visual when possible: boxes on checkbox column; first-line-band
  on wrapped rows; selected/disabled readable.
- Label unverified claims if no emulator run.

#### Deliverables

- Paint path + appearance helper if needed; docs; log.

#### Exit criteria

- Snapshot-driven paint in clip; first-line-band placement.
- No required input behaviour change.
- Phase record complete.

#### Phase completion record

**Status:** Complete (2026-07-30)

**Implemented:**
- New `clv_control_checkbox.c`: first-line-band geometry
  (`clv_ctrl_checkbox_resolve_rect`) and snapshot-driven paint
  (`clv_ctrl_checkbox_paint`) via `CLV_DrawOps`.
- Appearance defaults: ~9×9 plain outline + tick; shrink pad then box to
  min 5; selected-row pens; disabled sparse/ghost tick via separator pen.
- `clv_ctrl_paint_row_content` paints checkbox columns from the owned
  snapshot (skips text frags on `CLV_CTRL_COL_TYPE_CHECKBOX`).
- Demo: `On` checkbox column with varied VISIBLE/ENABLED/INTERACTIVE/
  checked states (including wrapped rows and a disabled row). Paint path
  only — clicks do not toggle.

**Commands / results:**
- `make custom-control-demo` — link OK; `bin/custom-control-demo` **40460** bytes
  (VBCC `+aos68k -O2 -size -final`, `-cpu=68000`, smart scroll on)
- `make custom-control-demo CLV_ENABLE_SMART_SCROLL=0` — **39216** bytes
- `make custom-control-demo-log` — **55028** bytes
- `make custom-control-demo-bench` — **57116** bytes
- Sequential rebuilds used (parallel VBCC can collide on shared temp `.asm`).

**Not done (deferred):** hit-test / arm / SELECT_UP / `CELL_TOGGLED` (C4–C5);
smart-scroll-specific checkbox tests (C6); Space toggle (C7); full demo
interaction matrix (C8).

**Runtime note:** Amiga/emulator visual check (2026-07-30): checkbox column
is the last demo column (`On`); boxes display correctly (checked/unchecked
variants); checked state persists correctly while scrolling up and down
(viewport blit / exposed-band paint).

**Next:** C4 may begin — cell hit-test + verified SELECT_DOWN/UP commit
against the same resolve geometry.

#### Next phase starting conditions

C4 may begin when C3 is Complete.

---

### D.4 Phase C4 — Mouse commit (arm / cancel / toggle event)

#### Agent reading set

1. SCOPE CORRECTION
2. §B.5–B.8, §B.10–B.11
3. §D.0, §D.4, §D.5 (selection rules preview), §D.11
4. §G, §F
5. Latest log (C3 Complete)
6. Source: `clv_control_input.c`, hit-test helpers, snapshot/setter

#### Objective

Verified SELECT_DOWN / SELECT_UP commit against the control box; mutate
snapshot; emit `CLV_EVENT_CELL_TOGGLED` with full fields.

#### Required work

- Hit-test: column + control rectangle (same geom as paint).
- Arm privately on SELECT_DOWN inside visible+enabled+interactive checkbox.
- SELECT_UP: commit if same armed identity; else cancel.
- On commit: toggle snapshot; fill event (`row`, `column`, `row_user_data`,
  `previous_value`, `cell_value`); return TRUE from `handle_input`.
- Cancel arm on: miss, other row/column, `set_rows` / `set_columns` /
  `set_bounds`, destroy.
- Outside-box SELECT_DOWN: existing row selection path.
- Do not paint inside `handle_input` (app refresh wiring finalized in C8).

#### Out of scope

- Tightening already-selected vs new-row selection matrix (C5).
- Smart-scroll (C6); Space (C7); polished demo (C8).

#### Validation

- Build; optional host/unit checks for arm/cancel identity.
- Emulator or documented checklist: commit once; cancel on release-outside.
- Event fields populated; snapshot matches `cell_value`.

#### Deliverables

- Input/hit-test changes; docs; log.

#### Exit criteria

- One commit per valid activation; cancel clears arm without toggle event.
- Snapshot-only mutation; no write-through to app `control_cells`.
- Phase record complete.

#### Phase completion record

**Status:** Complete (2026-07-30)

**Implemented:**
- `clv_ctrl_hit_interactive_checkbox` — row hit-test +
  `clv_ctrl_checkbox_resolve_rect` box test; requires snapshot
  VISIBLE|ENABLED|INTERACTIVE.
- SELECT_DOWN: arm privately on interactive checkbox hit; outside-box
  clears arm and keeps existing row-selection path (C5 matrix polish
  deferred).
- SELECT_UP: commit same armed identity → toggle owned snapshot only,
  emit `CLV_EVENT_CELL_TOGGLED` with `row`, `column`, `row_user_data`,
  `previous_value`, `cell_value`; else cancel arm with no event.
- Arm cancel on: checkbox miss / other cell, `set_rows` / `set_columns`
  (via snapshot refresh), `set_bounds`, `destroy`.
- `handle_input` does not paint.
- Demo: minimal IDCMP SELECT_UP delivery + `CELL_TOGGLED` row refresh
  (full interaction matrix remains C8).

**Commands / results:**
- `make custom-control-demo` — link OK; `bin/custom-control-demo` **41768** bytes
  (VBCC `+aos68k -O2 -size -final`, `-cpu=68000`, smart scroll on)
- Forced object rebuild + `make custom-control-demo CLV_ENABLE_SMART_SCROLL=0`
  — **41100** bytes
- `make custom-control-demo-log` — **56644** bytes
- `make custom-control-demo-bench` — **58524** bytes
- Sequential rebuilds (object wipe between smart-off / smart-on).

**Not done (deferred):** selection-vs-toggle matrix tightening (C5);
smart-scroll / resize / wrap formal checklist (C6); Space (C7); polished
demo matrix / authoritative-store sync (C8).

**Runtime note:** Amiga/emulator runtime confirmed (2026-07-30): interactive
`On` checkboxes are clickable; verified down/up toggles the box and prints
`Toggled row N col 4: …` (0↔1) while selection continues to emit
`Selected logical row …`. Screenshot evidence: multi-row toggles including
re-toggle of the same cell and selection+toggle sequences.

#### Next phase starting conditions

C5 may begin when C4 is Complete.

---

### D.5 Phase C5 — Selection vs checkbox interaction

#### Agent reading set

1. SCOPE CORRECTION
2. §B.5, §B.10
3. §D.0, §D.5, §D.11 (verified-click table)
4. §G, §F
5. Latest log (C4 Complete)
6. Source: `clv_control_input.c` SELECT_DOWN path

#### Objective

Implement the two-event sequence so selection and toggle never merge into a
compound event or silently drop a real selection change.

#### Required work

- SELECT_DOWN on checkbox, **different** selectable row → may emit
  `SELECTION_CHANGED` when selection actually changes; also arm.
- SELECT_DOWN on checkbox, **already selected** row → **no**
  `SELECTION_CHANGED`; arm only.
- SELECT_UP commit → `CELL_TOGGLED` only.
- Cancelled up → no toggle event.
- Document app pattern: on `CELL_TOGGLED`, update authoritative store, then
  `clv_control_render_logical_rows(c, row, -1)` (full demo wiring in C8).

#### Out of scope

- Smart-scroll (C6); keyboard (C7); full demo UX (C8).

#### Validation

- Checklist: other-row click, same-row click, release outside, rapid clicks.
- At most one `CLV_EventType` per successful `handle_input` filling.

#### Deliverables

- Input policy implementation; docs; log.

#### Exit criteria

- Matches §D.11 verified-click table.
- Phase record complete.

#### Phase completion record

**Status:** Complete (2026-07-30)

**Implemented:**
- `clv_control_input.c` SELECT_DOWN policy matches §D.11 verified-click
  table:
  - checkbox + other selectable row → arm + `SELECTION_CHANGED` when
    selection actually changes (never silently dropped);
  - checkbox + already-selected row → arm only (no selection/scroll
    event from that path);
  - checkbox + nonselectable → arm only;
  - outside box → clear arm; existing row-selection path (may still
    emit `SCROLL_CHANGED` on same-row make_visible).
- SELECT_UP commit → `CELL_TOGGLED` only; cancel → no event.
- At most one `CLV_EventType` per `handle_input` (documented in header).
- App pattern documented: on `CELL_TOGGLED`, update authoritative store,
  then `clv_control_render_logical_rows(c, row, -1)` (demo paints from
  snapshot; full store sync remains C8).
- §B.5 / §B.10 / public header / demo README updated.

**Commands / results:**
- `make custom-control-demo` — link OK; `bin/custom-control-demo` **41804** bytes
  (VBCC `+aos68k -O2 -size -final`, `-cpu=68000`, smart scroll on)
- Object wipe + `make custom-control-demo CLV_ENABLE_SMART_SCROLL=0`
  — **40560** bytes
- `make custom-control-demo-log` — **56880** bytes
- `make custom-control-demo-bench` — **58588** bytes
- Sequential rebuilds (object wipe between smart-off / smart-on / log / bench).

**Not done (deferred):** smart-scroll / wrap / resize checklist (C6);
Space (C7); polished demo / authoritative-store sync (C8).

**Validation checklist (code-path / link; Amiga runtime optional):**
- Other-row checkbox: SELECT_DOWN → `SELECTION_CHANGED`; SELECT_UP →
  `CELL_TOGGLED`.
- Same-row checkbox: SELECT_DOWN → no event (arm only); SELECT_UP →
  `CELL_TOGGLED`.
- Release outside → cancel, no toggle.
- Rapid clicks: each `handle_input` fills at most one event type.
- Cross-link succeeded for smart / nosmart / log / bench (VBCC `+aos68k`).

#### Next phase starting conditions

C6 may begin when C5 is Complete.

---

### D.6 Phase C6 — Smart-scroll, wrap, resize

#### Agent reading set

1. SCOPE CORRECTION
2. §B.3, §B.6–B.7, §B.9
3. §D.0, §D.6, §D.11 (first-line-band)
4. §G, §F
5. Latest log (C5 Complete)
6. Source: `clv_control_render.c` (`render_scrolled`, paint), layout
   invalidate, `set_bounds` / `set_rows`

#### Objective

Checkbox pixels remain correct under control-owned smart-scroll, wrap, and
relayout; arm cancelled on structural change.

#### Required work

- After scroll: blit moves pixels; exposed band paints current snapshot.
- Selection + `make_visible` that changes `scroll_y`: keep full viewport
  paint rule; verify checkboxes not stale.
- Wrapped rows: box stays on first-line band after resize/rewrap.
- `set_bounds` / `set_rows` / column replace: cancel arm; invalidate layout.

#### Out of scope

- Keyboard (C7); demo polish beyond what tests need (C8).

#### Validation

- Emulator checklist: line scroll, jump, resize, wrap, arm-then-resize.
- Smart-scroll-off twin if available.

#### Deliverables

- Fixes as needed; test notes; log.

#### Exit criteria

- No ghost/stale marks in tested scroll/relayout cases.
- Phase record complete.

#### Phase completion record

**Status:** Complete (2026-07-30)

**Implemented:**
- Smart-scroll exposed-band expansion now grows by
  `cell_padding_y + line_height` (first-line cell) so straddling checkbox
  artwork is fully redrawn from the owned snapshot after blit.
- Regional / full viewport paint paths already draw checkbox columns via
  `clv_ctrl_checkbox_paint` (first-line-band geom); selection +
  `make_visible` scroll remains full viewport (demo policy unchanged).
- Arm cancel on structural change: `set_bounds` (explicit), `set_rows` /
  `set_columns` (snapshot refresh), plus `clv_control_layout_invalidate`
  (padding / gap / any invalidate).
- Demo README C6 checklist (line scroll, jump, resize/wrap, arm-then-resize,
  smart-off twin).

**Commands / results:**
- Wipe `build/custom_listview_control/*.o` + example obj, then
  `make custom-control-demo` — **41836** bytes
- Wipe + `make custom-control-demo CLV_ENABLE_SMART_SCROLL=0` — **41152** bytes
- Wipe + `make custom-control-demo` / `-log` / `-bench` — link OK;
  log **56912**, bench **58620**
- VBCC `+aos68k -O2 -size -final`, `-cpu=68000`

**Not done (deferred):** Space toggle (C7); polished demo / authoritative
store sync (C8); Amiga/emulator visual checklist left unchecked (code-path
locked; runtime optional).

**Runtime note:** Cross-link only in this session. Emulator checklist is in
`examples/custom_control_demo/README.md` (Phase C6 section).

#### Next phase starting conditions

C7 may begin when C6 is Complete.

---

### D.7 Phase C7 — Keyboard (Space)

#### Agent reading set

1. SCOPE CORRECTION
2. §B.5, §B.8
3. §D.0, §D.7, §D.11
4. §G, §F
5. Latest log (C6 Complete)
6. Source: `clv_control_input.c` NAV_* path; demo RAWKEY translation

#### Objective

Space toggles the selected row’s sole eligible checkbox column;
`NAV_ACTIVATE` remains row activation only.

#### Required work

- Prefer demo RAWKEY → neutral `CLV_InputEvent` (add a dedicated input type
  only if unavoidable; document any API addition as a Design Change).
- If selected row has exactly one checkbox column with
  VISIBLE|ENABLED|INTERACTIVE: toggle snapshot; emit `CELL_TOGGLED`.
- Zero or multiple checkbox columns: no toggle.
- Disabled / display-only: no toggle.
- `NAV_ACTIVATE` → still `CLV_EVENT_ACTIVATED` only.

#### Out of scope

- Multi-column focus model; full demo docs (C8).

#### Validation

- Space on single-checkbox row; multi-column deferral; NAV_ACTIVATE unchanged.

#### Deliverables

- Input + demo key translate as needed; docs; log.

#### Exit criteria

- Matches C1 keyboard lock.
- Phase record complete.

#### Phase completion record

**Status:** Complete (2026-07-30)

**Summary:** Demo RAWKEY Space (`0x40`) maps to new neutral
`CLV_INPUT_TOGGLE` (DC-006 — unavoidable; `NAV_ACTIVATE` must stay
activation-only). Core finds the selected row’s sole
VISIBLE|ENABLED|INTERACTIVE checkbox column, toggles the owned snapshot,
emits `CELL_TOGGLED`. Zero or multiple eligible columns, display-only,
disabled, no selection, or keyboard disabled → no event. Pending mouse
arm is cleared on TOGGLE. `NAV_ACTIVATE` / Return unchanged
(`CLV_EVENT_ACTIVATED` only).

**Files:** `clv_control.h`, `clv_control_input.c`, `clv_control_internal.h`,
`examples/custom_control_demo/main.c`, README, this plan (§B / §D / §E.3 /
§F), developer log.

**Validation:** Code-path audit — single eligible → toggle; display-only /
disabled / no selection → defer; multi-eligible finder returns FALSE;
NAV_ACTIVATE path untouched. Cross-link VBCC `+aos68k` (see log for sizes).
Amiga/emulator interactive Space pass optional.

**Not done (deferred):** Multi-column focus model; polished demo /
authoritative-store sync (C8).

#### Next phase starting conditions

C8 may begin — C7 Complete.

---

### D.8 Phase C8 — Demo and integration documentation

#### Agent reading set

1. SCOPE CORRECTION
2. §D.0, §D.8, §D.11
3. §G, §F
4. Latest log (C7 Complete preferred; note if starting earlier)
5. `examples/custom_control_demo/`
6. Cross-link only: `docs/CLV_CUSTOM_CONTROL_DESIGN_AND_IMPLEMENTATION_PLAN.md`

#### Objective

Authoritative demo: data → input → both events → app store → regional
repaint; integrator-facing docs.

#### Required work

- Demo: SELECT_UP translation; handle `SELECTION_CHANGED` and
  `CELL_TOGGLED`; sync authoritative Booleans (`user_data`);
  `render_logical_rows` after toggle.
- Show interactive / disabled / display-only rows.
- Update custom-control README/docs; mark GadTools `clv_cellctl_*` as legacy
  non-authoritative.
- Public header comments: experimental checkbox cells; ownership / `set_rows`.

#### Out of scope

- Size campaign (C9); formal closure (C10).

#### Validation

- Emulator demo checklist.
- Docs do not present GadTools cellctl as the product path.

#### Deliverables

- Demo + docs; log.

#### Exit criteria

- Integrator can follow demo without private headers.
- Phase record complete.

#### Phase completion record

**Status:** Complete (2026-07-30)

**Delivered:**
- Demo authoritative store: mutable `g_demo_ctrl_store`; `user_data` points
  at On-column `UBYTE`; `CELL_TOGGLED` syncs `ev.cell_value` then
  `render_logical_rows`. SELECT_UP / SELECTION_CHANGED / interactive /
  display-only / disabled rows already present (C4–C7).
- Integrator docs in `examples/custom_control_demo/README.md` (public header
  only; row-kind table; event pattern).
- Legacy marks: demo README, `examples/05_draw_cellctl_checkbox/README.md`,
  `docs/CLV_CUSTOM_CONTROL_DESIGN_AND_IMPLEMENTATION_PLAN.md` banner,
  `docs/CLV_MODULE_ARCHITECTURE.md` cellctl notes.
- `clv_control.h`: experimental checkbox + ownership / `set_rows` /
  `set_checkbox_value` comments; GadTools cellctl called out as legacy.

**Validation:**
- `make custom-control-demo` (smart) → `bin/custom-control-demo` **43496**
- `make custom-control-demo CLV_ENABLE_SMART_SCROLL=0` → **42812**
- Docs do not present GadTools cellctl as the product path.
- Emulator interactive checklist left for operator (items in demo README C8).

**Out of scope left for later:** C9 size campaign / formal regression table;
C10 closure.

**Next:** C9 may begin when authorised (size + regression).

#### Next phase starting conditions

C9 may begin when C8 is Complete.

---

### D.9 Phase C9 — Size and regression

#### Agent reading set

1. SCOPE CORRECTION
2. §D.0, §D.9 (this card), §D.11
3. §G, §F
4. Latest log (C8 Complete)
5. Makefile custom-control targets; prior size notes in custom-control plan /
   developer log

#### Objective

Measure feature cost and run regression checks (smart on/off, baseline demos).

#### Required work

- Record `custom-control-demo` / nosmart (and any checkbox-focused twin) sizes
  with same flags as prior baselines.
- Regression: selection, scroll, wrap, toggle, Space if present.
- Note any optional-omit strategy if introduced; otherwise document “always
  linked with control package for now”.

#### Out of scope

- New features; closure narrative (C10).

#### Validation

- Size table in docs/log; regression checklist results.

#### Deliverables

- Measurements + log; update size docs if project has a custom-control size
  section.

#### Exit criteria

- Numbers recorded; no unexplained regressions in checklist.
- Phase record complete.

#### Phase completion record

**Status:** Complete (2026-07-30)

**Delivered:**
- Formal VBCC size campaign (full wipe of control objs between smart/off):
  smart **43496**, nosmart **42236** (+1260 smart), log **58736**,
  bench **60244**. Preserved twin `bin/custom-control-demo-nosmart`.
- Object attribution (smart tree): `clv_control_checkbox.o` **3264**.
- Cumulative checkbox feature cost vs pre-C2 appearance demo (37036):
  **+6460** smart executable.
- Optional omit: **not introduced** — checkbox remains always linked with
  the control package (`CLV_CUSTOM_CONTROL_OBJS` includes
  `clv_control_checkbox.o`). Documented in demo README / log.
- Regression checklist (selection, scroll, wrap, mouse toggle, Space)
  code-path audited; no unexplained behavioural regressions. C8 nosmart
  42812 corrected to 42236 (incomplete wipe artifact; smart unchanged).

**Validation:**
- `make custom-control-demo CLV_ENABLE_SMART_SCROLL=0` → **42236**
  (copied to `bin/custom-control-demo-nosmart`)
- `make custom-control-demo` → **43496** (matches C8 smart)
- `make custom-control-demo-log` → **58736**
- `make custom-control-demo-bench` → **60244**
- Code-path regression PASS (see developer log / demo README C9).
- Amiga/emulator interactive re-pass left for operator where prior
  phases already recorded runtime (C4 mouse) or package-era checklists.

**Not done (deferred):** Makefile checkbox omit flag; multi-column
keyboard focus. (Closure audit recorded under C10.)

#### Next phase starting conditions

C10 may begin — C9 Complete.

---

### D.10 Phase C10 — Closure audit

#### Agent reading set

1. SCOPE CORRECTION (entire rules list)
2. §A–§G (skim for contradictions)
3. §D.0 status table vs reality
4. Latest log (C9 Complete)
5. Spot-check: no GadTools ListView dependency in checkbox path; scrollbar
   remains external

#### Objective

Confirm the corrected feature is complete, documented, and that legacy
GadTools cellctl is clearly non-authoritative.

#### Required work

- Audit public API vs §D.11.
- Confirm LEGACY ARCHIVE not referenced as live authority in new docs.
- List known limitations / follow-ups.
- Mark overall status Complete or Partially complete with evidence.

#### Out of scope

- New implementation beyond tiny doc fixes.

#### Validation

- Checklist signed in completion record.

#### Deliverables

- Closure record in master plan §F + developer log.

#### Exit criteria

- No open “must fix” items without owners; docs consistent.
- Phase record complete.

#### Phase completion record

**Phase:** C10 — Closure audit  
**Status:** Complete  
**Date:** 2026-07-30  

**Public API vs §D.11 (spot-check `clv_control.h` / internals):**

| Lock | Evidence | Result |
|------|----------|--------|
| Column type mask `CLV_CTRL_COL_TYPE_*` | `clv_control.h` | PASS |
| `CLV_ControlCell` + `control_cells` parallel descriptors | `clv_control.h` | PASS |
| Snapshot copy on `set_rows`; no write-through by default | header contract + C2–C8 docs | PASS |
| `clv_control_set_checkbox_value` | public decl; no paint | PASS |
| `CLV_EVENT_CELL_TOGGLED` + `column` / `row_user_data` / `previous_value` / `cell_value` | `CLV_Event` | PASS |
| One event per `handle_input`; two-step mouse | header + input path | PASS |
| Private arm (`control_armed` / `armed_*`) not public | `clv_control_internal.h` | PASS |
| First-line-band geometry | `clv_control_checkbox.c` | PASS |
| Space → `CLV_INPUT_TOGGLE`; `NAV_ACTIVATE` unchanged | enum + DC-006 | PASS |
| Redraw via `render_logical_rows` (no cell-invalidate API) | public API | PASS |

**Architecture spot-checks:**

| Check | Result |
|-------|--------|
| Checkbox path (`clv_control_checkbox.c` / paint / resolve / input) uses no `LISTVIEW_KIND` / `GTLV_*` / `LV_DRAW` / `clv_cellctl_*` | PASS |
| Core remains IntuiMessage-free; demo owns IDCMP translate | PASS |
| Scrollbar remains external (`SCROLLER_KIND` in demo only) | PASS |
| GadTools `clv_cellctl_*` marked legacy in `clv_control.h`, demo README, module architecture, custom-control plan | PASS |
| LEGACY ARCHIVE not cited as live authority in §A–§G / demo / package docs | PASS |

**Doc consistency fixes this phase (tiny):**

- Header / SCOPE rule 6 / §D gate / §B audit intro brought current with
  shipped C2–C10 reality (removed stale “no checkbox today” / “C4 authorised”).

**Open “must fix” items:** none.

**Known limitations / follow-ups** (deferred; owned as product backlog —
not blocking closure):

| Item | Owner / note |
|------|----------------|
| Optional Makefile omit for `clv_control_checkbox.o` | Deferred (C9 policy: always linked) |
| Multi-column keyboard focus | Deferred since C7 |
| Indeterminate checkbox value | Deferred (out of initial scope) |
| Future cycle / compact-button cell types | Future phase cards |
| Optional Amiga/emulator re-pass of C8/C9 interactive checklists | Operator optional; prior C4 runtime + code-path audits stand |
| Eventual removal/archive of frozen GadTools `clv_cellctl_*` | Separate decision; retain for now |

**Overall corrected feature status:** **Complete** — interactive checkbox
cells on `src/custom_listview_control/` through C2–C10, with documented
deferred follow-ups only.

#### Next phase starting conditions

Corrected C2–C10 roadmap closed. Do not start legacy Phase 6. New work
needs an explicit Design Change / new phase card.

---

### D.11 Locked public/private design (C1)

> **This is the ABI / behaviour lock.** Phase cards above reference **§D.11**.
> (Formerly numbered §D.9 before the handoff-hardening pass.)

#### Column type mask

```c
#define CLV_CTRL_COL_TYPE_MASK      0x000FU
#define CLV_CTRL_COL_TYPE_TEXT      0x0000U
#define CLV_CTRL_COL_TYPE_CHECKBOX  0x0001U
/* Higher bits of CLV_ControlColumn.flags reserved for independent behaviours. */
```

Type is `(flags & CLV_CTRL_COL_TYPE_MASK)`. Future button/cycle types consume
other low-nibble values — never OR conflicting types.

#### Parallel cell descriptor

```c
typedef struct CLV_ControlCell
{
    UBYTE flags;  /* VISIBLE / ENABLED / INTERACTIVE — names locked in C2 */
    UBYTE value;  /* UNCHECKED / CHECKED */
} CLV_ControlCell;

typedef struct CLV_ControlRow
{
    CONST_STRPTR *cells;                 /* borrowed; length == column_count */
    const CLV_ControlCell *control_cells; /* optional; NULL = no controls */
    UWORD flags;                         /* existing; keep UWORD ABI */
    APTR user_data;                      /* borrowed; copied into events */
} CLV_ControlRow;
```

`control_cells` when non-NULL has length `column_count`. Entries for
non-checkbox columns are ignored (flags/value zero). Control **copies**
descriptors into an internal snapshot at `set_rows` (and on explicit setter);
it does **not** write through the borrowed pointer by default.

#### Snapshot setter

```c
BOOL clv_control_set_checkbox_value(
    CLV_Control *control,
    LONG row,
    UWORD column,
    UBYTE value);
```

Updates the control snapshot; does not paint (caller uses
`render_logical_rows` or full render). Used for reject-restore, async updates,
and tests.

#### Events (one `CLV_Event` per `handle_input` call)

```c
/* Existing fields retained; CELL_TOGGLED fills the new ones. */
typedef struct CLV_Event
{
    UWORD type;
    LONG  row;
    LONG  previous_row;   /* SELECTION_CHANGED */
    LONG  value;          /* scroll_y on scroll/selection; unused on toggle */
    UWORD column;         /* CELL_TOGGLED */
    APTR  row_user_data;  /* CELL_TOGGLED: row's user_data at commit */
    UBYTE previous_value; /* CELL_TOGGLED */
    UBYTE cell_value;     /* CELL_TOGGLED new value (avoids clash with LONG value) */
} CLV_Event;

#define CLV_EVENT_CELL_TOGGLED  /* next free enum value after ACTIVATED */
```

Verified-click sequence (locked):

| Step | Input | Possible event |
|------|-------|----------------|
| 1 | `SELECT_DOWN` on checkbox (other row) | `SELECTION_CHANGED` if selection actually changes |
| 1b | `SELECT_DOWN` on checkbox (already selected) | none for selection; arm only |
| 2 | `SELECT_UP` still on same armed checkbox | `CELL_TOGGLED` |
| 2b | `SELECT_UP` outside / other cell / cancelled | none |

Keyboard (C7 / DC-006):

| Input | Event |
|-------|-------|
| `CLV_INPUT_TOGGLE` (Space) + selected row has exactly one VISIBLE\|ENABLED\|INTERACTIVE checkbox | `CELL_TOGGLED` |
| `CLV_INPUT_TOGGLE` + zero/multiple eligible / none selected / keyboard off | none |
| `CLV_INPUT_NAV_ACTIVATE` (Return) | `CLV_EVENT_ACTIVATED` only |

#### Private arm state (inside opaque `CLV_Control`)

```c
BOOL  control_armed;
LONG  armed_row;
UWORD armed_column;
UBYTE armed_type;   /* column type / control kind */
```

Not exposed in the public API. Cleared on cancel conditions in §D.4.

#### Geometry placement (wrapped rows)

Centre the checkbox vertically within the **first rendered text-line band** of
the logical row, including the normal top cell padding. Horizontal alignment
follows the column. Do **not** centre in the full multi-line content height.

#### Redraw

First implementation: after a successful toggle (or setter), application calls
`clv_control_render_logical_rows(control, row, -1)`. No new cell-invalidate API.

---

## E. Design Change DC-004 — Correct implementation target

**Date:** 2026-07-30  
**Phase:** Corrective (post Phase 5)  
**Status:** Accepted

**Original design (Phases 0–5):**  
Interactive checkbox cells under `src/custom_listview/` (GadTools
`LISTVIEW_KIND`). DC-001 explicitly forbade placing cell-control sources under
`src/custom_listview_control/`.

**Evidence or problem:**  
The intended product behaviour — custom-drawn rows, owned smart-scroll,
variable-height logical rows, manual hit-test/redraw — exists only in
`src/custom_listview_control/`. GadTools ListView cannot own that architecture.
Continuing Phase 6 on `clv_cellctl_*` would deepen the wrong product.

**New design:**  
Authoritative checkbox-cell implementation target is
`src/custom_listview_control/`. Mis-scoped GadTools `clv_cellctl_*` work remains
in-tree as **legacy / non-authoritative**. Replacement plan §D replaces Phases
6–10 of the old roadmap. No mechanical port.

**Why this is better:**  
Matches the intended control; reuses the package that already owns scroll,
selection, wrap, and redraw; avoids fighting `LV_DRAW` / physical rows.

**Consequences:**  
- Agents must not implement further GadTools cellctl phases.
- DC-001 naming collision rationale is historical for the legacy tree; new
  symbols use `CLV_CTRL_*` / `clv_control_*` extensions instead of a second
  `cellctl` namespace inside the control package.
- C2+ proceeds under §D phase cards.

**Alternatives considered:**  
- Delete GadTools cellctl immediately — rejected; isolate instead.  
- Mechanical port of `clv_cellctl_*` — rejected; architectures differ.  
- Keep both as dual first-class products without a correction — rejected;
  confuses agents and integrators.

---

## E.2 Design Change DC-005 — C1 locks for custom-control checkbox cells

**Date:** 2026-07-30  
**Phase:** C1  
**Status:** Accepted

**Original design:**  
Post-correction proposed defaults (centre in full content band;
`NAV_ACTIVATE` toggles; event lacked `user_data`; public arm state unspecified;
possible direct mutation of app memory).

**Evidence or problem:**  
Review feedback required stable row identity, two-step events, snapshot
ownership, first-line-band placement, Space vs `NAV_ACTIVATE`, private arm
state.

**New design:**  
Exactly §D.1 / §D.11.

**Why this is better:**  
Stable app identity via `user_data`; clear event sequencing; safer ownership;
predictable visuals on wrapped rows; preserves existing activation semantics.

**Consequences:**  
C2+ must implement these locks. Experimental `CLV_ControlRow` / `CLV_Event`
ABI gains fields. `LONG value` remains scroll_y; checkbox uses `cell_value` /
`previous_value`.

**Alternatives considered:**  
- Compound single event — rejected.  
- Live write-through — rejected as default.  
- Mid-row vertical centre — rejected.  
- `NAV_ACTIVATE` toggles — rejected.

---

## E.3 Design Change DC-006 — `CLV_INPUT_TOGGLE` for Space

**Date:** 2026-07-30  
**Phase:** C7  
**Status:** Accepted

**Original design:**  
Prefer mapping Space through an existing neutral `CLV_InputEvent` type;
add a dedicated type only if unavoidable.

**Evidence or problem:**  
`CLV_INPUT_NAV_ACTIVATE` is locked to row activation
(`CLV_EVENT_ACTIVATED`) and must not toggle (§D.1 / DC-005). No other
existing input type means “toggle sole checkbox on selected row.” Demo
RAWKEY→neutral-event remains required; in-demo snapshot mutation would
bypass `handle_input` / `CELL_TOGGLED`.

**New design:**  
Add `CLV_INPUT_TOGGLE` after `CLV_INPUT_NAV_ACTIVATE`. Demo Space
(`0x40`) translates to it. Core (when keyboard enabled): clear pending
mouse arm; if the selected row has exactly one
VISIBLE|ENABLED|INTERACTIVE checkbox column, toggle the owned snapshot
and emit `CLV_EVENT_CELL_TOGGLED`; otherwise no event. Return /
`NAV_ACTIVATE` unchanged.

**Why this is better:**  
Preserves activation semantics; keeps Space on the same neutral
`handle_input` path as mouse commit; documents the ABI extension.

**Consequences:**  
Apps that translate RAWKEY must map Space → `CLV_INPUT_TOGGLE` (or omit
keyboard toggle). Enum gains one value at the end (no renumbering of
prior types).

**Alternatives considered:**  
- Reuse `NAV_ACTIVATE` for Space — rejected (DC-005).  
- Demo-only setter without event — rejected (breaks app pattern /
  event symmetry with mouse).  
- Multi-column keyboard focus — deferred (C7 out of scope).

---

## F. Overall status (corrected)

**Overall status:** **Complete** — corrected interactive checkbox cells on
`src/custom_listview_control/` (C0–C10); optional omit and multi-column
keyboard focus remain deferred; GadTools `clv_cellctl_*` remains legacy /
non-authoritative  
**Current phase:** C10 — Closure audit (**Complete**, 2026-07-30)  
**Do not start:** Legacy Phase 6; `clv_cellctl_*` product extensions; new
checkbox features without a Design Change / new phase card  
**Next required action:** None for the corrected C2–C10 roadmap. Track
deferred follow-ups from the C10 completion record when prioritising
future work.

---

## G. Agent session protocol (live — corrected phases)

Each new agent/session working on **C2–C10** must follow this sequence.
Do **not** use LEGACY §11 (“read this entire master document”).

### Suggested user prompt template

```text
Implement Phase C<N> only from docs/CLV_INTERACTIVE_CONTROL_CELLS_MASTER_PLAN.md.
Follow §G and the Agent reading set under §D.<card> for that phase.
Do not read or follow the LEGACY ARCHIVE.
Do not start any other phase.
When done: fill the phase completion record, update §D.0 and §F, append
docs/CLV_CONTROL_CELLS_DEVELOPER_LOG.md.
```

### At the start of the session

1. Read **only** the phase’s Agent reading set (plus this §G).
2. Read the latest developer-log entry; confirm the previous phase is Complete
   (or this phase is explicitly authorised, e.g. C2).
3. Inspect repository state / diff for the touched package
   (`src/custom_listview_control/`, demo as listed).
4. Work on **that phase only**, except tiny prerequisite fixes essential to
   complete it.
5. Do not repeat completed work unless evidence shows it is wrong.
6. Do not mechanically port `src/custom_listview/clv_cellctl_*`.

### During the session

1. Preserve classic Amiga constraints (68000, limited memory, older C).
2. Follow §D.11 locks; open a Design Change Record before breaking them.
3. Stay inside the phase Out of scope list.
4. Avoid unrelated refactoring.
5. Prefer evidence from builds/tests/runtime over assumptions.
6. Keep the LEGACY ARCHIVE closed unless a reading-set item explicitly names
   a historical fact.

### At the end of the session

1. Run the phase Validation list.
2. Fill the phase **Phase completion record**.
3. Update §D.0 status table and §F.
4. Append `docs/CLV_CONTROL_CELLS_DEVELOPER_LOG.md`.
5. Record exact commands and results.
6. Mark the phase Complete, Blocked, or Partially complete.
7. State next phase starting conditions (already on the card; confirm).
8. Leave the repository buildable, or document breakage clearly.

### Token / context discipline

- Prefer the reading set over whole-file reads of this master plan.
- Stop before the `# LEGACY ARCHIVE` banner by default.
- One phase per session.

---

# LEGACY ARCHIVE — Mis-scoped GadTools LISTVIEW_KIND work (Phases 0–5)

> **Nothing below this point may be used as design authority for
> `src/custom_listview_control/`; it records only the abandoned GadTools
> implementation.**
>
> Everything from this banner through the former Phase 5–10 roadmap documents
> work against **`src/custom_listview/`**. It is retained for history and for
> the frozen `clv_cellctl_*` code. **It is not the implementation target.**
> Do not execute “Phase 6” or later sections of this legacy archive.
> Headings below are marked **LEGACY — DO NOT EXECUTE** where agents might
> otherwise treat them as live instructions.
>
> Keyword search hits in this archive (e.g. “sole implementation target”,
> “Confirmed”, “Accepted”, “Phase 6”) are **historical** and are superseded by
> DC-004 / DC-005 / §A–§F above.

**Former status line (obsolete):** Phase 5 complete; Phase 6 ready  
**Former (revoked) rule:** “This plan modifies only `src/custom_listview/`”

---
## LEGACY — DO NOT EXECUTE — 1. Purpose of This Document

This is the controlling design and implementation plan for adding optional interactive controls to cells in the Custom ListView.

The first implemented control will be a compact, custom-drawn checkbox (also described here as a tickbox). The design must keep a clean route open for future drawn controls such as:

- cycle controls;
- compact buttons;
- action buttons that ask the host application to open an editor or details window;
- other small, row-oriented interactive controls that are appropriate for classic Amiga displays.

Only checkbox support is currently authorised for implementation. Future control identifiers may be added when their implementations are actually planned and supported. Public identifiers must not imply that an unimplemented control already works.

This document serves five purposes:

1. explain why interactive control cells are being added;
2. define the intended architecture and ownership rules;
3. protect executable size by making the feature optional;
4. define the control lifecycle from data preparation through destruction;
5. divide the work into independently reviewable phases that can be completed one per AI-agent session.

---

## 2. Why Interactive Controls Are Being Added

Tables and list views often need more than passive text. A checkbox column is useful for enabling packages, selecting actions, marking items, or changing a Boolean property without opening a separate editor.

A native GadTools `CHECKBOX_KIND` gadget can technically be positioned over a ListView cell, but it remains a separate Intuition gadget rather than becoming part of the row. That creates avoidable complexity:

- its visual position and its clickable gadget rectangle must be moved whenever the ListView scrolls;
- pixel-copy smart scrolling can move the drawn image without moving the real gadget hit area;
- wrapped and variable-height rows complicate gadget placement;
- a pool of visible gadgets would need rebinding after scrolling and relayout;
- additional gadget refresh operations can cause flicker and reduce the benefit of smart scrolling;
- the standard checkbox dimensions can be too large for compact rows;
- native gadget imagery gives less control over the exact visual style and size.

A custom-drawn control cell avoids these problems. The checkbox is drawn as part of the row and therefore naturally participates in:

- clipping;
- row redraws;
- smart scrolling;
- column alignment;
- variable row heights;
- selection highlighting;
- multiple independent ListView instances.

The host application still receives a meaningful control event. The control is therefore functionally real even though it is not an independent Intuition gadget.

---

## 3. Principal Design Decisions

### 3.1 The feature is an interactive control-cell system

The architecture will use the general concept of an **interactive control cell**. Checkbox support will be the first type-specific implementation.

The public API exposes only implemented control types. **Phase 1 accepts DC-001.** Final identifiers:

```c
#define CLV_CELLCTL_NONE      0
#define CLV_CELLCTL_CHECKBOX  1
```

The underlying type leaves room for future identifiers, but cycle or button identifiers must not be publicly advertised until those controls exist.

### 3.2 Control meaning and control appearance are separate

Per-row or per-cell data describes the semantic value:

- checked, unchecked, or possibly indeterminate;
- enabled or disabled;
- interactive or display-only;
- visible or absent where the row does not use the control.

Presentation configuration describes how checkboxes are drawn:

- plain outline or Workbench-style 3D frame;
- width and height;
- tick, cross, or fill mark;
- padding and alignment;
- selected-row treatment;
- disabled treatment;
- minimum and maximum usable dimensions.

The same checkbox appearance will normally apply to an entire control column. Per-cell visual overrides are not part of the first implementation unless a phase discovers a strong, documented requirement.

### 3.3 The host application owns business actions

CLV must not directly open an editor, alter application records, or perform application-specific work.

CLV will:

1. identify the logical row and column;
2. determine the control type and current state;
3. apply an authorised local state transition or report the requested transition;
4. redraw the affected cell or row;
5. return or emit a neutral control event.

The host application decides what that event means. A future button may report an action such as `EDIT`, but CLV must not implement the edit window itself.

### 3.4 Shared logic must not be duplicated merely to save a few bytes

Executable size matters, but common and delicate behaviour must remain centralised.

The control subsystem should use:

- one shared control-cell core for common geometry, clipping, hit-testing, row mapping, activation rules, redraw requests, and event creation;
- one small type-specific implementation per supported control;
- build-time omission of the entire subsystem or individual type modules when they are unused.

Do **not** duplicate hit-testing, scrolling, clipping, input-state, or event-building logic independently inside every control type just to avoid linking a small shared core.

### 3.5 Object omission is the primary size-control mechanism

Compile guards are useful for keeping public declarations and renderer references truthful, but the dependable binary-size mechanism is to omit unused translation units from the link.

Accepted module list (Phase 1 / DC-001):

```text
src/custom_listview/clv_cellctl.h              /* public */
src/custom_listview/clv_cellctl_internal.h     /* private */
src/custom_listview/clv_cellctl_core.c
src/custom_listview/clv_cellctl_checkbox.c
src/custom_listview/clv_bind_cellctl.c         /* checkbox-only drawn binder */
/* future, not implemented now:
   clv_cellctl_cycle.c
   clv_cellctl_button.c
*/
```

**Critical naming constraint:** `src/custom_listview_control/` already owns the public API family `CLV_Control` / `clv_control_*` / `CLV_CTRL_*` for a separate experimental custom-drawn viewport. Interactive cell-control modules must **not** reuse those identifiers, filenames, or log symbols. See Design Change DC-001 (Accepted) and §17 / §18.

A text-only or existing drawn profile must link none of the cell-control modules.

A checkbox-enabled profile should link:

```text
drawn renderer core objects (as for draw-basic / draw-selected)
clv_cellctl_core.o
clv_cellctl_checkbox.o
exactly one binder that calls clv_cellctl_install() (new or extended)
```

---

## 4. Scope

### 4.1 In scope for the initial project

- a compact custom-drawn checkbox control cell;
- plain and 3D checkbox styles, if both can be implemented without distorting the architecture;
- configurable fixed dimensions suitable for small Amiga rows;
- checked and unchecked states;
- enabled and disabled behaviour;
- display-only and interactive modes;
- mouse hit-testing limited to the checkbox cell/control rectangle;
- a clear policy for whether clicking elsewhere in the row only selects the row;
- keyboard activation where it fits the existing CLV keyboard model;
- correct mapping between visible/physical rows and logical rows;
- correct handling of wrapped and variable-height rows;
- redraw of only the smallest safe region;
- optional object modules and compile-time feature reporting;
- size comparison against an equivalent non-control build;
- documentation, examples, and regression coverage appropriate to the repository.

### 4.2 Out of scope for the initial project

- real embedded GadTools checkbox gadgets;
- cycle-control implementation;
- button-control implementation;
- popup menus;
- application-specific editor windows;
- arbitrary mixtures of unrelated control types in one cell;
- per-cell custom drawing callbacks unless the existing renderer architecture already makes this the safest option;
- drag interaction;
- multiple simultaneous active controls;
- a shared-library ABI;
- support claims not backed by compilation and runtime evidence.

---

## 5. Definitive Data Model (Phase 1)

Phase 1 replaces the earlier conceptual sketches. Exact public/private
declarations for Phase 2 headers are in §18. Summary:

- Naming family: `CLV_CELLCTL_*` / `clv_cellctl_*` (DC-001 **Accepted**).
- Control **type** is column-level (`CLV_PixelColumn.flags`).
- Control **value and interactivity** are per-cell via parallel `CLV_CellCtlDesc`
  (does **not** enlarge `CLV_CellPresentation` — DC-002).
- Appearance is column-uniform, set before prepare (prepare-time setter).
- Indeterminate checkbox is **deferred** (value type remains `UBYTE`).

### 5.1 Control identifier

```c
typedef UBYTE CLV_CellCtlType;

#define CLV_CELLCTL_NONE      0
#define CLV_CELLCTL_CHECKBOX  1
```

Column typing (in `clv_cellctl.h`, applied to `CLV_PixelColumn.flags`):

```c
#define CLV_COL_F_CELLCTL_CHECKBOX  (1U << 0)
```

A column without this flag is never treated as a control column, even if a
per-cell descriptor is non-zero. Per-cell descriptors for non-control columns
are ignored.

### 5.2 Common flags (per-cell)

```c
#define CLV_CELLCTL_F_VISIBLE      (1U << 0)
#define CLV_CELLCTL_F_ENABLED      (1U << 1)
#define CLV_CELLCTL_F_INTERACTIVE  (1U << 2)
```

Zero-initialised descriptors mean not visible (safe sparse default). A usable
interactive checkbox requires all three bits. Display-only: `VISIBLE|ENABLED`
without `INTERACTIVE`. Disabled: `VISIBLE` without `ENABLED` (or with
`ENABLED` clear).

### 5.3 Checkbox value

```c
#define CLV_CELLCTL_CHECKBOX_UNCHECKED  0
#define CLV_CELLCTL_CHECKBOX_CHECKED    1
/* CLV_CELLCTL_CHECKBOX_INDETERMINATE reserved — not in first public API */
```

Indeterminate is deferred (DC-003). Stored as `UBYTE` so a later phase can add
it without widening the descriptor.

### 5.4 Checkbox presentation

```c
#define CLV_CELLCTL_FRAME_PLAIN  0
#define CLV_CELLCTL_FRAME_3D     1

#define CLV_CELLCTL_MARK_TICK    0
#define CLV_CELLCTL_MARK_CROSS   1
#define CLV_CELLCTL_MARK_FILL    2

typedef struct CLV_CheckboxAppearance
{
    UBYTE width;
    UBYTE height;
    UBYTE frame_style;
    UBYTE mark_style;
    UBYTE pad_h;
    UBYTE pad_v;
} CLV_CheckboxAppearance;
```

**Defaults (when width/height are 0 or setter not called):**

| Field | Default |
|-------|---------|
| width / height | 9 / 9 |
| frame_style | `CLV_CELLCTL_FRAME_PLAIN` |
| mark_style | `CLV_CELLCTL_MARK_TICK` |
| pad_h / pad_v | 1 / 1 |

**Validation / clamp (Phase 3–4 enforce):** width/height in \[5, 15\]; invalid
frame/mark fall back to defaults. If preferred size+pad does not fit the cell
(common on Topaz/8 ~8 px rows), geometry **shrinks pad then box** down to the
minimum before skipping. Only when even the minimum cannot fit is drawing /
hit-testing skipped.

**Legibility fallback (Phase 4):** when `mark_style` is `TICK` and the painted
box is narrower or shorter than 7 px, paint uses `FILL` for that draw only
(API default remains `TICK` for the normal 9×9 box).

Uses DrawInfo/system pens; no large-palette assumption.

### 5.5 Per-cell descriptor

```c
typedef struct CLV_CellCtlDesc
{
    UBYTE flags;  /* CLV_CELLCTL_F_* */
    UBYTE value;  /* CLV_CELLCTL_CHECKBOX_* */
} CLV_CellCtlDesc;
```

Supplied at prepare as `row_cellctl[i]` → array of `column_count` descriptors
for data row `i` (same shape as `row_present`). May be NULL (no control state).
Header/separator rows have no control descriptors in the first implementation.

### 5.6 Event result

```c
typedef struct CLV_CellCtlEvent
{
    UWORD logical_row;
    UWORD physical_row;
    UWORD column;
    UWORD control_type;
    UWORD previous_value;
    UWORD value;
} CLV_CellCtlEvent;
```

No `user_data` / `action_id` in the first API (prepared nodes have no app
row pointer). Application correlates via logical row + column.

Delivery: **synchronous return** from the mouse helper — not a callback, not
folded into `CLV_SelectionResult`.

---

## 6. Rendering Behaviour

### 6.1 Geometry

The checkbox is drawn inside the resolved cell rectangle. Its box must be:

- clipped to the cell and ListView viewport;
- aligned according to column presentation (`CLV_PixelColumn.alignment`);
- vertically centred within the **first physical subline** of the logical row only (matches icon `subline_index == 0` policy);
- prevented from extending into the scrollbar reserve or adjacent columns;
- skipped when the available cell area is too small to draw safely.

**Wrapped rows:** draw and hit-test on **subline 0 only**. Continuation physical rows do not show or activate the checkbox.

Fixed pixel size is the initial sizing model (D-014). A later fit-to-row mode may be considered, but large wrapped rows must not cause giant checkboxes.

### 6.2 Plain style

The plain style draws a one-pixel outline with the row foreground pen (`apen`:
`TEXTPEN` or `FILLTEXTPEN`). The mark is inset by 2 px at ≥9×9, else 1 px.

### 6.3 3D style

The 3D style draws a recessed Workbench-like frame: shadow on top/left, shine
on bottom/right. Selection does not invert the bevel (no false “pressed” look).

### 6.4 Checked mark

Configurable `TICK` (default), `CROSS`, or `FILL`. At box sizes below 7×7,
`TICK` auto-falls back to `FILL` for that paint only so the checked state
remains readable at the provisional minimum (5×5).

### 6.5 Selected rows

Selection highlighting and checkbox state are separate. Frame and mark use the
renderer’s `apen` (already `FILLTEXTPEN` when selected). Bevel pens stay
shine/shadow. The control is not redrawn as pressed merely because the row is
selected.

### 6.6 Disabled controls

Disabled controls (`CLV_CELLCTL_F_ENABLED` clear) still paint, then apply a
1-of-4 dither ghost (same pattern family as row disable). Ghost pen is
`SHADOWPEN` on normal rows and `apen` on selected rows so the dither stays
visible on `FILLPEN`. Disabled controls must not react to input (Phase 5).

---

## 7. Input and State Behaviour

### 7.1 Mouse activation

**Confirmed (Phase 1):**

- clicking directly inside the control hit rectangle toggles an enabled, interactive checkbox;
- clicking elsewhere in the row performs normal row selection only (application still runs the selection adapter);
- a display-only checkbox never toggles;
- a disabled checkbox never toggles;
- clicks outside the visible/clipped part of the control do not activate it;
- activation uses **verified button-up**: SELECTDOWN arms if the press is inside the control; SELECTUP commits only if release is still inside the same control and the arm is valid; leaving the control before release cancels.

Application-owned durable state:

```c
typedef struct CLV_CellCtlInputState
{
    BOOL  armed;
    UWORD physical_row;
    UWORD logical_row;
    UWORD column;
    UWORD control_type;
} CLV_CellCtlInputState;
```

Zero-init before first use. Library does not allocate this.

### 7.2 State ownership

**Confirmed (Phase 1) — prepared-snapshot mutation; application-authoritative store:**

1. At prepare, CLV **copies** `CLV_CellCtlDesc` into prepared cellctl slot storage (feature-local; not fields on every text `CLV_RenderCell`).
2. On verified activation, CLV **mutates the prepared snapshot**, fills `CLV_CellCtlEvent` (`previous_value` / `value`), and returns success.
3. The **application** owns the authoritative Boolean (or other store). It must update that store from the event before the next prepare/rebuild.
4. To reject a toggle, the application calls `clv_cellctl_set_checkbox_value()` to restore the snapshot and refreshes the gadget.
5. After a full prepare from application arrays, the prepared snapshot is replaced by the application-supplied descriptors.

This avoids a live app pointer on render nodes, gives immediate visual feedback from the snapshot, and keeps rebuilds coherent with application data.

### 7.3 Keyboard activation

**Deferred to Phase 7.** `src/custom_listview/` has no keyboard/column-focus model. Do not invent one for the first checkbox phases and do not copy one from `src/custom_listview_control/`.

### 7.4 Redraw

**Confirmed (Phase 1):** No cell-level invalidate API. After a successful toggle:

1. prepared snapshot already holds the new value (or app restored it);
2. application refreshes via GadTools (`GT_SetGadgetAttrs` / `GT_RefreshWindow` as in existing examples);
3. safe invalidation unit is the **physical row** (or the whole list if that is simpler);
4. control-rectangle-only redraw is not required for the first implementation.

Correctness takes priority over partial-redraw optimisation. A tiny refresh helper may be added later if demos duplicate the sequence; it is not part of Phase 2–5 exit criteria.

---

## 8. Expected Lifecycle

### 8.1 Build-time lifecycle

1. The project selects a profile that either excludes or includes interactive controls.
2. When controls are excluded, no control-cell core, checkbox implementation, binder registration, demo code, or control-specific test code should enter the production executable.
3. When checkbox support is enabled, the profile reports truthful `CLV_HAS_*` values and links only the shared core plus checkbox implementation.
4. Future control modules remain absent unless explicitly enabled.

### 8.2 ListView construction lifecycle

1. The application defines its columns and identifies a column as a checkbox control column.
2. The application supplies checkbox appearance or accepts documented defaults.
3. Row/cell data supplies the checkbox state and flags.
4. Renderer/control setup validates the configuration.
5. The ListView and renderer are created using the existing ownership sequence.
6. Labels/prepared data are attached only after successful construction.

### 8.3 Prepare/layout lifecycle

1. The renderer prepares logical and physical rows as it already does.
2. For each checkbox cell, the control core resolves the cell rectangle and stores or derives only the minimum geometry required.
3. Control geometry must remain valid across wrapping, column resizing, viewport resizing, and scrolling.
4. If cached geometry is used, all invalidation conditions must be documented and implemented.

### 8.4 Draw lifecycle

1. GadTools invokes the existing ListView draw callback for a visible physical row.
2. The renderer resolves row state, selection state, and the cell rectangle.
3. The shared control core dispatches drawing to the checkbox implementation only when the cell is a supported checkbox control.
4. The checkbox renderer draws within the supplied clipped rectangle and returns without changing unrelated RastPort state, or restores any state it changes.

### 8.5 Input lifecycle

1. The application passes relevant input to the CLV input/control handler according to the existing event model.
2. The shared core maps mouse coordinates to ListView, physical row, logical row, and column.
3. It verifies visibility, enabled/interactivity flags, and the precise control hit rectangle.
4. It performs or requests the checkbox state transition according to the chosen ownership model.
5. It mutates the prepared checkbox snapshot and fills `CLV_CellCtlEvent`.
6. The application updates its authoritative store (or reverts the snapshot).
7. The application triggers GadTools refresh (physical row or list).

### 8.6 Relayout and scrolling lifecycle

The checkbox is not an independent gadget. It is redrawn as row content.

- When GadTools smart-scrolls a `LISTVIEW_KIND` list, already-rendered checkbox pixels move with their rows (GadTools-owned blit; not implemented inside `src/custom_listview/`).
- Newly exposed physical rows are painted via the normal `LV_DRAW` hook.
- Column resize or viewport resize invalidates relevant geometry and causes redraw through existing prepare/relayout paths plus application GadTools refresh.
- No gadget pool is repositioned.
- Do not copy scroll architecture from `src/custom_listview_control/`.

### 8.7 Destruction lifecycle

1. Stop delivering input to the ListView/control handler.
2. Detach GadTools labels according to the existing CLV ownership rules.
3. Free prepared lists and renderer-owned data in their established order.
4. Free any control-specific private allocations.
5. Destroy the renderer only after GadTools can no longer call its hook.
6. Application-owned row data and option strings remain application-owned unless the final API explicitly documents a copied value.

The checkbox implementation should ideally require no per-cell heap allocation.

---

## 9. Optional Feature and Binary-Size Requirements

### 9.1 Feature flags

**Confirmed repository convention (Phase 0):**

- Make `CLV_ENABLE_*` selects which objects to link.
- Generated `build/generated/<profile>/clv_profile_config.h` supplies matching `CLV_HAS_*` for client compiles.
- `src/custom_listview/clv_config.h` defaults every `CLV_HAS_*` to `0` unless a profile header is active.
- Object omission is the size mechanism; `CLV_HAS_*` only reports capability.

Existing `CLV_HAS_*` macros today (`src/custom_listview/clv_config.h`):

```text
CLV_HAS_ASCII, CLV_HAS_SORT, CLV_HAS_CELL_TRACKING, CLV_HAS_CHAR_WRAP,
CLV_HAS_RENDERER, CLV_HAS_SELECTION, CLV_HAS_PIXEL_WRAP, CLV_HAS_ICONS,
CLV_HAS_STYLES, CLV_HAS_DETAILS, CLV_HAS_PATH
```

**Final names (Phase 1):**

```text
CLV_ENABLE_CELLCTL / CLV_HAS_CELLCTL
CLV_ENABLE_CELLCTL_CHECKBOX / CLV_HAS_CELLCTL_CHECKBOX
```

Longer `CELL_CONTROL*` spellings are rejected (DC-001 prefers the short distinct family).

### 9.2 Dependencies

```text
CELLCTL_CHECKBOX -> CELLCTL -> RENDERER
```

Enabling checkbox support without the renderer must fail clearly at profile generation or build configuration time (`validate-custom` / PowerShell checks in the root Makefile).

**Profile policy (Phase 1):**

| Profile intent | Links |
|----------------|-------|
| Existing `draw-basic` / `draw-selected` / wrap / styled / details / full | **No** cellctl objects (unchanged) |
| New `draw-cellctl-checkbox` (Phase 2) | draw-basic core + **selection** + `clv_cellctl_core.o` + `clv_cellctl_checkbox.o` + `clv_bind_cellctl.o` |
| Pixel wrap | **Not** required for checkbox; optional later combo profile if demos need wrap |

Selection is recommended (and linked in the checkbox demo profile) so header/separator rejection stays consistent; drawing a checkbox glyph does not hard-require selection symbols.

### 9.3 Translation-unit isolation

Control implementation must not be placed wholesale inside a mandatory renderer-core source file.

Mandatory renderer files may contain a minimal guarded integration point, but should not directly force all control modules into every drawn build.

Ordinary text/icon rows must not pay per-cell control fields on `CLV_RenderCell` / `CLV_RenderFragment`. Control snapshot storage is feature-local (parallel slots on the prepared list when any control column is present).

### 9.4 Binder/registration integration

**Confirmed (Phase 0 + Phase 1):** Drawn profiles already use exactly one linked `clv_bind_*.c` that implements `clv_renderer_bind_optional()` and calls feature `*_install()` helpers to fill `g_clv_opt_fns` (`CLV_RendererOptFns` in `clv_renderer_internal.h`). Current binders:

| Binder | Installs |
|--------|----------|
| `clv_bind_none.c` | nothing |
| `clv_bind_wrapped.c` | `clv_pixel_wrap_install` |
| `clv_bind_styled.c` | icons + styles |
| `clv_bind_details.c` | pixel wrap + icons + styles |
| `clv_bind_full.c` | pixel wrap + icons + styles |
| `clv_bind_cellctl.c` (**new**, Phase 2) | `clv_cellctl_install` only |

**Accepted integration:**

1. Extend `CLV_RendererOptFns` with a NULL-safe cell-control **draw** pointer (hit-test stays in public `clv_cellctl_*` helpers, not the draw ops table).
2. `clv_cellctl_install()` lives in the cellctl modules.
3. Dedicated `clv_bind_cellctl.c` for the checkbox-enabled drawn profile.
4. Do **not** extend `clv_bind_full.c` / styled / wrapped binders in the first pass — keep existing profiles free of cellctl.
5. Keep baseline `draw-basic` / `draw-selected` on `clv_bind_none` with **no** cellctl objects linked.

Exactly one authoritative registration path must exist for a given build. Do not place cell-control code wholesale inside mandatory `clv_renderer_core.c` beyond a minimal guarded `clv_opt_*` call site.

### 9.5 Size evidence

The final implementation must report at least:

```text
baseline drawn profile executable size
checkbox-enabled profile executable size
absolute byte delta
percentage delta
```

Where practical, also report code, initialised data, and uninitialised data sections.

A no-control production build must be checked for accidental control symbols or modules.

Size measurement is evidence, not an instruction to duplicate shared code or make the design unsafe.

---

## 10. Documentation and Change-Control Rules

### 10.1 Master document updates

Every phase agent must update this master document before ending its session.

At minimum it must update:

- the phase status table;
- the completed phase’s record;
- confirmed design decisions;
- files added or changed;
- tests performed and their results;
- unresolved risks or blockers;
- the exact recommended starting point for the next phase.

An agent must not merely write a separate report and leave this plan stale.

### 10.2 Developer log

The first phase must create, if absent:

```text
docs/CLV_CONTROL_CELLS_DEVELOPER_LOG.md
```

Every phase must append a dated entry containing:

- phase number and title;
- agent/session objective;
- source files inspected;
- files modified, added, renamed, or removed;
- API and architecture decisions;
- build commands run;
- tests and runtime checks performed;
- measured sizes or performance data;
- defects found;
- known limitations;
- next recommended action.

The log is append-oriented. Existing entries must not be rewritten merely to make later work look cleaner. Corrections should be added as new entries that identify the corrected statement.

### 10.3 Authority to change the plan

Any phase agent may change this design when there is a strong technical reason. The agent must not silently drift from the plan.

Before implementing a material design change, the agent must add a **Design Change Record** to this document using this format:

```markdown
### Design Change DC-XXX — <short title>

**Date:** YYYY-MM-DD  
**Phase:** <phase number>  
**Status:** Proposed / Accepted / Rejected / Superseded

**Original design:**  
<what this document previously required>

**Evidence or problem:**  
<source findings, build constraints, runtime behaviour, API conflict, or measured result>

**New design:**  
<the replacement design>

**Why this is better:**  
<correctness, compatibility, maintainability, size, performance, or ownership reasoning>

**Consequences:**  
<affected APIs, files, profiles, tests, documentation, migration concerns>

**Alternatives considered:**  
<briefly list plausible alternatives and why they were rejected>
```

Material changes include:

- changing state ownership;
- changing public API shape;
- moving logic between mandatory and optional modules;
- changing input semantics;
- changing lifecycle/ownership rules;
- dropping a planned style or supported state;
- altering profile dependencies;
- introducing per-cell allocation;
- changing compatibility targets.

Small implementation details do not require a Design Change Record, but should still be mentioned in the developer log.

### 10.4 No unsupported completion claims

A phase may only be marked complete when its exit criteria are met. Compilation alone does not prove runtime behaviour. Emulator testing does not automatically prove stock 68000 performance. Any untested claim must be labelled as unverified.

---

## 11. Agent Session Protocol

> **LEGACY — DO NOT EXECUTE.** Live protocol is **§G** at the top of this
> document. Do not “read this entire master document.”

Each new agent/session must follow this sequence.

### At the start of the session

1. Read this entire master document.
2. Read `docs/CLV_CONTROL_CELLS_DEVELOPER_LOG.md` if it exists.
3. Inspect the current repository state and version-control diff.
4. Confirm the first phase whose status is not complete.
5. Work on **that phase only**, except for small prerequisite fixes that are essential to complete it.
6. Do not repeat completed work unless evidence shows it is wrong.

### During the session

1. Preserve classic Amiga constraints: 68000 target, limited memory, low resolution, restricted palettes, and older toolchains.
2. Follow current public/private header boundaries.
3. Keep C source compatible with the project’s established compiler/language policy.
4. Avoid unrelated refactoring.
5. Prefer evidence from source, builds, tests, and runtime behaviour over assumptions.
6. Record design deviations before or as they are made.

### At the end of the session

1. Run the phase-specific validation.
2. Update this master document.
3. Append to the developer log.
4. Record exact commands and results.
5. Mark the phase `Complete`, `Blocked`, or `Partially complete`.
6. State the next phase’s starting conditions.
7. Leave the repository buildable, or clearly document why it cannot yet build and what remains broken.

---

## 12. Phase Status Summary (LEGACY — DO NOT EXECUTE)

> **LEGACY TABLE ONLY.** Authoritative status is §D.0 / §F at the top of this
> document. Phases 0–5 below are frozen mis-scoped GadTools work. Phases 6–10
> are **cancelled** (superseded by corrected phases C6–C10).
> Do not treat “Complete” rows as permission to continue the GadTools roadmap.

| Phase | Title | Status | Completion evidence |
|------:|-------|--------|---------------------|
| 0 | Source and architecture audit | **Complete (legacy / mis-scoped)** | GadTools `src/custom_listview/` audit |
| 1 | Finalise public/private design and lifecycle | **Complete (legacy / mis-scoped)** | DC-001…003 as applied to GadTools |
| 2 | Optional build/profile skeleton | **Complete (legacy / mis-scoped)** | `CLV_ENABLE_CELLCTL*`, `clv_cellctl_*` |
| 3 | Shared control core and geometry | **Complete (legacy / mis-scoped)** | geom host tests; GadTools draw dispatch |
| 4 | Checkbox rendering | **Complete (legacy / mis-scoped)** | paint; size twin visual |
| 5 | Mouse input and state transition | **Complete (legacy / mis-scoped)** | `handle_mouse` commit; example IDCMP |
| 6 | Selection, wrapping, scrolling, and relayout | **Cancelled** | Superseded by corrected §D Phase C6 |
| 7 | Keyboard and accessibility behaviour | **Cancelled** | Superseded by corrected §D Phase C7 |
| 8 | Demo/example and integration documentation | **Cancelled** | Superseded by corrected §D Phase C8 |
| 9 | Regression, performance, and binary-size | **Cancelled** | Superseded by corrected §D Phase C9 |
| 10 | Closure audit and release-readiness review | **Cancelled** | Superseded by corrected §D Phase C10 |

---

# LEGACY — DO NOT EXECUTE — Implementation Phases

## LEGACY — DO NOT EXECUTE — Phase 0 — Source and Architecture Audit

### Objective

Establish exactly how the current renderer represents columns, cells, prepared rows, logical-to-physical row maps, hit-testing, optional operations, binders, feature profiles, redraws, and ownership.

### Required work

- Locate all current public and private renderer structures.
- Identify how cell presentation is currently supplied.
- Identify whether application data is copied, borrowed, or callback-driven.
- Trace draw-callback entry through cell rendering.
- Trace existing mouse/cell hit-testing.
- Trace logical and physical row mapping for wrapped rows.
- Trace smart-scroll and redraw invalidation paths.
- Inspect binder modules and generated `CLV_HAS_*` profile configuration.
- Identify the lowest-risk integration points for control drawing and input.
- Identify all existing tests, demos, size reports, and logging mechanisms that should be extended.
- Create `docs/CLV_CONTROL_CELLS_DEVELOPER_LOG.md` with an initial heading and Phase 0 entry.

### Deliverables

- Updated architecture sections in this master document, replacing conceptual assumptions with confirmed source facts.
- A proposed exact file/module list.
- A proposed public/private API boundary.
- A list of compatibility and ownership risks.
- Developer-log entry.

### Exit criteria

- No implementation code is required.
- The agent can point to exact source functions/structures for draw, prepare, hit-test, scroll, relayout, and binder integration.
- All uncertain assumptions are listed.
- Phase 1 has a concrete design question list.

### Phase completion record

**Status:** Complete  
**Date:** 2026-07-30  
**Implementation code changed:** None (audit/documentation only)

#### Confirmed source anchors

| Concern | Exact symbols / location |
|---------|--------------------------|
| Draw hook entry | `clv_renderer_create` sets `hook.h_SubEntry = clv_renderer_dispatch` (`clv_renderer_core.c`); GadTools `LV_DRAW` only |
| Structured draw | `clv_listview_draw_structured` (`clv_renderer_core.c`) — fills row, draws cells/fragments, icons, styles, dividers |
| Prepare | `clv_prepared_build_structured_present` (`clv_renderer_core.c`); text duplicated into owned `CLV_RenderCell` / `CLV_RenderFragment` |
| Presentation input | `CLV_CellPresentation` (`clv_renderer.h`); borrowed only during prepare |
| Columns | `CLV_PixelColumn` (`clv_types.h`); copied into `CLV_Renderer` / `CLV_PreparedList` |
| Private prepared storage | `CLV_RenderNode`, `CLV_RenderCell`, `CLV_RenderFragment` (`clv_prepared_internal.h`) |
| Logical↔physical map | `CLV_RenderMap` + `logical_first` / `logical_subline_count` on `CLV_PreparedList`; `clv_prepared_map_display`, `clv_prepared_resolve_physical` |
| Continuation identity | `CLV_FRAG_F_CONTINUATION` / `CLV_FRAG_F_FINAL_CONTINUATION` on fragments; `subline_index` on nodes/maps |
| Selection adapter | `clv_prepared_map_selectable`, `clv_handle_selection` (`clv_selection.c`) — physical `GTLV_Selected` only |
| ASCII hit-test only | `clv_cell_tracking_detect_ascii_column` / `clv_cell_tracking_detect_column` (`clv_cell_tracking.c`) — column index from char widths; **no** pixel-cell or row mapping |
| Optional ops | `CLV_RendererOptFns` / `g_clv_opt_fns` (`clv_renderer_ops.c`); installers `clv_pixel_wrap_install`, `clv_icons_install`, `clv_styles_install` |
| Binders | Exactly one of `clv_bind_{none,wrapped,styled,details,full}.c` → `clv_renderer_bind_optional` |
| Smart scroll / redraw | **Revised (scope correction):** `src/custom_listview/` implements **no** smart-scroll decision, raster copy, or exposed-band paint API. GadTools owns ListView scroll optimisation and calls `LV_DRAW` for rows to paint. App refresh uses `GT_SetGadgetAttrs` / `GT_RefreshWindow`. Measured `CLV_ENABLE_SMART_SCROLL` / `ScrollRaster*` work lives under `src/custom_listview_control/` (naming-collision location only — not an architecture source). Shared counters only: `clv_bench*.c/h`. |
| Static box icons | `CLV_ICON_EMPTY_BOX` / `CLV_ICON_FILLED_BOX` (`clv_icons.c`) — presentation only, not interactive |

#### Proposed exact file/module list

| Module | Role | Link when |
|--------|------|-----------|
| `clv_cellctl_core.c` / `.h` (private) | Geometry, clip, hit contract, event build, opt wrappers | Any cellctl profile |
| `clv_cellctl_checkbox.c` | Checkbox draw + toggle rules | Checkbox profile |
| `clv_cellctl.h` (public) | Public types/API for implemented controls only | Clients of cellctl |
| `clv_bind_cellctl.c` (or extended binders) | Calls `clv_cellctl_install()` (+ other installs as needed) | Checkbox-enabled drawn profiles |
| Minimal guarded call sites in `clv_renderer_core.c` | Dispatch draw via `clv_opt_*` only | Always linked with renderer; no-ops when ops NULL |
| Makefile / `tools/generate_profile_config.ps1` | `CLV_ENABLE_CELLCTL*` → objects + `CLV_HAS_*` | Profile generation |
| `examples/…` checkbox demo + `examples/size_compare` driver | Demo + size delta | Measurement profile |
| `tests/host/` geometry/map extensions | Host regression | Host suite |

Do **not** place sources under `src/custom_listview_control/`.

#### Proposed public/private API boundary

**Public (clients):**

- Control type IDs for implemented types only (`NONE`, `CHECKBOX`).
- Compact per-cell value/flags descriptor (or parallel array beside `CLV_CellPresentation`) supplied at prepare time.
- Optional column-level or renderer-options appearance (`CLV_CheckboxAppearance` or equivalent).
- Prepare entry that accepts control metadata (extend `clv_prepared_build_structured_present` or add a parallel present+controls API).
- Hit-test / input helper that maps window-relative coordinates → physical/logical/column/control result (application still owns IDCMP).
- Neutral event/result structure for toggle outcomes.
- Documented redraw guidance (no cell-level invalidate API exists today).

**Private:**

- Cached control fields on `CLV_RenderCell` / `CLV_RenderFragment` / `CLV_LvTempFrag`.
- `CLV_RendererOptFns` draw/hit pointers and `clv_cellctl_install`.
- Geometry resolution and RastPort save/restore helpers.
- Binder modules.

**Out of public API for this feature:** anything from `CLV_Control` / `clv_control_*` / `CLV_CTRL_*` / `CLV_InputEvent` (`src/custom_listview_control/` — naming collision only).

#### Compatibility and ownership risks

1. **Namespace collision** with experimental `custom_listview_control` — see DC-001.
2. **No application user-data / row pointer** on prepared nodes — checkbox state must be application-owned or an explicit copied field at prepare; changing caller arrays after prepare does not update the gadget.
3. **No pixel hit-test or mouse API** in `src/custom_listview/` — input remains application-side; library supplies mapping helpers only.
4. **No cell-level redraw** — toggle likely requires physical-row refresh or prepare+reattach; correctness over micro-optimisation. GadTools may still smart-scroll existing pixels; CLV only paints via `LV_DRAW`.
5. **`CLV_CellPresentation` ABI** — prefer parallel control descriptor over overloading `flags` / `icon_id` (icons are not interactive).
6. **`CLV_PixelColumn.flags` unused** by renderer today — usable for column-level “this column is a checkbox” if Phase 1 chooses column typing, but must be documented and not collide with future column flags.
7. **Wrapped rows** — state keys on logical row; draw/hit on physical subline; decide vertical alignment for multi-subline logical rows (Phase 1).
8. **Logging** — do not reuse `clv_control_log` / shared `CLV_ENABLE_LOGGING` file semantics from `src/custom_listview_control/`; `src/custom_listview/clv_log.h` macros are safe if kept printf-only.
9. **Size baseline** — compare against `size-draw-basic` (documented ~27,928 bytes in `docs/CLV_SIZE_REPORT.md`), not ASCII minimal or `custom-control-demo`.

#### Phase 1 concrete design question list

See also §14. **Phase 1 answered all items** (see Phase 1 completion record and §18). Historical list retained:

1. Accept DC-001 naming (`cellctl`) or an equally collision-free alternative. → **Accepted**
2. Control type at column level, per-cell, or both. → **Column type + per-cell desc**
3. State ownership model (CLV mutate vs application request vs configurable). → **Snapshot mutate + app store**
4. Exact public descriptor shape and whether `CLV_CellPresentation` grows. → **Parallel desc; no grow (DC-002)**
5. Event delivery: new struct vs extend selection path vs callback. → **New `CLV_CellCtlEvent`**
6. Indeterminate checkbox in first implementation or deferred. → **Deferred (DC-003)**
7. Mouse: button-down vs verified button-up. → **Verified up**
8. Vertical placement on wrapped logical rows. → **Subline 0 only**
9. Redraw unit: document GadTools refresh sequence; any new helper? → **Physical row / list; no helper yet**
10. Keyboard: defer. → **Phase 7**
11. Whether checkbox profile includes selection + pixel wrap by default. → **Selection yes; wrap no**
12. Host vs Amiga test split. → **Documented in §14**

#### Next phase starting conditions

~~Phase 1 may begin immediately.~~ **Superseded:** Phase 1 complete 2026-07-30. Phase 2 may begin using §18.

---

## LEGACY — DO NOT EXECUTE — Phase 1 — Finalise Public/Private Design and Lifecycle

### Objective

Convert the audit into a minimal, coherent API and internal design before implementation begins.

### Required work

- Decide whether control type belongs to the column definition, cell value, prepared row, or a combination.
- Decide the checkbox state ownership model.
- Decide whether events are returned, callback-driven, or integrated into an existing input-result structure.
- Define exact public names only for implemented functionality.
- Define private operations and internal context structures.
- Define appearance defaults and validation rules.
- Decide whether indeterminate state is in the first implementation.
- Define mouse activation semantics.
- Define redraw ownership and invalidation granularity.
- Define construction, relayout, and destruction responsibilities.
- Ensure ordinary text/icon rows do not gain unnecessary per-cell storage.
- Produce header-level API sketches, but do not implement substantial behaviour yet.

### Deliverables

- Updated definitive design in this document.
- Exact proposed public declarations.
- Exact proposed private declarations.
- Lifecycle and ownership table.
- Event/state transition table.
- Any required Design Change Records.
- Developer-log entry.

### Exit criteria

- The design has no known ownership ambiguity.
- Optional-build boundaries are explicit.
- All public identifiers describe supported behaviour only.
- Phase 2 can introduce the build skeleton without inventing architecture.

### Phase completion record

**Status:** Complete  
**Date:** 2026-07-30  
**Implementation code changed:** None (design/documentation only; no `.c`/`.h` skeleton yet — Phase 2)

#### Decisions (answers to Phase 0 question list)

| # | Decision |
|---|----------|
| 1 | **Accept DC-001** (`CLV_CELLCTL_*` / `clv_cellctl_*` / `CLV_HAS_CELLCTL*`). |
| 2 | Control **type at column** (`CLV_COL_F_CELLCTL_CHECKBOX`); **value/flags per-cell** via parallel `CLV_CellCtlDesc`. |
| 3 | **Prepared-snapshot mutation**; application owns authoritative store and syncs before next prepare. |
| 4 | Do **not** grow `CLV_CellPresentation` (DC-002). New prepare entry + parallel arrays. |
| 5 | New compact `CLV_CellCtlEvent` returned synchronously from mouse helper. |
| 6 | **Indeterminate deferred** (DC-003); `UBYTE` value leaves room. |
| 7 | **Verified button-up** with app-owned `CLV_CellCtlInputState`. |
| 8 | Draw/hit on **subline 0 only** (icon precedent). |
| 9 | Redraw = app GadTools refresh; safe unit = physical row; no new invalidate API yet. |
| 10 | Keyboard **deferred to Phase 7**. |
| 11 | New profile links selection + cellctl; **not** pixel wrap by default. |
| 12 | Host: geometry/hit/map/ownership; Amiga: pens/styles/IDCMP/refresh. |

#### Lifecycle and ownership table

| Stage | Owner | Responsibility |
|-------|-------|----------------|
| Profile / link | Build | Omit all cellctl objs unless `CLV_ENABLE_CELLCTL*` |
| Column flags + appearance | Application | Set `CLV_COL_F_CELLCTL_CHECKBOX`; optional `clv_cellctl_prepare_set_appearance` |
| Row descriptors | Application | Supply `CLV_CellCtlDesc` arrays at prepare (borrowed for call duration) |
| Prepare copy | CLV (cellctl) | Copy descriptors into prepared parallel slots; free with prepared list |
| Text / icons | CLV (existing) | Unchanged; checkbox columns typically use empty/`""` cell text |
| Draw | GadTools → hook → optional `clv_opt_draw_cellctl` | Checkbox paints inside cell clip on subline 0 |
| IDCMP | Application | Forward mouse coords + button codes to `clv_cellctl_handle_mouse` |
| Arm / cancel / commit | CLV + app `CLV_CellCtlInputState` | Verified up; CLV mutates snapshot on commit |
| Authoritative Boolean | Application | Update from event before next prepare |
| Reject toggle | Application | `clv_cellctl_set_checkbox_value` + refresh |
| Refresh | Application | `GT_SetGadgetAttrs` / `GT_RefreshWindow` (physical row or list) |
| Scroll | GadTools | Blit existing pixels; `LV_DRAW` for exposed rows |
| Destroy | Application / CLV | Stop input → detach labels → `clv_prepared_free` (frees cellctl slots) → free renderer |

#### Event / state transition table

| Condition | Armed? | Snapshot | Event | Notes |
|-----------|--------|----------|-------|-------|
| SELECTDOWN outside control | no | unchanged | none | Normal selection path may still run |
| SELECTDOWN on disabled/display-only/invisible | no | unchanged | none | |
| SELECTDOWN on interactive enabled control | yes | unchanged | none | Record physical/logical/column/type |
| SELECTUP while armed, pointer still in same control | clear | toggled | yes | `previous_value` / `value` filled |
| SELECTUP while armed, pointer left control | clear | unchanged | none | Cancel |
| SELECTUP not armed | — | unchanged | none | |
| App rejects after event | — | restored via setter | (already sent) | App must refresh |

Toggle rule: `UNCHECKED ↔ CHECKED` only.

#### Exact proposed public declarations

See §18.1 (`clv_cellctl.h` sketch).

#### Exact proposed private declarations

See §18.2 (`clv_cellctl_internal.h` + ops/prepared extensions).

#### Design Change Records

- DC-001 **Accepted**
- DC-002 **Accepted** — parallel descriptors; do not enlarge `CLV_CellPresentation`
- DC-003 **Accepted** — defer indeterminate

#### Next phase starting conditions

Phase 2 may begin immediately. No code blockers. Implement Makefile/`CLV_HAS_*` skeleton, empty modules, binder, and guarded include surfaces using §18 names only. Do not implement draw/hit behaviour beyond stubs that compile and link-omit cleanly.

---

## LEGACY — DO NOT EXECUTE — Phase 2 — Optional Build/Profile Skeleton

### Objective

Add the feature configuration and empty integration skeleton so checkbox support can be compiled in or completely omitted without implementing full behaviour yet.

### Required work

- Add repository-consistent enable and `CLV_HAS_*` feature concepts.
- Add dependency validation: checkbox requires control core and renderer.
- Add optional object lists/modules to the Makefile or profile-generation system.
- Add the chosen binder/registration integration.
- Add guarded public includes/declarations where appropriate.
- Add build profiles or test targets for baseline and checkbox-enabled drawn builds.
- Ensure baseline profiles link no checkbox/control implementation.
- Keep stubs minimal; no false claim of working checkbox behaviour.

### Validation

- Build every existing standard profile.
- Build the new checkbox-enabled profile.
- Compare link maps or symbol/module lists to confirm baseline omission.
- Run public-header validation.

### Deliverables

- Optional build skeleton.
- Updated profile table in this document.
- Initial size baseline.
- Developer-log entry.

### Exit criteria

- Existing profiles remain buildable.
- Checkbox-enabled profile builds its skeleton.
- Baseline build demonstrably omits control modules.
- Feature reporting matches linked objects.

### Phase completion record

**Completed:** 2026-07-30  
**Agent/session objective:** Optional build/profile skeleton only — no real
draw/hit/toggle behaviour.

#### Summary of what was done

- Added `CLV_ENABLE_CELLCTL` / `CLV_ENABLE_CELLCTL_CHECKBOX` and matching
  `CLV_HAS_CELLCTL` / `CLV_HAS_CELLCTL_CHECKBOX` (defaults 0 in `clv_config.h`;
  generated by `tools/generate_profile_config.ps1`).
- Dependency validation: `CELLCTL → RENDERER`, `CELLCTL_CHECKBOX → CELLCTL`
  in Makefile (`validate-custom`) and profile generator.
- New modules: `clv_cellctl.h`, `clv_cellctl_internal.h`, `clv_cellctl_core.c`,
  `clv_cellctl_checkbox.c`, `clv_bind_cellctl.c` (stubs; helpers return FALSE /
  NULL; draw installs a no-op ops pointer).
- Extended `CLV_RendererOptFns` with `draw_cellctl` + `clv_opt_draw_cellctl`
  (NULL-safe). Renderer core does **not** call it yet (Phase 3+).
- New profile `draw-cellctl-checkbox` and size twin `size-draw-cellctl-checkbox`.
- Existing profiles unchanged (no cellctl objects; `full-smoke` still omits
  cellctl). `make validate-cellctl-omission` proves omission vs inclusion.
- Public allowlist / umbrella / header compile matrix updated.
- Docs: `CLV_BUILD_PROFILES.md`, `CLV_MODULE_ARCHITECTURE.md`, size report.

#### Decisions locked this phase

| Topic | Decision |
|-------|----------|
| Profile name | `draw-cellctl-checkbox` |
| Binder | Dedicated `clv_bind_cellctl.c` only; existing binders untouched |
| full-smoke | Still omits cellctl (first pass) |
| Ops slot | `draw_cellctl` added now; core call site deferred to Phase 3+ |
| Prepare API | Declared + stub returns NULL |
| Working UI claim | Explicitly none — stubs only |

#### Build / validation evidence

| Check | Result |
|-------|--------|
| `validate-profile-config` / `validate-layout` / `validate-headers` | OK |
| `draw-basic`, `draw-selected`, `draw-wrapped`, `draw-styled`, `draw-details`, `full-smoke`, `ascii-minimal` | Linked |
| `draw-cellctl-checkbox` / `size-draw-cellctl-checkbox` | Linked |
| `validate-cellctl-omission` | OK (HAS macros, objects, link tag, size order) |
| Illegal ENABLE combos | Rejected (CELLCTL w/o RENDERER; CHECKBOX w/o CELLCTL) |

#### Initial size baseline (VBCC `+aos68k` `-O2 -size -final`, 2026-07-30)

| Profile | Bytes | Δ vs `size-draw-selected` |
|---------|------:|--------------------------:|
| `size-draw-basic` | 27940 | −1000 |
| `size-draw-selected` | 28940 | 0 |
| `size-draw-cellctl-checkbox` (stubs) | 29404 | **+464** |

Stub cost is provisional; real draw/hit/prepare will increase it. Full table in
`docs/CLV_SIZE_REPORT.md`.

#### Known limitations / follow-ups for Phase 3

- No geometry, hit-test, prepare slots, or gadget refresh helper.
- `clv_prepared_build_structured_cellctl` always returns NULL.
- `clv_opt_draw_cellctl` not yet called from structured draw.
- No dedicated interactive checkbox demo beyond reusing draw-basic shell.

#### Next phase starting conditions

Phase 3 may begin immediately. Implement shared control core geometry, private
draw-dispatch contract wiring, common hit-test contract, and neutral event
construction. Keep checkbox artwork and verified toggle commit for Phase 4–5.

---

## LEGACY — DO NOT EXECUTE — Phase 3 — Shared Control Core and Geometry

### Objective

Implement common control-cell infrastructure without yet completing checkbox artwork or state changes.

### Required work

- Resolve a control cell from the current row/column data.
- Calculate the cell and control rectangles.
- Apply alignment, padding, clipping, and minimum-size checks.
- Map visible/physical rows to logical rows correctly.
- Provide the private draw-dispatch contract.
- Provide the common hit-test contract.
- Provide neutral event/result construction without application behaviour.
- Avoid per-cell heap allocation unless proven necessary.
- Preserve and restore RastPort state according to current renderer conventions.

### Tests

- Geometry tests for left, centre, and right alignment where supported.
- Too-small-cell behaviour.
- Clipping at column and viewport edges.
- Wrapped physical-row mapping tests.
- Empty/hidden/disabled control data tests.

### Deliverables

- Shared control core.
- Internal geometry documentation.
- Tests or diagnostic evidence.
- Developer-log entry.

### Exit criteria

- Geometry can be tested independently of final checkbox drawing.
- Hit-testing uses the same resolved rectangle as drawing.
- No control code enters baseline builds.

### Phase completion record

**Completed:** 2026-07-30  
**Agent/session objective:** Shared control core geometry, prepare slots,
draw-dispatch wiring, hit-test contract, and neutral event helpers — without
checkbox artwork or verified toggle commit.

#### Summary of what was done

- Added pure geometry module `clv_cellctl_geom.c` / `.h`: appearance
  normalise/clamp, control-box resolve (alignment, padding, viewport clip,
  min-size skip), and point-in-box. Draw and hit call the same resolver.
- Geometry policy locked: **resolve at draw/hit** from column + appearance +
  row height; slots cache last-resolved `box_rel_*` only as diagnostics.
  Invalidation = next prepare/rebuild (existing path).
- `clv_prepared_build_structured_cellctl` wraps
  `clv_prepared_build_structured_present` then attaches parallel
  `CLV_CellCtlSlot` storage when any `CLV_COL_F_CELLCTL_CHECKBOX` column
  exists; `CLV_RenderNode.cellctl_row` points at the row base.
- Opaque `prepared->cellctl_slots` freed in `clv_prepared_free` (no hard
  cellctl symbol dependency from core).
- Ops `draw_cellctl` refined to **row-level** signature; structured draw
  calls `clv_opt_draw_cellctl` after cell/fragment text. Checkbox paint
  remains a no-op (Phase 4).
- Hit-test contract `clv_cellctl_hit_test`: window-relative MouseX/Y →
  gadget content (LeftEdge/TopEdge + 2px frame inset) → physical via
  `GTLV_Top` / `GTLV_ItemHeight` → display map (subline 0 only) → same
  control box as draw. `clv_cellctl_handle_mouse` arms/cancels only;
  commit deferred to Phase 5.
- `clv_cellctl_fill_event` for neutral event construction; get/set value
  operate on prepared slots.
- Host suite `make cellctl-geom-test` (23 cases). Amiga: profiles link;
  `validate-cellctl-omission` OK.

#### Decisions locked this phase

| Topic | Decision |
|-------|----------|
| Geometry timing | Resolve at draw/hit (not prepare-time absolute cache) |
| Mouse coords | Window-relative Intuition MouseX/MouseY; content = gadget edge + `CLV_CELLCTL_LIST_FRAME_INSET` (2), matching ASCII tracked inset |
| Draw ops signature | Row-level `draw_cellctl(rp, columns, count, rn, row_bounds, …)` |
| Slot storage | Parallel blob on prepared + `rn->cellctl_row`; not on every `CLV_RenderCell` |
| Toggle commit | Still Phase 5 |

#### Build / validation evidence

| Check | Result |
|-------|--------|
| Host `cellctl-geom-test` | 23/23 pass |
| `draw-cellctl-checkbox`, `draw-basic`, `draw-selected` | Linked |
| `validate-cellctl-omission` | OK |
| `validate-headers` | OK |

#### Size (VBCC `+aos68k` `-O2 -size -final`, 2026-07-30)

| Profile | Bytes | Notes |
|---------|------:|-------|
| `size-draw-basic` | 28004 | +64 vs prior (prepared/node optional fields) |
| `size-draw-selected` | 29004 | |
| `size-draw-cellctl-checkbox` | 33972 | **+4968 vs selected** (real geom/hit/prepare) |

#### Known limitations / follow-ups for Phase 4

- Checkbox paint still no-op (frame/mark/selected/disabled).
- No Amiga visual runtime proof of geometry yet.
- Mouse Y mapping uses `GTLV_ItemHeight` / font fallback; verify on target
  hardware against real `lvdm_Bounds` rows in Phase 4–5 demos.

#### Next phase starting conditions

Phase 4 may begin immediately. Implement checkbox artwork inside the resolved
clipped control rectangle; keep verified toggle commit for Phase 5.

---

## LEGACY — DO NOT EXECUTE — Phase 4 — Checkbox Rendering

### Objective

Implement compact checkbox artwork using the shared control core.

### Required work

- Draw unchecked and checked states.
- Implement the approved plain and/or 3D frame styles.
- Implement the approved mark style(s).
- Draw selected-row and disabled variants clearly.
- Validate dimensions and choose safe defaults.
- Ensure all drawing remains inside the supplied clipped rectangle.
- Avoid permanent mutation of RastPort state.
- Integrate the checkbox operations with the optional binder/registry.

### Visual validation matrix

Test at minimum:

- unchecked normal;
- checked normal;
- unchecked selected;
- checked selected;
- disabled unchecked;
- disabled checked;
- minimum supported size;
- default size;
- narrow column clipping;
- variable-height/wrapped logical row.

### Deliverables

- Working visual checkbox renderer.
- Screenshots or raster-dump evidence where the project supports it.
- Documented default appearance.
- Developer-log entry.

### Exit criteria

- Checkbox visuals are legible on the project’s target display modes.
- No input state transition is required yet.
- Rendering does not corrupt adjacent cells or scrolling areas.

### Phase completion record

**Completed:** 2026-07-30  
**Agent/session objective:** Compact checkbox artwork inside the shared
resolved clipped control rectangle — plain/3D frames, mark styles,
selected/disabled variants — without verified toggle commit.

#### Summary of what was done

- Implemented `clv_cellctl_checkbox_draw`: plain outline and recessed 3D
  frame; tick / cross / fill marks scaled to the painted box; TICK→FILL
  auto-fallback when either box dimension is < 7.
- Selected rows use renderer `apen` (FILLTEXTPEN); bevel never inverted.
- Disabled controls ghosted after paint (shadow dither; apen dither when
  selected).
- Drawing stays inside the clipped `control_bounds` from
  `clv_cellctl_draw_row`; AfPt restored after ghost; APen/DrMd restored by
  the row dispatcher.
- Dedicated demo `examples/05_draw_cellctl_checkbox/` with visual-matrix
  rows; size twin builds a real checkbox column via
  `clv_prepared_build_structured_cellctl`.
- Documented default appearance (9×9 plain + tick) in public header,
  example README, and §5.4 / §6.

#### Decisions locked this phase

| Topic | Decision |
|-------|----------|
| Default appearance | 9×9, `FRAME_PLAIN`, `MARK_TICK`, pad 1×1 (unchanged API) |
| Small-box tick | Auto-fallback to `FILL` when box w or h < 7 |
| Selected look | Use supplied `apen`; do not invert 3D bevel |
| Disabled look | Post-paint dither ghost; pen = shadow (normal) / apen (selected) |
| Min size clamp | Keep provisional \[5,15\]; 9×9 default remains recommended |
| Topaz-8 fit | Shrink pad then box to fit `lvdm_Bounds` row height |
| Inter-row gap | Reserve 1 px unused row height when row > MIN so boxes do not touch |

#### Build / validation evidence

| Check | Result |
|-------|--------|
| Host `cellctl-geom-test` | 25/25 pass (incl. Topaz-8 shrink + 1px gap) |
| `draw-cellctl-checkbox`, size twin | Linked (no VBCC warnings) |
| `validate-cellctl-omission` | OK |
| `validate-headers` | OK |
| Amiga/emulator visual run | **Pass** — `size-draw-cellctl-checkbox` on emulator (2026-07-30): checkboxes visible in On column (checked/unchecked/disabled), selected-row pens OK, scrolling OK after Topaz-8 shrink-to-fit + 1px inter-row gap |

#### Size (VBCC `+aos68k` `-O2 -size -final`, 2026-07-30)

| Profile | Bytes | Notes |
|---------|------:|-------|
| `size-draw-basic` | 28004 | unchanged |
| `size-draw-selected` | 29004 | unchanged |
| `size-draw-cellctl-checkbox` | 35180 | **+6176 vs selected** (+1208 vs Phase 3 geom) |

#### Known limitations / follow-ups for Phase 5

- ~~No verified mouse toggle commit or refresh helper.~~ **Done in Phase 5.**
- Fuller visual-matrix demo (`draw-cellctl-checkbox`) and RTG/lo-res
  matrix beyond the size twin remain optional follow-up checks.
- Mouse Y / `GTLV_ItemHeight` mapping still needs target verification with
  live hit testing in Phase 5.

#### Next phase starting conditions

Phase 4 paint is **emulator-verified working**. Phase 5 may begin
immediately: wire verified down/up toggle, snapshot mutation,
`CLV_CellCtlEvent` return, and smallest safe GadTools refresh while
preserving row-selection when the click is outside the control box.

---

## LEGACY — DO NOT EXECUTE — Phase 5 — Mouse Input and State Transition

### Objective

Make the checkbox safely interactive with the mouse and report a neutral application-facing result.

### Required work

- Integrate control hit-testing into the current input path.
- Verify the ListView instance, row, column, clipping, and control flags.
- Implement the approved down/up/verify semantics.
- Cancel activation when appropriate if the pointer leaves before release.
- Apply or request the state transition according to the Phase 1 ownership decision.
- Produce the control event/result.
- Trigger the smallest safe redraw.
- Ensure clicking outside the checkbox retains normal row-selection behaviour.
- Ensure disabled and display-only controls do not change.

### Tests

- click checked/unchecked transitions;
- press-inside/release-outside cancellation if supported;
- click outside the control but inside the row;
- disabled/display-only behaviour;
- first and last visible rows;
- partially clipped control;
- multiple ListView instances in one window if supported by the current library;
- repeated rapid clicks.

### Deliverables

- Functional mouse interaction.
- Event contract example.
- State-transition tests.
- Developer-log entry.

### Exit criteria

- State changes exactly once per valid activation.
- Row selection and checkbox activation do not interfere unexpectedly.
- The application can identify the affected logical row and column.

### Phase completion record

**Completed:** 2026-07-30  
**Agent/session objective:** Verified mouse down/up toggle, prepared-snapshot
mutation, `CLV_CellCtlEvent` delivery, and example GadTools refresh — without
breaking outside-control row selection.

#### Summary of what was done

- `clv_cellctl_handle_mouse` now commits on verified SELECTUP: mutates the
  prepared checkbox snapshot (`UNCHECKED ↔ CHECKED`), fills
  `CLV_CellCtlEvent`, returns TRUE. Still does not refresh the gadget.
- Pure host-testable helpers in `clv_cellctl_geom`:
  `clv_cellctl_flags_allow_arm`, `clv_cellctl_checkbox_next_value`,
  `clv_cellctl_input_transition` (arm / cancel / commit identity match).
  Core calls the transition helper then mutates the slot.
- Example `05_draw_cellctl_checkbox` wires `IDCMP_MOUSEBUTTONS`,
  zero-init `CLV_CellCtlInputState`, syncs application-authoritative
  `CLV_CellCtlDesc` store from the event, refreshes via `GTLV_Top` nudge +
  `GT_RefreshWindow`. `IDCMP_GADGETUP` selection path unchanged.
- Disabled / display-only never arm. Outside-box clicks leave input disarmed
  and return FALSE so normal row selection proceeds.
- Host `cellctl-geom-test`: 51/51 (toggle, arm flags, press/release cancel,
  rapid commits). Amiga: profiles linked; omission + headers OK.

#### Decisions locked this phase

| Topic | Decision |
|-------|----------|
| Commit semantics | Verified up; snapshot mutate + event; app refreshes |
| Refresh unit | Example uses whole-visible redraw via `GTLV_Top` re-assert (no new public helper) |
| Outside-control click | No arm / no event; selection adapter unchanged |
| Host vs Amiga | State machine + toggle rules on host; IDCMP/refresh on Amiga example |

#### Build / validation evidence

| Check | Result |
|-------|--------|
| Host `cellctl-geom-test` | 51/51 pass |
| `draw-cellctl-checkbox`, size twin | Linked (no VBCC warnings) |
| `validate-cellctl-omission` | OK |
| `validate-headers` | OK |
| Amiga/emulator live toggle | **Pending** — run `bin/draw-cellctl-checkbox` checklist in example README |

#### Size (VBCC `+aos68k` `-O2 -size -final`, 2026-07-30)

| Profile | Bytes | Notes |
|---------|------:|-------|
| `size-draw-basic` | 28004 | unchanged |
| `size-draw-selected` | 29004 | unchanged |
| `size-draw-cellctl-checkbox` | 36656 | **+7652 vs selected** (+1476 vs Phase 4 paint) |

#### Known limitations / follow-ups for Phase 6

- Emulator/live verification of hit Y mapping + toggle refresh still pending
  user run of `draw-cellctl-checkbox`.
- Wrapped-row / scroll / relayout integration is Phase 6.
- No public refresh helper yet (Phase 1: optional after demos).
- Multiple ListView instances in one window not demoed.

#### Next phase starting conditions

Phase 5 mouse commit is **implemented and host-verified**. Phase 6 may begin:
prove checkbox draw/hit remain aligned across smart scroll, jumps, wrapping,
column/viewport resize, and selection redraws; lock wrapped-row placement.

---

## LEGACY — DO NOT EXECUTE — Phase 6 — Selection, Wrapping, Scrolling, and Relayout Integration

> **CANCELLED (2026-07-30).** Mis-scoped GadTools roadmap. Do not implement.
> Corrected work is §D Phase C6 against `src/custom_listview_control/`.

### Objective

Prove that checkbox cells remain correct across the behaviours that make CLV more complex than a fixed-height list.

### Required work

- Test and fix smart one-step scrolling.
- Test larger jumps and full redraws.
- Test wrapped rows with multiple physical lines.
- Decide where the checkbox appears within a wrapped logical row.
- Prevent continuation lines from accidentally gaining separate controls unless explicitly intended.
- Test column resizing and minimum widths.
- Test viewport/window resizing.
- Test selection movement and deselection redraws.
- Test top/bottom visibility boundaries.
- Test multiple control-enabled ListView instances if supported.
- Verify cached geometry invalidation.

### Deliverables

- Integration fixes.
- Defined wrapped-row placement rule.
- Scroll/relayout test results.
- Developer-log entry.

### Exit criteria

- The visual checkbox and clickable rectangle remain aligned after every tested scroll and relayout operation.
- Smart scrolling does not leave stale marks or ghost controls.
- Wrapped rows map to the correct logical checkbox state.

### Phase completion record

_To be completed by the Phase 6 agent._

---

## LEGACY — DO NOT EXECUTE — Phase 7 — Keyboard and Accessibility Behaviour

> **CANCELLED (2026-07-30).** Mis-scoped GadTools roadmap. Corrected: §D Phase C7.

### Objective

Add or explicitly defer keyboard activation based on the actual navigation/focus facilities available in CLV.

### Required work

- Inspect completed keyboard-navigation support.
- Determine whether an active column/cell concept exists.
- Implement Space-to-toggle where the focused checkbox can be identified unambiguously.
- Ensure navigation keys do not toggle controls accidentally.
- Define disabled/display-only keyboard behaviour.
- Ensure keyboard and mouse paths produce equivalent events.
- If keyboard activation cannot be implemented cleanly, record a Design Change or deferral with exact dependencies.

### Deliverables

- Keyboard support or a documented, technically justified deferral.
- Updated event documentation.
- Developer-log entry.

### Exit criteria

- Keyboard behaviour is deterministic and documented, or the feature is explicitly deferred without leaving misleading API claims.

### Phase completion record

_To be completed by the Phase 7 agent._

---

## LEGACY — DO NOT EXECUTE — Phase 8 — Demo, Examples, and Integration Documentation

> **CANCELLED (2026-07-30).** Mis-scoped GadTools roadmap. Corrected: §D Phase C8.

### Objective

Provide a small, authoritative example showing how an application enables, supplies, receives, and destroys checkbox control cells.

### Required work

- Add or extend an example with a checkbox column.
- Demonstrate checked, unchecked, disabled, and display-only rows.
- Demonstrate handling of a control event in host application code.
- Demonstrate optional build/profile selection.
- Update `CUSTOM_LISTVIEW_INTEGRATION_GUIDE.md` or the current equivalent.
- Update public-header comments.
- Document ownership and lifecycle.
- Document that controls are custom-drawn, not embedded GadTools gadgets.
- Document how future control modules can reuse the shared core.

### Deliverables

- Compilable example.
- Updated integration guide.
- API usage documentation.
- Developer-log entry.

### Exit criteria

- A new integrator can add checkbox cells without reading private headers.
- The example follows the documented destruction order.
- Optional build instructions are accurate.

### Phase completion record

_To be completed by the Phase 8 agent._

---

## LEGACY — DO NOT EXECUTE — Phase 9 — Regression, Performance, and Binary-Size Validation

> **CANCELLED (2026-07-30).** Mis-scoped GadTools roadmap. Corrected: §D Phase C9.

### Objective

Measure the real cost and verify that the feature does not regress existing CLV behaviour.

### Required work

- Build all existing standard profiles.
- Run all existing tests.
- Run new control tests.
- Compare baseline and checkbox-enabled executable sizes.
- Inspect symbols/modules to prove omission in baseline builds.
- Benchmark redraw, smart scroll, and checkbox toggle where the benchmark framework permits.
- Check memory allocation/frees and repeated create/destroy cycles.
- Test on the project’s normal emulator profiles, including an A500-class 68000 profile where practical.
- Record which results are emulator-only and which are from real hardware, if any.

### Required size report

```text
Profile/build:
Compiler and flags:
Baseline bytes:
Checkbox-enabled bytes:
Delta bytes:
Delta percent:
Control core contribution, if measurable:
Checkbox module contribution, if measurable:
Unexpected symbols/modules in baseline: yes/no
```

### Deliverables

- Regression report in this document.
- Size and performance evidence.
- Any optimisation fixes that do not compromise the agreed architecture.
- Developer-log entry.

### Exit criteria

- Baseline builds exclude controls.
- Checkbox-enabled cost is measured and understood.
- No unresolved correctness regression remains.
- Performance is usable on the intended target or limitations are explicitly documented.

### Phase completion record

_To be completed by the Phase 9 agent._

---

## LEGACY — DO NOT EXECUTE — Phase 10 — Closure Audit and Release-Readiness Review

> **CANCELLED (2026-07-30).** Mis-scoped GadTools roadmap. Corrected: §D Phase C10.

### Objective

Perform a final evidence-based audit before declaring the checkbox control subsystem complete.

### Required work

- Review every section of this master document against the source.
- Review every developer-log entry.
- Confirm all phases have completion evidence.
- Review public/private header boundaries.
- Confirm no unimplemented control identifiers are advertised.
- Confirm lifecycle and ownership documentation matches code.
- Confirm optional object omission and feature reporting.
- Confirm examples build.
- Confirm all known defects and deferred work are documented.
- Identify whether the architecture is genuinely ready for a future cycle or button module without implementing either one.
- Remove obsolete temporary scaffolding and debug code unless intentionally retained behind diagnostics guards.

### Deliverables

- Final closure record.
- Final known-limitations list.
- Future-control readiness notes.
- Developer-log entry.

### Exit criteria

- Checkbox control is complete, documented, optional, measured, and working according to the supported test matrix.
- The master document accurately describes the final implementation rather than the original proposal.
- Future control work can begin through a new plan without destabilising checkbox support.

### Phase completion record

_To be completed by the Phase 10 agent._

---

## LEGACY — DO NOT EXECUTE — 13. Confirmed Design Decisions

> Historical GadTools decisions. **D-015, D-018, D-025 are Superseded by
> DC-004 / DC-005.** Do not use this table as authority for
> `src/custom_listview_control/`.

This section must be kept current as phases complete.

| ID | Decision | Status | Evidence/phase |
|----|----------|--------|----------------|
| D-001 | Controls are custom-drawn row content, not embedded GadTools gadgets. | Approved | Initial design discussion |
| D-002 | Checkbox is the only control implemented by this plan. | Approved | Initial design discussion |
| D-003 | The architecture keeps a generic control-type route for future controls. | Approved | Initial design discussion |
| D-004 | Public identifiers must not advertise unimplemented control types. | Approved | Initial design discussion |
| D-005 | Common control logic is shared, not duplicated per type solely to reduce binary size. | Approved | Initial design discussion |
| D-006 | Type-specific control modules remain independently optional. | Approved | Initial design discussion |
| D-007 | Object-file omission is the primary method of excluding unused code. | Approved | Existing CLV integration policy |
| D-008 | Compile guards and truthful `CLV_HAS_*` reporting complement object omission. | Approved | Existing CLV integration policy |
| D-009 | Semantic cell state is separate from checkbox presentation. | Approved | Initial design discussion |
| D-010 | Host applications own application-specific actions such as opening editor windows. | Approved | Initial design discussion |
| D-011 | Checkbox dimensions and visual style are configurable. | Approved | Initial design discussion |
| D-012 | No per-cell heap allocation is preferred. | **Accepted** | Phase 1: parallel prepared slots allocated once per prepare with the list; no per-draw heap |
| D-013 | Clicking the control toggles it; clicking elsewhere retains normal row behaviour. | **Accepted** | Phase 1 |
| D-014 | Fixed pixel dimensions are the initial sizing model. | **Accepted** | Phase 1; defaults 9×9, clamp \[5,15\] |
| D-015 | Feature lives in `src/custom_listview/` (GadTools ListView helpers), not `src/custom_listview_control/`. | **Superseded by DC-004** | Mis-scoped; corrected target is `custom_listview_control/` |
| D-016 | Integrate via `CLV_RendererOptFns` + binder/`*_install`, matching icons/styles/wrap. | Confirmed | Phase 0 audit; Phase 1: draw op only in table |
| D-017 | Application owns IDCMP; library provides pixel hit-test/mapping helpers only. | Confirmed | Phase 0: no mouse API in `src/custom_listview/` |
| D-018 | Do not reuse `CLV_Control` / `clv_control_*` / `CLV_CTRL_*` names from `src/custom_listview_control/`. | **Superseded by DC-004** | Feature now *extends* that package; legacy `clv_cellctl_*` keeps its distinct name |
| D-019 | Scroll optimisation for GadTools ListViews is not implemented inside `src/custom_listview/`; CLV paints via `LV_DRAW` when GadTools requests a row. | Confirmed | Phase 0 scroll correction |
| D-020 | Control type is column-level; value/flags are per-cell via `CLV_CellCtlDesc`. | **Accepted** | Phase 1 |
| D-021 | Do not enlarge `CLV_CellPresentation` for controls. | **Accepted** | DC-002 |
| D-022 | Prepared-snapshot mutation on commit; application owns authoritative store. | **Accepted** | Phase 1 §7.2 |
| D-023 | Events are synchronous `CLV_CellCtlEvent` returns (not selection-path, not callback). | **Accepted** | Phase 1 |
| D-024 | Verified button-up with app-owned `CLV_CellCtlInputState`. | **Accepted** | Phase 1 |
| D-025 | Checkbox draw/hit on wrapped subline 0 only. | **Superseded by DC-005** | Corrected: first content **line band** of logical row in `custom_listview_control/` (not GadTools physical subline) |
| D-026 | Indeterminate checkbox deferred from first implementation. | **Accepted** | DC-003 |
| D-027 | Keyboard activation deferred until a focus/column model exists (Phase 7). | **Accepted** | Phase 1 |
| D-028 | First checkbox profile = draw-basic + selection + cellctl; pixel wrap not default. | **Accepted** | Phase 1 §9.2 |

---

## LEGACY — DO NOT EXECUTE — 14. Open Questions

> Historical. Authoritative open work is §D (C2+). Do not reopen these as
> live tasks for the GadTools tree.

| # | Question | Resolution |
|---|----------|------------|
| 1 | Exact current cell-data representation? | **Resolved (P0)** — prepare copies text; `CLV_CellPresentation` at prepare. |
| 2 | Control type at column, cell, or both? | **Resolved (P1)** — type column; value/flags cell (`CLV_CellCtlDesc`). |
| 3 | CLV mutate vs application-owned transition? | **Resolved (P1)** — mutate prepared snapshot; app owns authoritative store (D-022). |
| 4 | Existing event/result structure to extend? | **Resolved (P1)** — new `CLV_CellCtlEvent`; do not extend selection. |
| 5 | Cell-level redraw available? | **Resolved (P1)** — no; document GadTools physical-row/list refresh. |
| 6 | Column identifiers across sort/relayout? | **Resolved (P1)** — column index only; note sort rebuilds. |
| 7 | Checkbox vertical align on multi-line wrap? | **Resolved (P1)** — subline 0 only. |
| 8 | Continuation physical rows cheaply identifiable? | **Resolved (P0)** — existing flags. |
| 9 | Binder/profile arrangement? | **Resolved (P1)** — `clv_bind_cellctl` + new profile; keep others free. |
| 10 | Minimum legible checkbox size? | **Resolved (P4 provisional)** — clamp \[5,15\] kept; recommended default **9×9**; TICK auto→FILL below 7 px. Confirm on lo-res/RTG via example checklist. |
| 11 | Indeterminate initially? | **Resolved (P1)** — deferred (DC-003). |
| 12 | Tick vs cross/fill default? | **Resolved (P4)** — API default remains **TICK**; paint auto-falls back to **FILL** when box < 7 px. |
| 13 | Keyboard activation with current nav? | **Resolved (P1)** — defer Phase 7. |
| 14 | Exact redraw path after control event? | **Resolved (P1)** — snapshot already updated; app `GT_SetGadgetAttrs` / `GT_RefreshWindow`. |
| 15 | Host vs Amiga tests? | **Resolved (P1)** — host geometry/hit/map; Amiga visual/IDCMP/refresh. |

**Still open / deferred (must remain listed):**

- Optional RTG / alternate-font visual matrix beyond the Topaz/8 size-twin
  emulator pass (2026-07-30). Paint path is accepted as working for Phase 4.
- Whether a small public refresh helper is worth adding after demos exist (not required for Phase 2–5).
- Coordinate space for mouse helper (window-relative vs gadget-relative):
  **Resolved (Phase 3)** — window-relative Intuition `MouseX`/`MouseY`; convert
  with gadget `LeftEdge`/`TopEdge` + `CLV_CELLCTL_LIST_FRAME_INSET` (2),
  matching `examples/02_ascii_tracked` / `CLV_ASCII_TEXT_INSET`. Physical row
  from `GTLV_Top` + `GTLV_ItemHeight` (font-height fallback).

---

## 15. Design Change Register

### Design Change DC-001 — Avoid `clv_control_*` naming for interactive cell controls

> **Historical (GadTools mis-scope).** DC-004 relocates the *feature* to
> `src/custom_listview_control/`, which already owns `clv_control_*`. New work
> extends `CLV_CTRL_*` / `clv_control_*` there. Legacy `clv_cellctl_*` under
> `src/custom_listview/` remains frozen and keeps its distinct name so the two
> trees do not collide.

**Date:** 2026-07-30  
**Phase:** 0 (proposed) / 1 (accepted)  
**Status:** Accepted

**Original design:**  
Module/API names such as `clv_control_core.c`, `clv_control_checkbox.c`, `CLV_CONTROL_*`, and conceptual `CLV_ControlEvent`.

**Evidence or problem:**  
`src/custom_listview_control/` already exports `CLV_Control`, `CLV_ControlConfig`, `CLV_ControlRow`, `CLV_ControlColumn`, `clv_control_*` functions, `CLV_CTRL_*` flags, and `clv_control_log` for a separate experimental custom-drawn viewport that is **not** GadTools `LISTVIEW_KIND`. Reusing the same “control” vocabulary for interactive cells inside `src/custom_listview/` would create dangerous public-API ambiguity and likely symbol/documentation collisions if both packages are present in the repository or a workspace.

`docs/CLV_CUSTOM_CONTROL_PHASE1_AUDIT.md` also separates that package from `src/custom_listview/` `CLV_HAS_*` / `clv_config.h`. Interactive cells **should** use profile `CLV_HAS_*` under `src/custom_listview/`, but under distinct feature names.

**New design:**  
Use a distinct prefix for the interactive cell-control feature family:

- files: `clv_cellctl_core.c`, `clv_cellctl_checkbox.c`, `clv_bind_cellctl.c`, `clv_cellctl.h`, `clv_cellctl_internal.h`
- public macros/types: `CLV_CELLCTL_*`, `CLV_COL_F_CELLCTL_*`, `CLV_CheckboxAppearance`, `CLV_CellCtlDesc`, `CLV_CellCtlEvent`, `CLV_CellCtlInputState`
- functions: `clv_cellctl_*`
- enables/HAS: `CLV_ENABLE_CELLCTL`, `CLV_HAS_CELLCTL`, `CLV_ENABLE_CELLCTL_CHECKBOX`, `CLV_HAS_CELLCTL_CHECKBOX`

Do not place cell-control sources under `src/custom_listview_control/`.

**Why this is better:**  
Preserves clarity between the experimental custom-drawn viewport (`src/custom_listview_control/`) and interactive control cells on GadTools ListView (`src/custom_listview/`). Avoids link/documentation collisions and follows the repository’s existing optional-module pattern under `src/custom_listview/`.

**Consequences:**  
§3 / §5 / §9 / §18 use these names. Phase 2 Makefile must use them. Old conceptual `CLV_CONTROL_*` sketches are retired.

**Alternatives considered:**  
- Keep `CLV_CONTROL_*` macros only (still confuses readers beside `CLV_Control`). Rejected.  
- `clv_controls_*` (plural) vs `clv_control_*` — still too close. Rejected as primary.  
- Nest under icons (`CLV_ICON_*` interactive) — conflates static icons with input. Rejected.

### Design Change DC-002 — Parallel cellctl descriptors; do not enlarge `CLV_CellPresentation`

**Date:** 2026-07-30  
**Phase:** 1  
**Status:** Accepted

**Original design:**  
Possible extension of `CLV_CellPresentation` or overload of `flags` / `icon_id` for control state.

**Evidence or problem:**  
`CLV_CellPresentation` is the Phase 5 style/icon/sort ABI. Growing it taxes every presentation-aware client and conflates passive icons (`CLV_ICON_EMPTY_BOX`) with interactive controls. Overloading `icon_id` would break the “icons are presentation only” rule.

**New design:**  
Introduce compact parallel `CLV_CellCtlDesc` arrays at prepare (same indexing shape as `row_present`). Prepared storage for control snapshots is feature-local parallel slots, not new fields on every `CLV_RenderCell`/`CLV_RenderFragment` in non-cellctl builds.

**Why this is better:**  
Keeps text/icon rows free of control ABI; matches optional-module and memory-conscious policies; clear separation of meaning vs appearance.

**Consequences:**  
New prepare entry (or present+cellctl wrapper) in Phase 2/3; public `clv_cellctl.h`; private slots freed with `clv_prepared_free`.

**Alternatives considered:**  
- Grow `CLV_CellPresentation` by two `UBYTE`s — small but permanent ABI/tax. Rejected.  
- Encode state in cell text — fragile, fights structured cells. Rejected.

### Design Change DC-003 — Defer indeterminate checkbox

**Date:** 2026-07-30  
**Phase:** 1  
**Status:** Accepted

**Original design:**  
Tri-state enum including indeterminate in the first checkbox API.

**Evidence or problem:**  
No existing tri-state in the renderer; first demos and hit/toggle rules are Boolean; indeterminate mark art and activation semantics add Phase 4/5 scope without a current consumer.

**New design:**  
Public API documents `UNCHECKED` / `CHECKED` only. Value remains `UBYTE` so a later phase can add indeterminate without widening `CLV_CellCtlDesc`.

**Why this is better:**  
Smaller first implementation; no false claim of tri-state support.

**Consequences:**  
Phase 4 draws only two value appearances unless DC-003 is superseded.

**Alternatives considered:**  
- Ship indeterminate as display-only initially — still needs public ID and draw path. Deferred entirely instead.

---

_Add further Design Change Records here in numerical order._

---

## 16. Overall Completion Record

> **LEGACY — DO NOT EXECUTE. Obsolete for directing work.** See §F.

**Overall status (legacy archive):** Phases 0–5 complete against wrong target;
Phases 6–10 cancelled  
**Authoritative status:** See §F (C1 locked; C2 gated)  
**Next required action:** Do **not** follow any “begin Phase 6” text in this
archive.

---

## LEGACY — DO NOT EXECUTE — 17. Confirmed Source Architecture (Phase 0)

> **Superseded as product authority by DC-004.** This section describes
> GadTools `src/custom_listview/` as it existed for the abandoned cellctl work.
> Phrases such as “sole implementation target” below are **historical and
> revoked**.

This section replaces prior conceptual assumptions with audited facts for the
GadTools Custom ListView helpers under `src/custom_listview/`.

**Terminology:** Prefer exact paths. Do not use “v1”, “old ListView”, or
“new ListView” as architectural identifiers.

### 17.1 Packages that must not be conflated

| Package | Path | Role |
|---------|------|------|
| **(LEGACY) GadTools CLV helpers** | `src/custom_listview/` | ASCII + drawn GadTools `LISTVIEW_KIND`; **abandoned** as checkbox-cell target (DC-004) |
| **(CORRECTED) Custom control** | `src/custom_listview_control/` | **Authoritative** checkbox-cell target — see §A–§F |

Interactive checkbox cells for the corrected plan belong under
`src/custom_listview_control/`. The sentence “belong exclusively under
`src/custom_listview/`” that formerly appeared here is **revoked**.

### 17.2 Public structures relevant to cells

- `CLV_PixelColumn` — pixel column geometry; `flags` currently unused by renderer.
- `CLV_CellPresentation` — per-cell style/icon/sort/flags; borrowed at prepare.
- `CLV_RendererOptions` / opaque `CLV_Renderer` — hook owner, copied columns/dividers/icon defaults.
- Opaque `CLV_PreparedList` — owns Exec list, copied columns, display map, logical arrays.
- `CLV_RenderMap` — physical → logical/subline/flags.
- Details: `CLV_DetailsRow` / builder (separate semantic path; still prepares into renderer nodes).

### 17.3 Private prepared representation

- `CLV_RenderNode` — physical node; `logical_index`, `subline_index`, cells or fragments.
- `CLV_RenderCell` — non-wrap cached text + layout + icon/style/sort.
- `CLV_RenderFragment` — wrap subline cache; continuation flags in high bits of `flags`.
- `ln_Name` for structured rows points at shared empty sentinel `g_clv_lv_empty_name` — never free as row text.

### 17.4 Ownership model (structured drawn path)

1. Application supplies header/data string pointers and optional `CLV_CellPresentation` arrays.
2. Prepare **duplicates** cell text into prepared storage; presentation fields are **copied** into cells/fragments.
3. Application may free/replace its source arrays after successful prepare.
4. Application must detach `GTLV_Labels` before `clv_prepared_free`.
5. Renderer hook must outlive gadget use; free renderer only after detach.

There is **no** retained application row `user_data` on structured render nodes.

Absence of interactive control-cell rendering/state/click handling in this tree
is expected and is the reason for this plan — not a cue to modify another package.

### 17.5 Draw path

```text
CreateGadget(..., GTLV_CallBack, clv_renderer_get_hook())
  → HookEntry → clv_renderer_dispatch (LV_DRAW only)
    → clv_listview_draw_structured (structured + flag)
      → background fill
      → per cell/fragment: optional icon, text, underline
      → optional clv_opt_draw_cellctl (row; subline 0 / DATA)
      → phase-2 dividers/rules
```

Draw does not allocate, wrap, or TextFit. Cell controls follow prepare-then-draw;
geometry is re-resolved in the draw/hit helpers from column + appearance.

### 17.6 Input and hit-testing today

- GadTools reports physical selection via gadget messages; examples call `clv_handle_selection`.
- `clv_cell_tracking_*` is ASCII character-column detection only.
- Pixel control hit-test exists when cellctl is linked: `clv_cellctl_hit_test` /
  `clv_cellctl_handle_mouse` (verified down/up commit mutates snapshot and
  fills `CLV_CellCtlEvent`; application owns IDCMP + GadTools refresh).
- No keyboard column focus in `src/custom_listview/` (Phase 7).

### 17.7 Scroll and redraw (corrected)

Inside `src/custom_listview/`:

- There is **no** function that chooses smart scroll vs full redraw.
- There is **no** raster-copy / `ScrollRaster*` implementation.
- Newly visible content is painted when GadTools calls `LV_DRAW` for physical rows.
- There is **no** CLV-owned full-redraw fallback path for scrolling.
- Wrapped content uses multiple fixed-height physical rows; GadTools still scrolls by item.

GadTools owns ListView scroll optimisation for `LISTVIEW_KIND` (historical note in
`docs/OldDocs/CUSTOM_LISTVIEW_EXTRACTION_AND_ARCHITECTURE.md`). Drawn cell
pixels therefore participate automatically when GadTools blits rows.

Separately, measured smart-scroll code (`CLV_ENABLE_SMART_SCROLL`, eligibility,
`ScrollRaster`/`ScrollRasterBF`, exposed-band regional paint) resides under
`src/custom_listview_control/` and the `custom-control-demo*` targets. That is
**not** part of this plan’s architecture. Shared benchmark counter definitions
in `src/custom_listview/clv_bench*.c/h` are instrumentation only.

Application-driven control toggles still need an explicit GadTools refresh
sequence; CLV has no cell-level invalidate API.

### 17.8 Lowest-risk integration points

1. **Prepare:** accept parallel `CLV_CellCtlDesc` arrays; copy into feature-local prepared slots (not fields on every text cell).
2. **Ops table:** add NULL-safe **draw** pointer; `clv_cellctl_install` from optional objects. Hit-test stays in public helpers.
3. **Draw:** in `clv_listview_draw_structured`, call optional draw beside icon drawing, inside existing clip, **subline 0 only**.
4. **Input:** `clv_cellctl_handle_mouse` using prepared list + window-relative coords + display map; application handles IDCMP.
5. **Refresh:** application-owned GadTools refresh (physical row or list); optional helper later if demos duplicate the sequence.

### 17.9 Tests, demos, size, logging to extend

| Asset | Path | Use for cellctl |
|-------|------|-----------------|
| Host viewport/clip tests | `tests/host/renderer_viewport_tests.c` | Control rect clip / scrollbar exclusion |
| Host display map | `tests/host/prepared_display_map_tests.c` | Wrapped click mapping |
| Host ownership | `tests/host/ownership_tests.c` | Metadata ownership / partial failure |
| Host suite | `tests/host/Makefile`, `README.md` | `make test` |
| Draw basic example | `examples/05_draw_basic/` | Baseline drawn shell |
| Draw cellctl checkbox | `examples/05_draw_cellctl_checkbox/` | Phase 5 mouse toggle + event contract |
| Draw wrapped | `examples/06_draw_wrapped/` | Wrap/continuation behaviour |
| Size harness | `examples/size_compare/`, `make sizes` | Delta vs `size-draw-basic` |
| Size report | `docs/CLV_SIZE_REPORT.md` | Record measurements |
| Profiles | `docs/CLV_BUILD_PROFILES.md`, root `Makefile` | New profile + HAS macros |
| Log macros | `src/custom_listview/clv_log.h` | Optional printf diagnostics |
| Avoid | `src/custom_listview_control/clv_control_log.*` | Other package logger |

### 17.10 Assumptions resolved or remaining after Phase 1

**Resolved in Phase 1:** DC-001 spelling; prepare copies control descriptors into snapshot slots; verified button-up needs app-owned `CLV_CellCtlInputState`.

**Still remaining:**

- Optional RTG / alternate-font checks beyond the Topaz/8 emulator pass that
  confirmed Phase 4 paint + scroll (`size-draw-cellctl-checkbox`, 2026-07-30).
- Emulator live verification of Phase 5 mouse toggle + refresh
  (`bin/draw-cellctl-checkbox` checklist).
- Exact mouse coordinate conversion details vs gadget borders (**Resolved P3** —
  window-relative + frame inset 2; see Phase 3 record).
- Wrapped/scroll/relayout control alignment (**Phase 6**).

---

## 18. Definitive Phase 1 API and Internal Design

Public/private surfaces from Phase 1, now present in the tree as Phase 2
skeleton sources (stubs until Phase 3+).

### 18.1 Public header sketch — `clv_cellctl.h`

```c
#ifndef CLV_CELLCTL_H
#define CLV_CELLCTL_H

#include "custom_listview/clv_platform.h"
#include <exec/types.h>

struct Gadget;
struct Window;
/* CLV_PreparedList forward-declared; include clv_renderer.h as needed */

#define CLV_CELLCTL_NONE      0
#define CLV_CELLCTL_CHECKBOX  1

#define CLV_COL_F_CELLCTL_CHECKBOX  (1U << 0)

#define CLV_CELLCTL_F_VISIBLE      (1U << 0)
#define CLV_CELLCTL_F_ENABLED      (1U << 1)
#define CLV_CELLCTL_F_INTERACTIVE  (1U << 2)

#define CLV_CELLCTL_CHECKBOX_UNCHECKED  0
#define CLV_CELLCTL_CHECKBOX_CHECKED    1

#define CLV_CELLCTL_FRAME_PLAIN  0
#define CLV_CELLCTL_FRAME_3D     1

#define CLV_CELLCTL_MARK_TICK    0
#define CLV_CELLCTL_MARK_CROSS   1
#define CLV_CELLCTL_MARK_FILL    2

#define CLV_CELLCTL_CHECKBOX_DEFAULT_WIDTH   9
#define CLV_CELLCTL_CHECKBOX_DEFAULT_HEIGHT  9
#define CLV_CELLCTL_CHECKBOX_MIN_WIDTH       5
#define CLV_CELLCTL_CHECKBOX_MIN_HEIGHT      5
#define CLV_CELLCTL_CHECKBOX_MAX_WIDTH      15
#define CLV_CELLCTL_CHECKBOX_MAX_HEIGHT     15

typedef struct CLV_CheckboxAppearance
{
    UBYTE width;
    UBYTE height;
    UBYTE frame_style;
    UBYTE mark_style;
    UBYTE pad_h;
    UBYTE pad_v;
} CLV_CheckboxAppearance;

typedef struct CLV_CellCtlDesc
{
    UBYTE flags;
    UBYTE value;
} CLV_CellCtlDesc;

typedef struct CLV_CellCtlEvent
{
    UWORD logical_row;
    UWORD physical_row;
    UWORD column;
    UWORD control_type;
    UWORD previous_value;
    UWORD value;
} CLV_CellCtlEvent;

typedef struct CLV_CellCtlInputState
{
    BOOL  armed;
    UWORD physical_row;
    UWORD logical_row;
    UWORD column;
    UWORD control_type;
} CLV_CellCtlInputState;

/* Prepare-time appearance (like icon layout setters). NULL/zeros → defaults. */
void clv_cellctl_prepare_set_appearance(const CLV_CheckboxAppearance *appearance);

/*
 * Mouse helper: window-relative Intuition MouseX/MouseY + SELECTDOWN/SELECTUP.
 * Returns TRUE when out_event is filled (committed toggle).
 * Mutates prepared snapshot on commit. Does not refresh the gadget.
 */
BOOL clv_cellctl_handle_mouse(
    CLV_PreparedList *prepared,
    struct Gadget *listview,
    struct Window *window,
    WORD mouse_x,
    WORD mouse_y,
    UWORD code,
    CLV_CellCtlInputState *input,
    CLV_CellCtlEvent *out_event);

BOOL clv_cellctl_set_checkbox_value(
    CLV_PreparedList *prepared,
    UWORD logical_row,
    UWORD column,
    UBYTE value);

BOOL clv_cellctl_get_checkbox_value(
    const CLV_PreparedList *prepared,
    UWORD logical_row,
    UWORD column,
    UBYTE *out_value);

#endif /* CLV_CELLCTL_H */
```

Prepare entry (declared in `clv_cellctl.h`; Phase 2 stub returns NULL):

```c
CLV_PreparedList *
clv_prepared_build_structured_cellctl(
    /* same parameters as clv_prepared_build_structured_present, then: */
    const CLV_CellCtlDesc *const *row_cellctl);
```

`row_cellctl` may be NULL. Column type still comes from `columns[i].flags`.
Implementation may wrap/call the present builder then attach slots (Phase 3 detail).

### 18.2 Private header sketch — `clv_cellctl_internal.h`

```c
/* Private — not for application includes */

typedef struct CLV_CellCtlSlot
{
    UBYTE flags;
    UBYTE value;
    UBYTE control_type; /* copied from column at prepare */
    /* last resolved relative box (draw/hit); authoritative resolve is live */
    WORD  box_rel_x;
    WORD  box_rel_y;
    UBYTE box_w;
    UBYTE box_h;
} CLV_CellCtlSlot;

/* On CLV_PreparedList when cellctl linked and any control column present:
 * cellctl_slots[logical_row * column_count + column] for data rows only.
 * Absent / NULL when feature unused or no control columns.
 * CLV_RenderNode.cellctl_row → row base.
 */

void clv_cellctl_install(void);

/* Ops table (clv_renderer_internal.h): row-level draw_cellctl. */
```

### 18.3 Optional-build boundary (Phase 2 must implement)

| Linked? | Objects |
|---------|---------|
| No cellctl | none of the above; `CLV_HAS_CELLCTL=0` |
| Checkbox profile | `clv_cellctl_core.o`, `clv_cellctl_checkbox.o`, `clv_bind_cellctl.o` (+ renderer + selection) |

Public identifiers advertise only `NONE` and `CHECKBOX`. No cycle/button symbols.

### 18.4 Files Phase 2 is expected to add

| Path | Initial content |
|------|-----------------|
| `src/custom_listview/clv_cellctl.h` | Public declarations from §18.1 |
| `src/custom_listview/clv_cellctl_internal.h` | Private sketches |
| `src/custom_listview/clv_cellctl_geom.c` / `.h` | Pure geometry (Phase 3) |
| `src/custom_listview/clv_cellctl_core.c` | Install, prepare slots, hit-test, get/set |
| `src/custom_listview/clv_cellctl_checkbox.c` | Checkbox paint (plain/3D, marks, ghost) |
| `src/custom_listview/clv_bind_cellctl.c` | Calls `clv_cellctl_install` |
| Makefile / profile generator / `clv_config.h` | ENABLE/HAS + dependency `CELLCTL_CHECKBOX → CELLCTL → RENDERER` |
