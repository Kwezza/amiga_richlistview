# RichListview Header Boundary Cleanup Report

**Date:** 2026-07-31  
**Principle:** Header ownership and dependency hygiene only  
**Status:** Complete (compile/link verified for all variants; runtime not re-run)

---

## 1. Executive summary

Public/private header ownership was cleaned up after the `CLV_*` → `RLV_*`
namespace migration. Application code now needs only:

```c
#include "rich_listview/rich_listview.h"
```

and, for Amiga V36 backend setup:

```c
#include "rich_listview/backends/rlv_backend_amiga_v36.h"
```

Key outcomes:

- `RLV_Control` remains **opaque** (already was; confirmed safe).
- `RLV_BackendV36` remains **opaque** (already was; confirmed safe).
- Legacy `rlv_types.h` was **redistributed and deleted**.
- Redundant `rlv_platform_api.h` wrapper was **deleted**.
- Unused GadTools-oriented `RLV_LISTVIEW_SCROLLBAR_BORDER` was **removed**.
- Internal `RLV_PixelColumn` moved into `rlv_internal.h`.
- Public `RLV_CellAlign` moved into `rich_listview.h`.
- All four build variants compile and link with **identical** binary sizes to
  the namespace-migration baseline.
- Runtime behaviour was **not intentionally changed**. Amiga/WinUAE interactive
  re-run was not performed in this task.

---

## 2. Header inventory and classification

| Path | Classification | Consumers | Notes |
|---|---|---|---|
| `rich_listview.h` | **PUBLIC_CORE** | Demo, all control `.c`, public audit | Primary application include |
| `rlv_draw.h` | **PUBLIC_CORE** | `rich_listview.h`, backend header, implementers | Draw ops / pens / viewport-move results |
| `backends/rlv_backend_amiga_v36.h` | **PUBLIC_BACKEND** | Demo, backend `.c`, public audit | Opaque backend + factory / pens helper |
| `rlv_platform.h` | **PRIVATE_INTERNAL** (transitively via draw) | `rlv_draw.h` | Amiga-only compile assert; not a standalone app API |
| `rlv_internal.h` | **PRIVATE_INTERNAL** | Control `.c` only | Full `RLV_Control`, layout/wrap/paint helpers |
| `rlv_platform_internal.h` | **PRIVATE_INTERNAL** | Platform, control, backend `.c` | malloc / free / strdup |
| `rlv_log.h` | **PRIVATE_OPTIONAL** | Demo (all variants), instrumented `.c` | Logging macros; stubs when logging off |
| `rlv_bench_internal.h` | **PRIVATE_OPTIONAL** | Demo, bench builds, via `rlv_internal.h` | Benchmark macros / counters |
| `rlv_types.h` | **OBSOLETE_OR_REDUNDANT** → **deleted** | Was public via `rich_listview.h` | Legacy mixed-repo leftovers |
| `rlv_platform_api.h` | **OBSOLETE_OR_REDUNDANT** → **deleted** | Was only included by `rlv_draw.h` | Thin historical wrapper |

### Pre-cleanup exposure problems

- `rich_listview.h` included `rlv_types.h`, which carried unused GadTools
  scrollbar-border narrative and exposed internal `RLV_PixelColumn`.
- `rlv_draw.h` included `rlv_platform_api.h`, a one-line wrap of
  `rlv_platform.h` with no added contract.
- Demo correctly used public core + backend headers, plus optional log/bench
  headers for those build variants.

---

## 3. Final public headers

```text
PUBLIC
  rich_listview.h
  rlv_draw.h                         (included by rich_listview.h)
  backends/rlv_backend_amiga_v36.h
```

`rlv_platform.h` is pulled transitively by `rlv_draw.h` solely for the Amiga
target assert. It is not advertised as an application include.

### Public surface (application-facing)

- Opaque `RLV_Control`
- `RLV_Config`, `RLV_Column`, `RLV_Row`, `RLV_Cell`
- `RLV_InputEvent`, `RLV_Event`
- Public enums / flags / callbacks (wrap, divider, input, event, cell action)
- `RLV_CellAlign`
- Lifecycle, row/column, selection, scroll, render, and input functions
- Drawing contract types required by config/pens: `RLV_DrawOps`, `RLV_Pens`,
  related font/viewport-move types in `rlv_draw.h`
- Backend create/destroy/ops/context/pens helpers

---

## 4. Final private headers

```text
PRIVATE
  rlv_internal.h
  rlv_platform.h              (policy assert; not a public API entry)
  rlv_platform_internal.h
  rlv_bench_internal.h
  rlv_log.h
```

Application product code must not include the private set. The demo may
include `rlv_log.h` / `rlv_bench_internal.h` when building optional log or
benchmark variants (harness exception only).

---

## 5. Opaque `RLV_Control` decision

**Result: opaque (confirmed / retained).**

```c
typedef struct RLV_Control RLV_Control;   /* rich_listview.h */
struct RLV_Control { ... };               /* rlv_internal.h only */
```

Opacity checks passed:

| Check | Result |
|---|---|
| Demo allocates `RLV_Control` directly | No — uses `rlv_create` |
| Application stores only pointers | Yes |
| `sizeof(RLV_Control)` in public/demo code | No matches |
| Public inline needing internal fields | None |
| Public macro dereferencing control internals | None |

No blocker. Full definition stays in `rlv_internal.h`.

---

## 6. Backend opacity decision

**Result: opaque (confirmed / retained).**

```c
typedef struct RLV_BackendV36 RLV_BackendV36;  /* public header */
struct RLV_BackendV36 { ... };                 /* backends/*.c only */
```

Application code uses create / destroy / set_rastport / set_font /
get_ops / get_context / pens_from_drawinfo. No public need for internal
RastPort / clip fields.

---

## 7. `rlv_types.h` item classification

| Item | Classification | Action |
|---|---|---|
| `RLV_CellAlign` | **PUBLIC_CORE** | Moved to `rich_listview.h` |
| `RLV_PixelColumn` | **PRIVATE_INTERNAL** | Moved to `rlv_internal.h` |
| `RLV_LISTVIEW_SCROLLBAR_BORDER` | **LEGACY_GADTOOLS_LEFTOVER** / **UNUSED** | Removed (zero `.c`/`.h` references) |
| File header comments re ASCII / `lvdm_Bounds` | **LEGACY_GADTOOLS_LEFTOVER** | Removed with file |
| `#include "rlv_platform.h"` from types | redundant coupling | Eliminated with file |

After redistribution, `rlv_types.h` had no independent purpose → **deleted**.
No empty compatibility stub retained.

---

## 8. Types moved and destinations

| Type / symbol | From | To |
|---|---|---|
| `RLV_CellAlign` | `rlv_types.h` | `rich_listview.h` |
| `RLV_PixelColumn` | `rlv_types.h` | `rlv_internal.h` |
| `RLV_LISTVIEW_SCROLLBAR_BORDER` | `rlv_types.h` | *(removed)* |

Field layout and numeric values for moved active types are unchanged.

---

## 9. Legacy definitions removed

- `RLV_LISTVIEW_SCROLLBAR_BORDER` (GadTools ListView scrollbar/frame pixel reserve)
- Stale comments referencing ASCII bridge, `lvdm_Bounds`, and mixed-repo
  “shared low-level CLV types” packaging
- Entire `rlv_types.h` file
- Entire `rlv_platform_api.h` thin wrapper

Active public headers contain **no** GadTools draw-hook types, no
`LVDrawMsg` / `lvdm_Bounds` geometry constants, and no private control
struct definition.

Allowed historical warnings remain in comments (do-not-reintroduce
`LISTVIEW_KIND` / `clv_cellctl_*` / former `CLV_PIXEL_WRAP_*` numeric
family note).

---

## 10. Redundant headers removed or retained

| Header | Decision |
|---|---|
| `rlv_types.h` | **Removed** |
| `rlv_platform_api.h` | **Removed** (Option B) |
| `rlv_platform.h` | **Retained** — Amiga assert included from `rlv_draw.h` |
| `rlv_log.h` | **Retained** private/optional |
| `rlv_bench_internal.h` | **Retained** private/optional |
| `rlv_internal.h` | **Retained** private |

---

## 11. Include dependency changes

### Target dependency direction (achieved)

```text
rich_listview.h
    └── rlv_draw.h
            └── rlv_platform.h          (Amiga assert only)

backends/rlv_backend_amiga_v36.h
    └── rlv_draw.h

rlv_internal.h
    ├── rich_listview.h
    └── rlv_bench_internal.h

implementation .c
    └── rlv_internal.h (+ rlv_log.h / rlv_platform_internal.h as needed)
```

### Changes made

- `rich_listview.h`: dropped `rlv_types.h`; gained `RLV_CellAlign`.
- `rlv_draw.h`: includes `rlv_platform.h` directly (no `rlv_platform_api.h`).
- Public headers do **not** include `rlv_internal.h`,
  `rlv_platform_internal.h`, `rlv_bench_internal.h`, or `rlv_log.h`.
- Control `.c` units continue to include `rlv_internal.h` as the private hub.

### Demo includes

```c
#include "rich_listview/rich_listview.h"
#include "rich_listview/backends/rlv_backend_amiga_v36.h"
#include "rich_listview/rlv_log.h"            /* optional variant harness */
#include "rich_listview/rlv_bench_internal.h" /* optional variant harness */
```

Product applications should omit the last two.

---

## 12. Public-header compile audit

Added:

- `tests/public_headers/rlv_public_core.c`
- `tests/public_headers/rlv_public_backend.c`
- Makefile target: `make public-header-audit`

| Audit | Result |
|---|---|
| `#include "rich_listview/rich_listview.h"` alone | **PASS** (object compiles) |
| Core + `backends/rlv_backend_amiga_v36.h` | **PASS** (object compiles) |
| Requires private headers | **No** |
| Requires implementation source paths beyond `-Isrc` | **No** |

Compile-only objects are produced; no linked audit executable is required.

---

## 13. Build results

Clean rebuild of all variants after header cleanup:

| Target | Result | Warnings (product sources) |
|---|---|---|
| `public-header-audit` | **SUCCESS** | None after audit source polish |
| `rich-listview-demo` | **SUCCESS** | None observed |
| `rich-listview-demo-log` | **SUCCESS** | None observed |
| `rich-listview-demo-bench` | **SUCCESS** | None observed |
| `rich-listview-demo-nosmart` | **SUCCESS** | None observed |

Object lists unchanged from the Makefile (no source files added/removed from
link sets). Header movement affected include graphs only.

Sources touched by this cleanup:

- `src/rich_listview/rich_listview.h`
- `src/rich_listview/rlv_draw.h`
- `src/rich_listview/rlv_internal.h`
- `src/rich_listview/rlv_platform.h`
- `src/rich_listview/rlv_platform_internal.h`
- `src/rich_listview/rlv_platform.c` (comment only)
- `src/rich_listview/rlv_log.h` (comment only)
- `src/rich_listview/backends/rlv_backend_amiga_v36.h` (comment only)
- Deleted: `rlv_types.h`, `rlv_platform_api.h`
- Added: `tests/public_headers/*.c`, Makefile `public-header-audit`
- `README.md` public-API section clarified

---

## 14. Binary size comparison

VBCC `+aos68k -O2 -size -final`, `-cpu=68000`.

| Target | Pre-cleanup (namespace report) | Post-cleanup | Δ |
|---|---:|---:|---:|
| Baseline `rich-listview-demo` | 44816 | 44816 | 0 |
| Logging `rich-listview-demo-log` | 59940 | 59940 | 0 |
| Benchmark `rich-listview-demo-bench` | 61464 | 61464 | 0 |
| No smart scroll `rich-listview-demo-nosmart` | 43556 | 43556 | 0 |

Exact size match confirms header redistribution did not alter generated code
or linked string/constant content in a measurable way. Removal of the unused
`RLV_LISTVIEW_SCROLLBAR_BORDER` macro had no binary effect (unused).

---

## 15. Runtime verification

| Item | Status |
|---|---|
| Host compile/link of all four variants | **Verified** |
| Public-header compile audit | **Verified** |
| Amiga/WinUAE window / render / scroll / input pass | **Not re-run** |

Do **not** claim interactive runtime equivalence from this task alone.
Prior Amiga/emulator success still applies to the post-namespace binary;
this cleanup changed headers/comments/includes only, and binary sizes are
byte-identical, which strongly supports behavioural identity but is not a
substitute for an Amiga interactive pass.

---

## 16. Residual legacy-reference search

Searched active `src/`, `examples/`, and `Makefile`:

| Pattern | Active hits | Interpretation |
|---|---:|---|
| `LVDrawMsg` | 0 | — |
| `lvdm_Bounds` | 0 | — |
| `GTLV_` | 0 | — |
| `LISTVIEW_KIND` | comments only | Boundary / do-not-reintroduce docs |
| `CLV_` | comments only | Historical wrap-constant note in public header |
| `clv_` | comments only | Excluded legacy modules / cellctl warnings |
| `rlv_types.h` | 0 in active code | Deleted; historical docs may still name it |
| `rlv_platform_api.h` | 0 in active code | Deleted |
| `RLV_LISTVIEW_SCROLLBAR_BORDER` | 0 | Removed |
| `sizeof(RLV_Control)` | 0 | Opacity safe |

Allowed: historical comments and audit documents. Not present in active
public headers: GadTools-only geometry constants, draw-hook types, private
`struct RLV_Control` definition, or old source-tree path includes.

---

## 17. Compatibility / source-impact concerns

- Applications that previously `#include "rich_listview/rlv_types.h"` must
  switch to `rich_listview.h` (or drop the include). No in-tree consumer
  remained after migration.
- Applications that included `rlv_platform_api.h` must stop; use
  `rich_listview.h` / backend header instead.
- `RLV_PixelColumn` is no longer visible through the public API. Any out-of-tree
  code that depended on it was already outside the intended contract.
- Demo still includes private optional log/bench headers; that is intentional
  for variant builds, not a product-API requirement.

---

## 18. Deferred work

Not done here (out of scope):

- Making demo log/bench includes conditional-only (cosmetic)
- Moving unused `RLV_LineStyle` out of `rlv_draw.h`
- Moving `RLV_FontMetrics` fully private (still in draw header; used by
  internal control state via public draw types)
- Shared-library / installed-header packaging layout
- Broader public API freeze / versioning
- Amiga interactive re-verification pass
- Further bench-counter legacy trim inside `rlv_bench_internal.h`

---

## 19. Behaviour confirmation

Runtime behaviour was **not intentionally changed**. This task moved header
ownership, deleted unused legacy declarations and a redundant wrapper, and
added a compile-only public-header audit. Paint, scroll, smart scroll,
layout, wrap, selection, keyboard, checkbox, events, ownership, backend
behaviour, logging, and benchmarks were not redesigned.

Evidence supporting behavioural identity:

1. No `.c` logic edits beyond a platform file comment.
2. Opaque control/backend already matched intended API.
3. All four variants link successfully.
4. Binary sizes are byte-identical to the namespace-migration baseline.

---

## 20. Final header map

```text
PUBLIC
  rich_listview.h
  rlv_draw.h
  backends/rlv_backend_amiga_v36.h

PRIVATE
  rlv_internal.h
  rlv_platform.h                 (Amiga assert; via rlv_draw.h)
  rlv_platform_internal.h
  rlv_bench_internal.h
  rlv_log.h

REMOVED
  rlv_types.h
  rlv_platform_api.h
```
