#!/usr/bin/env python3
"""Generate the three markdown audit reports from the CSV inventory."""
from __future__ import annotations

import csv
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
AUDIT = Path(__file__).resolve().parent
CSV_PATH = AUDIT / "LISTVIEW_AUDIT_MACHINE_READABLE.csv"


def load_rows():
    with CSV_PATH.open(encoding="utf-8") as f:
        return list(csv.DictReader(f))


def md_escape(s: str) -> str:
    return s.replace("|", "\\|").replace("\n", " ")


def table(rows, cols=None):
    if cols is None:
        cols = [
            "Path",
            "Classification",
            "Confidence",
            "SecondaryFlags",
            "BuiltBy",
            "KeyEvidence",
            "Dependencies",
            "ProposedExtractionAction",
        ]
    lines = [
        "| " + " | ".join(cols) + " |",
        "| " + " | ".join(["---"] * len(cols)) + " |",
    ]
    for r in rows:
        lines.append("| " + " | ".join(md_escape(r.get(c, "")) for c in cols) + " |")
    return "\n".join(lines)


def write_main(rows):
    counts = Counter(r["Classification"] for r in rows)
    split = [r for r in rows if "SPLIT_REQUIRED" in r["SecondaryFlags"]]
    cross = [
        r
        for r in rows
        if "LEGACY_DEPENDENCY" in r["SecondaryFlags"]
        or "RICHLISTVIEW_DEPENDENCY" in r["SecondaryFlags"]
    ]
    src = [r for r in rows if r["Path"].startswith("src/")]
    text = f"""# ListView Architecture File Audit

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

**Audited rows:** {len(rows)} (see CSV). AutoDocs corpus summarised as one exclusion row; host `.exe` binaries excluded as artefacts.

| Classification | Count |
|---|---:|
| LEGACY_GADTOOLS | {counts.get('LEGACY_GADTOOLS', 0)} |
| RICHLISTVIEW | {counts.get('RICHLISTVIEW', 0)} |
| GENERIC_REUSABLE | {counts.get('GENERIC_REUSABLE', 0)} |
| DEMO_OR_TEST | {counts.get('DEMO_OR_TEST', 0)} |
| BUILD_OR_TOOLING | {counts.get('BUILD_OR_TOOLING', 0)} |
| DOCUMENTATION | {counts.get('DOCUMENTATION', 0)} |
| UNCERTAIN | {counts.get('UNCERTAIN', 0)} |

| Metric | Count |
|---|---:|
| Files with `SPLIT_REQUIRED` | {len(split)} |
| Files with cross-architecture dependency flags | {len(cross)} |

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

The authoritative machine-readable table is `LISTVIEW_AUDIT_MACHINE_READABLE.csv` ({len(rows)} rows). Below: **all `src/` implementation files** (required completeness for product code), then notable mixed/build/demo entries. Remaining demo/test/doc rows are identical in the CSV.

### 6.1 All `src/` files

{table(src)}

### 6.2 Mixed / split-required files (all classifications)

{table(split)}

### 6.3 Cross-architecture dependency flags

{table(cross)}

### 6.4 Remaining audited paths

See CSV for examples, tests, tools, templates, and documentation rows ({len(rows) - len(src)} additional rows). Every tracked `src/` file appears above; every Makefile object group member under `src/` appears above.

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
"""
    (AUDIT / "LISTVIEW_ARCHITECTURE_FILE_AUDIT.md").write_text(text, encoding="utf-8")
    print("wrote LISTVIEW_ARCHITECTURE_FILE_AUDIT.md")


def write_dep_map(rows):
    text = """# ListView Dependency Map

**Companion to:** `LISTVIEW_ARCHITECTURE_FILE_AUDIT.md`  
**Date:** 2026-07-31

---

## 1. Module-level dependency list

### Legacy root modules

| Module | Objects | Depends on |
|---|---|---|
| platform | `clv_platform.o` | Amiga AllocMem |
| ascii_formatter | `clv_ascii_formatter.o` | platform, exec list, log |
| ascii_columns | `clv_ascii_columns.o` | platform, types, path_core, log |
| columns | `clv_columns.o` | platform |
| sort | `clv_sort.o` | platform |
| cell_tracking | `clv_cell_tracking.o` | ascii geometry |
| path_core / path | `clv_path_core.o`, `clv_path.o` | platform |
| char_wrap | `clv_char_wrap.o` | ascii, path, platform |
| renderer_core | `clv_renderer_core.o`, `prepared_display_map.o`, `renderer_columns.o`, `renderer_ops.o`, `renderer_setup.o` | platform, ascii_columns, gadtools LVDrawMsg, ops table |
| bind_* | one of `clv_bind_*.o` | installs into `g_clv_opt_fns` |
| selection | `clv_selection.o` | prepared maps, `GTLV_Selected` |
| pixel_wrap | `clv_pixel_wrap.o` | renderer internals (via install) |
| icons / styles | `clv_icons.o`, `clv_styles.o` | renderer ops |
| details | `clv_details.o`, `clv_details_prepare.o` | renderer prepare/ops |
| cellctl | `clv_cellctl_core.o`, `checkbox.o`, `geom.o` | renderer, selection, gadget geometry |

### RichListview root modules

| Module | Objects | Depends on |
|---|---|---|
| control core | `clv_control.o` | platform alloc, layout, scroll, render |
| layout | `clv_control_layout.o` | platform, wrap, `CLV_PixelColumn` |
| wrap | `clv_control_wrap.o` | platform, draw_ops measure (forked algo) |
| render | `clv_control_render.o` | draw_ops, checkbox |
| checkbox | `clv_control_checkbox.o` | draw_ops |
| input | `clv_control_input.o` | checkbox, scroll, layout |
| scroll | `clv_control_scroll.o` | control state only |
| backend v36 | `clv_backend_amiga_v36.o` | platform, graphics, optional bench |
| log (optional) | `clv_control_log.o` | DOS |

### Generic / shared modules

| Module | Consumers |
|---|---|
| `clv_platform*` | Legacy all profiles + Rich control |
| `clv_types.h` (`CLV_CellAlign`, `CLV_PixelColumn`) | Legacy + Rich public header |
| `clv_bench*` | Rich bench build (optional) |
| `clv_compiler.h`, `clv_sdk_compat.h` | Legacy (and potentially Rich later) |
| `clv_sort*`, `clv_path*` | Legacy only today; algorithmically generic |
| `clv_cellctl_geom*` | Legacy cellctl + host tests; parallel to control checkbox geom |

---

## 2. Cross-architecture edges

```text
custom_listview_control  --include-->  custom_listview/clv_types.h
custom_listview_control  --include-->  custom_listview/clv_platform.h
custom_listview_control  --include-->  custom_listview/clv_platform_internal.h
custom_listview_control  --include-->  custom_listview/clv_bench_internal.h
custom_listview_control  --link----->  clv_platform.o
custom_listview_control  --link----->  clv_bench.o          (bench only)
clv_control_wrap.c       --forked--->  algorithm from clv_pixel_wrap.c (no link)
CLV_CTRL_WRAP_* enums    --mirror--->  CLV_PIXEL_WRAP_* numeric values (soft)
```

**No** edge from control to renderer, selection, ascii formatter, binders, or cellctl objects.

**Reverse edges:** none required — legacy does not call into `custom_listview_control/`.

---

## 3. Function-pointer / binder dependencies

Legacy optional features are **not** hard-linked from renderer core:

```text
clv_renderer_create
  → clv_renderer_bind_optional()   /* defined in exactly one clv_bind_*.o */
       → clv_pixel_wrap_install / clv_icons_install / clv_styles_install / clv_cellctl_install
  → g_clv_opt_fns (clv_renderer_ops.o) holds function pointers
```

Marking any `clv_bind_*.c` as dead because nothing “calls it by name” from other `.c` files is incorrect — the linker resolves `clv_renderer_bind_optional`.

Rich draw path uses an explicit `CLV_DrawOps` table supplied at `clv_control_create` (backend installs concrete function pointers).

---

## 4. Suspected dead / unreferenced modules

| Item | Assessment |
|---|---|
| `examples/*_stub.c` | Likely historical stubs — verify Makefile before delete |
| Host `.exe` under `tests/host/` | Build artefacts; not source |
| `clv_cellctl_*` | **Not dead** — linked by `draw-cellctl-checkbox`; product-superseded by control |
| Binders | **Not dead** — required by drawn profiles |
| `clv_log.h` | Header-only; used widely as no-op macros |

---

## 5. Mermaid graphs

### 5.1 Architecture overview

```mermaid
flowchart TB
  subgraph AppLegacy[Legacy client examples]
    CG[CreateGadget LISTVIEW_KIND]
    GT[GT_SetGadgetAttrs GTLV_Labels / Selected]
  end

  subgraph Legacy[src/custom_listview]
    ASCII[ascii_formatter / char_wrap]
    REND[renderer_core + prepared maps]
    BIND[clv_bind_*.o]
    SEL[selection adapter]
    CELLCTL[cellctl_*]
    PLAT[clv_platform]
    TYPES[clv_types]
  end

  subgraph Rich[src/custom_listview_control]
    CTRL[clv_control_*]
    BE[backend_amiga_v36]
  end

  CG --> ASCII
  CG --> REND
  GT --> ASCII
  GT --> REND
  REND --> BIND
  GT --> SEL
  REND --> CELLCTL
  ASCII --> PLAT
  REND --> PLAT
  CTRL --> PLAT
  CTRL --> TYPES
  CTRL --> BE
```

### 5.2 RichListview internal call graph

```mermaid
flowchart LR
  create[clv_control_create] --> layout[layout_rebuild]
  layout --> wrap[wrap_prepare]
  setb[set_bounds] --> layout
  render[clv_control_render] --> paint[render_full / viewport / scrolled]
  paint --> cb[checkbox_paint]
  paint --> ops[CLV_DrawOps]
  input[handle_input] --> sel[set_selected / make_visible]
  input --> scroll[set_scroll_y]
  input --> cb
  ops --> be[backend_v36]
```

### 5.3 Legacy drawn paint path

```mermaid
flowchart LR
  gt[GadTools LISTVIEW] -->|LV_DRAW| hook[clv_renderer_dispatch]
  hook --> ops[g_clv_opt_fns]
  bind[clv_bind_*.o] -->|install| ops
  hook --> prep[CLV_RenderNode / prepared]
```

---

## 6. Profile → object quick reference

See `docs/CLV_BUILD_PROFILES.md` and root `Makefile` lines defining `CLV_ASCII_*_LIBS`, `CLV_DRAW_*_LIBS`, `CLV_FULL_LIBS`, `CLV_CUSTOM_CONTROL_LIBS`. Prefer the Makefile when docs omit `path_core` on ascii-minimal.
"""
    (AUDIT / "LISTVIEW_DEPENDENCY_MAP.md").write_text(text, encoding="utf-8")
    print("wrote LISTVIEW_DEPENDENCY_MAP.md")


def write_manifest(rows):
    def by_action(action):
        return [r for r in rows if r["ProposedExtractionAction"] == action]

    def bullet_paths(action, predicate=None):
        items = by_action(action)
        if predicate:
            items = [r for r in items if predicate(r)]
        return "\n".join(f"- `{r['Path']}` — {r['KeyEvidence'][:120]}" for r in items)

    text = f"""# RichListview Extraction Manifest

**Status:** Plan only — do **not** execute extraction from this document alone.  
**Date:** 2026-07-31  
**Principle:** Copy the working custom-control closure first; clean after the demo builds.

---

## Copy first into RichListview

These files are required for the currently working control (or its documented demo/bench/log variants):

### Control package (entire tree)

{bullet_paths('MOVE_TO_RICHLISTVIEW', lambda r: r['Path'].startswith('src/custom_listview_control'))}
{bullet_paths('COPY_TO_RICHLISTVIEW_THEN_CLEAN', lambda r: r['Path'].startswith('src/custom_listview_control'))}

### Shared foundations to copy then localise

{bullet_paths('DUPLICATE_SMALL_GENERIC_MODULE', lambda r: 'platform' in r['Path'] or r['Path'].endswith('clv_types.h') or 'bench' in r['Path'] or 'compiler' in r['Path'] or 'sdk_compat' in r['Path'] or 'exec_list' in r['Path'])}
{bullet_paths('COPY_TO_RICHLISTVIEW_THEN_CLEAN', lambda r: r['Path'].startswith('src/custom_listview/'))}

### Demo and control docs

{bullet_paths('COPY_TO_RICHLISTVIEW_THEN_CLEAN', lambda r: r['Path'].startswith('examples/custom_control_demo') or 'CUSTOM_CONTROL' in r['Path'] or 'CONTROL_CELLS' in r['Path'] or 'CELL_CONTROL' in r['Path'] or 'BENCHMARK' in r['Path'] or r['Path'].startswith('docs/audit/'))}

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

{bullet_paths('MOVE_TO_RICHLISTVIEW')}

(These have no meaningful legacy consumers.)

---

## Duplicate as small generic modules

Prefer duplication over a third repository:

{bullet_paths('DUPLICATE_SMALL_GENERIC_MODULE')}

---

## Split before migration

{bullet_paths('SPLIT_FILE_BEFORE_OR_DURING_EXTRACTION')}

Additional editorial splits (docs): `README.md`, `AGENTS.md`, interactive-cell master plans — see audit §9.

---

## Rewrite rather than copy

{bullet_paths('REWRITE_FOR_RICHLISTVIEW')}

Especially: do **not** copy `clv_selection.c` (physical `GTLV_Selected` adapter). Rich selection is already implemented in `clv_control_input.c`.

---

## Leave in legacy repository

Primary rule: everything under legacy ASCII/drawn/cellctl profiles, examples `00`–`08`, `size_compare`, host/header tests, v1 integration docs, binders, renderer.

Count of `LEAVE_IN_LEGACY_REPOSITORY` rows in CSV: **{len(by_action('LEAVE_IN_LEGACY_REPOSITORY'))}**.

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
"""
    (AUDIT / "RICHLISTVIEW_EXTRACTION_MANIFEST.md").write_text(text, encoding="utf-8")
    print("wrote RICHLISTVIEW_EXTRACTION_MANIFEST.md")


def main():
    rows = load_rows()
    write_main(rows)
    write_dep_map(rows)
    write_manifest(rows)


if __name__ == "__main__":
    main()
