# RichListview Adaptive Colours Refactor — Implementation Report

## 1. Investigation findings

Three near-duplicate Amiga V39+ pen helpers existed:

| Feature | Former backend | Policy module |
|---------|----------------|---------------|
| Alternate rows | `rlv_adaptive_row_pen.*` | `rlv_alternate_rows.c` |
| Title fill | `rlv_adaptive_title_pen.*` | `rlv_title_fill.c` |
| Selection fill | `rlv_adaptive_selection_pen.*` | `rlv_selection_fill.c` |

Shared machinery (RGB extract, luma, distance, ObtainBestPen, ReleasePen, V39/ColorMap preamble) was copy-pasted three times. Feature policy (blend ratios, validation, fallbacks) differed.

## 2. Duplication inventory (pre-refactor)

Duplicated ×3: `GetRGB32` extract, luma weights 77/150/29, RGB dist², RGB32 macros, V39 gate, ObtainBestPen + reject-ReleasePen, max target dist `(48²)*3`, min text luma 24.

Feature-specific retained in policy modules: darken 10% (rows), 45/55 blend (title), 65/35 + avoid-alt + text pick (selection).

## 3. Old architecture

Per-feature backend acquire/release; control modules owned requested/effective mode and `*_owned` flags; ColorMap borrowed via `RLV_BackendV36`; refresh on `rlv_set_pens` (title → rows → selection).

## 4. New shared-engine architecture

Single Amiga-backend unit:

- `src/rich_listview/backends/rlv_adaptive_colour.c`
- `src/rich_listview/backends/rlv_adaptive_colour.h`

Gate: `RLV_ENABLE_ADAPTIVE_COLOURS`.

API: `rlv_adaptive_colour_begin`, RGB/luma/blend/darken helpers, `rlv_adaptive_colour_resolve` (validate callback), `rlv_adaptive_colour_release`.

Policy remains in control modules (and new divider module). Validate callbacks keep feature-specific thresholds and logging tags (`ROW_BACKDROP`, `TITLE_FILL`, `SELECTION_FILL`, `ROW_DIVIDER`).

## 5. Backend boundary

Unchanged: generic code uses opaque `UWORD` pens via `RLV_Pens` / `RLV_DrawOps`. Engine casts `draw_context` to `RLV_BackendV36` and borrows ColorMap. No Amiga types in public headers. Pens remain per-control owned shares.

## 6. Compile-time gates

| Macro | Role |
|-------|------|
| `RLV_ENABLE_ADAPTIVE_COLOURS` | Shared engine object |
| `RLV_ENABLE_ADAPTIVE_ROW_PEN` | Row policy (requires `ALTERNATE_ROWS` + engine) |
| `RLV_ENABLE_ADAPTIVE_TITLE_PEN` | Title policy (requires engine) |
| `RLV_ENABLE_ADAPTIVE_SELECTION_PEN` | Selection policy (requires engine) |
| `RLV_ENABLE_ADAPTIVE_DIVIDERS` | Body-row divider policy (requires engine) |

`RLV_NEED_ADAPTIVE_COLORMAP` derives from `RLV_ENABLE_ADAPTIVE_COLOURS`.

## 7. Dependency rules

- Makefile auto-sets `RLV_ENABLE_ADAPTIVE_COLOURS := 1` when any feature is 1.
- `rlv_features.h` `#error` if a feature is on without the engine (or row pen without alternate rows).
- Engine object linked once per object tree.

## 8–10. Migrated row / title / selection

Acquire/validate logic moved into `rlv_alternate_rows.c`, `rlv_title_fill.c`, and `rlv_selection_fill.c` using the shared engine. Former `rlv_adaptive_*_pen.*` files deleted. Public enums/APIs preserved. Fallbacks unchanged (PATTERN / GREY_BLUE / SYSTEM).

## 11. Adaptive divider

New: `src/rich_listview/rlv_adaptive_divider.c` (always linked; policy gated).

- `RLV_RowDividerPenMode`: SYSTEM / ADAPTIVE
- `RLV_Config.row_divider_pen_mode`
- `rlv_set/get_row_divider_pen_mode`, effective getter when gated
- Source: alternate-row pen if active, else background; blend ~78% source + ~22% shadow
- Fallback: borrowed `pens.separator`
- Paint: only `rlv_draw_row_divider` via `rlv_row_divider_pen(c)`
- Column verticals, title frames, resize guides unchanged

Refresh order: title → alternate rows → **selection** → **divider**
(divider last so near-selection validation sees the final fill pen).

## 12. Full adaptive mode

`rlv_config_apply_full_adaptive_colours(RLV_Config *cfg)` expands once into available adaptive fields. Not a persistent override. Demo **Visual colours**: Standard / Full Adaptive / Custom.

## 13. Public API changes

- `RLV_RowDividerPenMode`, `row_divider_pen_mode` on `RLV_Config`
- Divider pen get/set (+ effective when compiled)
- `rlv_config_apply_full_adaptive_colours`
- Existing adaptive APIs unchanged

## 14. Ownership and cleanup

Borrowed: DrawInfo / caller pens / fallback separator. Owned: ObtainBestPen shares with `*_owned` flags. Release on refresh, teardown, destroy. Rejected pens released inside resolve.

## 15. Files added / removed / changed

**Added:** `rlv_adaptive_colour.c/.h`, `rlv_adaptive_divider.c`, this report.

**Removed:** `rlv_adaptive_row_pen.*`, `rlv_adaptive_title_pen.*`, `rlv_adaptive_selection_pen.*`.

**Changed:** `rlv_features.h`, `rlv_internal.h`, `rich_listview.h`, `rlv.c`, `rlv_alternate_rows.c`, `rlv_title_fill.c`, `rlv_selection_fill.c`, `rlv_render.c`, `Makefile`, demo `main.c`, overview/DevLog/demo README.

## 16. Demo / menu changes

Settings additions:

- Row divider colour (System / Adaptive)
- Visual colours (Standard / Full Adaptive / Custom)

Individual adaptive choices mark Custom. Full Adaptive expands once via the
helper, then applies live setters in order title → rows → selection → divider
with a single redraw.

## 17. Build matrix

| Build | Result |
|-------|--------|
| Default (all adaptive off) | Compiled + linked |
| `rich-listview-demo-adaptive` (all on, isolated tree) | Compiled + linked |
| `-log` | Compiled + linked |
| `-bench` | Compiled + linked |
| `-nosmart` | Compiled + linked |

Engine appears once under `build/rich_listview_adaptive/backends/rlv_adaptive_colour.o`. Default tree does not link the engine.

## 18. Binary-size comparisons

| Binary | Size (bytes) |
|--------|--------------|
| `rich-listview-demo` (adaptive off) | 61896 |
| `rich-listview-demo-adaptive` (all on + divider) | 71596 |
| Pre-refactor `rich-listview-demo-all-adaptive` (no divider) | 69460 |
| `-nosmart` | 59912 |
| `-log` | 82320 |

Adaptive delta vs default: +9700 bytes (includes new divider policy + engine once + four feature policies).

## 19. Duplicate code removed

Three backend units (~12–17 KB source each) replaced by one engine object (2536 bytes) plus policy embedded in existing modules. Feature objects no longer contain ObtainBestPen machinery.

## 20. Per-instance memory impact

When `RLV_ENABLE_ADAPTIVE_DIVIDERS=1`: +3 `UWORD` + 1 `BOOL` on `RLV_Control` (~8 bytes). Existing title/selection/row ownership fields unchanged.

## 21. Benchmark results

Not re-run under emulation in this pass. No redraw-path RGB/ObtainBestPen introduced; adaptive resolve remains on `rlv_set_pens` / mode changes only.

## 22. Tests performed and environment

| Check | Status |
|-------|--------|
| Compiled | Yes (VBCC +aos68k) |
| Linked | Yes (default, adaptive, log, bench, nosmart) |
| Emulator / hardware visual | Not run in this session |
| ObtainBestPen string in default binary | Absent |
| Engine object once in adaptive tree | Verified |

## 23. Regressions checked (compile-time)

Default appearance path unchanged (adaptive gates default 0). Smart scroll / sort / resize gates untouched. Divider paint scope limited to body row dividers.

## 24. Known limitations

- Adaptive pens still require graphics.library V39+ and a usable ColorMap.
- Full adaptive is expand-once; mixed fallback is per-feature effective mode.
- Adaptive divider intentionally does not recolour column separators or title frames.
- Single-feature make overrides into the default object tree still require a clean rebuild to avoid stale objects; use `rich-listview-demo-adaptive` for the all-on matrix.

## 25. Future tuning points

Named constants in policy modules: darken %, title/selection/divider blend ratios, luma deltas, max target distance, near-black divider reject threshold.
