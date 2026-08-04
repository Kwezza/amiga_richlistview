# RichListview — Stable Per-Logical-Row Tag Implementation Report

**Date:** 2026-08-04  
**Scope:** Complete opaque per-logical-row identity for every current row-related event.  
**Sorting / filtering / paging:** not implemented.

---

## 1. Executive summary

The repository already exposed `RLV_Row.user_data` (`APTR`) and
`RLV_Event.row_user_data`, but only `RLV_EVENT_CELL_CONTROL` reliably filled
the event field. This change formalises that field as the row’s opaque tag /
stable application identity and propagates it through every current
row-related event via a single helper, `rlv_event_set_row()`.

No second identity field (`row_tag`) was added. Structure sizes are unchanged.
The demo now assigns distinct numeric tags (`1000 + index`), shows index and
tag in the status line for selection / activation / cell-control, uses a
duplicate Name (“Alpha”) on rows 0 and 8 to prove text is not identity, and
syncs checkbox store state by tag scan.

---

## 2. Initial audit classification

**B — PARTIAL**

| Evidence | Result |
|----------|--------|
| `RLV_Row.user_data` present | Yes (`APTR`, borrowed) |
| `RLV_Event.row_user_data` present | Yes |
| Filled on `CELL_CONTROL` (checkbox + disclosure) | Yes |
| Filled on `SELECTION_CHANGED` | **No** (before) |
| Filled on `ACTIVATED` | **No** (before) |
| Filled on `SCROLL_CHANGED` | **No** (before) |
| Documented as CELL_CONTROL-only | Yes (before) |
| Demo used field as checkbox `UBYTE *` | Yes (before) |

---

## 3. Existing behaviour discovered

- Application rows are **borrowed** by `rlv_set_rows()`; control descriptors are
  **copied** into `cell_snapshot`.
- Layout / wrap / fragments store `logical_index` only — no per-fragment tag
  copy (correct).
- Hit-testing returns a logical row; wrapped Y bands map to that same index.
- Cell commits used `rlv_fill_cell_event()`; expand used a parallel manual fill
  that already copied `user_data`.
- Selection and activation set `event->row` only.
- `rlv_clear_event()` already zeroed `row_user_data`.

---

## 4. Files changed and files added

### Changed

| Path | Role |
|------|------|
| `src/rich_listview/rich_listview.h` | Formal tag/ownership comments; event field docs |
| `src/rich_listview/rlv_internal.h` | Declare `rlv_event_set_row` |
| `src/rich_listview/rlv_input.c` | Helper + all selection / scroll / activate / cell fills |
| `src/rich_listview/rlv_expand.c` | Disclosure event uses helper |
| `src/rich_listview/rlv.c` | `set_rows` tag-presence summary log |
| `examples/rich_listview_demo/main.c` | Numeric tags, status, checkbox sync by tag |
| `examples/rich_listview_demo/README.md` | Integrator + row table updates |
| `docs/RICHLISTVIEW_OVERVIEW.md` | Ownership / tag lifetime |
| `docs/CLV_FUTURE_IMPROVEMENTS_WISHLIST.md` | §4 marked delivered |
| `docs/DevLog.md` | Entry |
| `tests/public_headers/rlv_public_core.c` | Touch `user_data` / `row_user_data` |

### Added

| Path | Role |
|------|------|
| `docs/RICHLISTVIEW_ROW_TAG_IMPLEMENTATION_REPORT.md` | This report |

---

## 5. Public API changes

**No new fields or functions in the public umbrella header.**

Semantic / documentation change only:

- `RLV_Row.user_data` is documented as the opaque per-logical-row tag.
- `RLV_Event.row_user_data` is documented as returned for all row-related
  events, not only `CELL_CONTROL`.

Internal (not for applications): `rlv_event_set_row()` in `rlv_internal.h`.

---

## 6. Final tag field name and type

| Layer | Name | Type |
|-------|------|------|
| Row | `user_data` | `APTR` |
| Event | `row_user_data` | `APTR` |

Documentation may call this the row’s **opaque tag** or **stable application
identity**. The public identifier remains `user_data` / `row_user_data` to
avoid a source-breaking rename and a second overlapping concept.

---

## 7. Why the type is safe for classic 68k

- `APTR` is the Amiga SDK machine-sized pointer type (32-bit on 68000).
- Applications may cast a `ULONG` record ID to/from `APTR`, or store a real
  pointer to an application-owned record.
- RichListview never dereferences the value.
- No `uintptr_t` / `IPTR` dependency was introduced.

---

## 8. Source-compatibility considerations

- Field order of `RLV_Row` and `RLV_Event` is **unchanged** (`user_data` /
  `row_user_data` already existed at the same positions).
- Positional C89 aggregate initialisers are unaffected by this task.
- Zero-initialised rows remain tagless (`NULL`).
- Demo behaviour change: `user_data` is no longer a direct `UBYTE *` into the
  checkbox store; applications that copied the old demo pattern should follow
  the updated integrator notes (tag + lookup, or point at a record that
  contains the Boolean).

---

## 9. Ownership and lifetime rules

- Borrowed metadata: control does not allocate, free, dereference, or
  interpret the tag.
- `NULL` / zero is valid.
- Duplicate tags are permitted (application policy).
- Lifetime matches the borrowed row array: valid until the next
  `rlv_set_rows()` or `rlv_destroy()`.
- Do not retain `RLV_Event` (or `row_user_data`) past the immediate
  `handle_input` handler.

---

## 10. Event types that now return the tag

| Event | Tag behaviour |
|-------|----------------|
| `RLV_EVENT_SELECTION_CHANGED` | Tag of new logical row |
| `RLV_EVENT_ACTIVATED` | Tag of activated (selected) row |
| `RLV_EVENT_CELL_CONTROL` | Tag of control row (checkbox / disclosure) |
| `RLV_EVENT_SCROLL_CHANGED` | Tag of `selected_row` when valid; else `NULL` |
| `RLV_EVENT_NONE` | Cleared to `NULL` by `rlv_clear_event` |

---

## 11. Invalid / no-row event semantics

- `rlv_clear_event()` sets `row = -1` and `row_user_data = NULL`.
- `rlv_event_set_row()` sets `row_user_data = NULL` when the logical index is
  out of range or rows are detached.
- No stale tag is left from a previous event fill within one `handle_input`
  call (clear runs first).

---

## 12. Logical-row versus wrapped-fragment design

- Tags live only on the application `RLV_Row`.
- Wrap fragments / layout rows / checkbox snapshots do **not** store a copy.
- Hit-test and input paths resolve a logical row first; the helper then reads
  `control->rows[logical_row].user_data`.
- Clicking any wrapped band of a tall row therefore reports the same index
  and tag.

---

## 13. Internal storage or mapping changes

- No new tables, hash maps, or per-fragment fields.
- One shared helper: `rlv_event_set_row()`.
- Optional log on `set_rows`: count of non-NULL tags.
- Optional logs when emitting selection / activation / cell-control events.

---

## 14. Demo changes

- Every logical row gets `user_data = (APTR)(1000 + i)`.
- Row 8 Name changed from “Theta” to **“Alpha”** (same visible Name as row 0,
  tags **1000** vs **1008**).
- Status examples: `Selected row 4 tag 1004`, `Activated row 4 tag 1004`,
  `Checkbox row 4 tag 1004 value 1`.
- `demo_find_checkbox_by_tag()` linearly scans the demo store by tag; repaint
  still uses `ev.row`.

---

## 15. Build commands and results

```text
make rich-listview-demo
make rich-listview-demo-log
make rich-listview-demo-bench
make rich-listview-demo-nosmart
make public-header-audit
```

All completed with **exit code 0** (VBCC `+aos68k`, `-cpu=68000`).

Isolated object trees preserved (`build/rich_listview`, `_log`, `_bench`,
`_nosmart`). Normal builds do not link `rlv_log.o` / `rlv_bench.o`.

---

## 16. Compiler warnings or errors

| Item | Resolution |
|------|------------|
| Demo unused `col_name` (warning 153) | Removed |
| Bench `rlv_handle_input` optimizer passes (warning 172) | Pre-existing; unchanged |
| Errors | None |

---

## 17. Functional tests and results

| Test | Method | Result |
|------|--------|--------|
| Public headers compile alone | `public-header-audit` | PASS |
| Tag on CELL_CONTROL paths | Code review (`rlv_fill_cell_event` / expand) | PASS |
| Tag on SELECTION_CHANGED | Code review (`rlv_nav_select` + mouse SELECT_DOWN) | PASS |
| Tag on ACTIVATED | Code review (NAV_ACTIVATE) | PASS |
| Tag on SCROLL_CHANGED | Code review (line/page/position + same-row scroll) | PASS |
| Wrapped fragment → same logical tag | Hit-test returns logical index; helper uses row array | PASS (design) |
| Duplicate Name distinct tags | Demo rows 0 / 8 | PASS (data) |
| Checkbox sync by tag | `demo_find_checkbox_by_tag` | PASS (code) |

Interactive Amiga/emulator execution was **not** available in this session
(see §19–20).

---

## 18. Regression tests and results

| Area | Result |
|------|--------|
| Compile/link all variants | PASS |
| Public API field layout | Unchanged |
| No new hot-path allocation | PASS (direct indexed read) |
| Smart-scroll / nosmart trees | Still isolated and linked |
| Logging compile-out | Normal demo does not link logger |

Visual / behavioural Amiga regression: not run here.

---

## 19. Runtime environment used

| Step | Status |
|------|--------|
| Compiled | Yes (VBCC host cross-build on Windows) |
| Linked | Yes (all four demo variants + header audit objects) |
| Run under emulation | **No** |
| Run on physical Amiga | **No** |
| Visually verified | **No** |
| Behaviourally verified | Code-path audit only |

---

## 20. Tests not performed

Maintainer checklist for UAE / real hardware:

1. Select rows 0 and 8 (both “Alpha”) — status tags **1000** vs **1008**.
2. Keyboard Up/Down selection — status shows index + tag.
3. Return activation — `Activated row N tag T`.
4. Mouse checkbox toggle and Space toggle — `Checkbox row N tag T value V`;
   Apply recreate keeps store Booleans.
5. Click each wrapped line of Epsilon / Gamma — same logical row and tag.
6. Scroll and resize — tags still match the selected / activated row.
7. Change Settings and Apply — tags remain `1000 + index` after recreate.
8. Optional: attach a temporary row with `user_data = NULL` and confirm events
   report tag 0; attach two rows with the same tag and confirm no corruption.

---

## 21. `sizeof(RLV_Row)` and `sizeof(RLV_Event)`

Measured with a VBCC object that embeds `sizeof` as initialised `ULONG`s
(`build/sizeof_check.o` DATA: `0x0E`, `0x1A`, `0x04`):

| Type | Before | After | Delta |
|------|-------:|------:|------:|
| `RLV_Row` | 14 | 14 | 0 |
| `RLV_Event` | 26 | 26 | 0 |
| `APTR` | 4 | 4 | 0 |

Fields already existed; completing propagation did not enlarge the public
structs. (VBCC packs these structs tightly; that packing predates this change.)

---

## 22. Per-logical-row and per-fragment memory impact

| Storage | Delta |
|---------|------:|
| Per attached logical row (app `RLV_Row`) | 0 (field already present) |
| Per wrap fragment | 0 |
| Per layout row | 0 |
| Per checkbox snapshot cell | 0 |
| Control instance | 0 |

---

## 23. Binary-size changes

Host-linked Amiga executables (bytes):

| Variant | Before | After | Delta |
|---------|-------:|------:|------:|
| `rich-listview-demo` | 58072 | 58652 | +580 |
| `rich-listview-demo-log` | 76268 | 78200 | +1932 |
| `rich-listview-demo-bench` | 74772 | 76124 | +1352 |
| `rich-listview-demo-nosmart` | 55416 | 56700 | +1284 |

Growth is from helper call sites, logging strings (log build), and demo status
/ tag-scan code — not from larger row structures.

---

## 24. Benchmark impact

Not measured under emulation. No new work was added to render or smart-scroll
paths. Event fill does one bounds-checked pointer copy from the already-known
logical row.

---

## 25. Compatibility and regression risks

- Applications that treated `row_user_data` as meaningful **only** on
  `CELL_CONTROL` remain correct; other events now also set it.
- Applications that copied the old demo’s “`user_data` points at `UBYTE`”
  pattern still compile; they should update if they also want numeric tags.
- No ABI field insertion / reordering.

---

## 26. Known limitations

- No select/find/sort-by-tag API (out of scope).
- Tag uniqueness is not enforced.
- Demo tag lookup is a linear scan (acceptable for nine rows).
- Interactive runtime validation pending maintainer hardware/emulator pass.

---

## 27. Preparation for later sorting and paging

The tag is stored on the application record (`RLV_Row.user_data`), not on
viewport Y, wrap fragment index, or a sticky selection index. Future sort or
page replacement can reorder or replace the borrowed row array; events will
report whatever tag belongs to the row currently attached at the reported
logical index. Selection-preservation-by-tag during sort remains a later
feature and was not added here.

---

## 28. Explicit non-goals confirmation

**Sorting, filtering, and paging were not added.**  
No tag-index tables, uniqueness enforcement, ownership callbacks, or second
identity field (`row_tag`) were introduced.
