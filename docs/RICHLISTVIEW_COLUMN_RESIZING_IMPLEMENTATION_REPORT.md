# RichListview — Optional Column Resizing Implementation Report

**Date:** 2026-08-05  
**Scope:** Optional interactive two-column exchange resizing with SnoopDos-style
live preview. Horizontal scrolling is not implemented.  
**Status:** Complete (compiled and linked; not run on Amiga hardware/emulation
in this session).

---

## 1. Executive summary

RichListview gains optional column resizing gated by `RLV_ENABLE_COLUMN_RESIZE`
(default **0**). A separately linked `rlv_column_resize.c` owns the drag state
machine, hit-testing, preview drawing, and commit. Ordinary demos omit the
object and allocate no runtime width arrays.

**Ownership:** borrowed `RLV_Column` arrays remain immutable. The control owns
`WORD` runtime-width and minimum arrays copied from `width_pixels` on
`rlv_set_columns`. Layout reads those runtime widths when the feature is
compiled in. Commit updates control-owned widths and emits
`RLV_EVENT_COLUMN_RESIZED`.

**Interaction:** two-column exchange (pair total constant; later columns keep
the same X). Live drag uses a reversible XOR vertical guide plus a clipped
high-contrast left-title preview without rebuilding wrap/layout. Release
commits, rebuilds layout, and chooses regional vs full body paint.

---

## 2. Investigated architecture

| Topic | Finding |
|-------|---------|
| Width storage | Borrowed `RLV_Column.width_pixels` |
| Prepared geometry | Owned `col_geom[]` / `divider_x[]` in `rlv_layout_columns` |
| Cumulative X | LTR from viewport MinX; 1px dividers; last column grows to MaxX |
| Invalidation | `set_columns` / bounds → `rlv_layout_invalidate` → wrap rebuild |
| Mouse entry | App maps IDCMP → `RLV_InputEvent` |
| Drag moves | `RLV_INPUT_POINTER_MOVE` existed as a no-op |
| Header sort | `rlv_sort_handle_header_click` on `SELECT_DOWN` |
| Regional paint | `rlv_paint_viewport_area`, `rlv_render_logical_rows` |
| XOR | Added optional `RLV_DrawOps.draw_xor_vline` (V36: `COMPLEMENT`) |
| SnoopDos sources | Not present locally |

No fundamental blocker. Last-column grow-to-fill remains compatible with
pair-total-constant exchange.

---

## 3. Files changed and added

### Added

| Path | Role |
|------|------|
| `src/rich_listview/rlv_column_resize.c` | State machine, hit-test, preview, commit, public API |
| `src/rich_listview/rlv_column_resize.h` | Module note |
| `tests/column_resize/test_resize_math.c` | Host geometry invariant checks |
| `docs/RICHLISTVIEW_COLUMN_RESIZING_IMPLEMENTATION_REPORT.md` | This report |

### Changed

| Path | Role |
|------|------|
| `rich_listview.h` | Flags, event, `RLV_INPUT_CANCEL`, APIs |
| `rlv_draw.h` / V36 backend | `draw_xor_vline` |
| `rlv_internal.h` | Runtime width / drag fields + hooks |
| `rlv.c` | Lifecycle, stubs when off |
| `rlv_layout.c` | `rlv_column_effective_width` |
| `rlv_input.c` | Integration (resize before sort) |
| `rlv_render.c` | `rlv_render_header_column` / `rlv_render_resized_columns` |
| `Makefile` | Macro + isolated trees/targets |
| Demo / docs / public-header audit | Enablement and documentation |

---

## 4. Compile-time structure

| Macro | Default | Effect |
|-------|---------|--------|
| `RLV_ENABLE_COLUMN_RESIZE` | `0` | Omit `rlv_column_resize.o`; stubs in `rlv.c` |

| Target | Flags |
|--------|-------|
| `rich-listview-demo-colresize` | resize=1, sort=0 |
| `rich-listview-demo-sort-resize` | resize=1, sort=1 |
| `rich-listview-demo-sort-resize-log` | resize+sort+logging |
| Ordinary demos / sort-only | resize=0 |

Isolated trees: `build/rich_listview_colresize/`,
`build/rich_listview_sort_resize/`, `build/rich_listview_sort_resize_log/`.

---

## 5. Public API

```c
VOID rlv_set_column_resize_enabled(RLV_Control *c, BOOL enabled);
BOOL rlv_get_column_resize_enabled(const RLV_Control *c);
BOOL rlv_column_resize_is_active(const RLV_Control *c);
BOOL rlv_get_column_width(const RLV_Control *c, UWORD column, WORD *out);
BOOL rlv_set_column_width(RLV_Control *c, UWORD column, WORD width);
BOOL rlv_set_column_widths(RLV_Control *c, const WORD *widths, UWORD count);
BOOL rlv_reset_column_widths(RLV_Control *c);
BOOL rlv_set_column_min_width(RLV_Control *c, UWORD column, WORD min_width);
BOOL rlv_render_resized_columns(RLV_Control *c, UWORD left, UWORD right);
```

- Flag: `RLV_COL_F_NO_RESIZE`
- Event: `RLV_EVENT_COLUMN_RESIZED` with `resize_*` / `old_*` / `new_*` fields
- Hint: `event->value` = `RLV_RESIZE_REPAINT_REGIONAL` or `_FULL`
- Input: `RLV_INPUT_CANCEL`

When the feature is off, setters return `FALSE` / no-ops;
`rlv_get_column_width` still reads borrowed `width_pixels`.

---

## 6. Ownership of runtime widths

On `rlv_set_columns`, the control allocates `runtime_widths[]` and
`runtime_mins[]` (default min = 16). Layout uses
`rlv_column_effective_width()`. Interactive and programmatic width changes
never write borrowed `RLV_Column` memory. Applications may copy widths from
the event or from `rlv_get_column_width` into their own store.

---

## 7. Input state machine

```text
idle
  SELECT_DOWN on divider (±3 px, both columns unlocked, feature enabled)
    → arm; draw initial XOR guide; paint white clipped title (no black
      restore); consume click (no sort / selection)
  POINTER_MOVE while armed
    → erase guide; dirty-strip delta (grey / right-strip restore only);
      white clipped title at proposed width; draw guide (no layout rebuild,
      no full left/right header restore, no black title mid-drag)
  SELECT_UP while armed
    → erase guide; commit widths; layout rebuild; COLUMN_RESIZED
  CANCEL / set_rows / set_columns / set_bounds / destroy
    → erase guide; restore headers; leave widths unchanged
```

Divider hit-testing runs **before** `rlv_sort_handle_header_click`.

---

## 8. Guide and title preview

- Guide: `draw_xor_vline` from header top through viewport bottom
  (V36: `SetDrMd(COMPLEMENT)`, pens/mode restored).
- Title: fill proposed left width with `background` (same grey as the header
  face), draw title with `shine` (system light/white), clipped via
  `push_clip` / `text_fit` to the temporary width.
- Drag moves stay in preview mode: never call `rlv_render_header_column` for
  the pair. Shrink exposes a strip; left-of-divider is grey-filled, and any
  strip at/after the committed `divider_x` is restored with
  `rlv_render_header_column_area` (right column ∩ strip only).
- Body cells stay at committed geometry during the drag.

---

## 9. Cache invalidation and redraw

| Phase | Work |
|-------|------|
| Drag move | XOR + dirty header strip(s) + white title — no wrap/layout |
| Commit | Update runtime widths → invalidate → rebuild |
| Regional | Both columns `WRAP_NONE` **and** `content_height` unchanged →
  `rlv_render_resized_columns` (two headers + body strip) |
| Full | Otherwise → `rlv_render(control, 0)` |

---

## 10. Cancellation and cleanup

`rlv_column_resize_cancel` erases a visible guide (second XOR) and redraws
the pair headers. Called from cancel input, set_rows, set_columns,
set_bounds, and destroy (destroy skips visual erase when tearing down).

---

## 11. Tests and build matrix

| Check | Result |
|-------|--------|
| `tests/column_resize/test_resize_math.c` (host) | Pass |
| `make public-header-audit` | Pass |
| `make rich-listview-demo` | Linked |
| `make rich-listview-demo-nosmart` | Linked |
| `make rich-listview-demo-sort` | Linked |
| `make rich-listview-demo-colresize` | Linked |
| `make rich-listview-demo-sort-resize` | Linked |
| `make rich-listview-demo-sort-resize-log` | Linked |
| Amiga emulator / hardware | **Not run** |

### Manual Amiga test steps (when available)

1. Run `rich-listview-demo-sort-resize` on WB 2.04 and 3.x, low-res, 68000.
2. Drag Name|Date and Date|Description dividers; confirm later columns fixed.
3. Drag into minima; confirm clamp.
4. Click header away from divider; confirm sort still works.
5. Try disclosure/On dividers; confirm rejected.
6. Rapid left/right drag; confirm no XOR residue after cancel/release;
   frame inset still preserved.
7. Drag while scrolled; wrap Description and confirm full repaint path.
8. Right-button cancel mid-drag; key `R` reset.
9. **A500+ / 7 MHz drag preview:** drag slowly left and right — black title
   must not appear under the white preview; right column must not flash while
   the guide stays inside the left cell; shrinking back across the committed
   divider must restore the right title cleanly in the exposed strip only;
   cancel and commit must leave correct framed headers with black text / sort
   glyphs.

---

## 12. Code-size impact (linked executables)

| Binary | Bytes | Delta vs baseline |
|--------|------:|-------------------|
| `rich-listview-demo` | 57360 | — |
| `rich-listview-demo-colresize` | 62640 | **+5280** |
| `rich-listview-demo-sort` | 63700 | — |
| `rich-listview-demo-sort-resize` | 69088 | **+5388** vs sort |
| `rlv_column_resize.o` | ~7036 | — |

---

## 13. Known limitations

- No horizontal scrolling (by design for this task).
- No trailing elastic resize past the last column.
- Preview requires `draw_xor_vline`; without it the guide is skipped (title
  preview still paints).
- Apps must forward `POINTER_MOVE` while dragging (demo uses
  `WFLG_REPORTMOUSE`).
- Regional paint heuristic uses wrap mode + content height only.

---

## 14. Future horizontal-scrolling compatibility

Hit-testing and guide X use window-relative `col_geom` / `divider_x`
(content coordinates mapped into the current viewport). A future
`scroll_x` would subtract from pointer X (or add to content X) before
divider hit-test and guide placement. The two-column exchange keeps total
content width unchanged, so it does not force a horizontal scrollbar.

---

## 15. Logging

When `RLV_ENABLE_LOGGING` is on: arm, clamp, commit (with regional/full),
cancel, and ReportMouse transitions. No per-pixel logging.
