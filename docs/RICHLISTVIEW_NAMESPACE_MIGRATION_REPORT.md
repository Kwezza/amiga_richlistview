# RichListview Namespace Migration Report

**Date:** 2026-07-31  
**Principle:** Mechanical, comprehensive, behaviour-preserving rename  
**Status:** Complete (compile/link verified for all variants)

---

## 1. Summary

Inherited `CLV_*` / `clv_control_*` naming was renamed to RichListview-specific
`RLV_*` / `rlv_*` across active source, headers, the Makefile, the demo, and
current documentation. No API redesign, layout change, or intentional behaviour
change was made.

Preferred public include:

```c
#include "rich_listview/rich_listview.h"
```

---

## 2. Files renamed

| Old | New |
|---|---|
| `src/rich_listview/clv_control.h` | `src/rich_listview/rich_listview.h` |
| `src/rich_listview/clv_control_draw.h` | `src/rich_listview/rlv_draw.h` |
| `src/rich_listview/clv_control_platform.h` | `src/rich_listview/rlv_platform_api.h` |
| `src/rich_listview/clv_control_internal.h` | `src/rich_listview/rlv_internal.h` |
| `src/rich_listview/clv_control.c` | `src/rich_listview/rlv.c` |
| `src/rich_listview/clv_control_layout.c` | `src/rich_listview/rlv_layout.c` |
| `src/rich_listview/clv_control_wrap.c` | `src/rich_listview/rlv_wrap.c` |
| `src/rich_listview/clv_control_render.c` | `src/rich_listview/rlv_render.c` |
| `src/rich_listview/clv_control_checkbox.c` | `src/rich_listview/rlv_checkbox.c` |
| `src/rich_listview/clv_control_input.c` | `src/rich_listview/rlv_input.c` |
| `src/rich_listview/clv_control_scroll.c` | `src/rich_listview/rlv_scroll.c` |
| `src/rich_listview/clv_control_log.h` | `src/rich_listview/rlv_log.h` |
| `src/rich_listview/clv_control_log.c` | `src/rich_listview/rlv_log.c` |
| `src/rich_listview/clv_platform.h` | `src/rich_listview/rlv_platform.h` |
| `src/rich_listview/clv_platform.c` | `src/rich_listview/rlv_platform.c` |
| `src/rich_listview/clv_platform_internal.h` | `src/rich_listview/rlv_platform_internal.h` |
| `src/rich_listview/clv_types.h` | `src/rich_listview/rlv_types.h` |
| `src/rich_listview/clv_bench.c` | `src/rich_listview/rlv_bench.c` |
| `src/rich_listview/clv_bench_internal.h` | `src/rich_listview/rlv_bench_internal.h` |
| `src/rich_listview/backends/clv_backend_amiga_v36.h` | `src/rich_listview/backends/rlv_backend_amiga_v36.h` |
| `src/rich_listview/backends/clv_backend_amiga_v36.c` | `src/rich_listview/backends/rlv_backend_amiga_v36.c` |
| `examples/custom_control_demo/` | `examples/rich_listview_demo/` |

No duplicate compatibility headers were retained.

---

## 3. Headers renamed

| Old guard | New guard |
|---|---|
| `CLV_CONTROL_H` | `RICH_LISTVIEW_H` |
| `CLV_CONTROL_DRAW_H` | `RLV_DRAW_H` |
| `CLV_CONTROL_PLATFORM_H` | `RLV_PLATFORM_API_H` |
| `CLV_CONTROL_INTERNAL_H` | `RLV_INTERNAL_H` |
| `CLV_CONTROL_LOG_H` | `RLV_LOG_H` |
| `CLV_PLATFORM_H` | `RLV_PLATFORM_H` |
| `CLV_PLATFORM_INTERNAL_H` | `RLV_PLATFORM_INTERNAL_H` |
| `CLV_TYPES_H` | `RLV_TYPES_H` |
| `CLV_BENCH_INTERNAL_H` | `RLV_BENCH_INTERNAL_H` |
| `CLV_BACKEND_AMIGA_V36_H` | `RLV_BACKEND_AMIGA_V36_H` |

---

## 4. Public symbols renamed (compact mapping)

| Old | New |
|---|---|
| `CLV_Control` | `RLV_Control` |
| `CLV_ControlConfig` | `RLV_Config` |
| `CLV_ControlColumn` | `RLV_Column` |
| `CLV_ControlRow` | `RLV_Row` |
| `CLV_ControlCell` | `RLV_Cell` |
| `CLV_ControlWrapMode` | `RLV_WrapMode` |
| `CLV_ControlRowDividerStyle` | `RLV_RowDividerStyle` |
| `CLV_InputEvent` / `CLV_InputType` | `RLV_InputEvent` / `RLV_InputType` |
| `CLV_Event` / `CLV_EventType` | `RLV_Event` / `RLV_EventType` |
| `CLV_CellControlAction` | `RLV_CellControlAction` |
| `CLV_DrawOps` / `CLV_Pens` | `RLV_DrawOps` / `RLV_Pens` |
| `CLV_PixelColumn` / `CLV_CellAlign` | `RLV_PixelColumn` / `RLV_CellAlign` |
| `CLV_FontMetrics` | `RLV_FontMetrics` |
| `CLV_CTRL_*` | `RLV_*` (e.g. `RLV_COL_TYPE_CHECKBOX`) |
| `CLV_EVENT_*` | `RLV_EVENT_*` |
| `CLV_INPUT_*` | `RLV_INPUT_*` |
| `CLV_ENABLE_*` | `RLV_ENABLE_*` |
| `CLV_RENDER_*` | `RLV_RENDER_*` |
| `clv_control_create` | `rlv_create` |
| `clv_control_destroy` | `rlv_destroy` |
| `clv_control_set_bounds` | `rlv_set_bounds` |
| `clv_control_set_rows` / `set_columns` | `rlv_set_rows` / `rlv_set_columns` |
| `clv_control_set_selected` / `get_selected` | `rlv_set_selected` / `rlv_get_selected` |
| `clv_control_set_scroll_y` / `get_scroll_y` | `rlv_set_scroll_y` / `rlv_get_scroll_y` |
| `clv_control_render` | `rlv_render` |
| `clv_control_render_logical_rows` | `rlv_render_logical_rows` |
| `clv_control_render_scrolled` | `rlv_render_scrolled` |
| `clv_control_handle_input` | `rlv_handle_input` |
| `clv_control_make_visible` | `rlv_make_visible` |
| `clv_control_set_checkbox_value` | `rlv_set_checkbox_value` |
| `clv_control_set_keyboard_enabled` | `rlv_set_keyboard_enabled` |

Struct field order, types, signedness, and ownership rules were preserved.

---

## 5. Internal symbols renamed

| Old prefix / type | New |
|---|---|
| `clv_control_*` internals | `rlv_*` (e.g. `rlv_layout_rebuild`, `rlv_wrap_prepare`) |
| `clv_ctrl_*` helpers | `rlv_*` (e.g. `rlv_checkbox_paint`, `rlv_fill_cell_event`) |
| `CLV_RowLayout` | `RLV_RowLayout` |
| `CLV_ControlFrag` | `RLV_Frag` |
| `CLV_ControlCellWrap` | `RLV_CellWrap` |
| `CLV_CtrlLayoutSnapshot` | `RLV_LayoutSnapshot` |
| `CLV_ControlLineStyle` / `CLV_CTRL_LINE_*` | `RLV_LineStyle` / `RLV_LINE_*` |
| `g_clv_bench` | `g_rlv_bench` |

---

## 6. Backend / platform / log / benchmark symbols

| Old | New |
|---|---|
| `CLV_BackendV36` | `RLV_BackendV36` |
| `clv_backend_v36_*` | `rlv_backend_v36_*` |
| `clv_v36_*` | `rlv_v36_*` |
| `clv_platform_*` | `rlv_platform_*` |
| `CLV_PLATFORM_AMIGA` / `CLV_TARGET_AMIGAOS` | `RLV_PLATFORM_AMIGA` / `RLV_TARGET_AMIGAOS` |
| `clv_log_*` / `CLV_LOG` / `CLV_LOGF` | `rlv_log_*` / `RLV_LOG` / `RLV_LOGF` |
| `CLV_LOG_PATH` → `"PROGDIR:clv_control.log"` | `RLV_LOG_PATH` → `"PROGDIR:rlv.log"` |
| `clv_bench_*` / `CLV_BENCH_*` / `CLV_Bench*` | `rlv_bench_*` / `RLV_BENCH_*` / `RLV_Bench*` |
| Benchmark report paths | `PROGDIR:rlv_benchmark.txt` (+ debug log rename) |

Compile-time gating for logging (`RLV_ENABLE_LOGGING`) and benchmarks
(`RLV_ENABLE_BENCHMARKS`) is preserved.

---

## 7. Makefile target changes

| Old | New |
|---|---|
| `custom-control-demo` | `rich-listview-demo` |
| `custom-control-demo-log` | `rich-listview-demo-log` |
| `custom-control-demo-bench` | `rich-listview-demo-bench` |
| `custom-control-demo-nosmart` | `rich-listview-demo-nosmart` |

Defines: `-DRLV_PLATFORM_AMIGA=1`, `-DRLV_ENABLE_SMART_SCROLL=…`, etc.

**Required fix applied:** nosmart now uses an isolated object tree
`build/rich_listview_nosmart/` and a distinct binary
`bin/rich-listview-demo-nosmart`, so smart-on and smart-off builds do not share
objects.

VBCC `+aos68k`, `-cpu=68000`, `-O2 -size`, and `-lamiga -lauto` are unchanged.

---

## 8. Example changes

- Directory: `examples/rich_listview_demo/`
- Includes, types, call sites, macros, comments, README, and binary names updated
- Window title string: `"RichListview (Phase E4)"` (was `"CLV Custom Control (Phase E4)"`)
- Demo behaviour and layout intentionally unchanged

---

## 9. Compatibility policy

No external consumers of a published RichListview API were found.

Therefore:

- no `CLV_*` aliases;
- no wrapper functions;
- no transitional duplicate headers.

Historical audit docs under `docs/audit/` and `docs/CLV_*.md` keep extraction-era
names. The creation report retains extraction-time filenames and adds a pointer
to this migration report.

---

## 10. Build results

Compiler: VBCC `vc` (`C:\VBCC\bin\vc.exe`), host Windows.

| Target | Result | Warnings |
|---|---|---|
| `rich-listview-demo` | success | none observed |
| `rich-listview-demo-log` | success | none observed |
| `rich-listview-demo-bench` | success | none observed |
| `rich-listview-demo-nosmart` | success | none observed |

Baseline object list (smart on):

```text
build/rich_listview/rlv_platform.o
build/rich_listview/rlv.o
build/rich_listview/rlv_layout.o
build/rich_listview/rlv_wrap.o
build/rich_listview/rlv_render.o
build/rich_listview/rlv_checkbox.o
build/rich_listview/rlv_input.o
build/rich_listview/rlv_scroll.o
build/rich_listview/backends/rlv_backend_amiga_v36.o
build/examples/rich_listview_demo.o
```

Feature defines: `-DRLV_PLATFORM_AMIGA=1`, `-DRLV_ENABLE_SMART_SCROLL=1|0`,
optional `-DRLV_ENABLE_LOGGING` / `-DRLV_ENABLE_BENCHMARKS`.

### Build defect fixed during migration

A blind substring map `clv_bench.c` → `rlv_bench.c` corrupted identifiers such as
`g_clv_bench.count_only` into `g_rlv_bench.count_only` while leaving other
`g_clv_bench` references untouched. Fixed by unifying `g_clv_bench` →
`g_rlv_bench`. No API redesign.

---

## 11. Executable size comparison

Pre-rename baselines from `docs/RICHLISTVIEW_REPOSITORY_CREATION_REPORT.md`:

| Variant | Pre (bytes) | Post (bytes) | Δ |
|---|---:|---:|---:|
| Baseline | 44824 | 44816 | −8 |
| Logging | 60092 | 59940 | −152 |
| Benchmark | 61472 | 61464 | −8 |
| No smart scroll | 43564 | 43556 | −8 |

Interpretation: deltas match shorter embedded string literals (window title,
log/benchmark paths, target/profile names). No evidence of code-path removal
beyond the expected smart-scroll compile difference (baseline vs nosmart still
≈ +1260 bytes).

---

## 12. Runtime verification

**Not performed on this host.** Windows cannot execute the Amiga binaries.

Status: compile/link equivalence only. Do not claim window/render/input
runtime identity until WinUAE or Amiga hardware re-runs
`bin/rich-listview-demo` (and optionally log/bench variants).

Prior extraction runtime success on Amiga/emulator still applies to the
pre-rename binary; this migration did not intentionally alter behaviour.

---

## 13. Residual old-name search results

Searched active code/build/docs surfaces (`src/`, `examples/`, `Makefile`,
`README.md`, `agents.md`, `.github/copilot-instructions.md`):

| Pattern | Active hits | Notes |
|---|---:|---|
| `clv_control_` | 0 | — |
| `clv_backend_` | 0 | — |
| `clv_platform_` | 0 | — |
| `clv_bench_` | 0 | — |
| `clv_log_` | 0 | — |
| `custom-control-demo` | 0 | — |
| `custom_control_demo` | 0 | — |
| `CLV_` in `*.c`/`*.h`/`Makefile` | 1 | Comment: historical `CLV_PIXEL_WRAP_*` |
| `GTLV_` / `LVDrawMsg` | comments only | Agent “do not reintroduce” warnings |
| `LISTVIEW_KIND` | comments only | Boundary documentation |
| `clv_renderer_` / `clv_selection_` / `clv_cellctl_` | comments only | Excluded legacy references |

Historical reports under `docs/audit/`, `docs/CLV_*.md`, and
`docs/historical/` intentionally retain old names.

---

## 14. Deviations from the requested naming scheme

1. Public type shortenings followed the prompt examples
   (`CLV_ControlConfig` → `RLV_Config`, etc.).
2. `CLV_CTRL_*` macros became `RLV_*` (stripped `CTRL_`), matching the prompt.
3. `clv_control_platform.h` retained as thin wrapper `rlv_platform_api.h`
   (not removed; still included by `rlv_draw.h`).
4. Log path string shortened to `PROGDIR:rlv.log` (mechanical path rename).
5. One historical comment still mentions `CLV_PIXEL_WRAP_*` / `clv_renderer.h`
   as the v1 legacy constant family.

---

## 15. Deferred cleanup

Unchanged from product backlog (not done here):

- Trim GadTools-oriented leftovers in `rlv_types.h` if unused
- Public vs private header packaging review
- RichListview-specific tests
- Integration guide polish beyond the demo README
- Optional sort/path helper decisions
- Shared-library / ABI guarantees

---

## 16. Behaviour confirmation

Behaviour was **not intentionally changed**. This migration renamed symbols,
files, macros, Makefile targets/defines, and documentation only. Structure
layouts, event semantics, paint/scroll/input logic, ownership rules, and
68000 / AmigaOS 2.x–3.x targeting are preserved as source text aside from
identifier and string-literal renames.
