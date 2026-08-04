# RichListview — Dev Log

## 2026-08-04 — Demo Settings menu + Apply workflow

Moved divider / X pad / Y pad / row-gap visual test controls from bottom-row
cycle gadgets into a **Settings** menu with pending vs applied state.
**Apply** commits all pending values in one recreate + full paint + scroller
sync and stays disabled while pending matches applied. **Reset to defaults**
updates pending only. Startup defaults unchanged (dotted, X=1, Y=2, gap=1).

**Build:** `make rich-listview-demo` OK (`bin/rich-listview-demo` 54676 bytes;
was 54156). Not run under emulation in this session.

## 2026-07-31 — Expandable / collapsible rows

Implemented explicit per-row disclosure for RichListview: `+/-` custom cell
control, compact one-line collapsed layout, full wrap when expanded,
viewport anchoring, Collapse All, and Left/Right keyboard — independent of
checkbox, selection, and current-row visual state.

**Delivered**
- `RLV_ROW_EXPANDABLE` / `RLV_ROW_EXPANDED`, `RLV_COL_TYPE_DISCLOSURE`
- `rlv_expand_row` / `rlv_collapse_row` / `rlv_toggle_row` / `rlv_collapse_all`
- `rlv_expand.c` + `rlv_disclosure.c`; gate `RLV_ENABLE_EXPANDABLE_ROWS` (default on)
- `CELL_CONTROL` with `DISCLOSURE` + `EXPANDED`/`COLLAPSED` (mouse/keyboard only)
- Demo: disclosure column, mixed rows, Right/Left/`C`, status-line events
- Docs: overview §2.9, demo README, wishlist §2 status, implementation report

**Build:** `make rich-listview-demo` clean rebuild OK (demo ~52 KB; expand+disclosure
objects ~6 KB). Not run under emulation in this session.

**Session fixes**
- Disclosure column width: demo font-scaling remapped col 0 to `2 * font_w`
  (was overwriting with Name width).
- Single-line expandable rows (e.g. Zeta): suppress `+/-` when wrap is one line.
- Faster expand/collapse paint: `rlv_render_from_row` + smart-scroll
  `ScrollRaster` of content below the toggled row (row + exposed band only).
- Selection bleed on collapse: blit split at `min(old_h, new_h)` so the freed
  strip is overwritten (selected Beta no longer paints into Gamma).

**Report:** `docs/RICHLISTVIEW_EXPANDABLE_ROWS_IMPLEMENTATION_REPORT.md`

## 2026-07-31 — Control activation without full-row highlighting

Implemented the first wishlist item: embedded checkboxes can activate without selecting or fully highlighting their logical row.

**Delivered**
- Opt-in `RLV_CONTROL_ACTIVATE_KEEP_CURRENT` (default remains legacy `SELECT_ROW`)
- Current-row visuals: `FULL` / `MARKER` / `NONE`
- `rlv_render_cell_control()` for checkbox-only repaint with safe row/viewport fallback
- Demo keys `A` / `V` and CLI `KEEPCURRENT` / `MARKER` / `NOVISUAL`
- Docs, overview, wishlist status, and implementation report

**Follow-up fix**
- First post-change binaries crashed at layout rebuild (`Software Failure #8000000B`) because new `RLV_Control` fields were inserted mid-struct while the Makefile did not rebuild all objects on header change. Fields moved to the end of the struct; Makefile gained header dependencies; clean rebuild resolved it.

**Report:** `docs/RICHLISTVIEW_CONTROL_ACTIVATION_POLICY_IMPLEMENTATION_REPORT.md`
