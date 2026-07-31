# RichListview Repository Creation Report

**Date:** 2026-07-31  
**Principle followed:** Copy first → Build unchanged → Clean later  
**Status:** Baseline extraction succeeded (compile/link verified)

> **Namespace note (post-extraction):** After this report was written, the
> active API was mechanically renamed from inherited `CLV_*` /
> `clv_control_*` names to `RLV_*` / `rlv_*`. See
> `docs/RICHLISTVIEW_NAMESPACE_MIGRATION_REPORT.md`. Filenames and
> symbols listed below are the **extraction-time** names.

---

## New repository path

`C:\Amiga\Programming\RichListview`

## Source repository path

`C:\Amiga\Programming\amiga_custom_listview`

---

## Files copied

### Control package → `src/rich_listview/`

From `src/custom_listview_control/`:

| Source | Destination |
|---|---|
| `clv_control.h` | `src/rich_listview/clv_control.h` |
| `clv_control_draw.h` | `src/rich_listview/clv_control_draw.h` |
| `clv_control_platform.h` | `src/rich_listview/clv_control_platform.h` |
| `clv_control_internal.h` | `src/rich_listview/clv_control_internal.h` |
| `clv_control.c` | `src/rich_listview/clv_control.c` |
| `clv_control_layout.c` | `src/rich_listview/clv_control_layout.c` |
| `clv_control_wrap.c` | `src/rich_listview/clv_control_wrap.c` |
| `clv_control_render.c` | `src/rich_listview/clv_control_render.c` |
| `clv_control_checkbox.c` | `src/rich_listview/clv_control_checkbox.c` |
| `clv_control_input.c` | `src/rich_listview/clv_control_input.c` |
| `clv_control_scroll.c` | `src/rich_listview/clv_control_scroll.c` |
| `clv_control_log.h` | `src/rich_listview/clv_control_log.h` |
| `clv_control_log.c` | `src/rich_listview/clv_control_log.c` |
| `backends/clv_backend_amiga_v36.h` | `src/rich_listview/backends/clv_backend_amiga_v36.h` |
| `backends/clv_backend_amiga_v36.c` | `src/rich_listview/backends/clv_backend_amiga_v36.c` |

### Shared foundations → `src/rich_listview/`

From `src/custom_listview/`:

| File | Notes |
|---|---|
| `clv_platform.h` | Required |
| `clv_platform.c` | Required |
| `clv_platform_internal.h` | Required |
| `clv_types.h` | Required (preserved as-is aside from include path) |
| `clv_bench.c` | Optional; for `-bench` target |
| `clv_bench_internal.h` | Optional; included by control internals / backend |

### Demo

- `examples/custom_control_demo/main.c`
- `examples/custom_control_demo/README.md`

### Audit reports

Entire `docs/audit/` tree (including generators).

### Custom-control documentation

Copied to `docs/`:

- `CLV_CUSTOM_CONTROL_DESIGN_AND_IMPLEMENTATION_PLAN.md`
- `CLV_CUSTOM_CONTROL_KEYBOARD_NAVIGATION_PLAN.md`
- `CLV_CUSTOM_CONTROL_PHASE1_AUDIT.md`
- `CLV_CELL_CONTROL_EVENT_NOTIFICATION_PLAN.md`
- `CLV_CONTROL_CELLS_DEVELOPER_LOG.md`
- `CLV_BENCHMARK_HANDOFF.md`
- `CLV_BENCHMARK_IMPLEMENTATION_REPORT.md`

### Mixed-era docs → `docs/historical/`

- `CLV_INTERACTIVE_CONTROL_CELLS_MASTER_PLAN.md`
- `CLV_FUTURE_IMPROVEMENTS_WISHLIST.md`
- `docs/historical/README.md` (note: mixed-era planning)

### New repository files

- `README.md`
- `Makefile`
- `.gitignore`
- `docs/RICHLISTVIEW_REPOSITORY_CREATION_REPORT.md` (this file)

---

## Files intentionally excluded

### Legacy GadTools enhancer (not copied)

- All `clv_ascii*`, `clv_columns*`, `clv_char_wrap*`
- All `clv_renderer*`, `clv_prepared*`, `clv_selection*`
- `clv_pixel_wrap.c` (wrap already forked in `clv_control_wrap.c`)
- `clv_icons.c`, `clv_styles.c`, `clv_details*`
- All `clv_bind_*`
- All `clv_cellctl*`
- `custom_listview.h`, `clv_config.h`, `PUBLIC_HEADERS.txt`

### Generic headers listed in the extraction manifest but **not** required by the control build

These were **not** copied in this phase (control sources do not include them; baseline/log/bench linked without them):

- `clv_compiler.h`
- `clv_sdk_compat.h`
- `clv_exec_list_compat.h`
- `clv_sort.*`, `clv_path*`

**Discrepancy note:** `RICHLISTVIEW_EXTRACTION_MANIFEST.md` lists these under “shared foundations to copy then localise” / “duplicate as small generic modules”. Source/Makefile behaviour for `custom-control-demo*` does not link or include them. Preferring current build behaviour, they were deferred.

### Legacy examples / tests / tooling

- `examples/00_*` … `08_*`, `size_compare/`
- `tests/host/`, `tests/header_audit/`, `tests/headers/`
- Legacy profile Makefile targets, size harness, validate scripts, AutoDocs corpus

---

## New directory structure

```text
RichListview/
├── README.md
├── Makefile
├── .gitignore
├── bin/                          (build output; gitignored)
├── build/                        (build output; gitignored)
├── docs/
│   ├── audit/
│   ├── historical/
│   ├── RICHLISTVIEW_REPOSITORY_CREATION_REPORT.md
│   └── CLV_*.md (control plans/logs)
├── examples/
│   └── custom_control_demo/
└── src/
    └── rich_listview/
        ├── clv_control*.{c,h}
        ├── clv_platform*.{c,h}
        ├── clv_types.h
        ├── clv_bench*.{c,h}
        └── backends/
            └── clv_backend_amiga_v36.{c,h}
```

---

## Include-path changes

With `-Isrc`, includes were rewritten from the dual-tree layout to a single package:

| Old | New |
|---|---|
| `"custom_listview_control/..."` | `"rich_listview/..."` |
| `"custom_listview/clv_platform.h"` | `"rich_listview/clv_platform.h"` |
| `"custom_listview/clv_platform_internal.h"` | `"rich_listview/clv_platform_internal.h"` |
| `"custom_listview/clv_types.h"` | `"rich_listview/clv_types.h"` |
| `"custom_listview/clv_bench_internal.h"` | `"rich_listview/clv_bench_internal.h"` |

Same-directory includes in `clv_platform.c` (`"clv_platform_internal.h"`, `"clv_bench_internal.h"`) were left unchanged.

No `#include` resolves outside this repository. No Makefile `-I` points at `amiga_custom_listview`.

---

## Makefile targets created

| Target | Purpose |
|---|---|
| `custom-control-demo` | Baseline demo (default `all`) |
| `custom-control-demo-log` | Logging variant (`CLV_ENABLE_LOGGING`) |
| `custom-control-demo-bench` | Benchmark variant (`CLV_ENABLE_BENCHMARKS` + `clv_bench.o`) |
| `custom-control-demo-nosmart` | Rebuilds baseline with `CLV_ENABLE_SMART_SCROLL=0` |
| `clean` | Removes `build/` and `bin/` |
| `dirs` | Creates output directories |

Flags preserved from source: `+aos68k -c99 -cpu=68000 -O2 -size`, link `-lamiga -lauto`, `-DCLV_PLATFORM_AMIGA=1`.

---

## Exact baseline object list

```text
build/rich_listview/clv_platform.o
build/rich_listview/clv_control.o
build/rich_listview/clv_control_layout.o
build/rich_listview/clv_control_wrap.o
build/rich_listview/clv_control_render.o
build/rich_listview/clv_control_checkbox.o
build/rich_listview/clv_control_input.o
build/rich_listview/clv_control_scroll.o
build/rich_listview/backends/clv_backend_amiga_v36.o
build/examples/custom_control_demo.o
```

Executable: `bin/custom-control-demo`

---

## Build results

Compiler: VBCC `vc` (`C:\VBCC\bin\vc.exe`), host Windows.

### Baseline `custom-control-demo`

- **Result:** success (exit 0)
- **Warnings:** none observed
- **Size:** 44824 bytes
- **Compile (representative):**  
  `vc +aos68k -c99 -cpu=68000 -O2 -size -Isrc -DCLV_PLATFORM_AMIGA=1 -DCLV_ENABLE_SMART_SCROLL=1 -c -o <obj> <src>`
- **Link:**  
  `vc +aos68k -cpu=68000 -O2 -size -final -lamiga -lauto -o bin/custom-control-demo` + baseline object list above

### `custom-control-demo-log`

- **Result:** success
- **Size:** 60092 bytes
- Isolated object tree: `build/rich_listview_log/` + `clv_control_log.o` + shared `clv_platform.o`

### `custom-control-demo-bench`

- **Result:** success
- **Size:** 61472 bytes
- Isolated object tree: `build/rich_listview_bench/` including `clv_platform.o` and `clv_bench.o`

### `custom-control-demo-nosmart`

- **Result:** success after forced object rebuild
- **Size (with SMART_SCROLL=0):** 43564 bytes
- **Note:** Like the source Makefile, `custom-control-demo-nosmart` shares the baseline object directory. A plain second `make custom-control-demo-nosmart` after a smart-scroll build may no-op unless objects are cleaned. Forced rebuild with `CLV_ENABLE_SMART_SCROLL=0` confirmed compile/link success; baseline then restored with smart scroll enabled.

---

## Warnings and blockers

- No compile or link blockers.
- No unexpected missing shared dependencies beyond the deferred optional headers listed above.
- No new warnings suppressed.

---

## Runtime verification status

**Not performed.** This host cannot run Amiga binaries. Verification is compile/link only.

Do not claim window/render/input behaviour until WinUAE / real Amiga testing is done with `bin/custom-control-demo`.

---

## Search results proving independence

### Compile-facing files (`src/**/*.c`, `src/**/*.h`, `examples/**/*.c`, `Makefile`)

| Pattern | Hits | Interpretation |
|---|---:|---|
| `../amiga_custom_listview` | 0 | No path back to source repo |
| `src/custom_listview/` | 0 | No old tree includes |
| `src/custom_listview_control/` | 0 | Paths localised |
| `custom_listview/clv_` | 0 | Includes use `rich_listview/` |
| `GTLV_` | 0 | Control does not use GTLV tags |
| `LVDrawMsg` | 0 | No draw-hook message type |
| `LISTVIEW_KIND` | 3 | Comments / Makefile warning only |
| `clv_renderer_` | 1 | Demo comment: deliberately excluded |
| `clv_selection_` | 0 | — |
| `clv_cellctl_` | 2 | Comments only (legacy warning) |

Documentation under `docs/` and `docs/audit/` intentionally retains historical path names describing the source repository layout.

### Link independence

Baseline and optional variants link only objects under this repository’s `build/` tree. No legacy `clv_renderer_*.o`, `clv_selection.o`, `clv_ascii*.o`, `clv_bind_*.o`, or `clv_cellctl_*.o` is linked.

### Tests

No `tests/` tree was copied. Building does not require the legacy host/header test suites.

---

## Source changes made during extraction

Allowed path/localisation edits only:

1. Rewrote `#include` prefixes `custom_listview_control/` and `custom_listview/` → `rich_listview/` in copied control, platform/types, and demo sources.
2. Corrected one comment in `clv_control.h` after a blind path rewrite incorrectly said `clv_cellctl_*` lived under `src/rich_listview/` (restored wording that cellctl is legacy / outside this package).
3. Added new `Makefile`, `README.md`, `.gitignore`, historical README, and this report.

**Not changed:** public API names, event semantics, wrap/selection/smart-scroll behaviour, allocator behaviour, struct layouts.

---

## Open cleanup tasks

See **Deferred Cleanup** below.

---

## Confirmation: original repository intact

- No files were deleted, moved, or rewritten in `amiga_custom_listview` for this extraction.
- `src/custom_listview_control/` remains present in the source tree.
- Source `git status` showed only pre-existing untracked docs (`docs/audit/`, wishlist, audit prompt) — no destructive modifications from this task.
- Extraction used **copy** into the sibling directory `../RichListview/`.

---

## Deferred Cleanup

- ~~Rename `CLV_*` / `clv_control_*` to `RLV_*` / `rlv_*`~~ — done; see `docs/RICHLISTVIEW_NAMESPACE_MIGRATION_REPORT.md`
- Trim `rlv_types.h` (remove GadTools-oriented `RLV_LISTVIEW_SCROLLBAR_BORDER` narrative / unused constants if Rich does not need them)
- Review logging APIs vs any remaining legacy no-op logger naming in docs
- Add RichListview-specific tests (do not import the legacy host suite as-is)
- Review public vs private headers
- Create a final RichListview integration guide
- Decide whether optional sort/path helpers are needed
- ~~Consider isolated object trees for nosmart~~ — done as `rich-listview-demo-nosmart` with `build/rich_listview_nosmart/`
- Optionally vendor compiler / SDK compat headers if a future feature needs them
- Update demo README module path list to `src/rich_listview/` (partially done in namespace migration)
