# RichListview — Adaptive Title-Bar Pen Implementation Report

Date: 2026-08-06

## Summary

Optional title-row fill mode that attempts a calm solid colour by blending
the active-window title fill (`FILLPEN`) with the normal background
(`BACKGROUNDPEN`) through graphics.library V39+ shared-pen APIs. Capability
is determined from OS version, screen `ColorMap`, and colour validation —
not from an RTG-only detector. When omitted at compile time, RichListview
retains the existing Workbench 2.x-compatible patterned title fills with no
V39 pen-acquisition code linked.

## 1. Investigation findings

- Title fill is already centralised in `rlv_title_fill.c` /
  `rlv_title_fill_area`, called from full header, header-only, per-column
  resize, and resize-preview paths.
- Lightweight title modes (solid + patterns) are always linked; there is no
  `RLV_ENABLE_TITLE_FILL_MODES` gate and none was added.
- Semantic pens live in `RLV_Pens`, filled by
  `rlv_backend_v36_pens_from_drawinfo` from screen `DrawInfo`. Core code
  never sees `DrawInfo` / `ColorMap` directly.
- Alternating-row adaptive pens (`RLV_ENABLE_ADAPTIVE_ROW_PEN`) already
  provide the acquire/validate/release pattern and ColorMap borrowing via
  `rlv_backend_v36_set_colormap`.

## 2. Title rendering architecture

| Path | Fill |
|------|------|
| `rlv_draw_header` / `rlv_render_header_only` | Full header via `rlv_title_fill_area` |
| `rlv_render_header_column` / `_area` | Per-column fill (resize) |
| `rlv_cr_paint_pair_preview` | Preview fill |
| Sort / divider / frame | After fill; text uses JAM1 when patterned |

Interior fill covers the header rectangle; `rlv_draw_cell_frame` redraws the
3D bevel. Adaptive solid success paints one `set_pens` + `fill_rect`. Title
text BPen uses `rlv_title_fill_text_back_pen` so JAM2 text does not punch
background rectangles through a non-background solid face.

## 3. Active-title colour source

| Source | Role |
|--------|------|
| **`FILLPEN`** (`pens.selected_background`) | Active window borders / selected gadget fill (RKRM) — **used** |
| `BACKGROUNDPEN` | Normal background — **blend partner** |
| `BARBLOCKPEN` | Screen bar / menus (V39 `DRI_VERSION >= 2`) — **not used** |
| `BLOCKPEN` | Legacy screen title bar — **not used** |

Custom public screens without `SA_Pens` may still expose Fill/Background pens;
validation rejects unsuitable results rather than hard-coding palette indices.

## 4. Module / backend boundary

| Unit | Responsibility |
|------|----------------|
| `rlv_title_fill.c` | Mode normalize, resolve/refresh, paint choice, text back pen |
| `backends/rlv_adaptive_title_pen.c` | V39 RGB read, blend, `ObtainBestPen`, validate, `ReleasePen` |
| `rlv_backend_amiga_v36` | Borrowed ColorMap accessors (`RLV_NEED_ADAPTIVE_COLORMAP`) |
| Generic render | Calls `rlv_title_fill_area` only — no Amiga colour APIs |

Independent of `RLV_ENABLE_ADAPTIVE_ROW_PEN` (some luma helpers are duplicated
deliberately so either feature can link alone).

## 5. Compile-time feature design

```c
RLV_ENABLE_ADAPTIVE_TITLE_PEN   /* default 0; Makefile override */
```

When `0`:

- `rlv_adaptive_title_pen.o` is not compiled or linked.
- `RLV_TITLE_FILL_ADAPTIVE_BLEND` normalizes to `GREY_BLUE_STRIPES`.
- Demo omits the Adaptive blend menu item.
- Off executable contains no `ObtainBestPen` string (verified).

`RLV_NEED_ADAPTIVE_COLORMAP` is set when either adaptive row or title pen is on.

## 6. Public API changes

```c
RLV_TITLE_FILL_ADAPTIVE_BLEND = 6   /* always declared; behaviour gated */

VOID rlv_set_title_fill_style(RLV_Control *c, UWORD style);
UWORD rlv_get_title_fill_style(const RLV_Control *c);           /* requested */
#if RLV_ENABLE_ADAPTIVE_TITLE_PEN
UWORD rlv_get_title_fill_effective_style(const RLV_Control *c); /* resolved */
#endif
```

- `RLV_Config` layout unchanged (appended enum value only).
- Default / zero-init remains `RLV_TITLE_FILL_SOLID`.
- No `AUTO` mode (caller-controlled API).
- No public `ColorMap` / `DrawInfo` exposure.

## 7. Blend algorithm and weighting

```text
target = FILLPEN_RGB * 45% + BACKGROUNDPEN_RGB * 55%
```

Named constants: `RLV_ADAPTIVE_TITLE_FILL_PERCENT` (45) /
`RLV_ADAPTIVE_TITLE_BG_PERCENT` (55). Integer 68000-safe arithmetic;
resolved once outside render.

## 8. Pen acquisition and validation

1. Require `GfxBase->LibNode.lib_Version >= 39` and a borrowed ColorMap.
2. Read FILLPEN and BACKGROUNDPEN RGB via `GetRGB32`.
3. Compute blend; `ObtainBestPen(..., TAG_DONE)`.
4. Reject (and `ReleasePen`) when:
   - same pen as background or FILLPEN;
   - luma delta vs background &lt; 10;
   - luma delta vs FILLPEN &lt; 8 (avoid selection look-alike);
   - squared RGB distance to target &gt; `48²×3`;
   - too close to shine/shadow;
   - text/fill luma contrast &lt; 24.
5. On success, one owned pen per control instance.

## 9. Fallback order

```text
Adaptive solid title pen
  → GREY_BLUE_STRIPES (classic patterned title)
  → SOLID (descriptor / non-pattern backend fallback)
```

Four-colour Workbench 2.x never calls V39 allocation (version gate).

## 10. Title text contrast policy

Retain `TEXTPEN` for title glyphs. Reject adaptive fill when text/fill luma
delta is insufficient. No secondary text-pen allocation.

## 11. Ownership and cleanup

| Event | Action |
|-------|--------|
| `rlv_create` | Init requested style; defer acquire until pens/ColorMap |
| `rlv_set_pens` / `rlv_set_title_fill_style` | Release then re-resolve |
| Adaptive reject | `ReleasePen` immediately; effective = grey/blue |
| Switch away from adaptive | Release owned pen |
| `rlv_destroy` / partial failure | `rlv_title_fill_teardown` |

Caller/DrawInfo pens are never released. No global pen cache.

## 12. Files added and changed

| File | Change |
|------|--------|
| `src/rich_listview/backends/rlv_adaptive_title_pen.h` / `.c` | **New** |
| `src/rich_listview/rlv_features.h` | Macro + `RLV_NEED_ADAPTIVE_COLORMAP` |
| `src/rich_listview/rlv_title_fill.c` | Resolve/paint/text back pen |
| `src/rich_listview/rlv_internal.h` | Owned-pen instance fields |
| `src/rich_listview/rich_listview.h` | Enum + effective getter |
| `src/rich_listview/rlv.c` | Create/destroy/set_pens/setters |
| `src/rich_listview/rlv_render.c` | Text back pen |
| `src/rich_listview/rlv_column_resize.c` / `rlv_sort.c` | Matching BPen |
| `src/rich_listview/backends/rlv_backend_amiga_v36.*` | Shared ColorMap gate |
| `Makefile` | Flag + object lists for all variants |
| `examples/rich_listview_demo/main.c` | Menu + colormap + status |
| `docs/*`, demo README | Documentation |

## 13. Demo / menu changes

Settings → **Title fill** → **Adaptive blend** (`CHECKIT` exclusive group,
mask `~64`) only when `RLV_ENABLE_ADAPTIVE_TITLE_PEN=1`. Status reports
success or “adaptive fell back”. ColorMap is set whenever row or title
adaptive support is compiled in.

## 14. Build matrix and results

| Config | Compile | Link | Notes |
|--------|---------|------|-------|
| Adaptive title off (default) | OK | OK | No helper `.o` |
| Adaptive title on | OK | OK | Links `rlv_adaptive_title_pen.o` |
| Title on + logging | OK | OK | Isolated log tree |
| Title on + nosmart | OK | OK | Isolated nosmart tree |

Rebuild the default `build/rich_listview/` tree when flipping the flag on
that shared directory (same caveat as other default-tree feature overrides).

## 15. Code-size measurements (VBCC +aos68k)

| Variant | Executable | Δ vs title-off |
|---------|------------:|---------------:|
| Title adaptive off | 60248 | — |
| Title adaptive on | 62556 | +2308 |

| Object | Size |
|--------|-----:|
| `rlv_adaptive_title_pen.o` | 2316 |

Off build: helper absent; `ObtainBestPen` not present in the executable.

## 16. Per-instance memory impact

When the feature is compiled in, opaque `RLV_Control` gains approximately:

| Field | Approx. |
|-------|--------:|
| `title_fill_effective` | 2 bytes |
| `adaptive_title_pen` | 2 bytes |
| `adaptive_title_pen_owned` | 2 bytes (`BOOL`) |

Plus at most **one** owned shared pen when adaptive succeeds.
`RLV_Config` public size unchanged.

## 17. Benchmark / performance

No dedicated title-redraw timing run in this session. Adaptive work is
outside paint; success path is a solid `fill_rect` (same class as default
solid). Nosmart/logging builds linked cleanly with the feature enabled.

## 18. Tests performed and environment

| Test | Result |
|------|--------|
| Compile/link title off | OK |
| Compile/link title on | OK |
| Compile/link log + title on | OK |
| Compile/link nosmart + title on | OK |
| Helper omitted when off | OK |
| `ObtainBestPen` absent when off | OK |
| Emulator / physical Amiga visual | **Not run** |
| Workbench 2.x / AGA / RTG visual | **Required on target** |

## 19. Regressions checked (code-path audit)

Default solid and existing patterns unchanged when not selecting adaptive.
Header frame geometry, sort indicators, column resize, and row backdrops
are untouched except for title-text BPen consistency when adaptive solid
is active. Smart scroll and logging compile-out paths preserved.

## 20. Known limitations and tuning

- Does not promise a suitable pen on every eight-colour or AGA screen.
- Custom `SA_Pens` screens may fail validation and fall back to stripes.
- Blend ratio and luma/distance thresholds are named constants for
  real-hardware tuning (`TITLE_FILL adaptive` log lines in the logging build).
- Not an RTG-only feature; RTG follows the same capability path.
- Recommend visual checks on four-colour WB 2.x (expect stripes), AGA WB 3.x,
  and RTG Workbench with customised colours.
