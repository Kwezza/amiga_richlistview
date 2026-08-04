# RichListview — Dev Log

## 2026-08-04 — Default collapsed-content ellipsis on

Control and demo now default `RLV_ELLIPSIS_COLLAPSED_CONTENT` on
(horizontal clip still off). Demo Settings → Ellipsis checks match on load.

## 2026-08-04 — Initial expand creation parameter

Added `RLV_Config.initial_expand` (`ALL_OPEN` default / `ALL_COLLAPSED`)
applied on the first `set_rows` after create. Demo Settings → **Start rows**
with default All open; Apply recreates with the chosen policy.

## 2026-08-04 — Row display / long-word / ellipsis policies

Added public display policies and compact three-dot ellipsis rendering,
wired into the existing Settings menu + Apply workflow.

**Delivered**
- `RLV_ROWS_COLLAPSIBLE` / `ALWAYS_EXPANDED` / `SINGLE_LINE`
- `RLV_LONG_WORD_CLIP` / `WRAP` (applies to `RLV_WRAP_WORD`; WORD_OR_CHAR/PATH unchanged)
- `RLV_ELLIPSIS_COLLAPSED_CONTENT` / `HORIZONTAL_CLIP` (hand-drawn dots)
- Demo menu groups + defaults (Collapsible, Clip, ellipsis off)
- Status sample `Truncated` on `RLV_WRAP_WORD` for long-word tests
- Restored corrupted `docs/RICHLISTVIEW_OVERVIEW.md` from `dec2e16` and extended §2.10

**Build:** `make rich-listview-demo` + log/bench/nosmart + `public-header-audit`
all OK. Normal demo `57372` bytes (was `54676`). Not run under emulation.

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
- Docs: overview §2.9, demo README, wishlist status, implementation report

**Build:** `make clean && make rich-listview-demo` OK. Not run under emulation
in the original session.

**Report:** `docs/RICHLISTVIEW_EXPANDABLE_ROWS_IMPLEMENTATION_REPORT.md`

## 2026-07-31 — Control activation without full-row highlighting

Implemented the first wishlist item: embedded checkboxes can activate without selecting or fully highlighting their logical row.

**Delivered**
- Opt-in `RLV_CONTROL_ACTIVATE_KEEP_CURRENT` (default remains legacy `SELECT_ROW`)
- Current-row visuals: `FULL` / `MARKER` / `NONE`
- `rlv_render_cell_control()` for checkbox-only repaint with safe row/viewport fallback
- Demo keys `A` / `V` and CLI `KEEPCURRENT` / `MARKER` / `NOVISUAL`
- Docs, overview, wishlist status, and implementation report

**Report:** `docs/RICHLISTVIEW_CONTROL_ACTIVATION_POLICY_IMPLEMENTATION_REPORT.md`
