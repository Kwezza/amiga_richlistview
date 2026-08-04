# RichListview — Date-Sorting Readiness Report

**Date:** 2026-08-04 (updated after Date-column demo conversion)  
**Related:** `docs/RICHLISTVIEW_SORTING_IMPLEMENTATION_REPORT.md`  
**Scope:** Demo Date column + `RLV_SORT_CUSTOM` + `DateStamp` via
`RLV_SortSpec.context`.

---

## 1. Verdict

**READY**

The sorting-enabled demo now owns a **Date** column (former Type) that:

- displays application-formatted `DD-Mon-YYYY` strings;
- sorts with `RLV_SORT_CUSTOM` against Amiga `struct DateStamp` values;
- obtains those records through `RLV_SortSpec.context` (not file-scope
  globals inside the comparator, not cell-text parse, not `user_data`);
- leaves Description as ordinary wrapped prose (no `[HH:MM:SS]` decoration).

No universal date-string parser and no new public API were added.

---

## 2. Final Date-column design

| Item | Value |
|------|-------|
| Column index | 2 (was Type) |
| Title | `Date` |
| Display format | `DD-Mon-YYYY` ASCII (e.g. `04-Aug-2026`) |
| Heading placeholder | `-` |
| Sort kind | `RLV_SORT_CUSTOM` |
| First-click direction | `RLV_SORT_F_DEFAULT_DESC` (recent first) |
| Underlying key | `struct DateStamp` (`ds_Days`, `ds_Minute`, `ds_Tick`) |
| Record type | `DemoSortRecord { struct DateStamp stamp; }` |
| Storage | Static `g_demo_date_records[DEMO_MAX_ROWS]` |
| Spec context | `(APTR)g_demo_date_records` (borrowed for process life) |
| Comparator | `demo_compare_dates` — ascending `Days` then `Minute` then `Tick` |

Display buffers: `g_demo_date_display[i]` — separate from `DateStamp`.
RichListview never parses the formatted string.

---

## 3. Sample data (heading-first attachment order)

| Src | Name | Display | `ds_Days` | Notes |
|-----|------|---------|-----------|-------|
| 0 | `-- Category --` | `-` | 0 | `SORT_FIXED` barrier |
| 1 | Alpha | `04-Aug-2026` | 17747 | Newest |
| 2 | Beta | `17-Jan-2025` | 17183 | Equal with Zeta |
| 3 | Gamma | `29-Feb-2024` | 16860 | Leap day |
| 4 | Delta | `01-Dec-2025` | 17501 | |
| 5 | Epsilon | `03-Mar-2024` | 16863 | |
| 6 | Zeta | `17-Jan-2025` | 17183 | Equal key with Beta |
| 7 | Eta | `11-Nov-2023` | 16750 | Oldest |
| 8 | Alpha | `15-Jun-2025` | 17332 | Duplicate Name; distinct tag |

Lexical ASCII order of the display strings does **not** match chronological
order (e.g. `01-Dec-2025` sorts before `03-Mar-2024` lexically but after it
by date).

### Expected ASC (data run `view[1,9)`)

```text
Eta (11-Nov-2023), Gamma (29-Feb-2024), Epsilon (03-Mar-2024),
Beta (17-Jan-2025), Zeta (17-Jan-2025),   ← equal: Beta before Zeta
Alpha-dup (15-Jun-2025), Delta (01-Dec-2025), Alpha (04-Aug-2026)
```

### Expected DESC (first Date click)

```text
Alpha (04-Aug-2026), Delta (01-Dec-2025), Alpha-dup (15-Jun-2025),
Beta (17-Jan-2025), Zeta (17-Jan-2025),   ← equal: prior relative order
Epsilon (03-Mar-2024), Gamma (29-Feb-2024), Eta (11-Nov-2023)
```

Equal-key stability: engine keeps left on `cmp == 0` after DESC sign flip
(not ASC-then-reverse-map).

---

## 4. Comparator and context lifetime

```c
static LONG demo_compare_dates(const RLV_Control *control,
                               ULONG source_a, ULONG source_b,
                               UWORD column, APTR context);
```

- Ascending only; DESC applied by the sort engine.
- Ignores cell text and row tags.
- Bounds-checks source indices; null `context` → equal.
- Does not copy `DateStamp` by value onto large stack frames (pointer into
  the context array).

**Lifetime:** `g_demo_date_records` and `g_demo_sort_specs` are static.
`demo_install_sort` refreshes stamps + display strings, then
`rlv_set_rows` (clears active sort — policy A) and `rlv_set_sort_specs`.
Apply/recreate calls `demo_install_sort` again so context stays aligned
with the attached row array. Specs/context remain valid for the process.

Sorting applies only to the currently attached dataset.

---

## 5. Historical note (superseded teaching shortcut)

An earlier Description-column demo used a parallel `ULONG` seconds-of-day
array accessed as a file-scope global from the comparator (`context`
unused) plus `[HH:MM:SS]` decoration on Description text. That pattern is
**removed**. Date sorting lives on the Date column with proper `context`.

---

## 6. Application recipe

Apps can use the same pattern for file dates, logs, and schedules:

1. Keep `DateStamp` (or another typed key) in app records.
2. Format display strings separately into `RLV_Row.cells[col]`.
3. Install `RLV_SORT_CUSTOM` with `compare` + `context` → record base.
4. Compare ascending field-by-field; never parse the cell string.
5. Keep borrowed specs/context alive while installed; refresh keys when
   replacing the attached page.

---

## 7. Tests performed

| Item | Result |
|------|--------|
| `make rich-listview-demo` / `-sort` / `-sort-log` | Compiled + linked |
| `make public-header-audit` | OK |
| Code review: context path, equal keys, barrier | OK |
| Amiga Date-header ASC/DESC (`bin/rlv.log` 15:38) | **Verified** — chronological (not lexical); Beta before Zeta both dirs; `DEFAULT_DESC` first click; barrier `view[0]` |

---

## 8. Tests not performed

- Timed benchmarks / allocation-failure / 65535-row CUSTOM stress
- Live `DateStamp` from `DateStamp()` / filesystem
- Paging with replaced date-record pages
- Workbench 2.04-only visual pass

---

## 9. Confirmation

| Requirement | Status |
|-------------|--------|
| Display formatted by demo | Yes |
| Sort uses typed `DateStamp` | Yes |
| Comparator uses `context` | Yes |
| No date-string parser | Yes |
| No new public API | Yes |
| Description not the date demo | Yes |
| Equal dates stable (by design + equal Beta/Zeta keys) | **Yes** — verified in `rlv.log` |
| Attached-page only | Yes |
