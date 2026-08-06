# RichListview — Alternating Row Backdrops Implementation Report

Date: 2026-08-06

## Summary

Optional alternating body-row backdrops, split into two compile-time
capabilities:

1. **`RLV_ENABLE_ALTERNATE_ROWS`** — lightweight odd/even logical-row pens
   using a borrowed caller pen or the normal background.
2. **`RLV_ENABLE_ADAPTIVE_ROW_PEN`** — separate Amiga helper that attempts a
   shared pen approximately 10% darker than `BACKGROUNDPEN`, with runtime
   V39 gating and colour validation. Falls back to
   `RLV_ROW_BACKDROP_ALTERNATE_PATTERN` (sparse FILLPEN stipple + JAM1 text)
   without failing control creation.

Default runtime mode remains **STANDARD** (single-colour rows, previous
appearance). Adaptive code is not linked unless both macros are enabled.

## Architecture findings (inspection)

- Configuration uses `RLV_Config` structs (not tags); optional fields are
  appended with zero-init defaults, matching `title_fill_style`.
- Semantic pens live in `RLV_Control.pens` (`RLV_Pens`), filled by
  `rlv_backend_v36_pens_from_drawinfo` from screen `DrawInfo`.
- Body row fills are chosen in `rlv_paint_row_content`; selected rows use
  `selected_background` via `rlv_row_uses_selected_fill`. Empty viewport
  space and row gaps always fill with `pens.background`. There is no
  separate body-row pattern system — only title-row fills
  (`rlv_title_fill.c`) — so alternating rows do not compete with another
  body backdrop mode.
- Layout stores `layout_rows[].logical_index` (source index). Painting
  already resolves source before fill/text, so parity uses that index and
  remains stable through wrapping and scrolling.
- Feature macros and isolated Makefile object trees already exist for
  sorting, column resize, smart scroll, logging, and benchmarks.
- Demo Settings menus use CHECKIT exclusive groups; title fill is applied
  immediately (no Apply). Row backdrop follows that immediate pattern.

## Files changed

| File | Change |
|------|--------|
| `src/rich_listview/rlv_features.h` | **New** — feature macros + dependency `#error` |
| `src/rich_listview/rich_listview.h` | `RLV_RowBackdropMode`, config fields, setters/getters |
| `src/rich_listview/rlv_internal.h` | Instance state; `rlv_row_normal_backdrop_pen` macro when off |
| `src/rich_listview/rlv_alternate_rows.h` / `.c` | **New** — mode resolve, ownership, pen selection |
| `src/rich_listview/backends/rlv_adaptive_row_pen.h` / `.c` | **New** — V39 adaptive acquire/release |
| `src/rich_listview/backends/rlv_backend_amiga_v36.h` / `.c` | Borrowed ColorMap slot + accessors |
| `src/rich_listview/rlv.c` | Create/destroy/set_pens/setters |
| `src/rich_listview/rlv_render.c` | Normal-row and checkbox local fill use backdrop helper |
| `Makefile` | Feature flags, object lists for all variants |
| `examples/rich_listview_demo/main.c` | Settings → Row backdrop |
| `examples/rich_listview_demo/README.md` | Demo docs |
| `docs/RICHLISTVIEW_OVERVIEW.md` | Section 2.12 + feature tables |
| `docs/DevLog.md` | Entry |
| `docs/RICHLISTVIEW_ALTERNATE_ROWS_IMPLEMENTATION_REPORT.md` | This report |

## Public API / config names

```c
typedef enum RLV_RowBackdropMode {
    RLV_ROW_BACKDROP_STANDARD = 0,
    RLV_ROW_BACKDROP_ALTERNATE_PEN,
    RLV_ROW_BACKDROP_ADAPTIVE,
    RLV_ROW_BACKDROP_ALTERNATE_PATTERN
} RLV_RowBackdropMode;

/* RLV_Config (when RLV_ENABLE_ALTERNATE_ROWS != 0) */
UWORD row_backdrop_mode;
UWORD alternate_row_pen; /* borrowed for ALTERNATE_PEN */

VOID rlv_set_row_backdrop(RLV_Control *c, UWORD mode, UWORD alternate_pen);
UWORD rlv_get_row_backdrop_mode(const RLV_Control *c);           /* requested */
UWORD rlv_get_row_backdrop_effective_mode(const RLV_Control *c); /* after resolve */
```

Compile-time macros (Makefile defaults both `0`):

- `RLV_ENABLE_ALTERNATE_ROWS`
- `RLV_ENABLE_ADAPTIVE_ROW_PEN` (requires alternate rows)

## Requested vs effective mode

| Requested | Effective when successful | Notes |
|-----------|---------------------------|-------|
| STANDARD | STANDARD | No alternate pen |
| ALTERNATE_PEN | ALTERNATE_PEN | Caller pen borrowed if ≠ background; else STANDARD |
| ADAPTIVE | ALTERNATE_PEN | Owned adaptive pen on success |
| ADAPTIVE | ALTERNATE_PATTERN | Acquire failed / not compiled / V37 |
| ALTERNATE_PATTERN | ALTERNATE_PATTERN | Direct pattern request |

`rlv_get_row_backdrop_mode` returns the request; effective mode is
`STANDARD`, `ALTERNATE_PEN`, or `ALTERNATE_PATTERN` (never reports
`ADAPTIVE` as effective — acquisition is an implementation detail).

## Adaptive colour algorithm and rejection

1. Require `GfxBase->LibNode.lib_Version >= 39` and a borrowed ColorMap
   (`rlv_backend_v36_set_colormap`).
2. Read background RGB via `GetRGB32` (8-bit approx from high byte).
3. Target = background × (100 − `RLV_ADAPTIVE_ROW_DARKEN_PERCENT`) / 100
   (default 10%).
4. `ObtainBestPen` with left-justified 32-bit RGB (default tags).
5. Reject if:
   - same pen as background;
   - weighted luma delta vs background < `RLV_ADAPTIVE_MIN_LUMA_DELTA` (10);
   - squared RGB distance to target > `RLV_ADAPTIVE_MAX_TARGET_DIST_SQ`;
   - too close to text / fill / shine / shadow pens;
   - text/alternate luma contrast < `RLV_ADAPTIVE_MIN_TEXT_LUMA_DELTA` (24).
6. On reject, `ReleasePen` and fall back to `ALTERNATE_PATTERN`.

Thresholds are named internal constants intended for real-hardware tuning.

### Patterned fallback paint

Odd logical rows under `ALTERNATE_PATTERN` use the same sparse FILLPEN /
BACKGROUNDPEN stipple density as the title sparse-blue style, via
`fill_rect_pattern`. Body text (and ellipsis text) use `draw_text_jam1` so
glyphs do not punch solid rectangles through the stipple. Selection,
even rows, gaps, and empty viewport remain solid. Checkbox/disclosure
chrome still clears with `background` (solid grey interior).

## Pen ownership / lifecycle

| Pen | Ownership |
|-----|-----------|
| Caller `ALTERNATE_PEN` | Borrowed; never released |
| Adaptive obtained | Owned; released once on teardown / refresh |
| Semantic background | Borrowed; never released |

`rlv_set_pens` and `rlv_set_row_backdrop` refresh resolution (release then
re-acquire as needed). Partial create failure and destroy remain safe.

Hosts must call `rlv_backend_v36_set_colormap` before adaptive mode can
succeed (demo sets `win->WScreen->ViewPort.ColorMap`).

## Rendering precedence

1. Selected fill (`RLV_CURRENT_ROW_VISUAL_FULL`) — selected pens.
2. Effective alternating mode on non-selected body content (logical parity).
3. Standard `pens.background` — empty space, gaps, title (title fill separate).

No body pattern mode exists; title fills are orthogonal and unchanged.

## Demo controls

- Settings → **Row backdrop** (compiled only with alternate rows):
  Standard / Caller pen stripes / Adaptive darker
- Caller pen: borrows `SHADOWPEN` when distinct from text+background;
  otherwise `FILLPEN` (avoids pure-black RTG shadow pens)
- Status text notes adaptive → pattern fallback and fill-pen fallback


## Build matrix

| Config | Compile | Link | Notes |
|--------|---------|------|-------|
| Both off (default) | OK | OK | No alt/adaptive objects |
| Alternate on, adaptive off | OK | OK | `rlv_alternate_rows.o` only |
| Both on | OK | OK | + `rlv_adaptive_row_pen.o` |
| Alternate + nosmart | OK | OK | Isolated nosmart tree |
| `public-header-audit` | OK | — | Default macros |

Emulator / physical Amiga visual runs were **not** performed in this session.

## Measured sizes (VBCC +aos68k, demo executable)

| Variant | `bin/rich-listview-demo` | Δ vs baseline |
|---------|--------------------------:|--------------:|
| Baseline (both off) | 59828 | — |
| Alternate only | 61072 | +1244 |
| Full adaptive | 62668 | +2840 |

| Object | Baseline | Alt-only | Full |
|--------|---------:|---------:|-----:|
| `rlv.o` | 7668 | 8344 | 8348 |
| `rlv_render.o` | 13432 | 13500 | 13500 |
| `rlv_alternate_rows.o` | (absent) | 1132 | 1336 |
| `rlv_adaptive_row_pen.o` | (absent) | (absent) | 2196 |
| `rlv_backend_amiga_v36.o` | 7484 | 7484 | 7712 |
| `rich_listview_demo.o` | 27928 | 29152 | 29324 |

Disabling both features restores the baseline object set (no alt/adaptive
`.o` linked). Executable size matches the measured baseline build above.

## Deviations from the prompt

- Effective mode after adaptive success is reported as `ALTERNATE_PEN`
  (the render path only needs that), not as `ADAPTIVE`. Requested mode
  remains queryable for demos/status.
- Adaptive uses varargs `ObtainBestPen(..., TAG_DONE)` because the VBCC
  SDK’s `ObtainBestPenA` macro rejected inline tag pairs.
- ColorMap is supplied explicitly via `rlv_backend_v36_set_colormap`
  rather than reading from `BitMap` (no `bm_ColorMap` field on classic
  BitMap).
- No dedicated Makefile demo target for alternate rows; pass
  `RLV_ENABLE_ALTERNATE_ROWS=1` / `RLV_ENABLE_ADAPTIVE_ROW_PEN=1` on
  existing targets (same pattern as smart-scroll override).

## Real-hardware tuning recommendations

- Re-test thresholds on four-colour Workbench 2.x (expect STANDARD fallback),
  AGA Workbench 3.x, and RTG.
- Confirm shadow-pen stripes remain readable on the demo’s palette.
- Tune `RLV_ADAPTIVE_ROW_DARKEN_PERCENT` and luma/distance constants after
  visual checks; keep logging build for `ROW_BACKDROP` messages.
- Verify adaptive pen is released exactly once across mode changes and
  window close (logging build).
