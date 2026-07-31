# RichListview — Dev Log

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
