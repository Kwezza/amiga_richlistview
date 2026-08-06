# RichListview — Column-Resize Preview Refinement Report

**Date:** 2026-08-05  
**Scope:** Audit of the working drag preview, then replace the mid-drag header
path with a lower-flicker dedicated renderer, four-pixel quantisation, and a
body-only reversible guide.  
**Status:** Implemented and compiled/linked for the resize build matrix.
Host geometry/snap tests pass. Not run under Amiga emulation or hardware in
this session.

---

## 1. Audit — original draw path

### 1.1 Input and drag state

| Item | Finding |
|------|---------|
| Drag begin | `rlv_column_resize_handle_select_down` after ±3 px divider hit |
| Width from mouse | `proposed = clamp(orig_left + (x - press_x))` — **1 px steps** |
| Move frequency | Every `RLV_INPUT_POINTER_MOVE` while armed; skip only if width unchanged |
| Old guide X | `c->resize_guide_x` + `resize_guide_visible` |
| Release / cancel | Second XOR erase; `rlv_render_header_column` for both columns |
| Quantisation | None (pixel-accurate) |

### 1.2 Header redraw on move

```text
erase XOR guide
rlv_cr_paint_preview_delta (shrink only):
    grey strip left of committed divider
    rlv_render_header_column_area(right, strip)   ← full cell paint ∩ clip
rlv_cr_paint_white_title(left only)
draw XOR guide
```

`rlv_render_header_column_area` filled the **committed** right cell, called
`rlv_draw_cell_frame` (restoring the **committed** divider edge), drew the
**black** title, and drew the **sort indicator** — all clipped to the exposed
strip. The ordinary full-header renderer was not used on move, but this
regional restore reintroduced committed chrome.

Only the **left** title was painted shine-on-grey. The right title stayed in
committed style until a shrink strip restored it.

### 1.3 Cause of divider flash

When the preview guide moved **left** of the committed `divider_x`, the shrink
path restored the exposed strip with `rlv_render_header_column_area`. That
redraw’s `rlv_draw_cell_frame` painted the right cell’s left edge at the
**original** divider X inside the clip, so the old dividing line briefly
reappeared before the next white-title / guide update. Sort glyphs could also
flash in the same strip.

### 1.4 Vertical guide

One **continuous** `draw_xor_vline` from `header_bounds.MinY` through
`viewport_bounds.MaxY` (header + body). V36: `SetDrMd(COMPLEMENT)`, APen `~0`,
mode/pen restored. Erase = draw the same segment again. No body row repaint
between erase and redraw.

### 1.5 Geometry (authoritative)

| Quantity | Source |
|----------|--------|
| Header outer | `c->header_bounds` |
| Bevel | 1 px via `rlv_draw_cell_frame` (full dark box, shine top+left) |
| Column content | `col_geom[].left/right`; divider at `left + width` |
| Divider width | `RLV_DIVIDER_WIDTH` (1) |
| Cell frame right | `divider_x[col]` or last-column right / viewport MaxX |
| Text padding | `cell_padding_x` / `cell_padding_y` |
| Sort reserve | `rlv_sort_header_reserve_px` (normal header only) |
| Body | `viewport_bounds` |

### 1.6 Clipping already available

- `RLV_DrawOps.push_clip` / `pop_clip` (V36 region or soft clip)
- `text_fit` / width loop for truncation
- No permanent title-string mutation

---

## 2. New behaviour

### 2.1 Four-pixel quantisation

```c
#define RLV_COLUMN_RESIZE_STEP 4
```

Symmetric toward-zero buckets relative to `resize_press_x`:

- `|raw_delta| < 4` → snapped delta `0`
- `4..7` → `+4`, `-4..-7` → `-4`
- Negative values use absolute division so C truncation does not bias one side

Pipeline: raw delta → quantise → add to `orig_left` → min-width clamp. Preview
and commit share `rlv_cr_proposed_from_x`. Motion inside the current snapped
width performs **no** drawing.

### 2.2 Dedicated pair preview (`rlv_cr_paint_pair_preview`)

Does **not** call `rlv_render_header_column` / `_area` or sort drawing.

Draw order on each snapped update:

1. Erase previous body guide (`COMPLEMENT`)
2. Clear combined interior  
   `[pair_left+1 .. pair_right-1] × [header_minY+1 .. header_maxY-1]`
3. Left title shine-on-grey, clipped to temporary left content
4. Moving header divider (separator at guide, shine at `guide+DIVIDER_WIDTH`)
5. Right title shine-on-grey, clipped to temporary right content
6. Draw new body guide
7. Clip/pens restored via existing push/pop and backend XOR save/restore

Protected: outer left of left column, outer right of right column, top shine,
bottom shadow. Neighbour headers untouched.

### 2.3 Header divider vs body guide

| Region | Method |
|--------|--------|
| Header | Normal-pen 3D-style verticals at the snapped guide (not XOR) |
| Body | Single `COMPLEMENT` line, `viewport_bounds.MinY`..`MaxY` only |

The continuous header+body XOR line was dropped so the guide cannot damage the
fixed title bevel. Body guide still erases by redrawing the same segment.

### 2.4 Sort indicators

Suppressed for the whole drag (not drawn; no reserve). Sort state unchanged.
Release / cancel restores via `rlv_render_header_column`, which draws the
indicator again.

### 2.5 Commit / cancel

Unchanged state machine aside from preview visuals:

- Release: erase guide → restore both headers → commit snapped widths → layout
- Cancel / teardown: erase guide → restore both headers → widths unchanged

---

## 3. Files changed

| Path | Change |
|------|--------|
| `src/rich_listview/rlv_column_resize.c` | Quantise; pair preview; body-only guide |
| `src/rich_listview/rlv_internal.h` | `RLV_COLUMN_RESIZE_STEP`; comment updates |
| `src/rich_listview/rlv_render.c` | Comment on unused-during-drag area helper |
| `tests/column_resize/test_resize_math.c` | Snap + preview geometry cases |
| `Makefile` | `column-resize-geometry-test` host target |
| `docs/RICHLISTVIEW_COLUMN_RESIZE_PREVIEW_REFINEMENT_REPORT.md` | This report |
| `docs/RICHLISTVIEW_COLUMN_RESIZING_IMPLEMENTATION_REPORT.md` | Preview section |
| `docs/DevLog.md` | Entry |

---

## 4. Tests and builds

| Check | Result |
|-------|--------|
| `make column-resize-geometry-test` | Pass (all snap/geometry cases) |
| `make public-header-audit` | Pass (compiled) |
| `make rich-listview-demo` | Linked |
| `make rich-listview-demo-nosmart` | Linked |
| `make rich-listview-demo-colresize` | Linked |
| `make rich-listview-demo-sort-resize` | Linked |
| `make rich-listview-demo-sort-resize-log` | Linked |
| Amiga emulator / hardware | **Not run** |
| Visual verification | **Not run** (manual steps below) |

### Manual Amiga checks (when available)

1. Drag slowly: motion updates only every 4 px; no redraw inside a bucket.
2. Confirm old committed divider does not flash; both titles white on grey.
3. Right title stays inside its temporary cell; later columns untouched.
4. Sort glyph absent during drag; returns after release/cancel.
5. Outer 3D header frame intact; body guide erases cleanly on rapid moves.
6. Sort click away from divider still works; wrap rebuild only on release.

---

## 5. Code-size impact

Linked sizes after this refinement (VBCC `+aos68k`):

| Binary | Bytes | Delta vs prior report baseline |
|--------|------:|-------------------------------:|
| `rich-listview-demo` | 57412 | +52 (unrelated rebuild churn) |
| `rich-listview-demo-colresize` | 63996 | **+1356** vs 62640 |
| `rich-listview-demo-sort-resize` | 70648 | **+1560** vs 69088 |
| `rlv_column_resize.o` | 8004 | was ~7036 |

The pair-preview path is larger than the old left-only + strip-restore
helpers; four-pixel snapping adds little. Resize-disabled demos are
essentially unchanged.

---

## 6. Remaining limitations

- Preview still requires `draw_xor_vline` for the body guide; without it the
  guide is skipped (header preview still paints).
- `COMPLEMENT` on unusual screens/ham modes is untested; residue would need a
  body repaint fallback (not added).
- Apps must still forward `POINTER_MOVE` (`WFLG_REPORTMOUSE`).
- `rlv_render_header_column_area` remains in the tree but is unused by the
  live drag path.
