# RichListview Adaptive Row-Divider Activation — Completion Report

## 1. Audit findings

Feature inventory matches the adaptive-colours refactor report:

| Item | Status |
|------|--------|
| `RLV_ENABLE_ADAPTIVE_DIVIDERS` | Present (`rlv_features.h`, Makefile) |
| `RLV_ROW_DIVIDER_PEN_SYSTEM` / `ADAPTIVE` | Present |
| `RLV_Config.row_divider_pen_mode` | Present |
| Requested / effective / owned pen state | Present on `RLV_Control` |
| Getters / setters / teardown | Present |
| Shared engine + `rlv_adaptive_divider.c` | Present |
| Body paint via `rlv_row_divider_pen(c)` | Present in `rlv_draw_row_divider` only |
| `rlv_config_apply_full_adaptive_colours` sets divider | Present when gated |

Discrepancies / gaps found:

1. **Demo Full Adaptive live path** applied title → selection → **divider** → rows. Divider could resolve before the alternate-row source existed; rows did re-refresh the divider afterward, but the path fought the intended order and issued four redraws.
2. **Core refresh order** was title → rows → **divider** → selection. Divider near-selection validation therefore used the *previous* selection fill, then selection re-resolved without a final divider pass (except via `set_row_backdrop`).
3. **Demo ColorMap attach** listed row/title/selection gates only — omitted `RLV_NEED_ADAPTIVE_COLORMAP` (divider-only / colours-only builds would skip ColorMap).
4. **Stale binary** `bin/rich-listview-demo-all-adaptive` (69460 bytes) has **no** `ROW_DIVIDER` strings — pre-divider all-adaptive build. Current target is `rich-listview-demo-adaptive` (with divider).
5. Default / log / bench / nosmart builds keep adaptive macros at 0 (expected).

## 2. Report vs source

The refactor report was largely accurate on API, gates, paint scope, blend policy (~78/22), and fallback. It drifted on:

- live Full Adaptive apply order in the demo;
- ideal refresh order (divider should follow final selection as well as rows);
- ColorMap host wiring completeness for divider-only engine use.

## 3. Root cause

Adaptive divider **implementation existed** and Full Adaptive **did request** `RLV_ROW_DIVIDER_PEN_ADAPTIVE` when compiled in. Runtime “still looks like system separator” came from:

1. Testing risk: stale `all-adaptive` binary without the divider feature.
2. Live/create resolve ordering that could resolve the divider before alternate-row + final selection pens were current (fallback to `pens.separator` / SHADOWPEN looks identical to “system”).
3. ColorMap attach not keyed off `RLV_NEED_ADAPTIVE_COLORMAP`.

Drawing path itself was already correct for body horizontal dividers.

## 4. Files changed

- `src/rich_listview/rlv.c` — refresh order; divider refresh after selection changes
- `src/rich_listview/rlv_adaptive_divider.c` — reject candidate == separator
- `examples/rich_listview_demo/main.c` — Full Adaptive / Reset / recreate / startup order; ColorMap gate; single redraw for presets
- `docs/RICHLISTVIEW_ADAPTIVE_COLOURS_REFACTOR_REPORT.md` — order note
- `docs/DevLog.md` — entry
- this report

## 5. Full Adaptive wiring

`rlv_config_apply_full_adaptive_colours` still sets all available adaptive fields including `row_divider_pen_mode = ADAPTIVE`.

Demo **Visual colours → Full Adaptive** now:

1. expands config once;
2. applies live: title → row backdrop → selection → divider;
3. one full `rlv_render`;
4. keeps menu checks on Full Adaptive (does not bounce through Custom helpers);
5. reports divider fallback in the status line when effective ≠ adaptive.

Custom individual **Row divider colour** still marks Custom and calls `rlv_set_row_divider_pen_mode`.

## 6. Pen-resolution order

Authoritative order is now:

```text
title → alternate rows → selection → divider
```

Applied in `rlv_set_pens`, `rlv_set_row_backdrop` (selection then divider), `rlv_set_selection_fill_mode` (re-refresh divider), and demo preset/recreate/startup paths.

## 7. Drawing-path corrections

None required. Body dividers already use `rlv_row_divider_pen(c)`. Column separators, title frames, and resize guides still use `pens.separator` / other pens unchanged.

## 8. Fallback behaviour

Unchanged policy: on acquire/validate failure, effective SYSTEM, borrowed `pens.separator`, `owned = FALSE`. Added explicit reject when ObtainBestPen returns the separator pen index.

## 9. Ownership verification

Release-before-refresh, teardown, reject-ReleasePen inside the shared engine, and no release of the fallback separator remain as previously implemented.

## 10. Menu changes

No new items. Full Adaptive / Reset apply order corrected; ColorMap uses `RLV_NEED_ADAPTIVE_COLORMAP`. Adaptive divider submenu remains compile-gated.

## 11. Build results

| Build | Result | Size (bytes) |
|-------|--------|--------------|
| `rich-listview-demo` (adaptive off) | Compiled + linked | 62020 |
| `rich-listview-demo-adaptive` (all on) | Compiled + linked | 71880 |
| `rich-listview-demo-log` | Compiled + linked | 82448 |
| `rich-listview-demo-bench` | Compiled + linked | 80164 |
| `rich-listview-demo-nosmart` | Compiled + linked | 60036 |

Use `rich-listview-demo-adaptive` for the all-on matrix; do not use stale
`rich-listview-demo-all-adaptive`. Single-feature make overrides into the
default object tree still require a clean rebuild (confirmed this session).

Engine appears once under `build/rich_listview_adaptive/backends/rlv_adaptive_colour.o`.
Default binary has no `ROW_DIVIDER` diagnostic tag string.

## 12. Visual / functional tests

| Check | Status |
|-------|--------|
| Compiled / linked (default, adaptive, log, bench, nosmart) | Yes (this session) |
| Emulator / hardware visual | Not run in this session |
| String scan: adaptive binary contains `ROW_DIVIDER` | Yes |
| String scan: default binary lacks `ROW_DIVIDER` | Yes |
| String scan: stale `all-adaptive` lacks `ROW_DIVIDER` | Yes |

## 13. Size and benchmarks

| Binary | Before (refactor report) | After this fix |
|--------|--------------------------|----------------|
| default | 61896 | 62020 (+124) |
| adaptive | 71596 | 71880 (+284) |

Delta is consistent with small wiring/order changes in `rlv.c` and the demo
(no new per-instance fields; divider policy object ~2660 bytes). Benchmarks
not re-run under emulation.

## 14. Known limitations

- Adaptive pens still need graphics.library V39+ and a host-supplied ColorMap.
- On sparse palettes, ObtainBestPen may still fall back to the system separator (often identical to SHADOWPEN/black).
- Vertical grid lines remain non-adaptive by design.
- `rich-listview-demo-log` does not enable adaptive features unless rebuilt with the adaptive CFLAGS tree.
