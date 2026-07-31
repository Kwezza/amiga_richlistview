# Custom ListView — Future Improvement Wishlist

**Status:** Exploratory design notes  
**Purpose:** Record potential future enhancements for the reusable Amiga Custom ListView without committing them to the current implementation plan.

These ideas are intended to preserve the component's modular design: applications should be able to include only the features they need, with unused feature modules omitted from the final executable.

---

## 1. Control Activation Without Full-Row Highlighting

**Status (RichListview):** Implemented as an opt-in policy set — see
`RLV_ControlActivationPolicy`, `RLV_CurrentRowVisual`, and
`rlv_render_cell_control` in `rich_listview.h`, plus the demo keys `A` / `V`
and CLI `KEEPCURRENT` / `MARKER` / `NOVISUAL`. Broader wishlist ideas below
(expanded rows, alternate focus glyphs, etc.) remain future work.

### Motivation

Interactive cells such as checkboxes may represent the application's primary action, while the full-row highlight represents only navigation or focus.

For example, in a WHDLoad package browser:

- the checkbox means **install this package**;
- the current row means **this is the item currently under keyboard or mouse focus**;
- these states do not need to be visually or behaviourally coupled.

On a stock 7 MHz 68000, changing the selected row can cause a large wrapped row to be completely redrawn. A checkbox-only action should not require this when no other row presentation has changed.

### Desired behaviour

Provide a configurable ListView policy allowing control activation to occur without selecting or fully highlighting the logical row.

Possible modes include:

- conventional full-row selection;
- control activation without changing row selection;
- navigation focus without full-row fill;
- no visible row selection, with control state providing the main visual feedback.

### Lightweight focus alternatives

When full-row highlighting is disabled, the current keyboard row may still need a visible marker. Possible options include:

- a narrow marker at the left edge;
- a small arrow or glyph;
- a one-pixel row outline;
- focus around the active cell only;
- highlighting only the first column;
- a dotted focus indicator.

The current-row state, checkbox state, and future expanded-row state should remain independent.

### Performance goal

When a fully visible checkbox is toggled and no layout or selection state changes, redraw only the checkbox rectangle.

Fallback to a larger redraw when:

- the row is partly outside the viewport;
- the row must be made visible;
- the selection background changes;
- layout, wrapping, or other cell presentation changes;
- the renderer cannot safely reconstruct the local background.

---

## 2. Collapsible and Expandable Rows

### Motivation

Word-wrapped descriptions can make individual rows many physical lines high. This is useful for reading details but inefficient for browsing large datasets.

A compact mode could show one physical line per logical record, allowing many more entries to remain visible and reducing rendering work on original Amiga hardware.

### Proposed model

Each logical row may have an independent expanded state:

- **collapsed:** one compact display line;
- **expanded:** full wrapped content and optional detail fields.

The checkbox or other primary control should remain available in both states.

### Possible interaction models

#### Explicit per-row disclosure

A small disclosure control expands or collapses a row. The row remains in that state until changed again.

This is likely the most predictable model for a slow machine because expansion happens only when explicitly requested.

#### Single-row expansion

Only one logical row may be expanded at a time. Expanding another row automatically collapses the previous one.

This limits layout and memory cost but may cause frequent rebuilding during navigation.

#### Current-row automatic expansion

The focused row expands automatically. This may be visually convenient but could cause repeated relayout and flashing during rapid keyboard movement, so it should not be the default without testing.

### Application operations

Expose operations or hooks allowing the main program to request:

- expand one row;
- collapse one row;
- toggle one row;
- expand all;
- collapse all;
- optionally expand only visible or selected rows.

`Collapse All` is likely to be the most useful large-dataset operation on a stock 68000.

### Viewport stability

Changing a row's height alters the display map below it. The implementation should preserve visual stability where possible.

A useful rule is:

> Keep the first line of the expanded or collapsed row at the same screen Y position whenever possible.

If the expanded row cannot fit, scroll only enough to make the relevant content visible.

### Compact-row presentation

Collapsed rows should have an intentional layout rather than merely clipping wrapped output. Possibilities include:

- omit the description;
- show only the first line;
- show an ellipsis or continuation marker;
- display a reduced set of important columns;
- reserve the complete description for expanded mode.

### Optionality

Expandable-row support should be a separately linkable feature module. Applications with fixed-height rows should not carry its display-map, state, or interaction code.

---

## 3. Large-Dataset Paging and Windowing

### Motivation

A catalogue of roughly 5,000 WHDLoad packages may impose preparation, wrapping, mapping, sorting, and memory costs that are undesirable on a stock Amiga.

The application should not be required to attach all records to the ListView simultaneously.

### Architectural principle

Separate:

- the application's complete master dataset;
- the bounded subset currently attached to the ListView;
- the user-interface controls used to change that subset.

Example:

```text
Complete catalogue:       approximately 5,000 records
Current attached page:    100–200 records
Rows visible at once:     perhaps 10–25 records
```

The ListView should prepare and render only the current subset.

### Candidate navigation interfaces

#### Alphabetical navigation

Examples:

```text
# A B C D E ... Z
```

or a compact selector such as:

```text
Letter: [ A v ]
```

This suits game-title browsing but letter groups will be uneven and may still require paging.

#### Conventional page browser

Example:

```text
<<  <  Page 12 of 50  >  >>
```

Possible meanings:

- first page;
- previous page;
- next page;
- last page.

A textual range can improve orientation:

```text
Page 12 of 50 — Lemmings to Lotus III
```

#### Filter-first navigation

Filtering may be more useful than raw paging for a very large catalogue:

- title text;
- initial letter;
- genre;
- publisher;
- installed state;
- favourites;
- category.

Paging is then applied only when the filtered result remains large.

### Application versus library responsibility

#### Application responsibility

- own the complete dataset;
- preserve checked/install state across pages;
- perform global filtering and sorting;
- select the records for the current page;
- create the paging or alphabetical controls;
- choose page size and navigation policy.

#### ListView responsibility

- efficiently replace the attached dataset;
- preserve or deliberately reset navigation state;
- prepare only the current subset;
- return stable row identity in events;
- avoid retaining stale page data;
- support make-visible requests within the current subset.

### Optional page helper

A small optional paging helper module could provide:

- total count;
- page size;
- page count;
- current page;
- first and last record indices;
- first, previous, next, and last calculations.

The actual buttons or alphabetical controls should remain application-owned so smaller programs do not inherit unused interface code.

### Benchmarking questions

Before choosing a default page size, measure on stock-speed hardware or an accurate A500 profile:

- preparation time for 50, 100, 200, 500, and 5,000 compact records;
- memory cost per prepared logical row;
- page replacement latency;
- the additional cost of wrapping;
- display-map rebuild cost;
- interaction with collapsed and expanded rows;
- sorting and filtering time.

---

## 4. Per-Row Tags and Stable Application Identity

### Motivation

A row should be able to carry an application-defined identifier that is not displayed.

This avoids recovering the original record from visible text such as a game title, which may be duplicated, truncated, translated, edited, wrapped, or reordered.

The feature is similar to a row tag or hidden key used in other GUI frameworks.

### Required behaviour

Each logical row may carry an opaque application-defined value, for example:

- a numeric record ID;
- an index into an application-owned array;
- a pointer to an application-owned record;
- another machine-sized token.

The ListView returns the value unchanged in row-related events.

### Events that should expose the tag

The row tag should be available for:

- row selection;
- row activation;
- checkbox changes;
- future button presses;
- future cycle-value changes;
- expansion and collapse;
- double-click;
- future context actions.

An event should be able to report both transient view coordinates and stable identity:

```text
event type
logical row in current dataset
column
application row tag
new control state or value
```

### Logical row versus physical fragments

The tag belongs to the logical row.

If wrapping produces several physical display fragments, every fragment must resolve to the same logical row tag.

It must also remain associated with the record through:

- sorting;
- filtering;
- paging;
- expansion and collapse;
- display-map rebuilding.

### Ownership

The tag is borrowed metadata:

- CLV does not allocate or free it;
- zero or NULL is valid;
- the application must keep pointed-to data alive while the row is attached;
- the value must not be interpreted by CLV.

### Numeric ID versus pointer

Both use cases are valuable:

- a numeric ID is stable across rebuilding, persistence, and paging;
- a pointer offers immediate access to an in-memory record.

A minimal implementation could expose one opaque 32-bit or pointer-sized field that applications may use for either purpose.

### Core status

Unlike the more visible presentation features, row tags may justify inclusion in the core because they provide foundational identity for paging, callbacks, controls, and sorting at very low code cost.

If per-row storage cost must remain configurable, support can still be guarded or supplied through an optional prepared-row extension.

---

## 5. Interaction Between the Wishlist Features

These features should be designed to work together.

A likely WHDLoad browser configuration would be:

- a bounded page from the full package catalogue;
- compact, collapsed rows by default;
- explicit disclosure controls for details;
- checkboxes visible in collapsed and expanded states;
- checkbox clicks that do not force full-row highlighting;
- a lightweight current-row focus marker;
- checkbox-only partial redraw when safe;
- stable row tags returned in every callback;
- application-owned install selections preserved across page changes.

A smaller Amiga program installer might enable only:

- normal text columns;
- checkbox cells;
- row tags;
- conventional row selection.

It should not link paging, expansion, wrapping, or advanced navigation code unless those features are actually required.

---

## 6. Modularity and Executable-Size Policy

All substantial new capabilities should follow the existing size-conscious CLV model.

### General rules

- Keep the core small.
- Put optional behaviour in separate translation units.
- Omit unused object modules from the link.
- Use truthful compile-time feature macros.
- Avoid adding large per-row structures to applications that do not use the feature.
- Share genuinely common logic rather than duplicating it in each control type.
- Measure executable-size deltas for each new profile.

### Possible future feature flags

Names remain provisional, but the build system may eventually expose features such as:

```text
row tags
interactive controls
checkbox controls
partial control redraw
expandable rows
paged-data helper
lightweight navigation focus
```

### Example profiles

Possible profile combinations include:

```text
draw-basic
draw-checkbox
draw-checkbox-tags
draw-wrapped
draw-expandable-checkbox
draw-paged-expandable-checkbox
draw-full
```

Profiles should remain conveniences. Applications that care about every byte should still be able to specify the exact object-module list.

---

## 7. Suggested Order of Investigation

This document is a wishlist, not an implementation schedule. A sensible investigation order would be:

1. Add stable per-logical-row tags to the event model.
2. Optimise checkbox toggles with checkbox-only redraw.
3. Add a configurable policy preventing control activation from forcing row selection.
4. Investigate a lightweight navigation-focus indicator.
5. Design collapsed and expanded logical rows.
6. Benchmark compact-row preparation and scrolling at large record counts.
7. Add efficient dataset replacement/windowing.
8. Decide whether a small optional paging-state helper is justified.
9. Let each application choose its own paging, filtering, or alphabetical interface.

---

## 8. Open Questions

- Should clicking a checkbox ever move keyboard focus, even when it does not select the row?
- What is the cheapest clear focus indicator on a two- or four-colour screen?
- Should collapsed state be stored by the application, by CLV, or through a callback?
- Should the initial expansion implementation permit multiple expanded rows?
- Should expanding a row automatically make the entire logical row visible?
- What page size offers the best compromise on a stock A500?
- Can global filtering and sorting remain application-side without duplicating too much code?
- Should row tags be always present in the prepared-row core or compiled as an optional extension?
- Should a tag be a single opaque machine-sized value, or should CLV expose separate numeric ID and user-pointer fields?
- Which operations must preserve current-row position across page replacement?

---

**Document type:** Wishlist and architectural exploration  
**Implementation commitment:** None until incorporated into a dedicated, reviewed implementation plan
