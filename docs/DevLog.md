# RichListview — Dev Log

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

**Follow-up fixes (same session)**
- Demo font-scaling overwrote disclosure column 0 with Name's `10 * font_w`;
  remapped to `2 * font_w` disclosure + correct Name..On column indices.
- Disclosure `+/-` suppressed when wrap cache is a single line (e.g. Zeta) so
  expand is not a visual no-op; `RLV_ROW_EXPANDABLE` may still be set.
- `rlv_render_from_row` — disclosure toggles paint from the changed row
  downward when scroll is unchanged (was full viewport; visible flash on 7 MHz).
- Expand/collapse may `ScrollRaster` content below the toggled row (reuse
  smart-scroll blit) and repaint only the row + exposed band; Alpha-at-top no
  longer forces a full viewport redraw when the blit succeeds.
- Collapse blit started at the *old* row bottom, so the freed strip kept the
  tall selection fill and bled into the next row (selected Beta → Gamma).
  Fixed: shift origin is `screen_top + min(old_h, new_h)` so collapse
  overwrites that strip. Rebuild OK.

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
