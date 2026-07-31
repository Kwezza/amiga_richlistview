# CLV Interactive Control Cells — Developer Log

Append-only log for the interactive control-cells (checkbox) work tracked by
`docs/CLV_INTERACTIVE_CONTROL_CELLS_MASTER_PLAN.md`.

Do not rewrite earlier entries to tidy history. Add corrections as new entries
that identify what they supersede.

---

## 2026-07-30 — Phase 0: Source and Architecture Audit

**Phase:** 0 — Source and Architecture Audit  
**Status:** Complete  
**Agent/session objective:** Audit the current v1 renderer architecture and
update the master plan with confirmed facts; create this developer log. No
implementation code.

### Source files inspected

- `src/custom_listview/clv_renderer.h`
- `src/custom_listview/clv_renderer_internal.h`
- `src/custom_listview/clv_prepared_internal.h`
- `src/custom_listview/clv_types.h`
- `src/custom_listview/clv_config.h`
- `src/custom_listview/clv_renderer_core.c` (draw/prepare/dispatch symbols)
- `src/custom_listview/clv_renderer_ops.c` / ops table usage via internal header
- `src/custom_listview/clv_bind_*.c` (none/wrapped/styled/details/full)
- `src/custom_listview/clv_cell_tracking.h` / tracking role
- `src/custom_listview/clv_selection.c` (via audit: selection adapter)
- `src/custom_listview_control/clv_control.h` (namespace collision check only)
- Root `Makefile` (`CLV_ENABLE_*`, binders, profile generation)
- `docs/CLV_BUILD_PROFILES.md`, `docs/CLV_SIZE_REPORT.md`,
  `docs/CLV_CUSTOM_CONTROL_PHASE1_AUDIT.md` (naming/HAS guidance)
- `docs/CLV_INTERACTIVE_CONTROL_CELLS_MASTER_PLAN.md`

### Files modified, added, renamed, or removed

| Action | Path |
|--------|------|
| Updated | `docs/CLV_INTERACTIVE_CONTROL_CELLS_MASTER_PLAN.md` |
| Created | `docs/CLV_CONTROL_CELLS_DEVELOPER_LOG.md` (this file) |

No `.c` / `.h` implementation sources were added or changed.

### API and architecture decisions

- Interactive cell controls target **v1** `src/custom_listview/` GadTools
  renderer only — not the experimental `custom_listview_control` package.
- Confirmed prepare copies cell text; `CLV_CellPresentation` is prepare-time
  input only; structured draw uses cached cells/fragments.
- Confirmed optional features integrate through `CLV_RendererOptFns` +
  `clv_bind_*.c` + `*_install()`.
- Confirmed no v1 pixel hit-test, keyboard nav, smart scroll, or cell-level
  invalidate API.
- Proposed naming `clv_cellctl_*` / `CLV_CELLCTL_*` / `CLV_HAS_CELLCTL*` to
  avoid collision with `CLV_Control` / `clv_control_*` (Design Change DC-001,
  Status: Proposed).
- Static `CLV_ICON_EMPTY_BOX` / `CLV_ICON_FILLED_BOX` are presentation icons
  only — not a substitute for interactive checkbox cells.
- Developer log path normalised to `docs/` (repository convention); master
  plan previously said `Docs/`.

### Build commands run

None required for this documentation-only phase. No Amiga or host builds were
executed.

### Tests and runtime checks performed

None (audit only). Exit criteria explicitly require no implementation code.

### Measured sizes or performance data

None collected this phase. Size baseline for later comparison:
`size-draw-basic` per `docs/CLV_SIZE_REPORT.md` (do not use ASCII minimal or
`custom-control-demo` as the control-cell baseline).

### Defects found

None in product code (no code changes). Documentation risk identified: original
plan module names `clv_control_*` would collide conceptually and likely in
symbols/docs with the experimental custom-control package.

### Known limitations

- DC-001 naming not yet Accepted by Phase 1.
- State ownership, event shape, wrap vertical policy, indeterminate support,
  and redraw helper design remain Phase 1 decisions.
- Keyboard activation deferred pending a v1 focus/column model (Phase 7).

### Next recommended action

Begin **Phase 1 — Finalise Public/Private Design and Lifecycle**:

1. Accept or revise DC-001.
2. Lock public/private headers sketches, ownership/lifecycle, and event tables.
3. Produce the Phase 1 design question answers listed in the Phase 0 completion
   record and updated §14 of the master plan.
4. Do not implement behaviour or Makefile skeleton until Phase 1 exit criteria
   are met (Phase 2 owns the build skeleton).

---

## 2026-07-30 — Phase 0 Scope Clarification and Audit Correction

This entry clarifies and, where stated, supersedes parts of the original
Phase 0 entry.

### Authoritative implementation target

All interactive control-cell work in this plan applies exclusively to:

`src/custom_listview/`

Agents must not design, implement, patch, test, or derive the architecture
from:

`src/custom_listview_control/`

That directory may be inspected only when necessary to identify naming
collisions. Its architecture and existing behaviour are not evidence of how
interactive cells should be implemented in the target CLV component.

Imported or historical GadTools sources, including the original ListView and
checkbox implementations, are reference material only. They are not the
implementation target.

The terms "v1", "old ListView", and "new ListView" must not be used as
architectural identifiers because they are ambiguous. Agents must name the
exact repository path.

### Existing versus missing functionality

It is expected and correct that interactive control-cell rendering, state
handling, and click processing do not yet exist in `src/custom_listview/`.
Their absence is the reason for this implementation plan and is not evidence
that another ListView implementation should be modified.

### Smart-scroll audit requires correction

The previous statement that the target CLV has no smart-scroll support is not
accepted without further source tracing. Existing project work indicates that
smart scrolling has already been implemented and benchmarked.

Before Phase 1 begins, inspect the complete `src/custom_listview/` call path
and identify:

- the function that decides between smart scrolling and full redraw;
- the raster-copy or scroll operation;
- newly exposed row redraw handling;
- fallbacks that force a full redraw;
- interaction with variable-height or wrapped rows.

If smart scrolling is implemented outside the renderer modules, document that
exact location. If the previous statement meant there is no public smart-scroll
API, revise the finding to say so precisely.

### Phase status

Phase 0 remains complete for the general renderer and optional-feature audit,
but its scrolling/redraw finding is provisional until the correction above is
recorded in the master plan.

### Correction tracing result (same day)

Re-traced **only** under `src/custom_listview/` (plus naming-collision location
notes). Revised finding:

| Asked | Result in `src/custom_listview/` |
|-------|----------------------------------|
| Function that decides smart scroll vs full redraw | **None.** No such decision exists in renderer, selection, prepare, or ASCII modules. |
| Raster-copy / scroll operation | **None.** No `ScrollRaster`, `ScrollRasterBF`, `BltBitMap*`, or equivalent in this tree. |
| Newly exposed row redraw | GadTools invokes the existing `LV_DRAW` hook (`clv_renderer_dispatch` → structured/legacy draw) for rows it wants painted. CLV does not compute an “exposed band”. |
| Full-redraw fallback | **None** owned by CLV. Application examples use ordinary GadTools refresh (`GT_SetGadgetAttrs` / `GT_RefreshWindow`). |
| Variable-height / wrapped rows | GadTools ListView remains **fixed item height**. Wrapping creates multiple physical nodes / sublines (`CLV_RenderMap`, fragment continuation flags). Scroll is still by physical item under GadTools. |

**Precise revised statement (supersedes “no smart scroll in v1 renderer”):**

1. `src/custom_listview/` provides **no public and no private smart-scroll API** and **does not implement** pixel-shift scrolling.
2. For GadTools `LISTVIEW_KIND`, scroll optimisation is **owned by GadTools** (OS). Historical project note: `docs/OldDocs/CUSTOM_LISTVIEW_EXTRACTION_AND_ARCHITECTURE.md` — “No `ScrollRasterBF` in helper; GadTools owns scroll optimisation.” Drawn checkbox pixels therefore move with smart-scrolled row imagery when GadTools chooses to blit, then newly exposed physical rows are painted via `LV_DRAW`.
3. Project **smart-scroll implementation and benchmarks** that use `CLV_ENABLE_SMART_SCROLL`, `ScrollRaster`/`ScrollRasterBF`, eligibility checks, and scroll-copy counters live under `src/custom_listview_control/` (and the `custom-control-demo*` targets). Makefile line ~45–47 states that flag is for the experimental custom control. That package must **not** supply the interactive-cell architecture; it is cited here only to locate where the measured smart-scroll work actually resides.
4. `src/custom_listview/clv_bench.c` / `clv_bench_internal.h` define shared **instrumentation** symbols (including `CLV_BENCH_FEATURE_SMART_SCROLL` and scroll-copy counters). Those counters are incremented by `src/custom_listview_control/` call sites when the control demo is built with benchmarks — not by the GadTools renderer scroll path (there is none).

**Impact on other Phase 0 findings:** Unchanged for prepare/draw/ops/binders/hit-test/ownership. Changed: wording that used ambiguous “v1”; scroll/redraw must describe GadTools-owned scroll + CLV per-row `LV_DRAW`, not “no smart scroll anywhere.” Cell-control toggles still need a documented application-side GadTools refresh path because CLV still has no cell-level invalidate helper.

---

## 2026-07-30 — Phase 1: Finalise Public/Private Design and Lifecycle

**Phase:** 1 — Finalise Public/Private Design and Lifecycle  
**Status:** Complete  
**Agent/session objective:** Convert the Phase 0 audit into a minimal coherent
public/private API, ownership/lifecycle tables, and accepted design changes.
No implementation skeleton (Phase 2). No behaviour code.

Note: The session request named “Phase 1 — Source and Architecture Audit”;
that title is Phase **0** in the master plan (already complete). This entry
records the next authorised phase: Phase 1 design finalisation.

### Source files inspected

- `src/custom_listview/clv_renderer.h` (presentation, prepare APIs, render flags)
- `src/custom_listview/clv_prepared_internal.h` (`CLV_RenderCell` / fragment / node)
- `src/custom_listview/clv_renderer_internal.h` (`CLV_RendererOptFns`, prepared list)
- `src/custom_listview/clv_types.h` (`CLV_PixelColumn.flags`)
- `src/custom_listview/clv_config.h` (`CLV_HAS_*` pattern)
- `src/custom_listview/clv_selection.h` (event-result pattern to *not* overload)
- `src/custom_listview/clv_renderer_core.c` (icon `subline_index == 0` draw policy;
  `clv_prepared_build_structured_present` ownership)
- `examples/05_draw_basic/` (GadTools attach/refresh/detach pattern)
- `docs/CLV_INTERACTIVE_CONTROL_CELLS_MASTER_PLAN.md` (Phase 0 record, §17)
- `docs/CLV_CONTROL_CELLS_DEVELOPER_LOG.md`
- `docs/CLV_BUILD_PROFILES.md` (profile/binder pattern)

### Files modified, added, renamed, or removed

| Action | Path |
|--------|------|
| Updated | `docs/CLV_INTERACTIVE_CONTROL_CELLS_MASTER_PLAN.md` |
| Updated | `docs/CLV_CONTROL_CELLS_DEVELOPER_LOG.md` (this entry) |

No `.c` / `.h` implementation sources were added or changed.

### API and architecture decisions

- **DC-001 Accepted:** `CLV_CELLCTL_*` / `clv_cellctl_*` / `CLV_HAS_CELLCTL*`.
- **DC-002 Accepted:** parallel `CLV_CellCtlDesc`; do not enlarge
  `CLV_CellPresentation`.
- **DC-003 Accepted:** indeterminate deferred; value stays `UBYTE`.
- Column-level type (`CLV_COL_F_CELLCTL_CHECKBOX`); per-cell flags/value.
- State: mutate prepared snapshot on verified commit; application owns
  authoritative store and syncs before next prepare.
- Events: synchronous `CLV_CellCtlEvent` from `clv_cellctl_handle_mouse`.
- Mouse: verified button-up; app-owned `CLV_CellCtlInputState`.
- Wrap: draw/hit on subline 0 only (matches icon policy).
- Redraw: application GadTools refresh; safe unit = physical row or list.
- Keyboard: deferred to Phase 7.
- Profile: new `draw-cellctl-checkbox` = draw-basic + selection + cellctl +
  `clv_bind_cellctl`; pixel wrap not default; existing binders unchanged.
- Ops table: draw pointer only; hit-test remains public helpers.
- Prepared control storage: feature-local parallel slots, not universal
  `CLV_RenderCell` fields.
- Definitive sketches recorded in master plan §5, §7–§9, §18.

### Build commands run

None required (documentation/design only).

### Tests and runtime checks performed

None (no code). Exit criteria are design completeness for Phase 2.

### Measured sizes or performance data

None. Baseline for later phases remains `size-draw-basic` per
`docs/CLV_SIZE_REPORT.md`.

### Defects found

None in product code.

### Known limitations

- Headers/modules not yet in the tree (Phase 2).
- Mouse coordinate conversion vs gadget borders pinned provisionally;
  Phase 3 must confirm against AutoDocs/examples.
- Minimum legible checkbox size and mark default subject to Phase 4 visuals.
- No keyboard activation until Phase 7 dependency exists.

### Next recommended action

Begin **Phase 2 — Optional Build/Profile Skeleton**:

1. Add `CLV_ENABLE_CELLCTL` / `CLV_ENABLE_CELLCTL_CHECKBOX` and matching
   `CLV_HAS_*` via profile generation / `clv_config.h`.
2. Add stub `clv_cellctl_*.c/h`, `clv_bind_cellctl.c`, Makefile objects and
   dependency validation (`CELLCTL_CHECKBOX → CELLCTL → RENDERER`).
3. Add profile target; prove existing profiles link no cellctl symbols.
4. Do not implement real draw/hit/toggle behaviour yet.

---

## 2026-07-30 — Phase 2: Optional Build/Profile Skeleton

**Phase:** 2 — Optional Build/Profile Skeleton  
**Status:** Complete  
**Agent/session objective:** Add ENABLE/HAS flags, stub modules, binder,
profile targets, and prove baseline omission. No real draw/hit/toggle.

### Source files inspected

- Root `Makefile` (profiles, binders, validation)
- `tools/generate_profile_config.ps1`, `tools/validate_profile_config.ps1`
- `src/custom_listview/clv_config.h`, `clv_renderer_internal.h`,
  `clv_renderer_ops.c`, existing `clv_bind_*.c`
- `docs/CLV_BUILD_PROFILES.md`, `docs/CLV_SIZE_REPORT.md`
- Master plan §9 / §18 / Phase 2 exit criteria

### Files modified, added, renamed, or removed

| Action | Path |
|--------|------|
| Created | `src/custom_listview/clv_cellctl.h` |
| Created | `src/custom_listview/clv_cellctl_internal.h` |
| Created | `src/custom_listview/clv_cellctl_core.c` |
| Created | `src/custom_listview/clv_cellctl_checkbox.c` |
| Created | `src/custom_listview/clv_bind_cellctl.c` |
| Created | `examples/size_compare/size_draw_cellctl_checkbox.c` |
| Created | `tests/headers/hdr_cellctl.c` |
| Created | `tools/validate_cellctl_omission.ps1` |
| Updated | `Makefile` |
| Updated | `tools/generate_profile_config.ps1` |
| Updated | `tools/validate_profile_config.ps1` |
| Updated | `tools/size_report.ps1` |
| Updated | `src/custom_listview/clv_config.h` |
| Updated | `src/custom_listview/custom_listview.h` |
| Updated | `src/custom_listview/PUBLIC_HEADERS.txt` |
| Updated | `src/custom_listview/clv_renderer_internal.h` |
| Updated | `src/custom_listview/clv_renderer_ops.c` |
| Updated | `docs/CLV_BUILD_PROFILES.md` |
| Updated | `docs/CLV_MODULE_ARCHITECTURE.md` |
| Updated | `docs/CLV_SIZE_REPORT.md` (regenerated) |
| Updated | `docs/CLV_INTERACTIVE_CONTROL_CELLS_MASTER_PLAN.md` |
| Updated | `docs/CLV_CONTROL_CELLS_DEVELOPER_LOG.md` (this entry) |

### API and architecture decisions

- Profile `draw-cellctl-checkbox` = draw-basic core + selection + cellctl
  objects + `clv_bind_cellctl.o` (replaces `clv_bind_none`).
- `full-smoke` deliberately still omits cellctl.
- Ops table gained `draw_cellctl` + NULL-safe wrapper; structured draw does
  not call it yet.
- Link-omission proof uses `CLV_CELLCTL_MODULE_LINKED` tag ( `-final` strips
  most symbol names), object-file checks, HAS macros, and size ordering.
- Stub prepare returns NULL; mouse/get/set return FALSE.

### Build commands run

- `make validate-custom`, `validate-profile-config`, `validate-layout`,
  `validate-headers`
- Standard profiles including `draw-cellctl-checkbox` and remaining drawn
  profiles
- `make validate-cellctl-omission`
- Size harness + `tools/size_report.ps1`
- Illegal ENABLE combinations rejected

### Tests and runtime checks performed

- Compiled/linked only (host cannot run Amiga binaries here).
- Header compile matrix includes `hdr_cellctl.c`.
- No Amiga/emulator visual runtime test (stubs have no visible checkbox).

### Measured sizes or performance data

| Profile | Bytes | Notes |
|---------|------:|-------|
| size-draw-basic | 27940 | |
| size-draw-selected | 28940 | +1000 vs basic |
| size-draw-cellctl-checkbox | 29404 | **+464 vs selected** (stubs) |

### Defects found

- Initial binary string scan for `clv_cellctl_` failed under `-final` strip;
  corrected with explicit link tag + size/object checks.

### Known limitations

- No geometry, hit-test, prepare slots, draw call site, or toggle commit.
- Reuses `examples/05_draw_basic/main.c` for the profile shell (no checkbox
  demo yet).

### Next recommended action

Begin **Phase 3 — Shared Control Core and Geometry**: resolve control cells,
rectangles, clipping, mapping, private draw-dispatch call site, common
hit-test contract, and neutral events — still without finished checkbox art
or verified toggle.

---

## 2026-07-30 — Phase 3: Shared Control Core and Geometry

**Phase:** 3 — Shared Control Core and Geometry  
**Status:** Complete  
**Agent/session objective:** Geometry, prepare slots, draw-dispatch wiring,
hit-test contract, neutral events — no checkbox artwork or toggle commit.

### Source files inspected

- Master plan Phase 3 / §5–§8 / §18
- `clv_renderer_core.c` structured draw + prepared free
- `clv_renderer_ops.c` / `clv_renderer_internal.h` ops table
- `clv_renderer_setup.c` content viewport helpers
- `examples/02_ascii_tracked/main.c` (mouse coordinate precedent)
- `docs/AutoDocs/gadtools.doc` (`GTLV_Top` / `GTLV_ItemHeight`)

### Files modified, added, renamed, or removed

| Action | Path |
|--------|------|
| Created | `src/custom_listview/clv_cellctl_geom.c` |
| Created | `src/custom_listview/clv_cellctl_geom.h` |
| Created | `tests/host/cellctl_geom_tests.c` |
| Created | `tests/host/cellctl_geom_main.c` |
| Updated | `src/custom_listview/clv_cellctl_core.c` |
| Updated | `src/custom_listview/clv_cellctl_checkbox.c` |
| Updated | `src/custom_listview/clv_cellctl.h` |
| Updated | `src/custom_listview/clv_cellctl_internal.h` |
| Updated | `src/custom_listview/clv_prepared_internal.h` |
| Updated | `src/custom_listview/clv_renderer_internal.h` |
| Updated | `src/custom_listview/clv_renderer_core.c` |
| Updated | `src/custom_listview/clv_renderer_ops.c` |
| Updated | `Makefile` |
| Updated | `tools/validate_cellctl_omission.ps1` |
| Updated | `tools/size_report.ps1` |
| Updated | `tests/host/Makefile`, `tests/host/README.md` |
| Updated | `docs/CLV_BUILD_PROFILES.md` |
| Updated | `docs/CLV_MODULE_ARCHITECTURE.md` |
| Updated | `docs/CLV_SIZE_REPORT.md` |
| Updated | `docs/CLV_INTERACTIVE_CONTROL_CELLS_MASTER_PLAN.md` |
| Updated | `docs/CLV_CONTROL_CELLS_DEVELOPER_LOG.md` (this entry) |

### API and architecture decisions

- Geometry resolved at draw/hit (same helper); not prepare-time absolute cache.
- Mouse: window-relative MouseX/Y; content = gadget edge + 2px frame inset.
- Ops `draw_cellctl` is row-level; checkbox paint registered separately.
- Slots on prepared blob + `rn->cellctl_row`; freed opaquely in prepared_free.
- handle_mouse: arm/cancel only; commit Phase 5.

### Build commands run

- Host: `make cellctl-geom-test` (23/23)
- Amiga: `draw-cellctl-checkbox`, `draw-basic`, `draw-selected`, size twins,
  `validate-cellctl-omission`, `validate-headers`, size report

### Tests and runtime checks performed

- Host geometry/alignment/clip/size/point-in-box.
- Compiled/linked Amiga profiles only (no emulator visual run).

### Measured sizes or performance data

| Profile | Bytes | Notes |
|---------|------:|-------|
| size-draw-basic | 28004 | +64 vs Phase 2 (struct fields) |
| size-draw-selected | 29004 | |
| size-draw-cellctl-checkbox | 33972 | **+4968 vs selected** |

### Defects found

None blocking. Prior omission check briefly saw stale size binaries until
size twins were rebuilt.

### Known limitations

- Checkbox paint still no-op.
- No Amiga visual confirmation of hit/draw rectangles yet.
- Profile example still reuses draw-basic shell.

### Next recommended action

Begin **Phase 4 — Checkbox Rendering**: plain/3D frame, mark styles,
selected/disabled variants inside the resolved clipped control box.

---

## 2026-07-30 — Phase 4: Checkbox Rendering

**Phase:** 4 — Checkbox Rendering  
**Status:** Complete  
**Agent/session objective:** Compact checkbox artwork (plain/3D, marks,
selected/disabled) inside shared geometry; no toggle commit.

### Source files inspected

- Master plan Phase 4 / §5.4 / §6
- `clv_cellctl_checkbox.c` (prior no-op), `clv_cellctl_core.c` draw_row
- `clv_icons.c` / `clv_renderer_core.c` (ghost pattern, pens)
- `examples/05_draw_basic/main.c`, `examples/06_draw_wrapped/main.c`

### Files modified, added, renamed, or removed

| Action | Path |
|--------|------|
| Updated | `src/custom_listview/clv_cellctl_checkbox.c` |
| Updated | `src/custom_listview/clv_cellctl_core.c` (comment) |
| Updated | `src/custom_listview/clv_cellctl.h` (phase comment) |
| Created | `examples/05_draw_cellctl_checkbox/main.c` |
| Created | `examples/05_draw_cellctl_checkbox/README.md` |
| Updated | `examples/size_compare/size_draw_cellctl_checkbox.c` |
| Updated | `Makefile` (example source path) |
| Updated | `docs/CLV_INTERACTIVE_CONTROL_CELLS_MASTER_PLAN.md` |
| Updated | `docs/CLV_BUILD_PROFILES.md` |
| Updated | `docs/CLV_SIZE_REPORT.md` |
| Updated | `docs/CLV_MODULE_ARCHITECTURE.md` |
| Updated | `docs/CLV_CONTROL_CELLS_DEVELOPER_LOG.md` (this entry) |

### API and architecture decisions

- Default appearance unchanged: 9×9 plain + TICK, pad 1.
- TICK auto-falls back to FILL when painted box w or h < 7.
- Selected: use renderer apen; never invert 3D bevel.
- Disabled: post-paint dither ghost (shadow normal / apen selected).
- Clamp [5,15] remains provisional pending hardware visual check.

### Build commands run

- Host: `make cellctl-geom-test` (23/23)
- Amiga: `draw-cellctl-checkbox`, size twin, `validate-cellctl-omission`,
  `validate-headers`

### Tests and runtime checks performed

- Compiled/linked Amiga profiles (no emulator visual run).
- Example README lists visual-matrix checklist for Amiga/RTG.

### Measured sizes or performance data

| Profile | Bytes | Notes |
|---------|------:|-------|
| size-draw-basic | 28004 | unchanged |
| size-draw-selected | 29004 | unchanged |
| size-draw-cellctl-checkbox | 35180 | **+6176 vs selected** (+1208 vs Phase 3) |

### Defects found

None blocking. Initial `(void)selected` tripped VBCC warning 153; fixed by
using `selected` for disabled ghost pen choice.

### Known limitations

- No mouse toggle (Phase 5).
- No Amiga/RTG visual confirmation yet.
- Hit/Y mapping still needs live verification with Phase 5 input.

### Next recommended action

Begin **Phase 5 — Mouse Input and State Transition**: verified down/up,
snapshot mutate, `CLV_CellCtlEvent`, GadTools refresh; outside-control clicks
keep normal row selection.

---

## 2026-07-30 — Phase 4 follow-up: Topaz-8 shrink-to-fit

**Phase:** 4 (corrective follow-up after emulator visual)  
**Status:** Complete  
**Agent/session objective:** Fix empty "On" column on classic Topaz/8 rows;
add scroll rows to size twin.

### Evidence

Emulator screenshot of `size-draw-cellctl-checkbox` showed an empty "On"
column. Root cause: preferred 9×9 + pad 1 needs 11 px vertical; GadTools
ListView item height on Topaz/8 is ~8 px, so geometry returned FALSE and
skipped paint.

### Fix

- `clv_cellctl_resolve_control_box` now shrinks pad then box down to MIN
  before failing (same helper for draw and hit).
- Host test `topaz8_shrink` locks 9×9→fit in height 8.
- Size twin: 20 data rows for scrolling; divider reserve applied.

### Build / tests

- Host `cellctl-geom-test`: 25/25
- Rebuild `size-draw-cellctl-checkbox` / `draw-cellctl-checkbox`

### Next recommended action

Re-test `bin/size-draw-cellctl-checkbox` on the emulator (boxes in "On",
scrollbar usable). Prefer `bin/draw-cellctl-checkbox` for the visual-matrix
variants. Then Phase 5.

---

## 2026-07-30 — Phase 4 visual confirmation (emulator)

**Phase:** 4  
**Status:** Complete — **working on emulator**  
**Agent/session objective:** Record user/emulator confirmation that checkbox
paint and scrolling are good after Topaz-8 shrink + 1px inter-row gap.

### Evidence

- Binary: `bin/size-draw-cellctl-checkbox`
- Emulator screenshots: On-column checkboxes visible (checked / unchecked /
  disabled mix); selected row pens readable; ListView scrolling works with
  the expanded row set; boxes no longer fill the full row height (1px gap).

### Documents updated

- `docs/CLV_INTERACTIVE_CONTROL_CELLS_MASTER_PLAN.md` — Phase 4 validation
  evidence, status summary, §14 / §16 / §17.10
- `examples/05_draw_cellctl_checkbox/README.md` — checklist marked from
  size-twin pass
- This developer-log entry

### Next recommended action

Begin **Phase 5 — Mouse Input and State Transition**.

---

## 2026-07-30 — Phase 5: Mouse Input and State Transition

**Phase:** 5 — Mouse Input and State Transition  
**Status:** Complete (host state machine verified; Amiga toggle linked)  
**Agent/session objective:** Verified down/up toggle, snapshot mutation,
`CLV_CellCtlEvent`, example refresh; outside-control clicks keep selection.

### Source files inspected

- Master plan Phase 5 / §7.1–7.4 / Phase 1 event table
- `clv_cellctl_core.c` (prior arm/cancel-only handle_mouse)
- `clv_cellctl_geom.c` / `.h`
- `examples/05_draw_cellctl_checkbox/main.c` (Phase 4 paint demo)

### Files modified, added, renamed, or removed

| Action | Path |
|--------|------|
| Updated | `src/custom_listview/clv_cellctl_core.c` |
| Updated | `src/custom_listview/clv_cellctl_geom.c` |
| Updated | `src/custom_listview/clv_cellctl_geom.h` |
| Updated | `src/custom_listview/clv_cellctl.h` |
| Updated | `examples/05_draw_cellctl_checkbox/main.c` |
| Updated | `examples/05_draw_cellctl_checkbox/README.md` |
| Updated | `examples/size_compare/size_draw_cellctl_checkbox.c` |
| Updated | `tests/host/cellctl_geom_tests.c` |
| Updated | `tests/host/README.md` |
| Updated | `docs/CLV_INTERACTIVE_CONTROL_CELLS_MASTER_PLAN.md` |
| Updated | `docs/CLV_SIZE_REPORT.md` |
| Updated | `docs/CLV_BUILD_PROFILES.md` |
| Updated | `docs/CLV_MODULE_ARCHITECTURE.md` |
| Updated | `docs/CLV_CONTROL_CELLS_DEVELOPER_LOG.md` (this entry) |

### API and architecture decisions

- Verified button-up commit mutates prepared snapshot and fills event;
  library never refreshes GadTools (app owns refresh).
- Pure `clv_cellctl_input_transition` owns arm/cancel/commit identity rules;
  host-tested without Gadget stubs.
- Example refresh: re-assert `GTLV_Top` + `GT_RefreshWindow` (list structure
  unchanged; detach/reattach not required for slot-only mutation).
- App store: mutable `CLV_CellCtlDesc` rows updated from `CLV_CellCtlEvent`.

### Build commands run

- Host: `make cellctl-geom-test` (51/51)
- Amiga: `draw-cellctl-checkbox`, size twin, `draw-basic`, `draw-selected`,
  `validate-cellctl-omission`, `validate-headers`

### Tests and runtime checks performed

- Host: toggle value, arm flags, miss/disabled/display-only, release-outside
  cancel, different-column cancel, rapid five commits.
- Amiga: compiled/linked only — emulator live-toggle checklist remains in
  example README (pending user run).

### Measured sizes or performance data

| Profile | Bytes | Notes |
|---------|------:|-------|
| size-draw-basic | 28004 | unchanged |
| size-draw-selected | 29004 | unchanged |
| size-draw-cellctl-checkbox | 36656 | **+7652 vs selected** (+1476 vs Phase 4) |

### Defects found

None blocking.

### Known limitations

- Emulator live hit/toggle not yet confirmed this phase.
- Wrapped/scroll/relayout integration deferred to Phase 6.
- No public refresh helper (Phase 1 optional).

### Next recommended action

Begin **Phase 6 — Selection, Wrapping, Scrolling, and Relayout Integration**.
Prefer a quick emulator pass of `bin/draw-cellctl-checkbox` interaction
checklist first if convenient.

---

## 2026-07-30 — SCOPE CORRECTION: Phases 0–5 mis-scoped

**Phase:** Corrective (stops legacy Phase 6; opens corrected plan C0)  
**Status:** Complete (documentation only — **no implementation**)  
**Agent/session objective:** Declare Phases 0–5 targeted the wrong ListView;
isolate legacy GadTools `clv_cellctl_*` work; audit
`src/custom_listview_control/`; publish replacement plan; freeze coding
pending review.

### Supersedes

- Developer-log “Next recommended action” entries that say begin Phase 6 of
  the GadTools cellctl roadmap (including the Phase 5 entry immediately
  above).
- Master-plan “Authoritative target = `src/custom_listview/` only” rule
  (revoked; see DC-004).
- Any instruction to continue GadTools `LISTVIEW_KIND` / `LV_DRAW` /
  `GTLV_*` checkbox integration as the product path.

### Why

The intended ListView is the fully custom control under
`src/custom_listview_control/`. It manually owns row rendering, columns,
selection, scrolling, variable-height rows, hit testing, and redraw. The
standard scrollbar gadget is the only significant OS control involved.

Work under `src/custom_listview/` (GadTools `LISTVIEW_KIND`, `LV_DRAW`,
`GTLV_Top`, `GT_RefreshWindow`, `clv_cellctl_*`) is **not** that target.

### Files modified, added, renamed, or removed

| Action | Path |
|--------|------|
| Updated | `docs/CLV_INTERACTIVE_CONTROL_CELLS_MASTER_PLAN.md` — prominent SCOPE CORRECTION; §A–§F corrected purpose, control-package audit, replacement plan C0–C10, DC-004; legacy archive banner; Phases 6–10 cancelled; §12/§16 redirected |
| Updated | `docs/CLV_CONTROL_CELLS_DEVELOPER_LOG.md` (this entry) |

**No** `.c` / `.h` implementation sources were added, deleted, or changed.
Legacy `src/custom_listview/clv_cellctl_*`, profiles, and
`examples/05_draw_cellctl_checkbox/` remain in-tree, frozen.

### Architecture audit summary (`src/custom_listview_control/`)

| Area | Finding |
|------|---------|
| Row model | Logical-row authoritative; variable height; wrap caches; header not a row |
| Column model | `CLV_ControlColumn` (+ unused `flags`); text-only cells today |
| Selection | Control-owned single `selected_row`; gap = miss |
| Viewport | `bounds` / `header_bounds` / `viewport_bounds`; window-relative hit math |
| Smart-scroll | Control-owned `render_scrolled` + `ScrollRaster*`; not GadTools ListView |
| Input | Neutral `CLV_InputEvent`; `SELECT_UP` defined but no-op today |
| Redraw | `render` / `render_logical_rows` / `render_scrolled` |
| Events | Synchronous `CLV_Event`; no toggle/column field yet |
| Checkbox | **None** in this package |

### Replacement plan

See master plan §D (phases C1–C10). Gate: **no coding** until §B/§D reviewed.
Reuse only architecture-neutral concepts (artwork metrics, verified down/up,
optional size omission, app-owned Boolean). **No mechanical port** of
`clv_cellctl_*`.

### Build commands run

None required (documentation-only correction).

### Tests and runtime checks performed

None (no code changes).

### Measured sizes or performance data

None collected this entry. Legacy GadTools cellctl sizes remain historical.

### Defects found

Process defect: Phases 0–5 product target was inverted relative to the
intended custom control package. No new product bug filed against
`custom_listview_control/` (feature absent by design until corrected plan).

### Known limitations

- Corrected plan awaits human review before C1.
- Legacy GadTools demos may still build; they are non-authoritative.
- Dual trees (`clv_cellctl_*` vs future `CLV_CTRL_*` extensions) require clear
  docs so agents do not merge them.

### Next recommended action

1. **Review** master plan §A–§F (especially §B audit and §D plan).
2. After approval, authorise **Phase C1 — Design lock** only (still
   documentation).
3. Do **not** start legacy Phase 6. Do **not** implement checkbox code in
   either package until C1 is accepted and C2 is authorised.

---

## 2026-07-30 — Phase C1: Design lock (custom_listview_control)

**Phase:** C1 — Design lock for control architecture  
**Status:** Complete (documentation only — **no implementation**)  
**Agent/session objective:** Lock reviewer C1 recommendations into §D;
harden legacy-archive safeguards so keyword search cannot revive GadTools
authority.

### Reviewer decisions locked

1. **Events carry `row_user_data`** plus `row`, `column`, `previous_value`,
   `cell_value` (C field `cell_value` avoids clashing with existing
   `LONG value` scroll_y on `CLV_Event`).
2. **Two-step events:** SELECT_DOWN may emit `SELECTION_CHANGED`; SELECT_UP
   may emit `CELL_TOGGLED`. No compound event. Already-selected row → no
   selection event; release outside / other cell / lost capture → cancel arm,
   no toggle.
3. **Snapshot ownership:** control copies values; does not write borrowed app
   memory by default; public `clv_control_set_checkbox_value`.
4. **Parallel `CLV_ControlCell`** via optional `CLV_ControlRow.control_cells`
   (`NULL` = none). Keep `UWORD flags` ABI.
5. **Column type mask** in low nibble of `CLV_ControlColumn.flags`.
6. **First content line band** vertical placement (not mid-row wrap centre).
7. **Keyboard:** Space toggles sole checkbox column; `NAV_ACTIVATE` unchanged;
   defer if multiple checkbox columns.
8. **Private arm state** inside `CLV_Control` (no public input-state object).
9. **Redraw:** existing `clv_control_render_logical_rows` only; no cell
   invalidate API yet.

### Documentation safeguards

- Legacy archive banner: “Nothing below this point may be used as design
  authority for `src/custom_listview_control/`…”
- Phase / §12–§14 / §17 headings prefixed **LEGACY — DO NOT EXECUTE**
- D-015, D-018, D-025 marked **Superseded by DC-004 / DC-005**
- §17.1 “sole implementation target” language revoked in-place
- DC-005 records the C1 package

### Files modified

| Action | Path |
|--------|------|
| Updated | `docs/CLV_INTERACTIVE_CONTROL_CELLS_MASTER_PLAN.md` |
| Updated | `docs/CLV_CONTROL_CELLS_DEVELOPER_LOG.md` (this entry) |

No `.c` / `.h` changes.

### Next recommended action

Authorise **Phase C2 — Skeleton** when ready (types/flags/stubs only; still
no working checkbox UI claim).

---

## 2026-07-30 — Doc-hardening: C2–C10 phase cards + agent handoff

**Phase:** Documentation (between C1 and C2)  
**Status:** Complete — **no implementation**  
**Agent/session objective:** Make corrected phases safe for one-phase-per-agent
handoff without loading the LEGACY ARCHIVE or inventing scope.

### What changed

- Expanded **C2–C10** into full phase cards: reading set, objective, required
  work, out of scope, validation, deliverables, exit criteria, completion
  record stub, next-phase conditions.
- Locked ABI sketches renumbered to **§D.11** (was §D.9) so C9 can own §D.9.
- Added live **§G Agent session protocol** + suggested user prompt template.
- Status / §F / Gate: **C2 authorised**.
- §B.11 updated to match C1 locks.
- LEGACY §11 marked superseded by §G.

### Files modified

| Action | Path |
|--------|------|
| Updated | `docs/CLV_INTERACTIVE_CONTROL_CELLS_MASTER_PLAN.md` |
| Updated | `docs/CLV_CONTROL_CELLS_DEVELOPER_LOG.md` (this entry) |

### Next recommended action

Run a **C2-only** agent with the §G prompt template and §D.2 reading set.

---

## 2026-07-30 — Phase C2: Skeleton (ABI + snapshot/setter)

**Phase:** C2 — Optional build / data-model skeleton  
**Status:** Complete  
**Agent/session objective:** Add C1 public/private ABI and internal checkbox
snapshot so later phases have types and storage. No paint, hit-test,
arm/commit, or Space toggle.

### What changed

- Public header: column type mask; `CLV_ControlCell` + cell flag/value
  constants; `control_cells` on `CLV_ControlRow`; `CLV_EVENT_CELL_TOGGLED`;
  extended `CLV_Event`; `clv_control_set_checkbox_value`.
- Internal: owned row-major `cell_snapshot` copied from borrowed
  `control_cells` on `set_rows` / `set_columns`; freed on replace/destroy;
  private arm fields present but unused for behaviour.
- Setter updates snapshot only (no paint); returns FALSE on bad
  row/column/type/value or missing snapshot.
- Input: `clv_ctrl_clear_event` zeros new event fields; existing
  selection/scroll/NAV behaviour unchanged; `SELECT_UP` still no-op.
- Demo: explicit `control_cells = NULL`; still text-only UI.

### Files modified

| Action | Path |
|--------|------|
| Updated | `src/custom_listview_control/clv_control.h` |
| Updated | `src/custom_listview_control/clv_control_internal.h` |
| Updated | `src/custom_listview_control/clv_control.c` |
| Updated | `src/custom_listview_control/clv_control_input.c` |
| Updated | `examples/custom_control_demo/main.c` |
| Updated | `examples/custom_control_demo/README.md` (C2 size baseline) |
| Updated | `docs/CLV_INTERACTIVE_CONTROL_CELLS_MASTER_PLAN.md` (§D.0 / §D.2 / §F) |
| Updated | `docs/CLV_CONTROL_CELLS_DEVELOPER_LOG.md` (this entry) |

**Not touched:** `src/custom_listview/clv_cellctl_*` (legacy frozen).

### Build commands run

```text
make custom-control-demo
make custom-control-demo CLV_ENABLE_SMART_SCROLL=0
make custom-control-demo-log
make custom-control-demo-bench
```

All linked cleanly (VBCC `+aos68k`). Parallel VBCC compiles can collide on
shared temp `.asm`; sizes recorded from sequential rebuilds.

### Tests and runtime checks performed

- Cross-link only (host cannot run Amiga binaries in this session).
- Manual Amiga/emulator select/scroll check not run this session; behaviour
  should be unchanged (no checkbox paint/interaction claimed).

### Measured sizes or performance data

VBCC `+aos68k -O2 -size -final`, `-cpu=68000`:

| Target | Bytes | vs 2026-07-28 |
|--------|------:|--------------:|
| `bin/custom-control-demo` (smart on) | 37772 | +736 |
| `bin/custom-control-demo` (smart off) | 36528 | — |
| `bin/custom-control-demo-log` | 52348 | +792 |
| `bin/custom-control-demo-bench` | 54340 | +736 |

### Defects found

None blocking. Note: concurrent `make` of different control profiles sharing
the same VBCC temp asm name can fail with redefined labels — rebuild
sequentially.

### Known limitations

- No checkbox paint or geometry (C3).
- No arm/commit / `CELL_TOGGLED` emission (C4–C5).
- No Space toggle (C7).
- Optional Makefile omit flag deferred to C9.
- Snapshot always allocated when `row_count > 0` and `column_count > 0`
  (NULL `control_cells` rows store zeros).

### Next recommended action

Run a **C3-only** agent with the §G prompt template and §D.3 reading set
(paint from snapshot; no mouse commit yet).

---

## 2026-07-30 — Phase C3: Checkbox paint (geometry + draw path)

**Phase:** C3 — Checkbox geometry + paint in control draw path  
**Status:** Complete  
**Agent/session objective:** Draw checked/unchecked (and disabled/selected
variants) from the control snapshot inside the first content line band.
No input commit.

### What changed

- New `clv_control_checkbox.c`: `clv_ctrl_checkbox_resolve_rect` centres a
  compact plain box in the first text-line band (top `cell_padding_y` +
  `line_height`); horizontal alignment follows the column; pad-then-box
  shrink (~9 default, min 5).
- `clv_ctrl_checkbox_paint` reads owned `cell_snapshot` only; plain outline
  + tick; selected pens; disabled sparse tick / separator pen.
- `clv_ctrl_paint_row_content` paints checkbox-typed columns via the helper
  and skips text fragments for those columns.
- Demo: last column is `On` (`CLV_CTRL_COL_TYPE_CHECKBOX`) with per-row
  `control_cells` covering checked/unchecked, display-only, disabled, and
  invisible (heading). Paint-only — no toggle on click.
- Makefile links `clv_control_checkbox.o` into normal / log / bench control
  builds.

### Files modified

| Action | Path |
|--------|------|
| Added | `src/custom_listview_control/clv_control_checkbox.c` |
| Updated | `src/custom_listview_control/clv_control_internal.h` |
| Updated | `src/custom_listview_control/clv_control_render.c` |
| Updated | `src/custom_listview_control/clv_control.h` (header comment) |
| Updated | `Makefile` (control object lists) |
| Updated | `examples/custom_control_demo/main.c` |
| Updated | `examples/custom_control_demo/README.md` |
| Updated | `docs/CLV_INTERACTIVE_CONTROL_CELLS_MASTER_PLAN.md` (§D.0 / §D.3 / §F) |
| Updated | `docs/CLV_CONTROL_CELLS_DEVELOPER_LOG.md` (this entry) |

**Not touched:** `src/custom_listview/clv_cellctl_*` (legacy frozen); input
arm/commit paths.

### Build commands run

```text
make custom-control-demo
make custom-control-demo CLV_ENABLE_SMART_SCROLL=0
make custom-control-demo-log
make custom-control-demo-bench
```

All linked cleanly (VBCC `+aos68k`). Sequential rebuilds used for size
recording.

### Tests and runtime checks performed

- Cross-link (VBCC `+aos68k`) succeeded for smart / nosmart / log / bench.
- **Amiga/emulator visual (2026-07-30):** last-column `On` checkboxes
  display correctly; checked/unchecked state persists correctly when
  scrolling up and down.

### Measured sizes or performance data

VBCC `+aos68k -O2 -size -final`, `-cpu=68000`:

| Target | Bytes | vs C2 |
|--------|------:|------:|
| `bin/custom-control-demo` (smart on) | 40460 | +2688 |
| `bin/custom-control-demo` (smart off) | 39216 | +2688 |
| `bin/custom-control-demo-log` | 55028 | +2680 |
| `bin/custom-control-demo-bench` | 57116 | +2776 |

### Defects found

None blocking.

### Known limitations

- No hit-test / arm / `CELL_TOGGLED` (C4–C5).
- No Space toggle (C7).
- Demo interaction matrix / IDCMP SELECT_UP wiring deferred (C8).
- Formal smart-scroll / resize / wrap regression checklist deferred (C6);
  basic scroll persistence of checkbox pixels was confirmed visually.

### Next recommended action

Run a **C4-only** agent with the §G prompt template and §D.4 reading set
(hit-test + verified SELECT_DOWN/UP commit; reuse resolve geometry).

---

## 2026-07-30 — Phase C4: Mouse commit (arm / cancel / toggle event)

**Phase:** C4 — Cell hit-test + verified SELECT_DOWN/UP commit  
**Status:** Complete  
**Agent/session objective:** Arm on SELECT_DOWN inside an interactive
checkbox box; commit on SELECT_UP with the same identity; emit
`CLV_EVENT_CELL_TOGGLED` with full fields; mutate owned snapshot only.

### What changed

- `clv_control_input.c`: `clv_ctrl_hit_interactive_checkbox` reuses
  `clv_ctrl_checkbox_resolve_rect`; requires VISIBLE|ENABLED|INTERACTIVE.
- SELECT_DOWN arms privately on box hit; outside-box clears arm and keeps
  the existing row-selection path.
- SELECT_UP commits when release stays on the same armed checkbox
  (toggle snapshot → `CELL_TOGGLED` with `row` / `column` /
  `row_user_data` / `previous_value` / `cell_value`); otherwise cancels.
- Arm cleared on set_rows / set_columns (snapshot refresh), set_bounds,
  and destroy.
- No paint inside `handle_input`.
- Demo: IDCMP LMB-up → `SELECT_UP` to active control (even outside box);
  on `CELL_TOGGLED`, print + `render_logical_rows` for that row.
- Docs: §B.8 / §B.10 / §B.11 / §D.0 / §D.4 record / §F; demo README sizes.

### Files modified

| Action | Path |
|--------|------|
| Updated | `src/custom_listview_control/clv_control_input.c` |
| Updated | `src/custom_listview_control/clv_control.c` (arm cancel on destroy / set_bounds) |
| Updated | `src/custom_listview_control/clv_control.h` |
| Updated | `src/custom_listview_control/clv_control_internal.h` |
| Updated | `src/custom_listview_control/clv_control_checkbox.c` (comment) |
| Updated | `examples/custom_control_demo/main.c` |
| Updated | `examples/custom_control_demo/README.md` |
| Updated | `docs/CLV_INTERACTIVE_CONTROL_CELLS_MASTER_PLAN.md` (§D.0 / §D.4 / §F / §B) |
| Updated | `docs/CLV_CONTROL_CELLS_DEVELOPER_LOG.md` (this entry) |

**Not touched:** `src/custom_listview/clv_cellctl_*` (legacy frozen); C5
selection matrix; Space (C7); polished demo matrix (C8).

### Build commands run

```text
make custom-control-demo
# wipe build/custom_listview_control/*.o + example obj, then:
make custom-control-demo CLV_ENABLE_SMART_SCROLL=0
# wipe again, then:
make custom-control-demo
make custom-control-demo-log
make custom-control-demo-bench
```

All linked cleanly (VBCC `+aos68k`).

### Tests and runtime checks performed

- Cross-link (VBCC `+aos68k`) succeeded for smart / nosmart / log / bench.
- No host unit harness for control arm/cancel in this phase (optional).
- **Amiga/emulator runtime (2026-07-30):** confirmed working — interactive
  `On` checkboxes clickable; console shows `Selected logical row …` and
  `Toggled row N col 4: 0 -> 1` / `1 -> 0` across multiple rows (including
  re-toggle of the same cell). Screenshot evidence recorded by user.

### Measured sizes or performance data

VBCC `+aos68k -O2 -size -final`, `-cpu=68000`:

| Target | Bytes | vs C3 |
|--------|------:|------:|
| `bin/custom-control-demo` (smart on) | 41768 | +1308 |
| `bin/custom-control-demo` (smart off) | 41100 | +1884 |
| `bin/custom-control-demo-log` | 56644 | +1616 |
| `bin/custom-control-demo-bench` | 58524 | +1408 |

### Defects found

None blocking.

### Known limitations

- Selection-vs-toggle event matrix not fully documented/tightened (C5).
- No Space toggle (C7).
- Demo does not sync app-owned `control_cells` on toggle (snapshot only;
  C8 authoritative-store pattern).

### Next recommended action

Run a **C5-only** agent with the §G prompt template and §D.5 reading set
(selection vs checkbox interaction; verified-click table).

---

## 2026-07-30 — Phase C4 runtime confirmation (Amiga/emulator)

**Phase:** C4 follow-up  
**Status:** Complete — **working on Amiga/emulator**  
**Agent/session objective:** Record user screenshot evidence that checkbox
cells are clickable and emit `CELL_TOGGLED`.

### What changed

Documentation only — no code changes.

- Master plan §D.4 runtime note + §F marked runtime-verified.
- Developer log C4 entry updated; this confirmation entry appended.
- Demo README C4 size section notes visual/runtime pass.

### Evidence

`custom-control-demo` Output window shows sequences such as:

- `Selected logical row N` then `Toggled row N col 4: 0 -> 1`
- Same-cell re-toggle `1 -> 0` / `0 -> 1`
- Checkbox column paints checked/unchecked after toggle on wrapped rows

### Next recommended action

Authorise / run **Phase C5** when ready.

---

## 2026-07-30 — Phase C5: Selection vs checkbox interaction

**Phase:** C5 — Selection vs checkbox interaction + events  
**Status:** Complete  
**Agent/session objective:** Lock the two-event sequence so selection and
toggle never merge into a compound event or silently drop a real
selection change (§D.11 verified-click table).

### What changed

- `clv_control_input.c` SELECT_DOWN policy tightened:
  - interactive checkbox + other selectable row → arm +
    `SELECTION_CHANGED` when selection actually changes;
  - interactive checkbox + already-selected row → arm only (no
    selection/scroll event from that path);
  - interactive checkbox + nonselectable → arm only;
  - outside box → clear arm; existing row-selection path.
- SELECT_UP remains `CELL_TOGGLED` only on commit; cancel emits none.
- At most one `CLV_EventType` per `handle_input` (header + §B.10).
- Documented app pattern: on `CELL_TOGGLED`, update authoritative store,
  then `clv_control_render_logical_rows(c, row, -1)` (demo still paints
  from snapshot; full store sync is C8).
- §B.5 / §B.10 / §D.0 / §D.5 record / §F / demo README updated.

### Files modified

| Action | Path |
|--------|------|
| Updated | `src/custom_listview_control/clv_control_input.c` |
| Updated | `src/custom_listview_control/clv_control.h` |
| Updated | `examples/custom_control_demo/main.c` |
| Updated | `examples/custom_control_demo/README.md` |
| Updated | `docs/CLV_INTERACTIVE_CONTROL_CELLS_MASTER_PLAN.md` (§B.5 / §B.10 / §D.0 / §D.5 / §F) |
| Updated | `docs/CLV_CONTROL_CELLS_DEVELOPER_LOG.md` (this entry) |

**Not touched:** `src/custom_listview/clv_cellctl_*` (legacy frozen); C6
smart-scroll/wrap/resize; Space (C7); polished demo store sync (C8).

### Build commands run

```text
# wipe build/custom_listview_control/*.o + example obj, then:
make custom-control-demo
# wipe again, then:
make custom-control-demo CLV_ENABLE_SMART_SCROLL=0
# wipe again, then:
make custom-control-demo
make custom-control-demo-log
make custom-control-demo-bench
```

All linked cleanly (VBCC `+aos68k`).

### Tests and runtime checks performed

- Cross-link (VBCC `+aos68k`) succeeded for smart / nosmart / log / bench.
- Code-path checklist locked to §D.11 (other-row / same-row / cancel /
  one event type per call). Amiga/emulator matrix run not required for
  C5 exit (optional; demo checklist left unchecked for runtime).

### Measured sizes or performance data

VBCC `+aos68k -O2 -size -final`, `-cpu=68000`:

| Target | Bytes | vs C4 |
|--------|------:|------:|
| `bin/custom-control-demo` (smart on) | 41804 | +36 |
| `bin/custom-control-demo` (smart off) | 40560 | −540 |
| `bin/custom-control-demo-log` | 56880 | +236 |
| `bin/custom-control-demo-bench` | 58588 | +64 |

(Smart-off delta vs C4 is from a clean object wipe; C4’s 41100 may have
included a less-clean rebuild state.)

### Defects found

None blocking.

### Known limitations

- No Space toggle (C7).
- Demo does not sync app-owned `control_cells` on toggle (snapshot only;
  C8 authoritative-store pattern).
- Smart-scroll / wrap / resize checkbox correctness is C6.

### Next recommended action

Run a **C6-only** agent with the §G prompt template and §D.6 reading set
(smart-scroll, wrap, resize, regional redraw).

---

## Phase C6 Complete — 2026-07-30

**Phase:** C6 — Smart-scroll, wrap, resize, regional redraw  
**Status:** Complete  
**Agent/session objective:** Keep checkbox pixels correct under
control-owned smart-scroll, wrap, and relayout; cancel arm on structural
change; document checklist.

### What changed

- `clv_control_render.c`: smart-scroll exposed-band expansion grows by
  `cell_padding_y + line_height` (first-line cell) so straddling checkbox
  artwork is redrawn from the snapshot after blit (not half-clipped).
- `clv_control_layout.c`: `layout_invalidate` clears verified-click arm
  (covers padding/gap setters; set_rows/columns already clear via snapshot
  refresh; set_bounds clears explicitly).
- Public/header comments: `render_scrolled` must not follow selection+scroll
  or structural replace; exposed paint includes checkbox snapshot.
- Demo README: C6 size table + emulator checklist (line/jump/resize/wrap/
  arm-then-resize / smart-off twin).
- Master plan §B.7 / §B.9 / §D.0 / §D.6 record / §F updated.

### Files modified

| Action | Path |
|--------|------|
| Updated | `src/custom_listview_control/clv_control_render.c` |
| Updated | `src/custom_listview_control/clv_control_layout.c` |
| Updated | `src/custom_listview_control/clv_control.h` |
| Updated | `examples/custom_control_demo/main.c` |
| Updated | `examples/custom_control_demo/README.md` |
| Updated | `docs/CLV_INTERACTIVE_CONTROL_CELLS_MASTER_PLAN.md` (§B.7 / §B.9 / §D.0 / §D.6 / §F) |
| Updated | `docs/CLV_CONTROL_CELLS_DEVELOPER_LOG.md` (this entry) |

**Not touched:** `src/custom_listview/clv_cellctl_*` (legacy frozen); Space
(C7); polished demo store sync (C8).

### Build commands run

```text
# wipe build/custom_listview_control/*.o + example obj, then:
make custom-control-demo
# wipe again, then:
make custom-control-demo CLV_ENABLE_SMART_SCROLL=0
# wipe again, then:
make custom-control-demo
make custom-control-demo-log
make custom-control-demo-bench
```

All linked cleanly (VBCC `+aos68k`).

### Tests and runtime checks performed

- Cross-link (VBCC `+aos68k`) succeeded for smart / nosmart / log / bench.
- Code-path audit: blit + exposed snapshot paint; selection+`make_visible`
  full viewport; first-line-band resolve after rewrap; arm cancel on
  set_bounds / set_rows / set_columns / layout_invalidate.
- Amiga/emulator visual checklist left unchecked in demo README (optional).

### Measured sizes or performance data

VBCC `+aos68k -O2 -size -final`, `-cpu=68000`:

| Target | Bytes | vs C5 |
|--------|------:|------:|
| `bin/custom-control-demo` (smart on) | 41836 | +32 |
| `bin/custom-control-demo` (smart off) | 41152 | +592 |
| `bin/custom-control-demo-log` | 56912 | +32 |
| `bin/custom-control-demo-bench` | 58620 | +32 |

(Smart-off vs C5: C5’s 40560 looked low vs C4’s 41100; C6 41152 is ~+52
vs C4 nosmart after clean wipes.)

### Defects found

None blocking. Pre-C6 paths already painted checkboxes from the snapshot
on regional/full viewport draws; C6 hardened exposed-band expansion and
arm-cancel coverage.

### Known limitations

- No Space toggle (C7).
- Demo does not sync app-owned `control_cells` on toggle (snapshot only;
  C8 authoritative-store pattern).
- Emulator visual pass for C6 checklist not run in this session.

### Next recommended action

Run a **C8-only** agent with the §G prompt template and §D.8 reading set
(demo polish + authoritative-store sync docs).

---

## 2026-07-30 — Fix: smart-scroll half-tick (diagonal soft clip)

**Status:** Fixed  
**Symptom:** After scrolling a few pixels, checkbox ticks lost most of their
pixels (box frame intact; remnant sliver top or bottom depending on
scroll direction).

**Cause:** `clv_v36_draw_line` soft-clip rejected an entire diagonal stroke
when either endpoint lay outside the regional paint band, while
`fill_rect` still soft-intersected and cleared the in-band interior.
Smart-scroll exposed-band paints (with soft clip always armed beside
`InstallClipRegion`) therefore wiped tick pixels without redrawing them.

**Fix:** When hardware clip is active (`soft_only == FALSE`), pass diagonals
through to `Draw()` and let `InstallClipRegion` trim. When `soft_only`,
Cohen–Sutherland-clip the segment to the soft rect instead of
all-or-nothing reject.

### Files modified

| Action | Path |
|--------|------|
| Updated | `src/custom_listview_control/backends/clv_backend_amiga_v36.c` |
| Updated | `src/custom_listview_control/clv_control_render.c` (comment) |
| Updated | `docs/CLV_CONTROL_CELLS_DEVELOPER_LOG.md` (this entry) |

### Build / validation

```text
make custom-control-demo
```

Cross-link VBCC `+aos68k` clean. Re-check on Amiga/emulator: scroll a few
pixels over checked rows; ticks must stay complete.

---

## Phase C7 Complete — 2026-07-30

**Phase:** C7 — Keyboard toggle (Space; NAV_ACTIVATE unchanged)  
**Status:** Complete  
**Agent/session objective:** Space toggles the selected row’s sole eligible
checkbox; Return / `NAV_ACTIVATE` remains row activation only.

### What changed

- Public ABI: `CLV_INPUT_TOGGLE` after `NAV_ACTIVATE` (Design Change
  **DC-006** — unavoidable; cannot reuse `NAV_ACTIVATE`).
- `clv_control_input.c`: sole-eligible checkbox finder
  (VISIBLE|ENABLED|INTERACTIVE); TOGGLE clears pending mouse arm, toggles
  owned snapshot, emits `CELL_TOGGLED`; zero/multi/ineligible → no event.
  Shared toggle helper used by SELECT_UP commit and Space.
- Demo: RAWKEY Space `0x40` → `CLV_INPUT_TOGGLE`; printf / README updated.
- Master plan §B.5 / §B.8 / §B.10 / §B.11 / §D.0 / §D.7 / §E.3 / §F.

### Files modified

| Action | Path |
|--------|------|
| Updated | `src/custom_listview_control/clv_control.h` |
| Updated | `src/custom_listview_control/clv_control_input.c` |
| Updated | `src/custom_listview_control/clv_control_internal.h` |
| Updated | `examples/custom_control_demo/main.c` |
| Updated | `examples/custom_control_demo/README.md` |
| Updated | `docs/CLV_INTERACTIVE_CONTROL_CELLS_MASTER_PLAN.md` |
| Updated | `docs/CLV_CONTROL_CELLS_DEVELOPER_LOG.md` (this entry) |

**Not touched:** `src/custom_listview/clv_cellctl_*` (legacy frozen);
authoritative app-store sync / polished demo docs (C8); multi-column
keyboard focus.

### Build commands run

```text
# wipe build/custom_listview_control/*.o + example obj, then:
make custom-control-demo
# wipe again, then:
make custom-control-demo CLV_ENABLE_SMART_SCROLL=0
# wipe again, then:
make custom-control-demo
make custom-control-demo-log
make custom-control-demo-bench
```

All linked cleanly (VBCC `+aos68k`).

### Tests and runtime checks performed

- Cross-link (VBCC `+aos68k`) succeeded for smart / nosmart / log / bench.
- Code-path audit: sole eligible → toggle; display-only / disabled /
  no selection → defer; `find_sole` returns FALSE when count ≠ 1;
  `NAV_ACTIVATE` case unchanged (`CLV_EVENT_ACTIVATED` only).
- Amiga/emulator interactive Space checklist left unchecked in demo README
  (optional).

### Measured sizes or performance data

VBCC `+aos68k -O2 -size -final`, `-cpu=68000`:

| Target | Bytes | vs C6 |
|--------|------:|------:|
| `bin/custom-control-demo` (smart on) | 42332 | +496 |
| `bin/custom-control-demo` (smart off) | 41072 | −80 |
| `bin/custom-control-demo-log` | 57540 | +628 |
| `bin/custom-control-demo-bench` | 59172 | +552 |

### Defects found

None blocking.

### Known limitations

- Demo still has a single checkbox column; multi-column deferral is
  enforced in core but not exercised by a second demo column.
- Demo does not sync app-owned `control_cells` on toggle (snapshot only;
  C8 authoritative-store pattern).
- Emulator visual pass for Space checklist not run in this session.

### Next recommended action

Run a **C8-only** agent with the §G prompt template and §D.8 reading set
(demo polish + integration documentation).

---

## 2026-07-30 — Fix: smart-scroll half-tick (diagonal soft clip)

**Status:** Fixed  
**Symptom:** After scrolling a few pixels, checkbox ticks lost most of their
pixels (box frame intact; remnant sliver top or bottom depending on
scroll direction).

**Cause:** `clv_v36_draw_line` soft-clip rejected an entire diagonal stroke
when either endpoint lay outside the regional paint band, while
`fill_rect` still soft-intersected and cleared the in-band interior.
Smart-scroll exposed-band paints (with soft clip always armed beside
`InstallClipRegion`) therefore wiped tick pixels without redrawing them.

**Fix:** When hardware clip is active (`soft_only == FALSE`), pass diagonals
through to `Draw()` and let `InstallClipRegion` trim. When `soft_only`,
Cohen–Sutherland-clip the segment to the soft rect instead of
all-or-nothing reject.

### Files modified

| Action | Path |
|--------|------|
| Updated | `src/custom_listview_control/backends/clv_backend_amiga_v36.c` |
| Updated | `src/custom_listview_control/clv_control_render.c` (comment) |
| Updated | `docs/CLV_CONTROL_CELLS_DEVELOPER_LOG.md` (this entry) |

### Build / validation

```text
make custom-control-demo
```

Cross-link VBCC `+aos68k` clean (`bin/custom-control-demo` 43252). Re-check
on Amiga/emulator: scroll a few pixels over checked rows; ticks must stay
complete.

---

## Phase C8 Complete — 2026-07-30

**Phase:** C8 — Demo and integration documentation  
**Status:** Complete  
**Agent/session objective:** Authoritative demo store sync + integrator-facing
docs; mark GadTools `clv_cellctl_*` legacy / non-authoritative.

### What changed

- Demo: mutable `g_demo_ctrl_store` as app-owned checkbox descriptors;
  `user_data` → `&store[row][On].value`; on `CELL_TOGGLED` write
  `ev.cell_value` then `render_logical_rows`. Interactive / display-only /
  disabled / heading rows unchanged.
- `clv_control.h`: experimental checkbox ownership / `set_rows` /
  `set_checkbox_value` comments; legacy cellctl call-out.
- Docs: integrator section in custom-control demo README; legacy banners on
  cellctl example README, custom-control design plan, module architecture.
- Master plan §D.0 / §D.8 / §F + this log.

### Files modified

| Action | Path |
|--------|------|
| Updated | `examples/custom_control_demo/main.c` |
| Updated | `examples/custom_control_demo/README.md` |
| Updated | `examples/05_draw_cellctl_checkbox/README.md` |
| Updated | `src/custom_listview_control/clv_control.h` |
| Updated | `docs/CLV_CUSTOM_CONTROL_DESIGN_AND_IMPLEMENTATION_PLAN.md` |
| Updated | `docs/CLV_MODULE_ARCHITECTURE.md` |
| Updated | `docs/CLV_INTERACTIVE_CONTROL_CELLS_MASTER_PLAN.md` |
| Updated | `docs/CLV_CONTROL_CELLS_DEVELOPER_LOG.md` (this entry) |

**Not touched:** core toggle/paint/input paths (already C4–C7); size campaign
(C9); closure (C10); `src/custom_listview/clv_cellctl_*` sources.

### Build commands run

```text
# wipe build/custom_listview_control/*.o + example obj, then:
make custom-control-demo
# wipe again, then:
make custom-control-demo CLV_ENABLE_SMART_SCROLL=0
# wipe again, then:
make custom-control-demo
```

All linked cleanly (VBCC `+aos68k`).

### Tests and runtime checks performed

- Cross-link (VBCC `+aos68k`) succeeded for smart and nosmart demos.
- Code-path audit: `CELL_TOGGLED` syncs store via `row_user_data` before
  regional paint; heading `user_data` remains NULL; display-only/disabled
  still non-arming in core.
- Docs audit: GadTools cellctl presented as legacy, not product path.
- Amiga/emulator C8 checklist left unchecked in demo README (operator).

### Measured sizes or performance data

VBCC `+aos68k -O2 -size -final`, `-cpu=68000` (informational; formal C9):

| Target | Bytes |
|--------|------:|
| `bin/custom-control-demo` (smart on) | 43496 |
| `bin/custom-control-demo` (smart off) | 42812 |

(Prior post half-tick-fix smart baseline was 43252; C8 demo store wiring
≈ +244.)

### Defects found

None blocking.

### Known limitations

- Emulator interactive C8 checklist not run in this session.
- Formal size/regression campaign deferred to C9.
- Multi-column keyboard focus still deferred.

### Next recommended action

Run a **C9-only** agent with the §G prompt template and §D.9 reading set
(size + regression validation).

---

## Phase C9 Complete — 2026-07-30

**Phase:** C9 — Size and regression validation  
**Status:** Complete  
**Agent/session objective:** Formal size campaign for
`custom-control-demo` / nosmart (+ log/bench) and regression checklist
for selection, scroll, wrap, toggle, and Space. Document omit policy.

### What changed

- No product code changes (measurement + docs only).
- Formal wipe-and-rebuild size campaign; preserved
  `bin/custom-control-demo-nosmart` twin.
- Corrected C8 nosmart figure (42812 → **42236**) after full object wipe
  when switching `CLV_ENABLE_SMART_SCROLL`; smart **43496** unchanged.
- Documented omit policy: checkbox **always linked** with the control
  package for now (no new Makefile omit flag).
- Master plan §D.0 / §D.9 record / §F + demo README C9 section + this log.

### Files modified

| Action | Path |
|--------|------|
| Updated | `examples/custom_control_demo/README.md` (C9 sizes + regression) |
| Updated | `docs/CLV_INTERACTIVE_CONTROL_CELLS_MASTER_PLAN.md` (§D.0 / §D.9 / §F / header) |
| Updated | `docs/CLV_CONTROL_CELLS_DEVELOPER_LOG.md` (this entry) |

**Not touched:** `src/custom_listview_control/*` sources; GadTools
`clv_cellctl_*`; `docs/CLV_SIZE_REPORT.md` (GadTools size_compare profiles
only — different shell; custom-control sizes live in the demo README).

### Build commands run

```text
# wipe build/custom_listview_control/*.o + backends + example obj
# (+ log/bench trees at start), then:
make custom-control-demo CLV_ENABLE_SMART_SCROLL=0
# copy → bin/custom-control-demo-nosmart; wipe control objs again:
make custom-control-demo
make custom-control-demo-log
make custom-control-demo-bench
```

All linked cleanly (VBCC `+aos68k`).

### Tests and runtime checks performed

Regression checklist (code-path audit; no unexplained regressions):

| Area | Result |
|------|--------|
| Selection vs checkbox (two events / arm rules) | PASS |
| Ordinary smart scroll + first-line exposed-band expand | PASS |
| Selection / make_visible → full viewport (not smart) | PASS |
| Wrap / resize arm cancel + first-line-band geom | PASS |
| Mouse toggle arm/commit | PASS (C4 Amiga runtime prior) |
| Space `CLV_INPUT_TOGGLE` sole-eligible / NAV_ACTIVATE | PASS |
| Demo store sync on `CELL_TOGGLED` | PASS (C8) |

Amiga/emulator interactive re-pass of the consolidated checklist left for
operator; prior package-era selection/scroll/resize checklists remain
signed in the demo README.

### Measured sizes or performance data

VBCC `+aos68k -O2 -size -final`, `-cpu=68000` (formal C9):

| Target | Bytes |
|--------|------:|
| `bin/custom-control-demo` (smart on) | 43496 |
| `bin/custom-control-demo-nosmart` | 42236 |
| Smart − nosmart | +1260 |
| `bin/custom-control-demo-log` | 58736 |
| `bin/custom-control-demo-bench` | 60244 |
| `clv_control_checkbox.o` (smart tree) | 3264 |

Cumulative vs pre-C2 appearance demo (37036): **+6460**.

Object sizes (smart tree): control 4480, layout 3836, wrap 3832,
render 8248, checkbox 3264, input 6496, scroll 704, backend 6388.

### Defects found

None blocking. Prior C8 nosmart size was an incomplete-wipe artifact,
not a code regression.

### Known limitations

- Checkbox always linked (omit-at-link deferred; not introduced in C9).
- Multi-column keyboard focus still deferred.
- Closure audit is C10.
- Emulator re-pass of C9 consolidated interactive checklist optional.

### Next recommended action

Run a **C10-only** agent with the §G prompt template and §D.10 reading
set (closure audit).

---

## Phase C10 Complete — 2026-07-30

**Phase:** C10 — Closure audit  
**Status:** Complete  
**Agent/session objective:** Confirm corrected checkbox feature complete
and documented; public API matches §D.11; legacy GadTools cellctl clearly
non-authoritative; list deferred follow-ups; close C0–C10 roadmap.

### What changed

- Documentation / audit only (no product `.c` / `.h` changes).
- Public API spot-check vs §D.11: **PASS** (column type mask, parallel
  `CLV_ControlCell`, snapshot ownership, setter, `CELL_TOGGLED` fields,
  one-event rule, private arm, first-line band, `CLV_INPUT_TOGGLE`,
  `render_logical_rows` redraw).
- Checkbox path spot-check: no `LISTVIEW_KIND` / `GTLV_*` / `LV_DRAW` /
  `clv_cellctl_*` in `src/custom_listview_control/` checkbox paint/hit/
  input; scroller remains external demo `SCROLLER_KIND`.
- LEGACY ARCHIVE not used as live authority in §A–§G, demo README,
  module architecture, or custom-control plan checkbox note.
- Tiny consistency fixes: master-plan header, SCOPE rule 6, §B audit
  intro, §D gate (removed stale “no checkbox today” / “C4 authorised”).
- Overall corrected feature status: **Complete**.

### Files modified

| Action | Path |
|--------|------|
| Updated | `docs/CLV_INTERACTIVE_CONTROL_CELLS_MASTER_PLAN.md` (§D.0 / §D.9 next / §D.10 record / §F / header / SCOPE / §B / §D gate) |
| Updated | `docs/CLV_CUSTOM_CONTROL_DESIGN_AND_IMPLEMENTATION_PLAN.md` (checkbox status note) |
| Updated | `examples/custom_control_demo/README.md` (C10 closure note) |
| Updated | `docs/CLV_CONTROL_CELLS_DEVELOPER_LOG.md` (this entry) |

**Not touched:** `src/custom_listview_control/*` sources; GadTools
`clv_cellctl_*`; Makefile omit flags.

### Build commands run

None required (docs/audit only). Prior C9 wipe-and-rebuild sizes remain
the measured baseline (`custom-control-demo` 43496 smart / 42236 nosmart).

### Tests and runtime checks performed

| Check | Result |
|-------|--------|
| §D.11 ABI vs `clv_control.h` / internal arm | PASS |
| No GadTools ListView in checkbox path | PASS |
| Scrollbar external | PASS |
| Legacy docs not live authority | PASS |
| Open must-fix without owner | none |

No new Amiga/emulator run this phase (out of scope beyond tiny docs).

### Measured sizes or performance data

None new — C9 figures unchanged.

### Defects found

None blocking. Stale present-tense audit language in §B / §D gate fixed
as tiny doc edits.

### Known limitations / follow-ups

| Item | Status |
|------|--------|
| Makefile optional omit for checkbox object | Deferred (always linked) |
| Multi-column keyboard focus | Deferred |
| Indeterminate checkbox | Deferred |
| Future cycle / button cell types | Future phase cards |
| Optional Amiga re-pass of C8/C9 interactive checklists | Operator optional |
| Eventual cleanup of frozen `clv_cellctl_*` | Separate decision; retain |

### Next recommended action

Corrected C2–C10 roadmap is **closed**. Do not start legacy Phase 6.
Prioritise deferred follow-ups only via a new Design Change / phase card
when needed.

---

## 2026-07-30 — Phase E1: Cell-control event notification audit

**Phase:** E1 — Source audit and event design lock  
**Status:** Complete  
**Agent/session objective:** Audit live event/input/checkbox/demo paths
against `docs/CLV_CELL_CONTROL_EVENT_NOTIFICATION_PLAN.md`; lock ABI for
E2; no product behaviour changes.

### Source files inspected

- `src/custom_listview_control/clv_control.h`
- `src/custom_listview_control/clv_control_input.c`
- `src/custom_listview_control/clv_control_internal.h`
- `src/custom_listview_control/clv_control.c`
- `examples/custom_control_demo/main.c` (gadgets, geom, `demo_apply_input`)
- `docs/AutoDocs/gadtools.doc` (`TEXT_KIND` / `GTTX_Text` / `GT_SetGadgetAttrs`)

### Key findings

1. App already receives checkbox commits via `CLV_EVENT_CELL_TOGGLED`
   (row, column, `previous_value`, `cell_value`, `row_user_data`). Gap is
   generic naming/actions + demo status gadget, not a missing notification.
2. One `CLV_EventType` per `handle_input`; selection and toggle are
   separate SELECT_DOWN / SELECT_UP calls.
3. Mouse commit and Space/`CLV_INPUT_TOGGLE` share
   `clv_ctrl_toggle_checkbox_at()`; old value read from snapshot before
   mutate.
4. `LONG value` is scroll_y — must keep `UBYTE cell_value` for new state
   (EC-001).
5. `control_user_data` deferred — `CLV_ControlCell` has no natural slot.
6. Demo `CreateContext` chain + resize rebuild can host bordered
   `TEXT_KIND`; AutoDocs: post-create `GTTX_Text` borrows pointer (V36).
7. `CLV_EVENT_ACTIVATED` remains Return / `NAV_ACTIVATE` row activation.

### Design lock (EC-001 Accepted)

- Rename `CLV_EVENT_CELL_TOGGLED` → `CLV_EVENT_CELL_CONTROL`.
- Add `control_type` / `control_action` (`CLV_CTRL_ACTION_VALUE_CHANGED`
  for checkbox).
- Keep `row_user_data`, `previous_value`, `cell_value`.
- No callbacks / Exec messages / `control_user_data` in E2.

### Files modified

| Action | Path |
|--------|------|
| Updated | `docs/CLV_CELL_CONTROL_EVENT_NOTIFICATION_PLAN.md` (E1 answers, §3 lock, EC-001, completion) |
| Updated | `docs/CLV_CONTROL_CELLS_DEVELOPER_LOG.md` (this entry) |

**Not touched:** any `src/custom_listview_control/*` product sources.

### Build commands run

None (docs/audit only).

### Tests and runtime checks performed

| Check | Result |
|-------|--------|
| Trace SELECT_DOWN arm / SELECT_UP commit | PASS (code path) |
| Trace `CLV_INPUT_TOGGLE` → shared toggle helper | PASS (code path) |
| AutoDocs `GTTX_Text` borrow on Set | PASS (V36) |
| Demo CreateContext suitability | PASS |

### Next recommended action

Implement **Phase E2** — rename event, add action/type fields, fill from
shared toggle helper; update demo type/field names only (status gadget = E3).

---

## 2026-07-30 — Phase E2: Generic cell-control event

**Phase:** E2 — Generic cell-control event implementation  
**Status:** Complete  
**Agent/session objective:** Land locked `CLV_EVENT_CELL_CONTROL` ABI from
EC-001; fill from shared checkbox toggle helper; update demo handler names
only (no status gadget).

### Source files inspected / modified

| Action | Path |
|--------|------|
| Updated | `src/custom_listview_control/clv_control.h` |
| Updated | `src/custom_listview_control/clv_control_input.c` |
| Updated | `examples/custom_control_demo/main.c` |
| Updated | `examples/custom_control_demo/README.md` (live integrator text) |
| Updated | `docs/CLV_CELL_CONTROL_EVENT_NOTIFICATION_PLAN.md` |
| Updated | `docs/CLV_CONTROL_CELLS_DEVELOPER_LOG.md` (this entry) |

### API and behaviour

- Renamed `CLV_EVENT_CELL_TOGGLED` → `CLV_EVENT_CELL_CONTROL` (same ordinal).
- Added `CLV_CellControlAction` (`NONE`, `VALUE_CHANGED`, `PRESSED`).
- Extended `CLV_Event` with `control_type` / `control_action` between
  `column` and `row_user_data`; kept `UBYTE previous_value` / `cell_value`.
- `clv_ctrl_clear_event` zeros the new fields.
- New `clv_ctrl_fill_cell_event()` used by `clv_ctrl_toggle_checkbox_at()`
  (mouse SELECT_UP commit and Space/`CLV_INPUT_TOGGLE`).
- Checkbox commits set `control_type = CHECKBOX`,
  `control_action = VALUE_CHANGED`.
- No callbacks, no Exec messages, no `control_user_data`.
- Selection / scroll / `ACTIVATED` paths unchanged.

### Build commands run

```text
Remove-Item build\custom_listview_control\clv_control_input.o,
            build\examples\custom_control_demo.o
make custom-control-demo
```

Result: link OK — `bin/custom-control-demo` **43644** bytes
(VBCC `+aos68k -O2 -size -final`, smart scroll on).

### Tests and runtime checks performed

| Check | Result |
|-------|--------|
| Shared toggle helper fills CELL_CONTROL | PASS (code) |
| clear_event zeros control_type/action | PASS (code) |
| Demo handler uses CELL_CONTROL + new fields | PASS (compile) |
| Arm/cancel/disabled/display-only paths untouched | PASS (diff/path) |
| Amiga/emulator visual toggle re-check | Not run this session |

### Known limitations

- Demo still prints to console only; status `TEXT_KIND` is **E3**.
- Historical master-plan / older README size campaign text may still say
  `CELL_TOGGLED` as a past label; live API is `CELL_CONTROL`.

### Next recommended action

Implement **Phase E3** — bordered read-only `TEXT_KIND` status field under
the ListView; format `CLV_EVENT_CELL_CONTROL` into a persistent buffer and
update via `GT_SetGadgetAttrs(GTTX_Text)`.

---

## 2026-07-30 — Phase E3: Demo status field

**Phase:** E3 — Demo status field  
**Status:** Complete  
**Agent/session objective:** Prove `CLV_EVENT_CELL_CONTROL` delivery in the
demo window via a bordered read-only GadTools `TEXT_KIND` status gadget.

### Source files inspected / modified

| Action | Path |
|--------|------|
| Updated | `examples/custom_control_demo/main.c` |
| Updated | `examples/custom_control_demo/README.md` |
| Updated | `docs/CLV_CELL_CONTROL_EVENT_NOTIFICATION_PLAN.md` |
| Updated | `docs/CLV_CONTROL_CELLS_DEVELOPER_LOG.md` (this entry) |

### API and behaviour

- No control-core ABI change (E2 event path unchanged).
- Demo: `DemoGeom` gains `status_*`; `demo_compute_geom` reserves
  status + pad + action strip under the ListView.
- `CreateGadget(TEXT_KIND)` with `GTTX_Text` + `GTTX_Border, TRUE`
  (`GID_EVENT_STATUS`); persistent `g_demo_event_text[160]`.
- On `CLV_EVENT_CELL_CONTROL`, `demo_update_event_status` formats
  friendly type/action/row/column/old→new and calls
  `GT_SetGadgetAttrs(GTTX_Text)` (no gadget recreate on toggle).
- Resize recreate reuses the same buffer so last event text survives.
- Window title: `CLV Custom Control (Phase E3)`.

### Build commands run

```text
Remove-Item build\examples\custom_control_demo.o -ErrorAction SilentlyContinue
make custom-control-demo
```

Result: link OK — `bin/custom-control-demo` **44824** bytes
(+1180 vs E2 **43644**; VBCC `+aos68k -O2 -size -final`).

### Tests and runtime checks performed

| Check | Result |
|-------|--------|
| Status geom between list and Go strip | PASS (code) |
| Persistent GTTX_Text buffer | PASS |
| CELL_CONTROL updates status gadget | PASS (code path) |
| Mouse/keyboard share apply_input → status | PASS |
| Amiga/emulator visual toggle + resize | Not run this session |

### Known limitations

- Amiga UI visual confirmation of status text still recommended.
- E4 still owns future-control readiness / docs closure.

### Next recommended action

Implement **Phase E4** — future-control readiness and closure audit
(button/cycle semantics, docs, header comments, size delta).

---

## 2026-07-30 — Phase E4: Future-control readiness and closure audit

**Phase:** E4 — Future-control readiness and closure audit  
**Status:** Complete  
**Agent/session objective:** Confirm `CLV_EVENT_CELL_CONTROL` is generic
enough for future button/cycle cells; document ownership, lifetime, and
value semantics; close the event-notification plan.

### Source files inspected / modified

| Action | Path |
|--------|------|
| Updated | `src/custom_listview_control/clv_control.h` (comments) |
| Updated | `src/custom_listview_control/clv_control_input.c` (fill-helper comment) |
| Updated | `examples/custom_control_demo/main.c` (title Phase E4) |
| Updated | `examples/custom_control_demo/README.md` (integrator contract) |
| Updated | `docs/CLV_CELL_CONTROL_EVENT_NOTIFICATION_PLAN.md` |
| Updated | `docs/CLV_CONTROL_CELLS_DEVELOPER_LOG.md` (this entry) |

### API and behaviour

- No product behaviour or ABI field change (docs/comments only).
- Confirmed: public names have no checkbox leakage; `PRESSED` /
  `VALUE_CHANGED` cover future button/cycle; fill is centralised in
  `clv_ctrl_fill_cell_event`; stateless controls may leave values at 0;
  `row_user_data` borrowed; `control_user_data` deferred; synchronous
  stack delivery only.
- Window title: `CLV Custom Control (Phase E4)`.

### Build commands run

```text
Remove-Item build\examples\custom_control_demo.o -ErrorAction SilentlyContinue
Remove-Item build\custom_listview_control\clv_control_input.o -ErrorAction SilentlyContinue
make custom-control-demo
```

Result: link OK — `bin/custom-control-demo` **44824** bytes
(0 vs E3; VBCC `+aos68k -O2 -size -final`).

### Tests and runtime checks performed

| Check | Result |
|-------|--------|
| Name/action/payload leakage review | PASS |
| Future button/cycle fit without redesign | PASS |
| Centralised fill + clear_event | PASS |
| Demo formatting not in core | PASS |
| Closure checklist in event plan | PASS |
| Amiga/emulator visual re-check | Not run (not E4 blocker) |

### Known limitations

- Button and cycle cell types are not implemented yet; only the
  notification path is ready for them.
- Historical master-plan text may still say `CELL_TOGGLED` as a past
  label; live API remains `CELL_CONTROL`.

### Next recommended action

None for the event-notification plan. Separate feature work may add
button/cycle column types using the locked `CLV_EVENT_CELL_CONTROL`
delivery model.

---

