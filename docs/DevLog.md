# RichListview — Dev Log

## 2026-08-06 — Adaptive divider Full Adaptive activation fix

Audited adaptive body-row dividers against the colours-refactor report.
Feature, paint helper, and config expand-once helper were already present.
Fixed live Full Adaptive / recreate resolve order to
title → rows → selection → divider, re-refresh divider after selection
changes, and attach ColorMap via `RLV_NEED_ADAPTIVE_COLORMAP`. Status line
reports divider fallback under Full Adaptive. Prefer
`rich-listview-demo-adaptive` (stale `all-adaptive` binary lacks dividers).

**Report:** `docs/RICHLISTVIEW_ADAPTIVE_DIVIDER_ACTIVATION_REPORT.md`

## 2026-08-06 — Shared adaptive-colour engine + full adaptive mode

Consolidated the three duplicated adaptive pen backends into
`backends/rlv_adaptive_colour.*` gated by `RLV_ENABLE_ADAPTIVE_COLOURS`.
Migrated row/title/selection policy onto the engine; added adaptive body-row
divider pens (`RLV_ENABLE_ADAPTIVE_DIVIDERS`) and
`rlv_config_apply_full_adaptive_colours`. New isolated target
`rich-listview-demo-adaptive`.

**Report:** `docs/RICHLISTVIEW_ADAPTIVE_COLOURS_REFACTOR_REPORT.md`

## 2026-08-06 — Adaptive selection + title coexistence fix

Adaptive selection stopped treating the adaptive title pen as an avoid
colour. Title (~45% FILL) and selection (~65% FILL) share the same blend
family, so the proximity reject forced SYSTEM selection whenever adaptive
title was on.

## 2026-08-06 — Optional adaptive selection fill

Added compile-time `RLV_ENABLE_ADAPTIVE_SELECTION_PEN` and
`RLV_SELECTION_FILL_ADAPTIVE`: blends DrawInfo `FILLPEN` with the primary
row `BACKGROUNDPEN` via V39+ `ObtainBestPen`, with colour validation,
selected-text contrast policy, and SYSTEM fallback. Independent of row /
title adaptive pens. Demo Settings → Selection colour when compiled in.

**Report:** `docs/RICHLISTVIEW_ADAPTIVE_SELECTION_PEN_IMPLEMENTATION_REPORT.md`

## 2026-08-06 — Optional adaptive title-bar blend pen

Added compile-time `RLV_ENABLE_ADAPTIVE_TITLE_PEN` and
`RLV_TITLE_FILL_ADAPTIVE_BLEND`: blends DrawInfo `FILLPEN` (active window
title / selected fill) with `BACKGROUNDPEN` via V39+ `ObtainBestPen`, with
colour validation and grey/blue-stripe fallback. Independent of row
adaptive pens. Demo Settings → Title fill → Adaptive blend when compiled in.

**Report:** `docs/RICHLISTVIEW_ADAPTIVE_TITLE_PEN_IMPLEMENTATION_REPORT.md`

## 2026-08-06 — Row divider / gap hairline fix

With `row_gap >= 1`, solid/dotted row dividers were drawn on the last
content pixel while the gap band still painted a background pixel below
them, leaving a 1 px grey line above the next row fill. Dividers now
occupy the first gap pixel; gap background restore skips that pixel.

## 2026-08-06 — Adaptive row backdrop patterned fallback

When `RLV_ROW_BACKDROP_ADAPTIVE` cannot acquire a shared pen (V37, reject,
or adaptive not linked), effective mode is now
`RLV_ROW_BACKDROP_ALTERNATE_PATTERN`: sparse FILLPEN stipple on
BACKGROUNDPEN for odd logical rows with JAM1 body text. Selection and
even rows remain solid.

## 2026-08-06 — Optional adaptive alternating row backdrops

Added compile-time `RLV_ENABLE_ALTERNATE_ROWS` / `RLV_ENABLE_ADAPTIVE_ROW_PEN`,
`RLV_RowBackdropMode`, config + setters, logical-row parity rendering,
and an Amiga V39+ adaptive darker-pen helper with colour validation and
safe patterned fallback. Demo Settings → Row backdrop exercises the modes.

**Report:** `docs/RICHLISTVIEW_ALTERNATE_ROWS_IMPLEMENTATION_REPORT.md`

## 2026-08-06 — Configurable title-row fill patterns

Added `RLV_TitleFillStyle` (solid default, grey/blue stripes, grey/white
stripes), `RLV_Config.title_fill_style`, `rlv_set_title_fill_style` /
`rlv_get_title_fill_style`, optional `fill_rect_pattern` draw op, internal
descriptor table in `rlv_title_fill.c`, and `RLV_RENDER_HEADER_ONLY`.
Demo Settings menu includes Title fill choices.

**Report:** `docs/RICHLISTVIEW_TITLE_FILL_IMPLEMENTATION_REPORT.md`

## 2026-08-05 — Column-resize preview refinement (4 px snap, pair preview)

Audited the working drag preview. Divider flash came from shrink-path
`rlv_render_header_column_area` redrawing the committed cell frame (and
sort glyph) at the old divider. Replaced mid-drag painting with a dedicated
two-title interior preview, four-pixel relative quantisation, sort
suppression during drag, and a body-only `COMPLEMENT` guide.

**Report:** `docs/RICHLISTVIEW_COLUMN_RESIZE_PREVIEW_REFINEMENT_REPORT.md`

## 2026-08-05 — Column-resize drag preview redraw (7 MHz)

Drag moves no longer restore full left/right headers with black titles.
Arm paints a white clipped title once; each move erases the XOR guide,
applies a dirty-strip delta (grey left of committed divider; clipped
`rlv_render_header_column_area` for exposed right-header pixels only),
redraws the white title, then draws the guide.

**Build:** `make rich-listview-demo-sort-resize` and
`rich-listview-demo-sort-resize-log` linked OK. Emulator/hardware visual
retest on A500+ still required (black-under-white and right-column flash
should be gone).

## 2026-08-05 — Fix Address Error after column resize (A500+ / 68000)

On Workbench 2.x / A500+, `rich-listview-demo-sort-resize-log` crashed with
Software Failure **error `#80000003`** (Address Error) immediately after a
successful column-resize commit.

`PROGDIR:rlv.log` showed arm → commit → full paint → scroller sync all
completing, then a post-drag `IDCMP_MOUSEMOVE` with odd
`IAddress=0xc8001b`. With `WFLG_REPORTMOUSE` enabled during the drag,
Intuition delivers moves whose `IAddress` is **not** a `Gadget*`. The demo
treated every `MOUSEMOVE` as a gadget-class message, cast that pointer, and
read `GadgetID` — unaligned access on 68000.

**Fix (demo only):** `demo_idcmp_is_gadget_class` is limited to
`GADGETUP` / `GADGETDOWN`. `demo_gadget_from_imsg` resolves a gadget from
`IAddress` only for those classes (and rejects odd addresses); scroller
`MOUSEMOVE` is accepted only when `IAddress` equals the known scroller
pointer. Logging no longer dereferences report-mouse addresses.

Also in the same session: resize preview title uses grey `background` +
`shine` text (avoids blue fill flash on each move).

Rebuild: `make rich-listview-demo-sort-resize-log` (and non-log resize
targets). Retest on A500+ recommended.

## 2026-08-05 — Optional interactive column resizing

Added `RLV_ENABLE_COLUMN_RESIZE` (default off) with separately linked
`rlv_column_resize.o`. Control-owned runtime widths; two-column exchange
(pair total constant); XOR guide + clipped shine title on grey header;
divider hit zone before sorting; `RLV_EVENT_COLUMN_RESIZED` with regional
vs full repaint hint. Demo: `make rich-listview-demo-colresize` and
`make rich-listview-demo-sort-resize` (disclosure/On locked; key R reset;
right-button cancel; `WFLG_REPORTMOUSE` while dragging).

**Build:** public-header-audit, default, nosmart, sort, colresize,
sort-resize, sort-resize-log OK. Geometry host test passed.
Default demo 57360; colresize 62640 (+5280); sort 63700; sort-resize
69088 (+5388 vs sort). `rlv_column_resize.o` ~7036.

**Report:** `docs/RICHLISTVIEW_COLUMN_RESIZING_IMPLEMENTATION_REPORT.md`

## 2026-08-04 — Demo Type column → Date (DateStamp + context)

Sorting demo: former Type column is Date (`DD-Mon-YYYY` display).
`RLV_SORT_CUSTOM` compares Amiga `DateStamp` via `RLV_SortSpec.context`
(`DemoSortRecord`). Description no longer carries timestamp decoration.
Equal keys: Beta/Zeta `17-Jan-2025`. Builds: normal / sort / sort-log +
header audit OK. Amiga `rlv.log` 15:38 verified Date DESC/ASC, equal keys,
barrier, and non-lexical chronological order.
Reports: sorting + date-sorting readiness updated.

## 2026-08-04 — Date-sorting readiness audit

Audited then converted: Date column uses source-indexed `DateStamp` records
via `RLV_SortSpec.context` (not cell text, not `user_data`). Verdict now
**READY**. `docs/RICHLISTVIEW_DATE_SORTING_READINESS_REPORT.md`

## 2026-08-04 — Sorting policy clarifications

DESC merge now flips only the sign test (no `cmp = -cmp`). Barriers are
`RLV_ROW_SORT_FIXED` only. Documented: no auto-resort on cell edits;
`set_rows` clears active sort (identity, specs kept); non-sortable header
hits consumed without event; `rlv_clear_sort` clears indicator state and
anchors viewport like `rlv_sort` (silent). Report §§10–17 updated.

## 2026-08-04 — Optional attached-page sorting

Added `RLV_ENABLE_SORTING` (default off) with separately linked `rlv_sort.o`,
stable iterative merge sort over a view↔source map (borrowed rows never
reordered), header-click + triangle indicator, text/numeric/boolean/custom
kinds, and `RLV_ROW_SORT_FIXED` / nonselectable barriers. Public row indices
remain source/attachment order; tags preserved. Profile:
`make rich-listview-demo-sort`.

**Build:** normal/log/bench/nosmart/sort + `public-header-audit` OK.
Normal demo 59252 bytes; sort demo 65820 (+6568); `rlv_sort.o` 8128.
Not run under emulation.

**Report:** `docs/RICHLISTVIEW_SORTING_IMPLEMENTATION_REPORT.md`

## 2026-08-04 — Stable per-logical-row tags

Completed the existing `RLV_Row.user_data` / `RLV_Event.row_user_data`
contract so every row-related event returns the opaque tag alongside the
logical index (`rlv_event_set_row`). No second identity field. Demo uses
numeric tags `1000+` (duplicate Name “Alpha” on rows 0 and 8) and syncs
checkbox state by tag scan.

**Build:** all variants + `public-header-audit` OK. Normal demo ~58656 bytes
(was 58072). `sizeof(RLV_Row/Event)` unchanged (14 / 26). Not run under
emulation.

**Report:** `docs/RICHLISTVIEW_ROW_TAG_IMPLEMENTATION_REPORT.md`

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

## 2026-08-06 — Column-resize horizontal pointer

Added V36 `SetPointer()` / `ClearPointer()` resize cursor gated by
`RLV_ENABLE_COLUMN_RESIZE`: hover over valid header dividers, retained
during drags, host sync via `rlv_column_resize_wants_pointer()` and
`rlv_backend_v36_sync_column_resize_pointer()`.
