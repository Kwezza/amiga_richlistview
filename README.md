# RichListview

RichListview is a full custom ListView control for classic AmigaOS.

It owns:

- viewport rendering
- scrolling
- selection
- keyboard navigation
- row layout
- embedded cell controls
- application-facing events

It does **not** use GadTools `LISTVIEW_KIND` as its row renderer.

A normal GadTools `SCROLLER_KIND` may be used as a companion scrollbar
(see `examples/rich_listview_demo/`).

## Origin

This repository was extracted from the earlier
[`amiga_custom_listview`](../amiga_custom_listview) project.

The legacy GadTools draw-hook enhancer and ASCII label formatter are
**not** included here.

Audit evidence for the split lives under [`docs/audit/`](docs/audit/).

## Public API

Preferred include:

```c
#include "rich_listview/rich_listview.h"
```

Optional Amiga V36 backend setup:

```c
#include "rich_listview/backends/rlv_backend_amiga_v36.h"
```

Public types and macros use the `RLV_*` prefix. Functions use `rlv_*`.
`RLV_Control` and `RLV_BackendV36` are opaque. Application code must not
include private headers (`rlv_internal.h`, `rlv_platform_internal.h`,
`rlv_bench_internal.h`, `rlv_log.h`).
## Build (VBCC / AmigaOS 68k)

Requires VBCC with the `+aos68k` target and Amiga SDK libraries.

```text
make rich-listview-demo
```

Output: `bin/rich-listview-demo`

Optional variants:

```text
make rich-listview-demo-log
make rich-listview-demo-bench
make rich-listview-demo-nosmart
```

CPU target is 68000. Flags: `-c99 -cpu=68000 -O2 -size`, link
`-lamiga -lauto`. Smart-scroll on/off builds use isolated object trees.

## Agent warning

**Do not reintroduce the legacy GadTools renderer, GTLV selection
adapter, ASCII formatter, binders, or `clv_cellctl` implementation into
this repository.**

Those belong in the legacy enhancer repository. RichListview already
owns paint, selection, wrap, and checkbox cells via `src/rich_listview/`.

## Layout

```text
src/rich_listview/     control + Amiga V36 backend + platform/types
examples/rich_listview_demo/
docs/audit/            extraction audit reports
docs/historical/       mixed-era planning notes
```

See [`docs/RICHLISTVIEW_REPOSITORY_CREATION_REPORT.md`](docs/RICHLISTVIEW_REPOSITORY_CREATION_REPORT.md)
for the extraction record, and
[`docs/RICHLISTVIEW_NAMESPACE_MIGRATION_REPORT.md`](docs/RICHLISTVIEW_NAMESPACE_MIGRATION_REPORT.md)
for the `CLV_*` → `RLV_*` rename.
