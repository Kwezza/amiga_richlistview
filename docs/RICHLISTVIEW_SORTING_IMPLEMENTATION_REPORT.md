# RichListview — Optional Sorting Implementation Report

**Date:** 2026-08-04  
**Scope:** Optional stable view-order sorting of the currently attached row array.  
**Paging / filtering:** not implemented.

---

## 1. Executive summary

RichListview gained an optional, separately linkable sorting system gated by
`RLV_ENABLE_SORTING` (default **0**). Ordinary demos omit `rlv_sort.o` and
allocate no sort maps. The sorting build
(`make rich-listview-demo-sort`) links `rlv_sort.o` and enables header-click
sorting with a small ascending/descending triangle indicator. A logging twin
(`make rich-listview-demo-sort-log`) dumps the view↔source map to
`PROGDIR:rlv.log`.

Borrowed `RLV_Row` arrays are **never reordered**. A control-owned
`UWORD` view↔source map provides display order. Public `event->row`,
selection, checkbox, and expand APIs remain **source (attachment) indices**
so `rows[event->row]` and `row_user_data` stay coherent. Sort barriers are
`RLV_ROW_SORT_FIXED` only (headings typically also set
`RLV_ROW_NONSELECTABLE`). The algorithm is an iterative bottom-up merge
sort (stable, non-recursive).

**Attached-page only:** sorting the current `rlv_set_rows` dataset does not
globally sort a paged master catalogue.

**Verification (2026-08-04):** header-click sorting exercised on classic Amiga
OS libraries (intuition/gadtools/graphics v47-class) via
`rich-listview-demo-sort-log`. Earlier session verified Name / Pos; session
at 15:38 verified **Date** (`DateStamp` CUSTOM, `kind=6`) ASC/DESC, equal
Beta/Zeta keys, barrier, and `DEFAULT_DESC` first click. See §§22–25 / §36.

---

## 2. Initial architecture audit

| Finding | Result |
|---------|--------|
| Index space | Single logical index = `rows[]` attachment order; `layout_rows[].logical_index` was identity |
| Header | Fixed column header separate from data; no header hit-test |
| Barriers | `RLV_ROW_NONSELECTABLE` headings already in demo |
| Optional pattern | Explicit objects + feature macros + isolated trees (expand / log / bench / nosmart) |
| Row tags | Delivered; must survive sort |
| Structure enlargement | **Not required** — new flag bit + new event enum value only |

---

## 3. Files changed and added

### Added

| Path | Role |
|------|------|
| `src/rich_listview/rlv_sort.c` | Sort engine, maps, header click, indicator |
| `src/rich_listview/rlv_sort.h` | Thin optional include note |
| `docs/RICHLISTVIEW_SORTING_IMPLEMENTATION_REPORT.md` | This report |

### Changed

| Path | Role |
|------|------|
| `src/rich_listview/rich_listview.h` | Sort kinds/API, `RLV_ROW_SORT_FIXED`, `RLV_EVENT_SORT_CHANGED` |
| `src/rich_listview/rlv_internal.h` | Map fields + helpers |
| `src/rich_listview/rlv.c` | Destroy/set_rows hooks; stubs when sorting off |
| `src/rich_listview/rlv_layout.c` | View-ordered layout; source for wrap heights |
| `src/rich_listview/rlv_input.c` | View-order nav; `make_visible`; header click |
| `src/rich_listview/rlv_render.c` | Paint by source; header indicator inset |
| `src/rich_listview/rlv_checkbox.c` / `rlv_disclosure.c` | Geometry via view slot |
| `src/rich_listview/rlv_expand.c` | Reheight/anchor via view slot |
| `Makefile` | `RLV_ENABLE_SORTING`, `rich-listview-demo-sort`, `rich-listview-demo-sort-log` |
| `examples/rich_listview_demo/main.c` | Sort specs, Date/Pos data, status line |
| `tests/public_headers/rlv_public_core.c` | Touch sort enums / event |
| Docs | Overview, wishlist, DevLog, demo README, date-sorting readiness |

---

## 4. Final public API

```c
BOOL rlv_set_sort_specs(RLV_Control *c, const RLV_SortSpec *specs, UWORD count);
BOOL rlv_sort(RLV_Control *c, UWORD column, UWORD direction);
BOOL rlv_get_sort_state(const RLV_Control *c, UWORD *column_out, UWORD *direction_out);
BOOL rlv_clear_sort(RLV_Control *c);
LONG rlv_source_row_of(const RLV_Control *c, LONG view_row);
LONG rlv_view_row_of(const RLV_Control *c, LONG source_row);
```

Kinds: `NONE`, `TEXT_NOCASE`, `TEXT_CASE`, `SIGNED`, `UNSIGNED`, `BOOLEAN`, `CUSTOM`.  
Directions: `RLV_SORT_ASC`, `RLV_SORT_DESC`.  
Flag: `RLV_SORT_F_DEFAULT_DESC` for first header activation.  
Specs are **borrowed** until the next `set_sort_specs` or destroy.

When `RLV_ENABLE_SORTING=0`, setters return `FALSE`; view/source helpers are identity.

---

## 5. Optional build/link mechanism

| Macro | Default | Effect |
|-------|---------|--------|
| `RLV_ENABLE_SORTING` | `0` | Omit `rlv_sort.o`; stubs in `rlv.c`; no map fields used |

Isolated trees:

- `build/rich_listview_sort/` → `bin/rich-listview-demo-sort`
- `build/rich_listview_sort_log/` → `bin/rich-listview-demo-sort-log`
  (`RLV_ENABLE_SORTING` + `RLV_ENABLE_LOGGING`)

Normal / log / bench / nosmart keep `RLV_ENABLE_SORTING=0`.

---

## 6. Sort kinds

| Kind | Source | Notes |
|------|--------|-------|
| Text (case/nocase) | Full `cells[col]` string | Not clipped/wrapped/ellipsis text |
| Signed / unsigned | Complete cell text parse | Empty → EMPTY class; junk/overflow → INVALID; after OK in ASC |
| Boolean | `cell_snapshot` CHECKED/UNCHECKED | ASC: false then true |
| Custom | `RLV_SortCompareFn` | Ascending sense; control applies DESC |

Dates/times: format display strings in the application; sort with
`RLV_SORT_CUSTOM` + `context` to typed keys (`DateStamp`, timestamps,
records). No universal date parser. Demo Date column:
`docs/RICHLISTVIEW_DATE_SORTING_READINESS_REPORT.md`.

---

## 7. Comparator / callback contract

```c
typedef LONG (*RLV_SortCompareFn)(const RLV_Control *control,
                                  ULONG source_a, ULONG source_b,
                                  UWORD column, APTR context);
```

Return &lt;0 / 0 / &gt;0 for ascending order. Do not reverse for DESC. The
engine may see any non-zero magnitude — it only tests sign. Pass context
pointers/scalars; do not copy whole rows onto the stack.

---

## 8. View-order map design

- `view_to_source[view]` / `source_to_view[source]` (`UWORD`, max 65535 rows)
- `NULL` maps = identity (no allocation until first sort/ensure)
- `layout_rows[view].logical_index = source`
- `cell_snapshot`, `cell_wraps`, `row_expand` remain **source-indexed**

---

## 9. View-row / source-row / event semantics

| Concept | Meaning after sorting |
|---------|------------------------|
| Source row | Index into attached `rows[]` |
| View row | Display position |
| `event->row` | **Source** index |
| `row_user_data` | Tag of that source row |
| `rlv_get_selected` | Source index |

---

## 10. Fixed-row / sort-barrier behaviour

A row is a barrier **only** when `RLV_ROW_SORT_FIXED` is set.
`RLV_ROW_NONSELECTABLE` is orthogonal (cannot be selected ≠ pinned during
sort). Contiguous non-barrier view runs sort independently; barriers stay
in place.

**Policy:** headings and other fixed separators should set

```text
RLV_ROW_NONSELECTABLE | RLV_ROW_SORT_FIXED
```

A non-selectable informational row may still participate in a sortable run;
a selectable summary row may still be pinned with `SORT_FIXED` alone.

---

## 11. Stable sorting algorithm

Iterative bottom-up merge sort over each view run. Equal keys keep prior
relative order (`cmp <= 0` takes left). No recursion; scratch buffer
`row_count * sizeof(UWORD)` allocated on the heap for the merge only.

**Descending:** reverse comparison **sense during merge** (not “sort ASC
then reverse the map”). Implementation flips only the sign test:

```c
if (direction == RLV_SORT_DESC) {
    if (cmp < 0)
        cmp = 1;
    else if (cmp > 0)
        cmp = -1;
}
```

Never `cmp = -cmp` (avoids `INT_MIN` / `LONG_MIN` overflow). Custom
comparators already normalize to −1 / 0 / +1 inside the primary compare,
but the merge path must not assume that for all kinds.

Amiga Name DESC map dumps showing `Alpha(src1)` before `Alpha(src8)` are
consistent with this stable-on-equality behaviour (a full-map reverse of
ASC would have swapped those equals).

---

## 12. Stack-safety analysis

No recursive sort; no large automatic arrays in the sort path; callbacks
receive scalar indices. Suitable for small Amiga default stacks.

---

## 13. Allocation and ownership

| Item | When | Size |
|------|------|------|
| View map pair | First sort / ensure | `2 * N * sizeof(UWORD)` |
| Merge scratch | During sort only | `N * sizeof(UWORD)` |
| Specs | Borrowed | 0 |

Freed / identity on `set_rows` (maps cleared + **active sort cleared**;
specs kept — policy A), `clear_sort` (identity maps; active cleared), and
`destroy`. Allocation failure leaves the previous view order intact.

**`rlv_set_rows` policy (A):** replacement rows appear in **attachment
order**. Active column/direction/indicator are cleared (`sort_active=0`).
Borrowed specs remain so the application can call `rlv_sort` again after a
page refresh. There is **no** mixed state of “indicator on, rows unsorted.”

---

## 14. Selection preservation

Selected **source** index is retained across sort and `clear_sort`. No false
`SELECTION_CHANGED` merely because the view index moved.

---

## 15. Viewport preservation

Records the source row at the viewport top before sort / clear; restores
that row’s new view `top_y` as `scroll_y` when possible; then
`make_visible` on the selected source row. Exact pixel preservation of a
mid-row fragment is not guaranteed for variable-height wraps.

---

## 16. Expansion, checkbox state, and re-sort policy

Expand bits and checkbox snapshot stay source-indexed, so they follow the
record through view permutation.

**No automatic re-sort** when the application (or input path) changes:

- checkbox / Boolean snapshot;
- cell text (name, score, timestamp string);
- values a custom comparator reads.

The visible order may then disagree with the active header indicator until
the application calls `rlv_sort(column, direction)` again (or clears).
Jumping the clicked row on every checkbox toggle was judged more
disorienting for v1. A future `rlv_resort()` could reapply the stored
column/direction.

---

## 17. Header-click behaviour

`SELECT_DOWN` in the header hit-tests columns:

| Hit | Behaviour |
|-----|-----------|
| Sortable column | Toggle direction (or first-click ASC / `DEFAULT_DESC`); emit `RLV_EVENT_SORT_CHANGED` |
| Non-sortable column (no spec) | **Consumed** — no row selection, no event |
| Outside header | Not a header click — normal input path |

**Deliberate API decision (v1):** consume non-sortable header hits so
selection cannot fire “through” the header. This may surprise apps that
want filter / column-info / resize / app-managed global sort gestures on
those headers. A future `RLV_EVENT_HEADER_CLICK` (or returning unhandled
so the embedding app can hit-test) is listed as a follow-up; not a current
bug.

Programmatic `rlv_sort` / `rlv_clear_sort` do **not** emit
`SORT_CHANGED` (header-click only).

---

## 17b. `rlv_clear_sort` semantics

| Aspect | Behaviour |
|--------|-----------|
| View maps | Restored to identity (attachment order) |
| Active sort | Cleared (`sort_active=0`; column/direction reset; indicator off) |
| Layout / wrap | Invalidate + rebuild in identity order |
| Selection | Same **source** row preserved |
| Checkbox / expand | Unchanged (source-indexed) |
| Viewport | Top source + `make_visible(selection)` like `rlv_sort` |
| Event | **Silent** — no `RLV_EVENT_SORT_CHANGED` |

---

## 18. Sort indicator rendering

7-pixel triangle in the active header cell (text inset reserved). Uses
existing text pen / `fill_rect`. No Unicode / new fonts.

---

## 19. Demo changes

Sort profile (`RLV_ENABLE_SORTING`):

| Column | Kind | Notes |
|--------|------|-------|
| Name | `TEXT_NOCASE` | Duplicate “Alpha” rows keep distinct tags / source indices |
| Date | `CUSTOM` + `DEFAULT_DESC` | `DateStamp` via `RLV_SortSpec.context`; display `DD-Mon-YYYY` is presentation only |
| Pos | `UNSIGNED` | Numeric `1,2,10` (not textual `1,10,2`) |
| On | `BOOLEAN` | Checkbox snapshot |

Description is ordinary wrapped text (not a sort key). Former Type column
is Date; former Description `[HH:MM:SS]` teaching decoration is removed.

Comparator `demo_compare_dates` reads `DemoSortRecord.stamp` through
`context` (`g_demo_date_records`). Equal keys: Beta and Zeta both
`17-Jan-2025` / `ds_Days=17183`. Fixed `-- Category --` heading is a
**top** sort barrier (`NONSELECTABLE` + `SORT_FIXED`). Status line shows
sort column/direction plus view/source/tag. Logging twin:
`rich-listview-demo-sort-log` (map lines include `name=` and `date=`).

See `docs/RICHLISTVIEW_DATE_SORTING_READINESS_REPORT.md` for sample dates,
expected ASC/DESC order, and context lifetime.

---

## 20. Build commands

```text
make rich-listview-demo
make rich-listview-demo-sort
make rich-listview-demo-sort-log
make rich-listview-demo-log
make rich-listview-demo-bench
make rich-listview-demo-nosmart
make public-header-audit
```

---

## 21. Compiler warnings and errors

VBCC warning 153 (`(void)param` no effect) on sort stubs in non-sorting
`rlv.c` (same pattern as expand stubs) and unused `control`/`column`
parameters in `demo_compare_dates`. Sort demo/build: no errors.
`rlv_handle_input` optimizer pass note (pre-existing class).

---

## 22–24. Functional / regression / stability tests

| Area | Status |
|------|--------|
| Compile + link normal / log / bench / nosmart / sort / sort-log | **Done** |
| Public-header audit | **Done** |
| Sort strings absent from normal binary | **Verified** |
| Header-click + triangle indicator | **Verified on Amiga** (visual + log) |
| Name ASC / DESC | **Verified** via `rlv.log` (earlier + 15:38 sessions) |
| Date ASC / DESC (`DateStamp` CUSTOM + context) | **Verified** via `rlv.log` 15:38 (`date=` maps) |
| Type ASC / DESC (retired column) | N/A — replaced by Date |
| Pos ASC / DESC (unsigned numeric) | **Verified** via `rlv.log` (earlier session) |
| Top heading barrier (`view[0]` fixed; run `[1,9)`) | **Verified** |
| Stable equal keys (Alpha; Pos=1; Date Beta/Zeta) | **Verified** (Date: Beta `src2` before Zeta `src6` ASC and DESC) |
| Viewport top source preserved (`top_src=0`) | **Verified** in sort-end logs |
| Date first click = DESC (`DEFAULT_DESC`) | **Verified** (`SORT header click col=2 dir=1`) |
| Mid-list barrier UX (pre-fix demo) | **Observed then fixed** — heading moved to top |

### Amiga `rlv.log` spot-checks

Log path: `bin/rlv.log` (copied from `PROGDIR:rlv.log`). Environment:
intuition 47.53, gadtools 47.17, graphics 47.10, layers 46.2, dos 47.30;
VBCC `+aos68k` / 68000.

#### Earlier session (Name / Type / Pos — Type since retired)

Specs then `count=5`. Map cost: **36** bytes persistent + **18** scratch.

**Name ASC** (`col=1 dir=0`, kind=`TEXT_NOCASE`):

```text
view[0]=Category (barrier)
view[1..8]= Alpha(src1), Alpha(src8), Beta, Delta, Epsilon, Eta, Gamma, Zeta
```

**Name DESC** (`col=1 dir=1`): Zeta → … → Alpha(`src1`), Alpha(`src8`) —
equal keys retain prior relative order.

**Type ASC** (historical — `col=2` was `TEXT_CASE` before Date conversion):

```text
Delta(Data), Beta(Library), Zeta(Library), Eta(PathTest),
Alpha(Tool), Epsilon(Tool), Gamma(Tool), Alpha(VeryLon…)
```

**Pos ASC** (`col=4 dir=0`, kind=`UNSIGNED`):

```text
Gamma(1), Alpha(src8,1), Beta(2), Epsilon(3), Eta(4),
Delta(5), Zeta(7), Alpha(src1,10)
```

**Pos DESC** (`col=4 dir=1`): `10,7,5,4,3,2,1,1`.

#### Date column session (2026-08-04 15:38, post Type→Date)

Specs installed: `SORT set_specs count=4` (Name, Date, Pos, On).
`kind=6` = `RLV_SORT_CUSTOM`. Map alloc `rows=9 bytes=36`, scratch `18`.
Multiple Date toggles + Name ASC/DESC in the same run; maps reproduced.

**Date first click DESC** (`col=2 dir=1` — `DEFAULT_DESC`):

```text
view[0]=Category date=-          (barrier)
view[1]=Alpha     date=04-Aug-2026  src1
view[2]=Delta     date=01-Dec-2025  src4
view[3]=Alpha     date=15-Jun-2025  src8
view[4]=Beta …    date=17-Jan-2025  src2
view[5]=Zeta      date=17-Jan-2025  src6   ← equal: Beta before Zeta
view[6]=Epsilon   date=03-Mar-2024  src5
view[7]=Gamma     date=29-Feb-2024  src3
view[8]=Eta …     date=11-Nov-2023  src7
```

Chronological recent-first; **not** lexical (`01-Dec-2025` before
`03-Mar-2024` lexically, but correctly after `15-Jun-2025` by date).

**Date ASC** (`col=2 dir=0`):

```text
view[0]=Category date=-
view[1]=Eta …     date=11-Nov-2023  src7
view[2]=Gamma     date=29-Feb-2024  src3
view[3]=Epsilon   date=03-Mar-2024  src5
view[4]=Beta …    date=17-Jan-2025  src2
view[5]=Zeta      date=17-Jan-2025  src6   ← equal: Beta before Zeta
view[6]=Alpha     date=15-Jun-2025  src8
view[7]=Delta     date=01-Dec-2025  src4
view[8]=Alpha     date=04-Aug-2026  src1
```

Every Date/Name sort logged `SORT run view[1,9)` and
`SORT end … top_src=0`.

---

## 25. Runtime environments used

| Environment | Role |
|-------------|------|
| Host VBCC `+aos68k` cross-build | Compile / link / size measurement |
| Classic Amiga (OS libraries v47-class; logs 14:49 and 15:38) | Header-click sorting + `rlv.log` map verification (incl. Date) |

Exact host vs UAE vs physical board was not distinguished beyond the library
versions recorded in the log.

---

## 26. Tests not performed

Timed sort benchmarks, allocation-failure injection, 5000-row investigation,
explicit Workbench 2.04-only visual pass, On-column log spot-checks.

---

## 27–28. Object / executable sizes (2026-08-04 cross-build)

| Binary | Bytes |
|--------|------:|
| `rich-listview-demo` | 59252 |
| `rich-listview-demo-sort` | 65908 |
| `rich-listview-demo-sort-log` | 87284 |
| Delta (sort − normal) | **+6656** |
| `rlv_sort.o` (sort tree) | ~8128 |

Core `.o` sizes shift slightly when sorting is compiled in (view helpers in
layout/input/render paths).

---

## 29–30. Per-row / temporary memory

| Cost | Formula |
|------|---------|
| Persistent maps (when active) | 4 bytes/row (`UWORD`×2) |
| Merge scratch (transient) | 2 bytes/row |

---

## 31. Timing benchmarks

Not measured this session. Do not claim 68000 sort timings without data.

---

## 32. Compatibility risks

- Apps that assumed `layout_rows[i]` identity without going through public
  APIs could break if they included private headers (unsupported).
- New `RLV_EVENT_SORT_CHANGED` may trigger `-Wswitch` in exhaustive switches.
- Max 65535 attached rows when sorting (UWORD map).

---

## 33. Known limitations

- No third “restore original” click cycle beyond `rlv_clear_sort`.
- No secondary sort keys beyond stable prior order.
- No locale text / floating-point / date parsing.
- Header indicator is minimal (low-colour OK; not themed).
- No auto-resort after cell / checkbox edits (call `rlv_sort` again).
- Non-sortable header clicks are consumed without an app event.
- `set_rows` clears active sort rather than reapplying it (policy A).

---

## 34. Preparation for future paging

Comparison helpers operate on attached source indices and opaque tags.
Applications (or a future page helper) should sort the full result set,
then attach a page — RichListview sorting stays page-local.

---

## 35. Explicit non-goals confirmation

**Paging and filtering were not implemented.** Sorting applies only to the
currently attached dataset.

---

## 36. Date-column conversion (2026-08-04)

| Item | Detail |
|------|--------|
| Files | `examples/rich_listview_demo/main.c`, `rlv_sort.c` (log `date=`), docs |
| Display | `DD-Mon-YYYY` ASCII in column 2 |
| Key type | Amiga `struct DateStamp` in `DemoSortRecord` |
| Comparator | `demo_compare_dates` via `RLV_SortSpec.context` |
| Context lifetime | Static records + specs; refreshed in `demo_install_sort` / Apply |
| Sample / order | See date-sorting readiness report |
| Equal keys | Beta + Zeta `17-Jan-2025` — **verified** Beta `src2` before Zeta `src6` ASC and DESC |
| Builds | normal / sort / sort-log / public-header-audit OK |
| Runtime | **Verified** on Amiga via `bin/rlv.log` 15:38 (Date DESC/ASC, `DEFAULT_DESC`, barrier, non-lexical order) |
| Parser / API | None added |
