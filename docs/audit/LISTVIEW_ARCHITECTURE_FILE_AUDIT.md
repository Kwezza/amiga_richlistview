# ListView Architecture File Audit

**Date:** 2026-07-31  
**Scope:** Read-only classification of the `amiga_custom_listview` repository before any RichListview extraction.  
**Prompt:** `docs/AI_AGENT_PROMPT_AUDIT_AND_CLASSIFY_LISTVIEW_FILES.md`  
**Companion files:** `LISTVIEW_DEPENDENCY_MAP.md`, `RICHLISTVIEW_EXTRACTION_MANIFEST.md`, `LISTVIEW_AUDIT_MACHINE_READABLE.csv`

---

## 1. Executive summary

This repository contains **two distinct ListView architectures** that share the `CLV_*` / `clv_*` namespace and a thin platform layer:

1. **Legacy GadTools enhancement** (`src/custom_listview/`) — extends `LISTVIEW_KIND` with ASCII label lists and/or a custom `GTLV_CallBack` / `LV_DRAW` hook, prepared physical rows, selection adapters, binders, and an experimental GadTools checkbox path (`clv_cellctl_*`).
2. **Full custom control / RichListview precursor** (`src/custom_listview_control/`) — owns viewport painting, scrolling, hit-testing, keyboard NAV, selection, and checkbox cells via `CLV_DrawOps`. It does **not** use `LISTVIEW_KIND`, `GTLV_*`, or `LVDrawMsg`.

The working custom control links only `clv_platform.o` (+ optional `clv_bench.o`) from the legacy tree and includes `clv_types.h` / `clv_platform*.h` / `clv_bench_internal.h`. Wrap logic was **copied** into `clv_control_wrap.c` rather than linked from `clv_pixel_wrap.c`.

**Highest extraction risk:** shared naming (`CLV_*`), dual checkbox implementations (`clv_cellctl_*` vs `clv_control_checkbox.c`), and agent guidance (`README.md` / `AGENTS.md`) that still describe the repo as a GadTools enhancer only.

**Recommended next step:** copy the complete custom-control dependency closure into a new RichListview tree and clean after the demo still builds — do not hand-pick files from memory.

**Audited rows:** 315 (see CSV). AutoDocs corpus summarised as one exclusion row; host `.exe` binaries excluded as artefacts.

| Classification | Count |
|---|---:|
| LEGACY_GADTOOLS | 40 |
| RICHLISTVIEW | 15 |
| GENERIC_REUSABLE | 18 |
| DEMO_OR_TEST | 121 |
| BUILD_OR_TOOLING | 18 |
| DOCUMENTATION | 103 |
| UNCERTAIN | 0 |

| Metric | Count |
|---|---:|
| Files with `SPLIT_REQUIRED` | 7 |
| Files with cross-architecture dependency flags | 13 |

---

## 2. Definitions of the two architectures

### LEGACY_GADTOOLS

Enhances the stock GadTools `LISTVIEW_KIND` gadget. Clients call `CreateGadget(LISTVIEW_KIND, …)`. GadTools owns scrolling, visible slots, and **physical** selection (`GTLV_Selected`). CLV supplies:

- ASCII: formatted `ln_Name` strings in `struct List` attached via `GTLV_Labels`.
- Drawn: `clv_renderer_get_hook()` → `GTLV_CallBack`; `clv_renderer_dispatch` handles `LV_DRAW` / `struct LVDrawMsg`.
- Wrapped logical records become **multiple physical GadTools nodes**; maps live in prepared/display structures.
- `clv_handle_selection` remaps/rejects non-selectable physical rows via `GT_SetGadgetAttrs(GTLV_Selected, …)`.

### RICHLISTVIEW (current `custom_listview_control`)

A custom-drawn control that paints into a window `RastPort` through `CLV_DrawOps` (Amiga V36 backend). The app owns IDCMP translation into `CLV_InputEvent`. Companion gadgets (e.g. `SCROLLER_KIND`) are ordinary GadTools gadgets, **not** a ListView. Selection is a **logical row** index inside `CLV_Control`. Variable-height / wrapped rows are owned by the control’s layout caches, not as multiple GadTools label nodes.

### Row-index vocabulary (do not conflate)

| Term | Legacy GadTools path | RichListview path |
|---|---|---|
| Source / application record | App cell table | App row store / `row_user_data` |
| Logical row | Prepared logical index | `CLV_Control` row index |
| Physical / display row | GadTools label node / `GTLV_Selected` | Viewport paint band (not a gadget item index) |
| Wrapped subline | Extra physical nodes or fragment lines | Fragments inside `CLV_ControlCellWrap` |
| Visible slot | GadTools visible item | Viewport Y range vs `scroll_y` |

---

## 3. Repository inventory summary

| Area | Role |
|---|---|
| `src/custom_listview/` | Legacy enhancer + shared platform/types/bench |
| `src/custom_listview_control/` | Experimental full custom control |
| `examples/01_*` … `08_*`, `00_*`, `size_compare/` | Legacy profile demos / size harness |
| `examples/custom_control_demo/` | Working RichListview demo |
| `tests/host/`, `tests/header_audit/`, `tests/headers/`, wrap/ABI tests | Almost entirely v1 / legacy |
| `Makefile` + `tools/` | Explicit object lists for both architectures |
| `templates/` | GadTools window lifecycle (usable by either embedding app) |
| `docs/` | Mix of v1 integration docs and custom-control plans |
| `docs/AutoDocs/` | Third-party Amiga AutoDocs (reference; not product code) |

Build boundary evidence: root `Makefile` defines `CLV_*_LIBS` profile sets separately from `CLV_CUSTOM_CONTROL_LIBS = $(CLV_PLATFORM_OBJS) + control objects` and states the control does not link v1 renderer/selection.

---

## 4. Legacy architecture call flow

```text
App: CreateGadget(LISTVIEW_KIND, … [, GTLV_CallBack = clv_renderer_get_hook()])
  ASCII:
    format rows → clv_ascii_create_justified_list / clv_char_wrap_*
    GT_SetGadgetAttrs(GTLV_Labels, list)
    GadTools draws ln_Name
  Drawn:
    clv_renderer_create → clv_renderer_bind_optional() [one clv_bind_*.o]
    clv_prepared_build_* → physical Node list + display map
    GT_SetGadgetAttrs(GTLV_Labels, clv_prepared_get_labels())
    paint: HookEntry → clv_renderer_dispatch(LVDrawMsg / LV_DRAW)
    select: GTLV_Selected → clv_handle_selection(…)
    optional: IDCMP mouse → clv_cellctl_handle_mouse (cellctl profile)
Cleanup:
    GT_SetGadgetAttrs(GTLV_Labels, ~0)  /* detach BEFORE free */
    free list / prepared / wrap context
    free renderer after gadget cannot call hook
```

Roots: client `CreateGadget`; `clv_renderer_core.c` (`clv_renderer_dispatch`); `clv_selection.c`; `clv_ascii_formatter.c` / `clv_char_wrap.c`; binders `clv_bind_*.c`.

---

## 5. RichListview architecture call flow

```text
App: OpenWindow / DrawInfo
  clv_backend_v36_create(rp, font)
  cfg.draw_ops = clv_backend_v36_get_ops()
  clv_control_create(&cfg)
  set_pens / set_columns / set_rows / set_bounds
    → clv_control_layout_rebuild → clv_control_wrap_prepare
  clv_control_render (full / viewport / logical_rows / scrolled)
Loop:
  IDCMP → CLV_InputEvent → clv_control_handle_input → CLV_Event
  app paints from event (selection / scroll / CELL_CONTROL)
Dispose:
  clv_control_destroy → clv_backend_v36_destroy
```

Roots: `clv_control_create` (`clv_control.c`); `clv_control_render*` (`clv_control_render.c`); `clv_control_handle_input` (`clv_control_input.c`); `clv_control_set_scroll_y` (`clv_control_scroll.c`); backend `clv_backend_amiga_v36.c`.

**No** `GTLV_*`, `LV_DRAW`, or `LVDrawMsg` in control sources (comments only).

---

## 6. Complete file classification table

The authoritative machine-readable table is `LISTVIEW_AUDIT_MACHINE_READABLE.csv` (315 rows). Below: **all `src/` implementation files** (required completeness for product code), then notable mixed/build/demo entries. Remaining demo/test/doc rows are identical in the CSV.

### 6.1 All `src/` files

| Path | Classification | Confidence | SecondaryFlags | BuiltBy | KeyEvidence | Dependencies | ProposedExtractionAction |
| --- | --- | --- | --- | --- | --- | --- | --- |
| src/custom_listview/clv_platform.h | GENERIC_REUSABLE | High | PUBLIC_API;COPY_CANDIDATE;RICHLISTVIEW_DEPENDENCY | all profiles; custom-control-demo | CLV_PLATFORM_AMIGA assert; included by control via clv_control_platform.h | none | DUPLICATE_SMALL_GENERIC_MODULE |
| src/custom_listview/clv_platform.c | GENERIC_REUSABLE | High | PRIVATE_INTERNAL;COPY_CANDIDATE;RICHLISTVIEW_DEPENDENCY | CLV_PLATFORM_OBJS all profiles+control | clv_platform_malloc/free/strdup | Amiga AllocMem | DUPLICATE_SMALL_GENERIC_MODULE |
| src/custom_listview/clv_platform_internal.h | GENERIC_REUSABLE | High | PRIVATE_INTERNAL;COPY_CANDIDATE;RICHLISTVIEW_DEPENDENCY | via clv_platform.o | declares allocation shim; included by control .c files | clv_platform.h | DUPLICATE_SMALL_GENERIC_MODULE |
| src/custom_listview/clv_compiler.h | GENERIC_REUSABLE | High | PUBLIC_API;COPY_CANDIDATE | header-only | CLV_COMPILER_* detection | none | DUPLICATE_SMALL_GENERIC_MODULE |
| src/custom_listview/clv_sdk_compat.h | GENERIC_REUSABLE | High | PUBLIC_API;COPY_CANDIDATE | header-only | NewList and SDK fallbacks | Amiga SDK | DUPLICATE_SMALL_GENERIC_MODULE |
| src/custom_listview/clv_exec_list_compat.h | GENERIC_REUSABLE | High | PUBLIC_API;COPY_CANDIDATE | header-only | thin NewList wrapper | clv_sdk_compat.h | DUPLICATE_SMALL_GENERIC_MODULE |
| src/custom_listview/clv_types.h | GENERIC_REUSABLE | Medium | PUBLIC_API;SPLIT_REQUIRED;COPY_CANDIDATE;RICHLISTVIEW_DEPENDENCY;MISLEADING_NAME | header; used by both arches | CLV_CellAlign; CLV_PixelColumn; CLV_LISTVIEW_SCROLLBAR_BORDER; comments cite lvdm_Bounds | clv_platform.h | SPLIT_FILE_BEFORE_OR_DURING_EXTRACTION |
| src/custom_listview/clv_config.h | LEGACY_GADTOOLS | High | PUBLIC_API;LEAVE_BEHIND | profile clients via generated CLV_HAS_* | CLV_HAS_* defaults for v1 feature families | none | LEAVE_IN_LEGACY_REPOSITORY |
| src/custom_listview/clv_log.h | GENERIC_REUSABLE | Medium | PRIVATE_INTERNAL;MISLEADING_NAME | header-only macros in legacy .c | clv_log_info/error no-op macros; name collides with control clv_log_* | none | REWRITE_FOR_RICHLISTVIEW |
| src/custom_listview/clv_bench.c | GENERIC_REUSABLE | High | PRIVATE_INTERNAL;COPY_CANDIDATE;RICHLISTVIEW_DEPENDENCY | custom-control-demo-bench | timing harness used by control bench build | clv_bench_internal.h | COPY_TO_RICHLISTVIEW_THEN_CLEAN |
| src/custom_listview/clv_bench_internal.h | GENERIC_REUSABLE | High | PRIVATE_INTERNAL;COPY_CANDIDATE;RICHLISTVIEW_DEPENDENCY | control bench + backend include | bench macros; included by clv_control_internal.h and v36 backend | none | COPY_TO_RICHLISTVIEW_THEN_CLEAN |
| src/custom_listview/custom_listview.h | LEGACY_GADTOOLS | High | PUBLIC_API;LEAVE_BEHIND | umbrella include | includes v1 public headers only | v1 public headers | LEAVE_IN_LEGACY_REPOSITORY |
| src/custom_listview/PUBLIC_HEADERS.txt | BUILD_OR_TOOLING | High | LEAVE_BEHIND | header allowlist tooling | lists v1 public basenames including clv_cellctl.h | none | LEAVE_IN_LEGACY_REPOSITORY |
| src/custom_listview/clv_ascii.h | LEGACY_GADTOOLS | High | PUBLIC_API;SPLIT_REQUIRED;LEAVE_BEHIND | ascii-* and draw-* (columns bridge) | ASCII format API + pixel-column bridge for drawn prepare; lvdm_Bounds comments | clv_types.h | LEAVE_IN_LEGACY_REPOSITORY |
| src/custom_listview/clv_ascii_columns.c | LEGACY_GADTOOLS | High | SPLIT_REQUIRED;LEAVE_BEHIND | CLV_ASCII_COLUMNS_OBJS ascii+draw profiles | format_header/row + clv_ascii_columns_calc_pixel_columns | clv_log.h; path_core; types | LEAVE_IN_LEGACY_REPOSITORY |
| src/custom_listview/clv_ascii_internal.h | LEGACY_GADTOOLS | High | PRIVATE_INTERNAL;LEAVE_BEHIND | ascii_columns/prepare internals | content-viewport bridge internals | ascii | LEAVE_IN_LEGACY_REPOSITORY |
| src/custom_listview/clv_ascii_formatter.h | LEGACY_GADTOOLS | High | PUBLIC_API;LEAVE_BEHIND | ascii profiles | justified struct List for GTLV_Labels; detach docs | exec lists | LEAVE_IN_LEGACY_REPOSITORY |
| src/custom_listview/clv_ascii_formatter.c | LEGACY_GADTOOLS | High | LEAVE_BEHIND | CLV_ASCII_FORMATTER_OBJS | clv_ascii_create/free_justified_list | platform; log; exec list | LEAVE_IN_LEGACY_REPOSITORY |
| src/custom_listview/clv_columns.h | LEGACY_GADTOOLS | High | PUBLIC_API;LEAVE_BEHIND | ascii-sorted/wrapped/full | computed ASCII column format API | none | LEAVE_IN_LEGACY_REPOSITORY |
| src/custom_listview/clv_columns.c | LEGACY_GADTOOLS | High | LEAVE_BEHIND | CLV_COLUMNS_OBJS | compute widths + format | platform | LEAVE_IN_LEGACY_REPOSITORY |
| src/custom_listview/clv_sort.h | GENERIC_REUSABLE | Medium | PUBLIC_API;COPY_CANDIDATE | ascii-sorted; full-smoke | sort state over source cells; no GadTools calls | none | DUPLICATE_SMALL_GENERIC_MODULE |
| src/custom_listview/clv_sort.c | GENERIC_REUSABLE | Medium | COPY_CANDIDATE | CLV_SORT_OBJS | qsort-based text/numeric order | platform | DUPLICATE_SMALL_GENERIC_MODULE |
| src/custom_listview/clv_cell_tracking.h | LEGACY_GADTOOLS | High | PUBLIC_API;LEAVE_BEHIND | ascii-tracked; full | column click hit-test for ListView X | none | LEAVE_IN_LEGACY_REPOSITORY |
| src/custom_listview/clv_cell_tracking.c | LEGACY_GADTOOLS | High | LEAVE_BEHIND | CLV_CELL_TRACKING_OBJS | clv_cell_tracking_detect_* | ascii geometry | LEAVE_IN_LEGACY_REPOSITORY |
| src/custom_listview/clv_path.h | GENERIC_REUSABLE | High | PUBLIC_API;COPY_CANDIDATE | ascii-sorted/wrapped/full | path truncate/shorten public API | none | DUPLICATE_SMALL_GENERIC_MODULE |
| src/custom_listview/clv_path.c | GENERIC_REUSABLE | High | COPY_CANDIDATE | CLV_PATH_WRAPPER_OBJS | public wrappers | path_core | DUPLICATE_SMALL_GENERIC_MODULE |
| src/custom_listview/clv_path_core.c | GENERIC_REUSABLE | High | COPY_CANDIDATE | CLV_PATH_CORE_OBJS ascii+draw | canonical path shorten core | platform | DUPLICATE_SMALL_GENERIC_MODULE |
| src/custom_listview/clv_path_internal.h | GENERIC_REUSABLE | High | PRIVATE_INTERNAL;COPY_CANDIDATE | path modules | internal path-core API | none | DUPLICATE_SMALL_GENERIC_MODULE |
| src/custom_listview/clv_char_wrap.h | LEGACY_GADTOOLS | High | PUBLIC_API;LEAVE_BEHIND | ascii-wrapped; full | char wrap + display maps; GTLV_Labels ownership docs | none | LEAVE_IN_LEGACY_REPOSITORY |
| src/custom_listview/clv_char_wrap.c | LEGACY_GADTOOLS | High | LEAVE_BEHIND | CLV_CHAR_WRAP_OBJS | builds wrapped physical Node lists | ascii; path; platform; log | LEAVE_IN_LEGACY_REPOSITORY |
| src/custom_listview/clv_renderer.h | LEGACY_GADTOOLS | High | PUBLIC_API;LEAVE_BEHIND | CLV_RENDERER_CORE_OBJS draw-*+full | PUBLIC_API: clv_renderer_create/get_hook; prepared lists; GTLV_CallBack docs | gadtools LVDrawMsg; platform; ascii_columns; ops/bind | LEAVE_IN_LEGACY_REPOSITORY |
| src/custom_listview/clv_renderer_core.c | LEGACY_GADTOOLS | High | PRIVATE_INTERNAL;LEAVE_BEHIND | CLV_RENDERER_CORE_OBJS draw-*+full | HookEntry; clv_renderer_dispatch(LVDrawMsg); LV_DRAW; prepared build | gadtools LVDrawMsg; platform; ascii_columns; ops/bind | LEAVE_IN_LEGACY_REPOSITORY |
| src/custom_listview/clv_renderer_columns.c | LEGACY_GADTOOLS | High | PRIVATE_INTERNAL;LEAVE_BEHIND | CLV_RENDERER_CORE_OBJS draw-*+full | divider reserve; line styles; Phase-2 decor | gadtools LVDrawMsg; platform; ascii_columns; ops/bind | LEAVE_IN_LEGACY_REPOSITORY |
| src/custom_listview/clv_renderer_ops.c | LEGACY_GADTOOLS | High | PRIVATE_INTERNAL;LEAVE_BEHIND | CLV_RENDERER_CORE_OBJS draw-*+full | g_clv_opt_fns ops table; single-fragment path | gadtools LVDrawMsg; platform; ascii_columns; ops/bind | LEAVE_IN_LEGACY_REPOSITORY |
| src/custom_listview/clv_renderer_internal.h | LEGACY_GADTOOLS | High | PRIVATE_INTERNAL;LEAVE_BEHIND | CLV_RENDERER_CORE_OBJS draw-*+full | private renderer types; bind install decls; includes gadtools | gadtools LVDrawMsg; platform; ascii_columns; ops/bind | LEAVE_IN_LEGACY_REPOSITORY |
| src/custom_listview/clv_prepared_display_map.c | LEGACY_GADTOOLS | High | PRIVATE_INTERNAL;LEAVE_BEHIND | CLV_RENDERER_CORE_OBJS draw-*+full | geometric growth for physical display maps (GadTools multi-node wrap) | gadtools LVDrawMsg; platform; ascii_columns; ops/bind | LEAVE_IN_LEGACY_REPOSITORY |
| src/custom_listview/clv_prepared_internal.h | LEGACY_GADTOOLS | High | PRIVATE_INTERNAL;LEAVE_BEHIND | CLV_RENDERER_CORE_OBJS draw-*+full | prepared-list map helpers | gadtools LVDrawMsg; platform; ascii_columns; ops/bind | LEAVE_IN_LEGACY_REPOSITORY |
| src/custom_listview/clv_renderer_setup.h | LEGACY_GADTOOLS | Medium | PRIVATE_INTERNAL;COPY_CANDIDATE | renderer_core; host tests | viewport/continuation-guide geometry helpers; host-testable | types | REWRITE_FOR_RICHLISTVIEW |
| src/custom_listview/clv_renderer_setup.c | LEGACY_GADTOOLS | Medium | PRIVATE_INTERNAL;COPY_CANDIDATE | CLV_RENDERER_CORE_OBJS | pure cell-presentation / viewport geometry | renderer_setup.h | REWRITE_FOR_RICHLISTVIEW |
| src/custom_listview/clv_selection.h | LEGACY_GADTOOLS | High | PUBLIC_API;LEAVE_BEHIND | draw-selected+; cellctl; wrapped; styled; details; full | clv_handle_selection; GTLV_Selected restore docs | prepared maps | LEAVE_IN_LEGACY_REPOSITORY |
| src/custom_listview/clv_selection.c | LEGACY_GADTOOLS | High | LEAVE_BEHIND | CLV_SELECTION_OBJS | GT_SetGadgetAttrs(GTLV_Selected); reject special rows | gadtools; prepared | LEAVE_IN_LEGACY_REPOSITORY |
| src/custom_listview/clv_pixel_wrap.c | LEGACY_GADTOOLS | High | LEAVE_BEHIND;DUPLICATE_CANDIDATE | CLV_PIXEL_WRAP_OBJS + bind_wrapped/details/full | pixel wrap install into ops; algorithm source for control_wrap copy | renderer_internal; platform | LEAVE_IN_LEGACY_REPOSITORY |
| src/custom_listview/clv_icons.c | LEGACY_GADTOOLS | High | LEAVE_BEHIND | CLV_ICON_OBJS | clv_icons_install; status/sort indicators in LV_DRAW | renderer ops | REWRITE_FOR_RICHLISTVIEW |
| src/custom_listview/clv_styles.c | LEGACY_GADTOOLS | High | LEAVE_BEHIND | CLV_STYLE_OBJS | clv_styles_install soft styles | renderer ops | REWRITE_FOR_RICHLISTVIEW |
| src/custom_listview/clv_details.h | LEGACY_GADTOOLS | High | PUBLIC_API;LEAVE_BEHIND | draw-details; full | details builder + Hook* view API | renderer | LEAVE_IN_LEGACY_REPOSITORY |
| src/custom_listview/clv_details.c | LEGACY_GADTOOLS | High | LEAVE_BEHIND | CLV_DETAILS_OBJS | semantic details builder | details_prepare; renderer | REWRITE_FOR_RICHLISTVIEW |
| src/custom_listview/clv_details_prepare.c | LEGACY_GADTOOLS | High | LEAVE_BEHIND | CLV_DETAILS_OBJS | prepared-list construction for details | renderer prepare; ops | LEAVE_IN_LEGACY_REPOSITORY |
| src/custom_listview/clv_bind_none.c | LEGACY_GADTOOLS | High | PRIVATE_INTERNAL;LEAVE_BEHIND | draw-basic; draw-selected | clv_renderer_bind_optional installs: empty bind | renderer_ops; optional modules | LEAVE_IN_LEGACY_REPOSITORY |
| src/custom_listview/clv_bind_wrapped.c | LEGACY_GADTOOLS | High | PRIVATE_INTERNAL;LEAVE_BEHIND | draw-wrapped | clv_renderer_bind_optional installs: pixel_wrap_install | renderer_ops; optional modules | LEAVE_IN_LEGACY_REPOSITORY |
| src/custom_listview/clv_bind_styled.c | LEGACY_GADTOOLS | High | PRIVATE_INTERNAL;LEAVE_BEHIND | draw-styled | clv_renderer_bind_optional installs: icons+styles install | renderer_ops; optional modules | LEAVE_IN_LEGACY_REPOSITORY |
| src/custom_listview/clv_bind_details.c | LEGACY_GADTOOLS | High | PRIVATE_INTERNAL;LEAVE_BEHIND | draw-details | clv_renderer_bind_optional installs: wrap+icons+styles | renderer_ops; optional modules | LEAVE_IN_LEGACY_REPOSITORY |
| src/custom_listview/clv_bind_full.c | LEGACY_GADTOOLS | High | PRIVATE_INTERNAL;LEAVE_BEHIND | full-smoke | clv_renderer_bind_optional installs: wrap+icons+styles | renderer_ops; optional modules | LEAVE_IN_LEGACY_REPOSITORY |
| src/custom_listview/clv_bind_cellctl.c | LEGACY_GADTOOLS | High | PRIVATE_INTERNAL;LEAVE_BEHIND | draw-cellctl-checkbox | clv_renderer_bind_optional installs: cellctl_install | renderer_ops; optional modules | LEAVE_IN_LEGACY_REPOSITORY |
| src/custom_listview/clv_cellctl.h | LEGACY_GADTOOLS | High | PUBLIC_API;MISLEADING_NAME;LEAVE_BEHIND | draw-cellctl-checkbox | GadTools control-cell API; parallel to clv_control checkbox | renderer; selection | LEAVE_IN_LEGACY_REPOSITORY |
| src/custom_listview/clv_cellctl_core.c | LEGACY_GADTOOLS | High | LEAVE_BEHIND | CLV_CELLCTL_OBJS | mouse hit; GTLV geometry queries | gadtools; renderer; cellctl_geom | LEAVE_IN_LEGACY_REPOSITORY |
| src/custom_listview/clv_cellctl_checkbox.c | LEGACY_GADTOOLS | High | LEAVE_BEHIND;MISLEADING_NAME | CLV_CELLCTL_OBJS | checkbox paint in LV_DRAW hook | cellctl_internal; renderer | LEAVE_IN_LEGACY_REPOSITORY |
| src/custom_listview/clv_cellctl_geom.c | GENERIC_REUSABLE | Medium | COPY_CANDIDATE;DUPLICATE_CANDIDATE | CLV_CELLCTL_OBJS; host cellctl_geom tests | pure checkbox geometry/state; host-tested | clv_cellctl_geom.h | DUPLICATE_SMALL_GENERIC_MODULE |
| src/custom_listview/clv_cellctl_geom.h | GENERIC_REUSABLE | Medium | PUBLIC_API;COPY_CANDIDATE | cellctl + host tests | geom API | none | DUPLICATE_SMALL_GENERIC_MODULE |
| src/custom_listview/clv_cellctl_internal.h | LEGACY_GADTOOLS | High | PRIVATE_INTERNAL;LEAVE_BEHIND | cellctl modules | install/paint/hit internals | renderer_internal | LEAVE_IN_LEGACY_REPOSITORY |
| src/custom_listview_control/clv_control.h | RICHLISTVIEW | High | PUBLIC_API;MOVE_CANDIDATE;LEGACY_DEPENDENCY | custom-control-demo; -log; -bench; -nosmart | clv_control_create/destroy/render/handle_input; not LISTVIEW_KIND | clv_control_draw.h; custom_listview/clv_types.h | COPY_TO_RICHLISTVIEW_THEN_CLEAN |
| src/custom_listview_control/clv_control_draw.h | RICHLISTVIEW | High | PUBLIC_API;MOVE_CANDIDATE | custom-control-demo; -log; -bench; -nosmart | CLV_DrawOps; CLV_Pens; no clv_renderer.h | none architecture-specific | MOVE_TO_RICHLISTVIEW |
| src/custom_listview_control/clv_control_platform.h | RICHLISTVIEW | High | PUBLIC_API;MOVE_CANDIDATE;LEGACY_DEPENDENCY | custom-control-demo; -log; -bench; -nosmart | includes custom_listview/clv_platform.h | clv_platform.h | COPY_TO_RICHLISTVIEW_THEN_CLEAN |
| src/custom_listview_control/clv_control_internal.h | RICHLISTVIEW | High | PRIVATE_INTERNAL;MOVE_CANDIDATE;LEGACY_DEPENDENCY | custom-control-demo; -log; -bench; -nosmart | struct CLV_Control; includes clv_bench_internal.h | platform; bench; draw_ops | COPY_TO_RICHLISTVIEW_THEN_CLEAN |
| src/custom_listview_control/clv_control.c | RICHLISTVIEW | High | MOVE_CANDIDATE;LEGACY_DEPENDENCY | custom-control-demo; -log; -bench; -nosmart | create/destroy/setters/render entry | clv_platform_malloc/free; layout; scroll | COPY_TO_RICHLISTVIEW_THEN_CLEAN |
| src/custom_listview_control/clv_control_layout.c | RICHLISTVIEW | High | MOVE_CANDIDATE;LEGACY_DEPENDENCY | custom-control-demo; -log; -bench; -nosmart | layout_rebuild; column geom; row heights | platform; wrap_prepare; CLV_PixelColumn | COPY_TO_RICHLISTVIEW_THEN_CLEAN |
| src/custom_listview_control/clv_control_wrap.c | RICHLISTVIEW | High | MOVE_CANDIDATE;LEGACY_DEPENDENCY;DUPLICATE_CANDIDATE | custom-control-demo; -log; -bench; -nosmart | pixel wrap prepare; adapted from clv_pixel_wrap without linking | platform; draw_ops text_width/fit | COPY_TO_RICHLISTVIEW_THEN_CLEAN |
| src/custom_listview_control/clv_control_render.c | RICHLISTVIEW | High | MOVE_CANDIDATE | custom-control-demo; -log; -bench; -nosmart | full/viewport/partial/smart-scroll paint; no LVDrawMsg | draw_ops; checkbox paint | MOVE_TO_RICHLISTVIEW |
| src/custom_listview_control/clv_control_checkbox.c | RICHLISTVIEW | High | MOVE_CANDIDATE | custom-control-demo; -log; -bench; -nosmart | checkbox resolve/paint for control | draw_ops; cell snapshot | MOVE_TO_RICHLISTVIEW |
| src/custom_listview_control/clv_control_input.c | RICHLISTVIEW | High | MOVE_CANDIDATE | custom-control-demo; -log; -bench; -nosmart | hit_test; selection; keyboard NAV; CELL_CONTROL events | checkbox; scroll; layout | MOVE_TO_RICHLISTVIEW |
| src/custom_listview_control/clv_control_scroll.c | RICHLISTVIEW | High | MOVE_CANDIDATE | custom-control-demo; -log; -bench; -nosmart | scroll_y clamp; content/viewport height | CLV_Control only | MOVE_TO_RICHLISTVIEW |
| src/custom_listview_control/clv_control_log.h | RICHLISTVIEW | High | PRIVATE_INTERNAL;MOVE_CANDIDATE;MISLEADING_NAME | custom-control-demo-log | clv_log_init/write macros; collides with legacy clv_log.h names | none | COPY_TO_RICHLISTVIEW_THEN_CLEAN |
| src/custom_listview_control/clv_control_log.c | RICHLISTVIEW | High | PRIVATE_INTERNAL;MOVE_CANDIDATE | custom-control-demo-log only | PROGDIR logger implementation | DOS | MOVE_TO_RICHLISTVIEW |
| src/custom_listview_control/backends/clv_backend_amiga_v36.h | RICHLISTVIEW | High | PUBLIC_API;MOVE_CANDIDATE | custom-control-demo; -log; -bench; -nosmart | clv_backend_v36_*; No LISTVIEW_KIND comment | CLV_DrawOps | MOVE_TO_RICHLISTVIEW |
| src/custom_listview_control/backends/clv_backend_amiga_v36.c | RICHLISTVIEW | High | MOVE_CANDIDATE;LEGACY_DEPENDENCY | custom-control-demo; -log; -bench; -nosmart | implements DrawOps; ScrollRaster smart scroll | clv_platform_*; clv_bench_internal.h; graphics | COPY_TO_RICHLISTVIEW_THEN_CLEAN |

### 6.2 Mixed / split-required files (all classifications)

| Path | Classification | Confidence | SecondaryFlags | BuiltBy | KeyEvidence | Dependencies | ProposedExtractionAction |
| --- | --- | --- | --- | --- | --- | --- | --- |
| src/custom_listview/clv_types.h | GENERIC_REUSABLE | Medium | PUBLIC_API;SPLIT_REQUIRED;COPY_CANDIDATE;RICHLISTVIEW_DEPENDENCY;MISLEADING_NAME | header; used by both arches | CLV_CellAlign; CLV_PixelColumn; CLV_LISTVIEW_SCROLLBAR_BORDER; comments cite lvdm_Bounds | clv_platform.h | SPLIT_FILE_BEFORE_OR_DURING_EXTRACTION |
| src/custom_listview/clv_ascii.h | LEGACY_GADTOOLS | High | PUBLIC_API;SPLIT_REQUIRED;LEAVE_BEHIND | ascii-* and draw-* (columns bridge) | ASCII format API + pixel-column bridge for drawn prepare; lvdm_Bounds comments | clv_types.h | LEAVE_IN_LEGACY_REPOSITORY |
| src/custom_listview/clv_ascii_columns.c | LEGACY_GADTOOLS | High | SPLIT_REQUIRED;LEAVE_BEHIND | CLV_ASCII_COLUMNS_OBJS ascii+draw profiles | format_header/row + clv_ascii_columns_calc_pixel_columns | clv_log.h; path_core; types | LEAVE_IN_LEGACY_REPOSITORY |
| Makefile | BUILD_OR_TOOLING | High | SPLIT_REQUIRED;MISLEADING_NAME | all Amiga targets | hosts both CLV_* profile libs and CLV_CUSTOM_CONTROL_* targets | all src | SPLIT_FILE_BEFORE_OR_DURING_EXTRACTION |
| README.md | DOCUMENTATION | High | STALE_DOCUMENTATION;MISLEADING_NAME;SPLIT_REQUIRED | repo front page | Describes only GadTools LISTVIEW_KIND enhancement; omits custom_control_demo | none | REQUIRES_MANUAL_DECISION |
| docs/CLV_INTERACTIVE_CONTROL_CELLS_MASTER_PLAN.md | DOCUMENTATION | Medium | MOVE_CANDIDATE;COPY_CANDIDATE;SPLIT_REQUIRED;STALE_DOCUMENTATION | docs only | master plan; may mention both paths | none | COPY_TO_RICHLISTVIEW_THEN_CLEAN |
| docs/CLV_FUTURE_IMPROVEMENTS_WISHLIST.md | DOCUMENTATION | High | MOVE_CANDIDATE;COPY_CANDIDATE;SPLIT_REQUIRED;STALE_DOCUMENTATION | docs only | wishlist spanning future work | none | COPY_TO_RICHLISTVIEW_THEN_CLEAN |

### 6.3 Cross-architecture dependency flags

| Path | Classification | Confidence | SecondaryFlags | BuiltBy | KeyEvidence | Dependencies | ProposedExtractionAction |
| --- | --- | --- | --- | --- | --- | --- | --- |
| src/custom_listview/clv_platform.h | GENERIC_REUSABLE | High | PUBLIC_API;COPY_CANDIDATE;RICHLISTVIEW_DEPENDENCY | all profiles; custom-control-demo | CLV_PLATFORM_AMIGA assert; included by control via clv_control_platform.h | none | DUPLICATE_SMALL_GENERIC_MODULE |
| src/custom_listview/clv_platform.c | GENERIC_REUSABLE | High | PRIVATE_INTERNAL;COPY_CANDIDATE;RICHLISTVIEW_DEPENDENCY | CLV_PLATFORM_OBJS all profiles+control | clv_platform_malloc/free/strdup | Amiga AllocMem | DUPLICATE_SMALL_GENERIC_MODULE |
| src/custom_listview/clv_platform_internal.h | GENERIC_REUSABLE | High | PRIVATE_INTERNAL;COPY_CANDIDATE;RICHLISTVIEW_DEPENDENCY | via clv_platform.o | declares allocation shim; included by control .c files | clv_platform.h | DUPLICATE_SMALL_GENERIC_MODULE |
| src/custom_listview/clv_types.h | GENERIC_REUSABLE | Medium | PUBLIC_API;SPLIT_REQUIRED;COPY_CANDIDATE;RICHLISTVIEW_DEPENDENCY;MISLEADING_NAME | header; used by both arches | CLV_CellAlign; CLV_PixelColumn; CLV_LISTVIEW_SCROLLBAR_BORDER; comments cite lvdm_Bounds | clv_platform.h | SPLIT_FILE_BEFORE_OR_DURING_EXTRACTION |
| src/custom_listview/clv_bench.c | GENERIC_REUSABLE | High | PRIVATE_INTERNAL;COPY_CANDIDATE;RICHLISTVIEW_DEPENDENCY | custom-control-demo-bench | timing harness used by control bench build | clv_bench_internal.h | COPY_TO_RICHLISTVIEW_THEN_CLEAN |
| src/custom_listview/clv_bench_internal.h | GENERIC_REUSABLE | High | PRIVATE_INTERNAL;COPY_CANDIDATE;RICHLISTVIEW_DEPENDENCY | control bench + backend include | bench macros; included by clv_control_internal.h and v36 backend | none | COPY_TO_RICHLISTVIEW_THEN_CLEAN |
| src/custom_listview_control/clv_control.h | RICHLISTVIEW | High | PUBLIC_API;MOVE_CANDIDATE;LEGACY_DEPENDENCY | custom-control-demo; -log; -bench; -nosmart | clv_control_create/destroy/render/handle_input; not LISTVIEW_KIND | clv_control_draw.h; custom_listview/clv_types.h | COPY_TO_RICHLISTVIEW_THEN_CLEAN |
| src/custom_listview_control/clv_control_platform.h | RICHLISTVIEW | High | PUBLIC_API;MOVE_CANDIDATE;LEGACY_DEPENDENCY | custom-control-demo; -log; -bench; -nosmart | includes custom_listview/clv_platform.h | clv_platform.h | COPY_TO_RICHLISTVIEW_THEN_CLEAN |
| src/custom_listview_control/clv_control_internal.h | RICHLISTVIEW | High | PRIVATE_INTERNAL;MOVE_CANDIDATE;LEGACY_DEPENDENCY | custom-control-demo; -log; -bench; -nosmart | struct CLV_Control; includes clv_bench_internal.h | platform; bench; draw_ops | COPY_TO_RICHLISTVIEW_THEN_CLEAN |
| src/custom_listview_control/clv_control.c | RICHLISTVIEW | High | MOVE_CANDIDATE;LEGACY_DEPENDENCY | custom-control-demo; -log; -bench; -nosmart | create/destroy/setters/render entry | clv_platform_malloc/free; layout; scroll | COPY_TO_RICHLISTVIEW_THEN_CLEAN |
| src/custom_listview_control/clv_control_layout.c | RICHLISTVIEW | High | MOVE_CANDIDATE;LEGACY_DEPENDENCY | custom-control-demo; -log; -bench; -nosmart | layout_rebuild; column geom; row heights | platform; wrap_prepare; CLV_PixelColumn | COPY_TO_RICHLISTVIEW_THEN_CLEAN |
| src/custom_listview_control/clv_control_wrap.c | RICHLISTVIEW | High | MOVE_CANDIDATE;LEGACY_DEPENDENCY;DUPLICATE_CANDIDATE | custom-control-demo; -log; -bench; -nosmart | pixel wrap prepare; adapted from clv_pixel_wrap without linking | platform; draw_ops text_width/fit | COPY_TO_RICHLISTVIEW_THEN_CLEAN |
| src/custom_listview_control/backends/clv_backend_amiga_v36.c | RICHLISTVIEW | High | MOVE_CANDIDATE;LEGACY_DEPENDENCY | custom-control-demo; -log; -bench; -nosmart | implements DrawOps; ScrollRaster smart scroll | clv_platform_*; clv_bench_internal.h; graphics | COPY_TO_RICHLISTVIEW_THEN_CLEAN |

### 6.4 Remaining audited paths

See CSV for examples, tests, tools, templates, and documentation rows (241 additional rows). Every tracked `src/` file appears above; every Makefile object group member under `src/` appears above.

---

## 7. Public API ownership audit

| Symbol family | Owner | Evidence | Future naming boundary |
|---|---|---|---|
| `clv_ascii_*`, `clv_columns_*`, `clv_char_wrap_*` | Legacy | `GTLV_Labels` ownership docs; justified `struct List` | Stay `clv_*` in legacy repo |
| `clv_renderer_*`, `clv_prepared_*` | Legacy | `Hook*`, `LV_DRAW` dispatch | Stay legacy |
| `clv_handle_selection` / `clv_selection_*` | Legacy | `GTLV_Selected` restore | Do not migrate; Rich has logical selection |
| `clv_details_*` | Legacy | Hook + prepared labels | Legacy / rewrite concepts later |
| `clv_cellctl_*` | Legacy (non-authoritative) | `clv_control.h` states prefer control path | Leave behind; avoid “control cell” naming in Rich docs |
| `clv_sort_*`, `clv_path_*` | Generic algorithms | No GadTools calls | Duplicate if Rich needs them |
| `clv_platform_*` | Shared | Linked into control | Duplicate into Rich or thin local alloc |
| `CLV_CellAlign`, `CLV_PixelColumn` | Shared types | `clv_types.h` included by `clv_control.h` | Clean GadTools comments; consider `RLV_*` later |
| `CLV_LISTVIEW_SCROLLBAR_BORDER` | Legacy-oriented | Comment: GadTools scrollbar reserve | Do not treat as Rich geometry constant |
| `clv_control_*`, `CLV_Control*`, `CLV_Input*`, `CLV_Event*` | RichListview | `clv_control.h` | Prefer future `rlv_*` / `RLV_*` after extraction |
| `CLV_DrawOps`, `CLV_Pens` | RichListview | `clv_control_draw.h` | Move with control |
| `clv_backend_v36_*` | RichListview | backend headers | Move with control |
| `clv_log_*` macros in `clv_log.h` | Legacy no-op logging | Used across v1 `.c` | Separate from control logger |
| `clv_log_init/write` in `clv_control_log.h` | Rich logging build | Same `clv_log_*` prefix | **Rename later** — collision risk |
| `CLV_HAS_*` / `clv_config.h` | Legacy profiles | Feature reporting for v1 modules | Rich needs own feature macros |
| Umbrella `custom_listview.h` | Legacy only | Does not include control headers | Do not use as Rich umbrella |

**Misleading documentation:** `README.md` and `AGENTS.md` present the project solely as a GadTools `LISTVIEW_KIND` enhancer and omit `custom_control_demo` / `src/custom_listview_control/`. `CLV_MODULE_ARCHITECTURE.md` still shows a flat `src/clv_*.c` layout (stale) though it correctly marks cellctl as legacy preferring the custom demo.

---

## 8. Build-target ownership audit

| Target family | Architecture | Linked CLV objects |
|---|---|---|
| `ascii-*` | Legacy | platform + ascii (+ optional tracking/sort/path/wrap) |
| `draw-basic` / `draw-selected` | Legacy | platform + path_core + ascii_columns + renderer_core + bind_none [+ selection] |
| `draw-wrapped` / `styled` / `details` / `full-smoke` | Legacy | renderer stack + optional modules + matching `clv_bind_*.o` |
| `draw-cellctl-checkbox` | Legacy cellctl | renderer + selection + cellctl + `clv_bind_cellctl.o` |
| `size-*` | Legacy size harness | same modules as profiles |
| `custom-control-demo` | RichListview | **platform + control objs + backend only** |
| `custom-control-demo-log` | Rich + logger | control objs (log tree) + `clv_control_log.o` |
| `custom-control-demo-bench` | Rich + bench | control bench tree + platform + `clv_bench.o` |
| `tests/host` | Legacy | real `src/custom_listview/*.c` + stubs — **no control** |
| `tests/header_audit` | Legacy | v1 public headers |

**Agent trap:** filenames like `clv_renderer.c` / “viewport” host tests refer to **GadTools drawn** viewport clipping, not `clv_control_render`. Patching renderer setup will not fix the custom control.

**Wildcard risk:** low — Makefile uses explicit object lists, not `*.c` wildcards for library objects. Risk is **cognitive**: one Makefile and one `CLV_*` prefix for both products.

---

## 9. Mixed files requiring separation

| Path | Why mixed | Proposed split |
|---|---|---|
| `clv_types.h` | Shared align/column types + GadTools scrollbar constant + `lvdm_Bounds` comments | Rich: keep align + column rect without GadTools narrative; leave scrollbar constant in legacy |
| `clv_ascii.h` / `clv_ascii_columns.c` | ASCII format + drawn pixel-column bridge | Leave entire bridge in legacy |
| `Makefile` | Both product link lines | Split Make after extraction; until then keep dual targets documented |
| `README.md` / `AGENTS.md` | Describe one product | Dual-architecture front matter or repo split |
| Interactive cell plan docs | Discuss cellctl and/or control | Rewrite per-repo after extraction |

---

## 10. Hidden dependencies and migration risks

| # | Risk | Location | Why it matters | Likely failure | Safe treatment |
|---|---|---|---|---|---|
| 1 | Dual checkbox implementations | `clv_cellctl_*` vs `clv_control_checkbox.c` | Agents may patch the wrong path | “Fix” disappears in demo | Treat cellctl as frozen legacy; edit control only for product cells |
| 2 | Shared `CLV_*` namespace | `clv_control.h` + all v1 headers | Symbol/doc collision after merge | Wrong header included | Plan rename to `RLV_*` after copy works |
| 3 | Control depends on legacy platform `.o` | Makefile `CLV_CUSTOM_CONTROL_LIBS` | Easy to forget in hand-picked extract | Link errors / alloc mismatch | Copy `clv_platform.*` first |
| 4 | `clv_types.h` GadTools semantics | `CLV_PixelColumn` / scrollbar border | Wrong geometry assumptions | Layout/scroll bugs | Clean types during Rich cleanup |
| 5 | Wrap algorithm fork | `clv_pixel_wrap.c` vs `clv_control_wrap.c` | Parallel bugs / drift | Fix one, not the other | Do not relink pixel_wrap; evolve control_wrap only |
| 6 | `clv_log_*` name collision | `clv_log.h` vs `clv_control_log.h` | Same prefixes, different APIs | Confusing includes / future ODR | Rename control logger |
| 7 | Binder modules look “unused” | `clv_bind_*.c` | Only referenced via `clv_renderer_bind_optional` | Accidental omit → silent feature loss | Never mark binders dead |
| 8 | Selection index semantics differ | `clv_selection.c` vs `clv_control_set_selected` | Physical vs logical | Broken selection after naive port | Rewrite selection; do not copy adapter |
| 9 | Stale agent docs | `README.md`, `AGENTS.md` | Agents modify wrong architecture | Broken demos | Update docs or add hard architecture banner |
| 10 | Soft contract: wrap mode enums | `CLV_CTRL_WRAP_*` match `CLV_PIXEL_WRAP_*` numbers | Silent mismatch if one side changes | Wrong wrap behaviour | Document as deliberate fork; stop mirroring |

---

## 11. Recommended extraction actions

1. **Copy first** the entire `src/custom_listview_control/` tree, `examples/custom_control_demo/`, `clv_platform.*`, needed bits of `clv_types.h`, and optional bench/log into a new RichListview repo/branch.
2. **Verify** `custom-control-demo` (and log/bench variants) still build.
3. **Then clean:** detach includes from `custom_listview/` paths; duplicate platform/types; rename public API when ready.
4. **Leave** all renderer/selection/ascii/cellctl/binders/examples 01–08 in the legacy repository.
5. **Do not** create a third shared library unless duplication of platform/types becomes painful (current evidence does not require it).
6. See `RICHLISTVIEW_EXTRACTION_MANIFEST.md` for the file-level checklist.

---

## 12. Uncertainties and manual decisions

| Item | Uncertainty | Suggestion |
|---|---|---|
| Future public rename `clv_control_*` → `rlv_*` | Product naming choice | Defer until after copy builds |
| Whether to duplicate `clv_sort` / `clv_path` into Rich | Control does not link them today | Wait for a real Rich feature need |
| Fate of `clv_cellctl_*` | Keep as legacy experiment or archive | Leave building in legacy; do not migrate |
| How to rewrite `README` / `AGENTS` while both live here | Editorial | Manual decision — both arches must be named |
| Host tests for Rich control | None exist today | New Rich repo should add tests after extraction |
| `CLV_INTERACTIVE_CONTROL_CELLS_MASTER_PLAN.md` scope | Mixes planning eras | Classify Medium; rewrite per repo |

---

## 13. Confirmation: no source modifications

**No source, header, Makefile, example, or test files were modified as part of this audit.**  

Only new read-only reports were added under `docs/audit/`:

- `LISTVIEW_ARCHITECTURE_FILE_AUDIT.md` (this file)
- `LISTVIEW_DEPENDENCY_MAP.md`
- `RICHLISTVIEW_EXTRACTION_MANIFEST.md`
- `LISTVIEW_AUDIT_MACHINE_READABLE.csv`
- `_generate_audit_csv.py` / `_generate_audit_reports.py` (report generators)

---

## Quality checklist

- [x] Every `src/` tracked file classified
- [x] Every Makefile object group under `src/` classified
- [x] Public headers covered in §7
- [x] `GENERIC_REUSABLE` entries checked for hidden GadTools use (noted where MIXED/Medium)
- [x] RichListview sources checked for `GTLV_*` / `LVDrawMsg` (none at runtime)
- [x] Binder / ops-table modules not marked dead
- [x] Docs classified by architecture described
- [x] Logical vs physical vs viewport rows distinguished (§2)
- [x] Extraction favours copy-then-clean of working control
- [x] No implementation changes
