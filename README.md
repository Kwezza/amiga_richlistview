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
(see `examples/custom_control_demo/`).

## Origin

This repository was extracted from the earlier
[`amiga_custom_listview`](../amiga_custom_listview) project.

The legacy GadTools draw-hook enhancer and ASCII label formatter are
**not** included here.

Audit evidence for the split lives under [`docs/audit/`](docs/audit/).

## API naming (temporary)

The current public API still uses `CLV_*` and `clv_control_*` names
carried over from the source tree.

Renaming to `RLV_*` / `rlv_*` is deferred until after extraction
stability is proven. Do not rename during early cleanup without a
dedicated migration plan.

## Build (VBCC / AmigaOS 68k)

Requires VBCC with the `+aos68k` target and Amiga SDK libraries.

```text
make custom-control-demo
```

Output: `bin/custom-control-demo`

Optional parity targets (same as the source repository):

```text
make custom-control-demo-log
make custom-control-demo-bench
make custom-control-demo-nosmart
```

CPU target is 68000. Flags match the extracted custom-control build
(`-c99 -cpu=68000 -O2 -size`, link `-lamiga -lauto`).

## Agent warning

**Do not reintroduce the legacy GadTools renderer, GTLV selection
adapter, ASCII formatter, binders, or `clv_cellctl` implementation into
this repository.**

Those belong in the legacy enhancer repository. RichListview already
owns paint, selection, wrap, and checkbox cells via `src/rich_listview/`.

## Layout

```text
src/rich_listview/     control + Amiga V36 backend + platform/types
examples/custom_control_demo/
docs/audit/            extraction audit reports
docs/historical/       mixed-era planning notes
```

See [`docs/RICHLISTVIEW_REPOSITORY_CREATION_REPORT.md`](docs/RICHLISTVIEW_REPOSITORY_CREATION_REPORT.md)
for the extraction record.
