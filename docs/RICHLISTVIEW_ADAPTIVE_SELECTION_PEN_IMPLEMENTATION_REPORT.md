# RichListview — Adaptive Selection Fill Implementation Report

Date: 2026-08-06

## Summary

Optional selected-row fill mode that softens the Workbench selection colour
by blending system `FILLPEN` with the primary normal row background
(`BACKGROUNDPEN` / `pens.background`) through graphics.library V39+ shared-pen
APIs. Capability is determined from OS version, screen `ColorMap`, and colour
validation — not from an RTG-only detector. When omitted at compile time,
RichListview retains pixel-equivalent Workbench `FILLPEN` / `FILLTEXTPEN`
selection with no V39 pen-acquisition helper linked.

## 1. Investigation findings

- Selection paint is **not** a single function: full-row body paint in
  `rlv_render.c`, checkbox interiors in `rlv_checkbox.c`, disclosure
  interiors in `rlv_disclosure.c`, and the MARKER current-row bar all read
  selected pens.
- Gate for full-row highlight: `rlv_row_uses_selected_fill` (selected row +
  `RLV_CURRENT_ROW_VISUAL_FULL`).
- Semantic pens live in `RLV_Pens`, filled by
  `rlv_backend_v36_pens_from_drawinfo` from `FILLPEN` / `FILLTEXTPEN`.
- Adaptive title and row helpers already own the acquire/validate/release
  pattern and ColorMap borrowing via `RLV_NEED_ADAPTIVE_COLORMAP`.
- **Critical:** `pens.selected_background` must remain the system FILLPEN —
  title and row adaptive code treat it as the semantic fill source. Adaptive
  selection therefore stores owned pens separately and exposes paint helpers.

## 2. Current selection-rendering architecture

| Path | Behaviour |
|------|-----------|
| Full / viewport / logical-row paint | Selected fill then text (`JAM2`) |
| Selection-only `rlv_render_logical_rows` | Same pens; old row restored via normal/alt backdrop |
| Checkbox / disclosure | Selected fill + selected text for frame/tick |
| MARKER visual | Narrow bar uses selection fill pen |
| Pattern alternate rows | Odd-row stipple uses system FILLPEN (unchanged) |
| Title fill | Uses `pens.selected_background` as FILLPEN (unchanged) |

Selection is drawn once per logical-row band (all columns share one fill).
Control cells do not override the selected fill; they paint into it.

## 3. Adaptive-colour infrastructure reused

| Reused idea | Source |
|-------------|--------|
| V39 `GetRGB32` / `ObtainBestPen` / `ReleasePen` | Title / row helpers |
| Luma + squared RGB distance validation | Same thresholds family |
| Per-instance ownership + refresh on `set_pens` | Title / alternate rows |
| `RLV_NEED_ADAPTIVE_COLORMAP` backend accessors | Shared gate |
| Demo immediate menu apply + status fallback text | Title fill |

Helpers remain **separate link units** (no shared colour .o) so selection can
build alone without row/title adaptive objects.

## 4. Backend boundary

| Unit | Responsibility |
|------|----------------|
| `rlv_selection_fill.c` | Mode normalize, resolve/refresh, paint pens |
| `backends/rlv_adaptive_selection_pen.c` | Blend, acquire, validate, text-pen choice, release |
| `rlv_backend_amiga_v36` | Borrowed ColorMap (`RLV_NEED_ADAPTIVE_COLORMAP`) |
| Render / checkbox / disclosure | Call `rlv_selection_fill_pen` / `_text_pen` only |

Generic control code never sees `DrawInfo` or `ColorMap`.

## 5. Compile-time feature design

```c
RLV_ENABLE_ADAPTIVE_SELECTION_PEN   /* default 0; Makefile override */
```

When `0`:

- `rlv_adaptive_selection_pen.o` is not compiled or linked.
- `RLV_SELECTION_FILL_ADAPTIVE` normalizes to `SYSTEM`.
- Ownership fields compile out of `RLV_Control`.
- Demo omits the Adaptive blend selection menu item.
- Default executable does not contain the helper object.

`rlv_selection_fill.o` remains in the core object list (normalize + API
stubs path); RGB/acquisition code stays in the gated helper.

`RLV_NEED_ADAPTIVE_COLORMAP` is set when row, title, **or** selection
adaptive helpers are enabled.

## 6. Public API changes

```c
typedef enum RLV_SelectionFillMode {
    RLV_SELECTION_FILL_SYSTEM = 0,    /* default FILLPEN / FILLTEXTPEN */
    RLV_SELECTION_FILL_ADAPTIVE = 1
} RLV_SelectionFillMode;

/* RLV_Config.selection_fill_mode — zero-init = SYSTEM */

VOID rlv_set_selection_fill_mode(RLV_Control *c, UWORD mode);
UWORD rlv_get_selection_fill_mode(const RLV_Control *c); /* requested */
#if RLV_ENABLE_ADAPTIVE_SELECTION_PEN
UWORD rlv_get_selection_fill_effective_mode(const RLV_Control *c);
#endif
```

- No caller-supplied selection pen (not present previously; not added).
- Default appearance unchanged.
- Invalid / unavailable adaptive values resolve to SYSTEM.
- Adaptive success is not guaranteed.

## 7. Blend sources and chosen ratio

| Source | Role |
|--------|------|
| `pens.selected_background` (FILLPEN) | System selection fill — **65%** |
| `pens.background` (BACKGROUNDPEN) | Primary normal row background — **35%** |

One selection colour for the whole control. Odd/even adaptive row colours are
**not** blend partners; they are avoid-pens during validation when present.

Named constants: `RLV_ADAPTIVE_SEL_FILL_PERCENT` (65) /
`RLV_ADAPTIVE_SEL_BG_PERCENT` (35). Integer 68000-safe arithmetic; resolved
once outside render.

## 8. Pen acquisition

1. Require `GfxBase` ≥ 39 and a borrowed ColorMap.
2. Read FILLPEN and BACKGROUNDPEN RGB via `GetRGB32`.
3. Compute blend; `ObtainBestPen(..., TAG_DONE)`.
4. If the returned pen equals system FILLPEN, `ReleasePen` and fail
   (fall back to borrowed DrawInfo pens — never own/release FILLPEN as
   “adaptive”).
5. On validation failure, `ReleasePen` immediately.

## 9. Returned-colour validation

Reject (and release) when:

| Check | Threshold |
|-------|-----------|
| Same as background pen | exact |
| Luma vs background | &lt; `RLV_ADAPTIVE_SEL_MIN_BG_LUMA_DELTA` (10) |
| Weaker than system selection | &lt; 55% of FILLPEN–bg luma delta |
| Far from requested blend | `dist_sq` &gt; `48²×3` |
| Near shine / shadow | luma delta &lt; 8 |
| Near avoid pens (alt row) | luma delta &lt; 8 |
| Selected text contrast | see §10 |

Adaptive title fill is **not** used as an avoid-pen: both features blend
FILLPEN with BACKGROUNDPEN (title ~45%, selection ~65%), so a proximity
reject against the title pen falsely forces SYSTEM whenever adaptive title
is active.

## 10. Selected text contrast policy

1. Prefer `FILLTEXTPEN` if luma delta vs fill ≥ 24.
2. Else use `TEXTPEN` if it is better **and** ≥ 24.
3. Else reject the adaptive fill (fallback to system selection + FILLTEXTPEN).

No new text pen is allocated.

## 11. Fallback behaviour

```text
Adaptive selection fill
    ↓ unsupported / unavailable / unsuitable / unreadable
Standard Workbench FILLPEN + FILLTEXTPEN
```

Creation never fails because adaptive selection is unavailable. No requester.
Optional logging records the reason (`SELECTION_FILL adaptive ...`).

## 12. Ownership and cleanup

| Pen class | Ownership |
|-----------|-----------|
| DrawInfo FILLPEN / FILLTEXTPEN | Borrowed |
| Adaptive fill from `ObtainBestPen` | Owned; released once |
| Rejected acquire | Released immediately |

Refresh / teardown paths: `set_pens`, mode change, row-backdrop change,
title adaptive re-resolve, `rlv_destroy`. Per-instance ownership only.

## 13. Interaction with adaptive rows and title

- Selection fill completely replaces normal/alternate backdrop on selected
  rows (`rlv_row_uses_selected_fill`).
- Deselection restores logical-row parity backdrop.
- Adaptive selection resolves **after** title and row refresh so the
  alternate-row avoid-pen is current when present. Adaptive title is not
  treated as an avoid colour (shared FILL/BG blend family).
- Selection remains the strongest row-level state.

## 14. Files added and changed

**Added**

- `src/rich_listview/backends/rlv_adaptive_selection_pen.c`
- `src/rich_listview/backends/rlv_adaptive_selection_pen.h`
- `src/rich_listview/rlv_selection_fill.c`
- `docs/RICHLISTVIEW_ADAPTIVE_SELECTION_PEN_IMPLEMENTATION_REPORT.md`

**Changed**

- `rlv_features.h`, `rich_listview.h`, `rlv_internal.h`, `rlv.c`
- `rlv_render.c`, `rlv_checkbox.c`, `rlv_disclosure.c`
- `Makefile`, `examples/rich_listview_demo/main.c`
- `docs/RICHLISTVIEW_OVERVIEW.md`, `docs/DevLog.md`
- `examples/rich_listview_demo/README.md`

## 15. Demo / menu changes

Settings → **Selection colour**:

- System (default)
- Adaptive blend *(when `RLV_ENABLE_ADAPTIVE_SELECTION_PEN=1`)*

Immediate apply: `rlv_set_selection_fill_mode` + `RLV_RENDER_VIEWPORT_ONLY`.
Status line reports adaptive success or fallback. Adaptive item omitted when
compiled out. Reset restores SYSTEM.

## 16. Build matrix

| Build | Result |
|-------|--------|
| Default (`SELECTION=0`) | Linked; helper absent |
| `RLV_ENABLE_ADAPTIVE_SELECTION_PEN=1` alone | Linked |
| All adaptive row/title/selection = 1 | Linked |
| `rich-listview-demo-log` + selection | Linked |
| `rich-listview-demo-nosmart` + selection | Linked |
| `rich-listview-demo-bench` (selection off) | Linked |

## 17. Code-size measurements

Host VBCC `+aos68k` sizes (bytes):

| Binary | Size | Notes |
|--------|------|-------|
| Default OFF | 60752 | No helper .o |
| Selection ON alone | 63672 | **+2920** vs OFF |
| All adaptive features | 69460 | rows+title+selection |
| Helper `rlv_adaptive_selection_pen.o` | 2992 | gated |
| Always-linked `rlv_selection_fill.o` (OFF) | ~small | normalize only |

## 18. Memory impact

| Item | Cost |
|------|------|
| Always | +1 `UWORD` requested mode on `RLV_Control` / `RLV_Config` |
| Feature on | + effective mode, fill pen, text pen, owned flag (~8 bytes) |
| Owned pens | 0 or 1 shared ColorMap pen per instance |

No per-row allocation. No hot-path heap use.

## 19. Benchmark results

Not re-run under emulation in this session. Architecture keeps selection paint
as stored-pen `set_pens` + `fill_rect` equivalent to prior FILLPEN path when
SYSTEM is selected; RGB work runs only on create / `set_pens` / mode change.

## 20. Tests performed and environments

| Check | Status |
|-------|--------|
| Compiled (OFF / ON / all / log / nosmart / bench) | Yes |
| Linked | Yes |
| Emulator visual | **Not run** |
| Physical hardware | **Not run** |
| Behavioural | Code-path review only |

## 21. Regressions checked

- Default SYSTEM path still uses DrawInfo pens via helpers.
- Title / row adaptive continue to read unmodified `pens.selected_background`.
- Pattern alternate stipple still uses system FILLPEN.
- Feature-off build excludes helper object.

## 22. Known limitations and tuning points

- Success depends on palette depth and `ObtainBestPen` quality; 4-/8-colour
  screens often fall back to SYSTEM.
- Blend ratio and thresholds are named constants in the helper — tune after
  AGA/RTG visual testing.
- No caller selection pen API.
- Not RTG-only; V39+ graphics.library is required for adaptive success.
