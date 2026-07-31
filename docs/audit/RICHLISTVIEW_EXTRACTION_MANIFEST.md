# RichListview Extraction Manifest

**Status:** Plan only — do **not** execute extraction from this document alone.  
**Date:** 2026-07-31  
**Principle:** Copy the working custom-control closure first; clean after the demo builds.

---

## Copy first into RichListview

These files are required for the currently working control (or its documented demo/bench/log variants):

### Control package (entire tree)

- `src/custom_listview_control/clv_control_draw.h` — CLV_DrawOps; CLV_Pens; no clv_renderer.h
- `src/custom_listview_control/clv_control_render.c` — full/viewport/partial/smart-scroll paint; no LVDrawMsg
- `src/custom_listview_control/clv_control_checkbox.c` — checkbox resolve/paint for control
- `src/custom_listview_control/clv_control_input.c` — hit_test; selection; keyboard NAV; CELL_CONTROL events
- `src/custom_listview_control/clv_control_scroll.c` — scroll_y clamp; content/viewport height
- `src/custom_listview_control/clv_control_log.c` — PROGDIR logger implementation
- `src/custom_listview_control/backends/clv_backend_amiga_v36.h` — clv_backend_v36_*; No LISTVIEW_KIND comment
- `src/custom_listview_control/clv_control.h` — clv_control_create/destroy/render/handle_input; not LISTVIEW_KIND
- `src/custom_listview_control/clv_control_platform.h` — includes custom_listview/clv_platform.h
- `src/custom_listview_control/clv_control_internal.h` — struct CLV_Control; includes clv_bench_internal.h
- `src/custom_listview_control/clv_control.c` — create/destroy/setters/render entry
- `src/custom_listview_control/clv_control_layout.c` — layout_rebuild; column geom; row heights
- `src/custom_listview_control/clv_control_wrap.c` — pixel wrap prepare; adapted from clv_pixel_wrap without linking
- `src/custom_listview_control/clv_control_log.h` — clv_log_init/write macros; collides with legacy clv_log.h names
- `src/custom_listview_control/backends/clv_backend_amiga_v36.c` — implements DrawOps; ScrollRaster smart scroll

### Shared foundations to copy then localise

- `src/custom_listview/clv_platform.h` — CLV_PLATFORM_AMIGA assert; included by control via clv_control_platform.h
- `src/custom_listview/clv_platform.c` — clv_platform_malloc/free/strdup
- `src/custom_listview/clv_platform_internal.h` — declares allocation shim; included by control .c files
- `src/custom_listview/clv_compiler.h` — CLV_COMPILER_* detection
- `src/custom_listview/clv_sdk_compat.h` — NewList and SDK fallbacks
- `src/custom_listview/clv_exec_list_compat.h` — thin NewList wrapper
- `src/custom_listview/clv_bench.c` — timing harness used by control bench build
- `src/custom_listview/clv_bench_internal.h` — bench macros; included by clv_control_internal.h and v36 backend

### Demo and control docs

- `examples/custom_control_demo/main.c` — IDCMP→CLV_InputEvent; SCROLLER_KIND companion; paints via clv_control_render
- `examples/custom_control_demo/README.md` — documents no LISTVIEW_KIND; control architecture
- `docs/CLV_CUSTOM_CONTROL_DESIGN_AND_IMPLEMENTATION_PLAN.md` — RichListview design/implementation plan
- `docs/CLV_CUSTOM_CONTROL_KEYBOARD_NAVIGATION_PLAN.md` — control keyboard NAV plan
- `docs/CLV_CUSTOM_CONTROL_PHASE1_AUDIT.md` — Phase1 custom control audit
- `docs/CLV_CELL_CONTROL_EVENT_NOTIFICATION_PLAN.md` — control cell events plan
- `docs/CLV_CONTROL_CELLS_DEVELOPER_LOG.md` — developer log for control cells
- `docs/CLV_INTERACTIVE_CONTROL_CELLS_MASTER_PLAN.md` — master plan; may mention both paths
- `docs/CLV_BENCHMARK_HANDOFF.md` — control bench handoff
- `docs/CLV_BENCHMARK_IMPLEMENTATION_REPORT.md` — control bench implementation
- `docs/audit/LISTVIEW_ARCHITECTURE_FILE_AUDIT.md` — main audit report
- `docs/audit/LISTVIEW_DEPENDENCY_MAP.md` — dependency map
- `docs/audit/RICHLISTVIEW_EXTRACTION_MANIFEST.md` — extraction plan
- `docs/audit/LISTVIEW_AUDIT_MACHINE_READABLE.csv` — machine-readable classifications

**Minimum first build set (recommended):**

1. All of `src/custom_listview_control/`
2. `src/custom_listview/clv_platform.c` + `clv_platform.h` + `clv_platform_internal.h`
3. `src/custom_listview/clv_types.h` (then strip GadTools-only comments/constants)
4. `examples/custom_control_demo/main.c` (+ README)
5. A new Rich-only Makefile linking the same object set as `CLV_CUSTOM_CONTROL_LIBS`

Optional follow-ons: `clv_bench.*`, `clv_control_log.*` for parity with `-bench` / `-log` targets.

---

## Move after verification

After the copied tree builds and the demo runs on Amiga/WinUAE:

- `src/custom_listview_control/clv_control_draw.h` — CLV_DrawOps; CLV_Pens; no clv_renderer.h
- `src/custom_listview_control/clv_control_render.c` — full/viewport/partial/smart-scroll paint; no LVDrawMsg
- `src/custom_listview_control/clv_control_checkbox.c` — checkbox resolve/paint for control
- `src/custom_listview_control/clv_control_input.c` — hit_test; selection; keyboard NAV; CELL_CONTROL events
- `src/custom_listview_control/clv_control_scroll.c` — scroll_y clamp; content/viewport height
- `src/custom_listview_control/clv_control_log.c` — PROGDIR logger implementation
- `src/custom_listview_control/backends/clv_backend_amiga_v36.h` — clv_backend_v36_*; No LISTVIEW_KIND comment

(These have no meaningful legacy consumers.)

---

## Duplicate as small generic modules

Prefer duplication over a third repository:

- `src/custom_listview/clv_platform.h` — CLV_PLATFORM_AMIGA assert; included by control via clv_control_platform.h
- `src/custom_listview/clv_platform.c` — clv_platform_malloc/free/strdup
- `src/custom_listview/clv_platform_internal.h` — declares allocation shim; included by control .c files
- `src/custom_listview/clv_compiler.h` — CLV_COMPILER_* detection
- `src/custom_listview/clv_sdk_compat.h` — NewList and SDK fallbacks
- `src/custom_listview/clv_exec_list_compat.h` — thin NewList wrapper
- `src/custom_listview/clv_sort.h` — sort state over source cells; no GadTools calls
- `src/custom_listview/clv_sort.c` — qsort-based text/numeric order
- `src/custom_listview/clv_path.h` — path truncate/shorten public API
- `src/custom_listview/clv_path.c` — public wrappers
- `src/custom_listview/clv_path_core.c` — canonical path shorten core
- `src/custom_listview/clv_path_internal.h` — internal path-core API
- `src/custom_listview/clv_cellctl_geom.c` — pure checkbox geometry/state; host-tested
- `src/custom_listview/clv_cellctl_geom.h` — geom API

---

## Split before migration

- `src/custom_listview/clv_types.h` — CLV_CellAlign; CLV_PixelColumn; CLV_LISTVIEW_SCROLLBAR_BORDER; comments cite lvdm_Bounds
- `Makefile` — hosts both CLV_* profile libs and CLV_CUSTOM_CONTROL_* targets

Additional editorial splits (docs): `README.md`, `AGENTS.md`, interactive-cell master plans — see audit §9.

---

## Rewrite rather than copy

- `src/custom_listview/clv_log.h` — clv_log_info/error no-op macros; name collides with control clv_log_*
- `src/custom_listview/clv_renderer_setup.h` — viewport/continuation-guide geometry helpers; host-testable
- `src/custom_listview/clv_renderer_setup.c` — pure cell-presentation / viewport geometry
- `src/custom_listview/clv_icons.c` — clv_icons_install; status/sort indicators in LV_DRAW
- `src/custom_listview/clv_styles.c` — clv_styles_install soft styles
- `src/custom_listview/clv_details.c` — semantic details builder

Especially: do **not** copy `clv_selection.c` (physical `GTLV_Selected` adapter). Rich selection is already implemented in `clv_control_input.c`.

---

## Leave in legacy repository

Primary rule: everything under legacy ASCII/drawn/cellctl profiles, examples `00`–`08`, `size_compare`, host/header tests, v1 integration docs, binders, renderer.

Count of `LEAVE_IN_LEGACY_REPOSITORY` rows in CSV: **219**.

Notable leave-behinds that look “control-related” but are not the product path:

- `src/custom_listview/clv_cellctl_*` + `clv_bind_cellctl.c` + `examples/05_draw_cellctl_checkbox/`
- Entire `clv_renderer_*` / `clv_selection_*` / `clv_pixel_wrap.c` stacks

---

## Documentation to replace or rewrite

| Document | Action |
|---|---|
| `README.md` | Rewrite to name both architectures or become legacy-only after split |
| `AGENTS.md` / `.github/copilot-instructions.md` | Add hard banner: which tree is which; forbid patching cellctl for control bugs |
| `docs/CLV_MODULE_ARCHITECTURE.md` | Refresh paths; keep legacy; link to Rich docs separately |
| `docs/CUSTOM_LISTVIEW_INTEGRATION_GUIDE.md` | Remains legacy GadTools ingestion guide |
| Custom-control plan/log docs | Copy into Rich; mark legacy cellctl sections historical |
| `docs/AutoDocs/` | Keep available to both as Amiga API reference |

---

## Tests and demos to bring across

| Item | Action |
|---|---|
| `examples/custom_control_demo/` | **Copy first** — golden behavioural reference |
| Host tests under `tests/host/` | Leave (v1); author new Rich host/Amiga tests later |
| `draw-cellctl-checkbox` example | Leave in legacy |
| Size harness | Leave in legacy (measures v1 profiles) |

---

## Build files to create later

- New RichListview root `Makefile` (or CMake later — not required) with:
  - control objects
  - platform object
  - optional log/bench trees
  - `CLV_ENABLE_SMART_SCROLL`
  - demo target equivalent to `custom-control-demo`
- Generated feature header for Rich-only flags (do not reuse `CLV_HAS_RENDERER` etc. without review)
- CI/validate scripts cloned only where they still apply (`validate-no-host-branches` style)

Do **not** copy the full legacy profile matrix into Rich.

---

## Open questions blocking extraction

1. Final public prefix: keep `clv_control_*` temporarily vs rename to `rlv_*` immediately after copy?
2. Should Rich vendor a trimmed `clv_types.h` or define its own `RLV_CellAlign` / column struct?
3. Is `clv_cellctl_*` retained indefinitely in the legacy repo for historical demos, or archived after a deprecation notice?
4. Will sort/path modules be needed in Rich soon enough to copy now?
5. Single monorepo with two products vs hard repository split — product decision (`REQUIRES_MANUAL_DECISION` on README/AGENTS).
6. Who owns updating `AGENTS.md` so future AI agents cannot patch the wrong ListView?

---

## Explicit non-goals of this manifest

- Do not delete legacy code in-place during extraction.
- Do not invent a third shared Amiga ListView library yet.
- Do not relink `clv_pixel_wrap.o` into Rich to “dedupe” wrap — the fork is intentional.
