# RichListview — Title-Row Fill Patterns Implementation Report

Date: 2026-08-06

## Summary

Configurable title-row (column header) interior fills: solid grey (default),
grey/blue vertical stripes, grey/white vertical stripes, blue/grey
checkerboard, sparse blue stipple, and wide grey/blue stripes. Colours come
from the control's semantic `RLV_Pens` snapshot (mapped from screen
`DrawInfo` by the V36 backend). One patterned `RectFill` per header region;
bevels, text, sort indicators, and column dividers are unchanged.

## Files changed

| File | Change |
|------|--------|
| `src/rich_listview/rich_listview.h` | `RLV_TitleFillStyle`, `RLV_Config.title_fill_style`, setters/getters, `RLV_RENDER_HEADER_ONLY` |
| `src/rich_listview/rlv_draw.h` | Optional `fill_rect_pattern` draw op |
| `src/rich_listview/rlv_internal.h` | `title_fill_style` on control; title-fill + `rlv_render_header_only` declarations |
| `src/rich_listview/rlv_title_fill.c` | **New** — descriptor table and `rlv_title_fill_area` |
| `src/rich_listview/rlv.c` | Create/config, setters, `rlv_render` header-only branch |
| `src/rich_listview/rlv_render.c` | Header fill via helper; `rlv_render_header_only` |
| `src/rich_listview/backends/rlv_backend_amiga_v36.c` | `rlv_v36_fill_rect_pattern` (`SetAfPt` + restore) |
| `Makefile` | `rlv_title_fill.o` in all RLV object lists |
| `examples/rich_listview_demo/main.c` | Settings → Title fill menu |
| `docs/RICHLISTVIEW_OVERVIEW.md` | Section 2.11 |
| `docs/DevLog.md` | Entry |

## Public constants / config

- `RLV_TITLE_FILL_SOLID` (0) — default
- `RLV_TITLE_FILL_GREY_BLUE_STRIPES` (1)
- `RLV_TITLE_FILL_GREY_WHITE_STRIPES` (2)
- `RLV_TITLE_FILL_BLUE_GREY_CHECKERBOARD` (3)
- `RLV_TITLE_FILL_SPARSE_BLUE_STIPPLE` (4)
- `RLV_TITLE_FILL_WIDE_GREY_BLUE_STRIPES` (5)
- `RLV_Config.title_fill_style` — zero-init = solid
- `RLV_RENDER_HEADER_ONLY` (`1UL << 1`) — header repaint without viewport
- Unsupported values normalize to `RLV_TITLE_FILL_SOLID`

## Predefined pattern geometry

| Style | Pattern rows | Blue source pen | Grey source pen | Blue pixels |
|------|--------------:|-----------------|-----------------|------------:|
| `GREY_BLUE_STRIPES` | 1 x 16 | `FILLPEN` | `BACKGROUNDPEN` | 8/16 (1:1) |
| `GREY_WHITE_STRIPES` | 1 x 16 | `SHINEPEN` | `BACKGROUNDPEN` | 8/16 (1:1) |
| `BLUE_GREY_CHECKERBOARD` | 2 x 16 | `FILLPEN` | `BACKGROUNDPEN` | 16/32 (1:1) |
| `SPARSE_BLUE_STIPPLE` | 4 x 16 | `FILLPEN` | `BACKGROUNDPEN` | 16/64 (1:3) |
| `WIDE_GREY_BLUE_STRIPES` | 1 x 16 | `FILLPEN` | `BACKGROUNDPEN` | 4/16 (1:3) |

The checkerboard uses `0xAAAA, 0x5555`. Sparse stipple uses
`0x8888, 0x2222, 0x4444, 0x1111` so the blue pixels shift between rows and do
not form continuous vertical or horizontal blue runs. Wide stripes use
`0x1111`, giving one blue column for every three grey columns.

## Semantic pen resolution

`rlv_backend_v36_pens_from_drawinfo` maps:

| Role | DrawInfo | Used for |
|------|----------|----------|
| `background` | `BACKGROUNDPEN` | Solid fill; stripe background |
| `selected_background` | `FILLPEN` | Blue stripe foreground |
| `shine` | `SHINEPEN` | White stripe foreground |
| `text` | `TEXTPEN` | Title text (unchanged) |

Descriptor table entries reference internal pen roles; `rlv_title_fill_area`
resolves roles through the instance `c->pens` at paint time.

## RastPort state restoration

`rlv_v36_fill_rect_pattern`:

1. Saves `DrawMode`, `FgPen`, `BgPen`.
2. Sets `JAM2`, pattern via `SetAfPt(pattern_ptr, height_exp)`, `RectFill`.
3. Restores solid fill: `SetAfPt(rp, NULL, 0)`.
4. Restores pens and `DrawMode`.

Title text and frame drawing follow with `set_pens` / `JAM2` as before.

## Redraw paths audited

| Path | Fill behaviour |
|------|----------------|
| `rlv_render_full` → `rlv_draw_header` | Full `header_bounds` patterned fill; per-column frame/text/indicator |
| `rlv_render_header_only` | Same as `rlv_draw_header` |
| `rlv_render_header_column` | Per-column fill + frame/text/indicator (resize commit / cancel) |
| `rlv_render_header_column_area` | Regional repair (clip + fill + frame/text/indicator) |
| `rlv_cr_paint_pair_preview` | Patterned/solid fill via `rlv_title_fill_area` + preview titles |
| `rlv_render` + `RLV_RENDER_VIEWPORT_ONLY` | Header untouched |
| Sort indicator redraw | Uses header column paths above |
| Row / viewport paints | Header untouched |
| Demo title-fill menu | `rlv_set_title_fill_style` + `rlv_render(..., RLV_RENDER_HEADER_ONLY)` |

Interior fill replaces prior `fill_rect` with background pen only; frame
pixels are still covered then redrawn with `rlv_draw_cell_frame` (same as
before).

## Compatibility and performance

- AmigaOS 2.04+ `SetAfPt` / `RectFill` only; no RTG/CyberGraphX/ReAction.
- Static read-only descriptor table; no heap allocation in render paths.
- Single patterned fill per header or column cell (not per-stripe loops).
- Invalid style values normalize to solid at create/set.
- Backends without `fill_rect_pattern` fall back to solid background.

## Tests performed

| Test | Result |
|------|--------|
| `make rich-listview-demo` | Compiled and linked OK |
| Pattern `SetAfPt` pointer fix + JAM1 header text | Compiled OK; visual retest required |
| Default solid vs prior behaviour | Same code path when style = solid |
| Emulator / hardware visual | **Required** — not run in this session |
| Workbench 2.x / 3.x | **Required** — target both |

## Remaining limitations

- No caller-supplied custom patterns (descriptor table reserved for future API).
- `RLV_RENDER_HEADER_ONLY` does not redraw the outer control frame (one-pixel shadow outline).
