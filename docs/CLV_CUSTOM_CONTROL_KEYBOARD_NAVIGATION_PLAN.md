# CLV Custom Control — Keyboard Navigation Plan

**Branch:** `experiment/clv-custom-control`  
**Status:** **Implemented** in Phase 5.5 (2026-07-27)  
**Date:** 2026-07-27 (plan); implementation recorded in living design  
**Living design:** [CLV_CUSTOM_CONTROL_DESIGN_AND_IMPLEMENTATION_PLAN.md](CLV_CUSTOM_CONTROL_DESIGN_AND_IMPLEMENTATION_PLAN.md)  
**Scope of this document:** Original investigation/plan. Production code lives
in `clv_control.h` / `clv_control_input.c` / `examples/custom_control_demo/`.

---

## 1. Executive summary

| Question | Answer |
|----------|--------|
| Should keyboard navigation precede Phase 6? | **Yes.** Insert **Phase 5.5** before Phase 6. |
| Does it help deterministic Phase 6 profiling? | **Yes.** A neutral exercise path calls the same navigation/scroll ops as real keys, without synthesising `IDCMP_RAWKEY`. |
| Are public API changes required? | **Experimental control API only** (`CLV_InputType` / `CLV_EventType` / optional `get_selected`). **No CLV v1** public API changes. |
| Recommended ordering | Phase 5 complete → **Phase 5.5 Keyboard + Exercise** → Phase 6 optimisation / size / V2 recommendation |

Keyboard navigation is recorded as a Medium deferred limitation in the Phase 5 completion record. Phase 6 instructions currently forbid adding keyboard; that must be revised after Phase 5.5 lands. The feature is small, reuses proven selection / make-visible / paint paths, and supplies a fixed workload for later timing comparisons (smart-scroll on vs off).

---

## 2. Current implementation findings

Verified against source (not summaries alone).

### 2.1 Design / audit records

| Source | Finding |
|--------|---------|
| Design §3.2 Deferred | Multi-select, h-scroll, etc.; keyboard later listed as Phase 4/5 deferred feature |
| Phase 4 deliberately deferred | “Keyboard navigation unless trivial and well-contained” — not shipped |
| Phase 5 optional | “Basic keyboard up/down navigation” — **not** delivered |
| Phase 5 limitations register | “No keyboard navigation \| Medium \| Future” |
| Phase 6 handoff | Explicitly: do **not** add keyboard |
| Design §11.1 / Phase 4 decision | Neutral `CLV_InputEvent`; **IDCMP translation in the demo**; core GadTools- and IntuiMessage-free |
| Phase 1 audit §5.4 / §8 | Originally preferred backend IDCMP translation; **implementation put translation in the demo** |

### 2.2 Neutral input architecture (already present)

[`src/custom_listview_control/clv_control.h`](../src/custom_listview_control/clv_control.h):

- `CLV_InputType`: `SELECT_DOWN` / `SELECT_UP` / `POINTER_MOVE` / `SCROLL_LINE_*` / `SCROLL_PAGE_*` / `SCROLL_POSITION`
- `CLV_InputEvent` `{ type, x, y, value }`
- `CLV_EventType`: `NONE`, `SELECTION_CHANGED`, `SCROLL_CHANGED`
- `clv_control_handle_input`, `set_selected`, `make_visible`, scroll accessors
- Experimental surface only — not CLV v2

**No** `CLV_INPUT_NAV_*`, activation event, or keyboard types today.

### 2.3 Reusable paths ([`clv_control_input.c`](../src/custom_listview_control/clv_control_input.c))

| Capability | Function / behaviour |
|------------|----------------------|
| Selectability | Static `clv_ctrl_row_selectable` — range + `CLV_CTRL_ROW_NONSELECTABLE` |
| Mouse select | `CLV_INPUT_SELECT_DOWN` → `clv_control_hit_test` → set `selected_row` → `clv_control_make_visible` |
| Make-visible | Minimal pixel clamp using `layout_rows[].top_y` + `content_height` |
| Line scroll | `SCROLL_LINE_*` by `line_height`; **selection unchanged** |
| Page scroll | `SCROLL_PAGE_*` by `max(vp_h - line_h, line_h)`; **selection unchanged** |
| Scroll clamp | `clv_control_set_scroll_y` in [`clv_control_scroll.c`](../src/custom_listview_control/clv_control_scroll.c) |
| Relayout selection | `clv_ctrl_preserve_selection_after_relayout` in [`clv_control.c`](../src/custom_listview_control/clv_control.c) |

### 2.4 Missing capabilities

- No selectable-row walk helper (`next` / `prev` / first / last) — only reject on non-selectable.
- No `clv_control_get_selected` (selection is write-oriented from outside; read via events or internals).
- No keyboard focus / active-control field on `CLV_Control` (**correct** — must stay application-owned; no hidden global).
- Demo IDCMP mask has **no** `IDCMP_RAWKEY` ([`examples/custom_control_demo/main.c`](../examples/custom_control_demo/main.c)).
- Backend [`clv_backend_amiga_v36`](../src/custom_listview_control/backends/clv_backend_amiga_v36.h) is **draw/clip only** — no input helpers.

### 2.5 Redraw policy to preserve (Phase 5)

[`demo_apply_input`](../examples/custom_control_demo/main.c):

| Situation | Paint |
|-----------|-------|
| `SELECTION_CHANGED`, `scroll_y` unchanged | `clv_control_render_logical_rows(previous, new)` |
| `SELECTION_CHANGED` + make-visible scrolled | **Full viewport** (never smart-scroll) |
| Scroll alone | `clv_control_render_scrolled` (smart when eligible) |
| Activation without visual change | No repaint |

Keyboard navigation must emit the same events so the demo reuses this policy unchanged.

### 2.6 Logic to centralise

Mouse and keyboard must share one authoritative path:

1. `clv_ctrl_row_selectable` (or equivalent)
2. Assign `selected_row`
3. `clv_control_make_visible`
4. Fill `CLV_Event` (`SELECTION_CHANGED` and/or scroll side-effects)

Page navigation should resolve targets through `layout_rows`, not by counting “visible logical rows” alone.

```text
IDCMP_RAWKEY (demo)
  → demo_translate_rawkey → CLV_InputEvent
  → active_control (app state)
  → clv_control_handle_input
  → demo_apply_input paint + demo_sync_scroller
```

---

## 3. Verified Amiga key mapping

### 3.1 Provenance

- Repo AutoDocs (`docs/AutoDocs/intuition.doc`, `input.doc`, `console.doc`, `keymap.doc`) document `IDCMP_RAWKEY`, qualifier **names**, keyboard repeat (`WA_RptQueue`, input device period/threshold), and that cursor/Help/function keys are **non-vanilla**.
- AutoDocs do **not** define cursor hex codes.
- Numeric values verified on the host Amiga SDK (not invented from memory):
  - Classic `intuition/intuition.h`: `CURSORUP` `0x4C`, `CURSORDOWN` `0x4D` (present since 1.x/2.x)
  - `devices/inputevent.h`: `IECODE_UP_PREFIX`, `IEQUALIFIER_LSHIFT` / `RSHIFT` / `CONTROL` / `REPEAT`
  - Newer `libraries/keymap.h`: `RAWKEY_PAGEUP` / `PAGEDOWN` / `HOME` / `END` marked **“Not on classic keyboards”**
- Prefer **`CURSORUP` / `CURSORDOWN`** for WB2.x/3.x portability.
- For Return use a **demo-local** `#define` of **`0x44`** (same value as modern `RAWKEY_RETURN`) so builds do not depend on OS3.2-only `RAWKEY_*` macros.

### 3.2 Recommended mapping

| Physical key | Qualifiers | Keydown code | Neutral action | Upstroke | Compatibility notes |
|--------------|------------|--------------|----------------|----------|---------------------|
| Cursor Up | none | `CURSORUP` `0x4C` | `CLV_INPUT_NAV_PREV` | Ignore (`IECODE_UP_PREFIX`) | OS repeat OK; treat `IEQUALIFIER_REPEAT` as down |
| Cursor Down | none | `CURSORDOWN` `0x4D` | `CLV_INPUT_NAV_NEXT` | Ignore | Same |
| Cursor Up | L/R Shift | `0x4C` + Shift | `CLV_INPUT_NAV_PAGE_UP` | Ignore | **Not** `SCROLL_PAGE_*` (scroll-only) |
| Cursor Down | L/R Shift | `0x4D` + Shift | `CLV_INPUT_NAV_PAGE_DOWN` | Ignore | |
| Cursor Up | Control | `0x4C` + `IEQUALIFIER_CONTROL` | `CLV_INPUT_NAV_FIRST` | Ignore | |
| Cursor Down | Control | `0x4D` + Control | `CLV_INPUT_NAV_LAST` | Ignore | |
| Return | ignore Shift/Ctrl for this action | `0x44` | `CLV_INPUT_NAV_ACTIVATE` | Ignore | Optional activation |
| Space | — | `0x40` | **Unused** | — | Not justified; reserve for future multi-select |
| Page Up / Home / End | — | non-classic keymap notes | **Do not map** | — | Classic Amiga keyboards lack these |

**Qualifier rules:**

- Shift = `(qual & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) != 0`
- Control = `(qual & IEQUALIFIER_CONTROL) != 0`
- If both Shift and Control are held with a cursor key, prefer **Control** (first/last) over page.

### 3.3 IDCMP / API mechanics

| Topic | Decision |
|-------|----------|
| IDCMP flags | **`IDCMP_RAWKEY` only** (not `VANILLAKEY`) so cursor keys and upstrokes remain reliable |
| `RawKeyConvert` / `MapRawKey` | **Avoid** for navigation; fixed raw codes suffice |
| `console.device` | Do not open for this feature |
| Message loop | Keep `GT_GetIMsg` / `GT_ReplyIMsg` (demo already uses them); reply before handling except `SIZEVERIFY` |
| Key repeat | Prefer Intuition/OS repeated RAWKEY messages; **no** `IDCMP_INTUITICKS` repeat engine |
| Home/End/PgUp/PgDn | Do not assume PC keyboard keys |

---

## 4. Behaviour specification

### 4.1 Distinctions

| Action class | Meaning | Input type |
|--------------|---------|------------|
| Move selection prev/next logical row | Change `selected_row`; `make_visible` | `NAV_PREV` / `NAV_NEXT` |
| Scroll one font line without changing selection | Pixel scroll only | Existing `SCROLL_LINE_*` |
| Move by one viewport page (selection-centric) | Jump selection by page step | `NAV_PAGE_*` |
| Jump first/last selectable | Absolute selection | `NAV_FIRST` / `NAV_LAST` |
| Activate selected row | Notify app; no selection/scroll change | `NAV_ACTIVATE` |

### 4.2 Locked defaults

- **No wrap** at list ends (clamp; no-op → `CLV_EVENT_NONE` / handle_input FALSE).
- **Skip** `CLV_CTRL_ROW_NONSELECTABLE` and out-of-range indices.
- Mouse, keyboard, and programmatic selection share one selectable + make-visible path.
- **Space unused.** Return activates only if a selectable row is currently selected.

### 4.3 Per-operation behaviour

| Op | Behaviour |
|----|-----------|
| `NAV_NEXT` | From `selected_row` (or −1): next selectable. If none selected, first selectable from index 0. Else no-op at end. |
| `NAV_PREV` | Symmetric; if none selected, last selectable. |
| `NAV_FIRST` / `NAV_LAST` | First/last selectable; no-op if empty, all non-selectable, or already there. |
| `NAV_PAGE_DOWN` | **Selection-centric page:** `step = max(vp_h - line_h, line_h)`. `origin_top` = selected row’s `layout_rows[].top_y`, or current `scroll_y` if none. Target = first selectable with `top_y >= origin_top + step`, else last selectable. Then select + `make_visible`. |
| `NAV_PAGE_UP` | Symmetric backward (`top_y <= origin_top - step`, else first selectable). |
| `SCROLL_LINE_*` / `SCROLL_PAGE_*` | Unchanged: scroll only; used by scroller gadget and exercise mode. |
| `NAV_ACTIVATE` | If current selection is selectable → `CLV_EVENT_ACTIVATED` with `row` set; **no** scroll/selection change; usually **no** repaint. |

### 4.4 Edge cases

| Case | Result |
|------|--------|
| Empty list | All NAV_* → no-op |
| All rows non-selectable | All NAV_* → no-op |
| No selection + NEXT | Select first selectable |
| No selection + PREV | Select last selectable |
| No selection + ACTIVATE | No-op |
| Partial / clipped rows | Existing `make_visible`; if scroll moves with selection → full viewport paint |
| Boundaries | Clamp; do not wrap |

### 4.5 Why this page model

- Reuses the existing page step (`viewport_height - line_height`) already used by `SCROLL_PAGE_*`.
- Driven by the layout cache (correct for variable-height wrapped rows).
- Matches classic “move selection by a page” better than scroll-only or preserving the old row’s screen Y.
- Preserves one text-line overlap when scrolling/selecting by that step.

---

## 5. Multi-instance keyboard-focus model

### 5.1 Policy

| Rule | Detail |
|------|--------|
| Storage | `CLV_Control *active_control` (and optional associated scroller) in **application/demo window state** |
| Not in | `CLV_Control`, backend, or any **global** mutable active-control |
| Click in viewport | That control becomes active (even on non-selectable hit or gap: if pointer is inside that control’s viewport bounds, focus transfers; selection still follows hit-test rules) |
| Scroller gadget | Events for a control’s scroller keep/set that control active |
| Click outside all viewports | **Preserve** active (keyboard still works after chrome clicks) |
| Routing | Only the active control receives NAV_* / keyboard-driven commands |
| Independence | Each control retains its own `selected_row` and `scroll_y` |

### 5.2 Two-control event-routing example

```text
Demo state:
  ctrl[0], ctrl[1]
  active = ctrl[0]   /* initial: first control, or NULL until first click */

IDCMP_MOUSEBUTTONS (LMB down at mx,my):
  if point in ctrl[0].viewport → active = ctrl[0]; SELECT_DOWN → ctrl[0]
  else if point in ctrl[1].viewport → active = ctrl[1]; SELECT_DOWN → ctrl[1]
  else → leave active unchanged; no SELECT_DOWN to either

IDCMP_RAWKEY:
  if active == NULL → ignore
  else translate → clv_control_handle_input(active, …) → paint/sync for that instance

IDCMP_GADGET* on scroller for ctrl[i]:
  active = ctrl[i]; apply scroll input to ctrl[i]
```

Phase 5.5 demo may remain **single-control** but must implement the focus-pointer pattern and document dual-control routing. Dual-control runtime demo remains optional (Phase 5 accepted static multi-instance audit only).

---

## 6. Proposed API and internal changes

### 6.1 Decision: extend `CLV_InputEvent` (no `clv_control_navigate`)

Exercise mode and real keyboard both call `clv_control_handle_input`. A separate navigate API would duplicate the authoritative path.

### 6.2 Experimental public declarations (`clv_control.h`)

```c
/* experimental — not CLV v1 / not final CLV v2 */
typedef enum CLV_InputType
{
    CLV_INPUT_SELECT_DOWN = 0,
    CLV_INPUT_SELECT_UP,
    CLV_INPUT_POINTER_MOVE,
    CLV_INPUT_SCROLL_LINE_UP,
    CLV_INPUT_SCROLL_LINE_DOWN,
    CLV_INPUT_SCROLL_PAGE_UP,
    CLV_INPUT_SCROLL_PAGE_DOWN,
    CLV_INPUT_SCROLL_POSITION,
    CLV_INPUT_NAV_PREV,
    CLV_INPUT_NAV_NEXT,
    CLV_INPUT_NAV_PAGE_UP,
    CLV_INPUT_NAV_PAGE_DOWN,
    CLV_INPUT_NAV_FIRST,
    CLV_INPUT_NAV_LAST,
    CLV_INPUT_NAV_ACTIVATE
} CLV_InputType;

typedef enum CLV_EventType
{
    CLV_EVENT_NONE = 0,
    CLV_EVENT_SELECTION_CHANGED,
    CLV_EVENT_SCROLL_CHANGED,
    CLV_EVENT_ACTIVATED   /* NAV_ACTIVATE only */
} CLV_EventType;

LONG clv_control_get_selected(const CLV_Control *c); /* -1 if none */
```

| Symbol | Layer |
|--------|-------|
| `CLV_INPUT_NAV_*` / `CLV_EVENT_ACTIVATED` / `get_selected` | **Experimental** public (control header) |
| `clv_ctrl_find_selectable`, page helpers | **Internal** static in `clv_control_input.c` |
| `demo_translate_rawkey`, `demo_run_exercise`, `active_control` | **Demo-only** |
| Raw-key translation in control core / backend | **Not** for Phase 5.5 |

### 6.3 Internal selectable-row search (pseudocode)

```c
/* dir: +1 forward, -1 backward; start = index to begin search after/before */
static LONG clv_ctrl_find_selectable(const CLV_Control *c, LONG start, LONG dir);

/* NAV_NEXT: start = selected_row; if selected_row < 0, find from -1 going +1 */
/* Page: compute origin_top, step; scan layout_rows for first matching selectable */
```

### 6.4 Result events

| Op outcome | `CLV_Event` |
|------------|-------------|
| Selection changed (optional scroll via make_visible) | `SELECTION_CHANGED` (`row`, `previous_row`, `value` = scroll_y) |
| Scroll only (`SCROLL_*`) | `SCROLL_CHANGED` |
| Activate | `ACTIVATED` (`row` = selected; `previous_row` = −1) |
| No-op | `NONE` / handle_input FALSE |

### 6.5 Raw-key translation (demo pseudocode)

```c
/* demo-only */
#define DEMO_RAWKEY_RETURN  0x44

static BOOL demo_translate_rawkey(UWORD code, UWORD qual, CLV_InputEvent *out)
{
    UWORD key;

    if ((code & IECODE_UP_PREFIX) != 0)
        return FALSE;
    key = (UWORD)(code & ~IECODE_UP_PREFIX);

    if (key == DEMO_RAWKEY_RETURN) {
        out->type = (UWORD)CLV_INPUT_NAV_ACTIVATE;
        return TRUE;
    }
    if (key == CURSORUP || key == CURSORDOWN) {
        if ((qual & IEQUALIFIER_CONTROL) != 0)
            out->type = (key == CURSORUP) ? CLV_INPUT_NAV_FIRST
                                          : CLV_INPUT_NAV_LAST;
        else if ((qual & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) != 0)
            out->type = (key == CURSORUP) ? CLV_INPUT_NAV_PAGE_UP
                                          : CLV_INPUT_NAV_PAGE_DOWN;
        else
            out->type = (key == CURSORUP) ? CLV_INPUT_NAV_PREV
                                          : CLV_INPUT_NAV_NEXT;
        return TRUE;
    }
    return FALSE;
}
```

### 6.6 Active-control routing (demo)

```c
CLV_Control *active_control; /* window-local; not global */

/* on SELECT_DOWN that lands in a control viewport: active_control = that control */
/* on RAWKEY: if (active_control) demo_apply_input(active_control, …) */
```

---

## 7. File-by-file implementation plan

For the **Phase 5.5 implementation agent** (not this investigation).

| Path | Responsibility | Functions / types | Ownership | CLV v1? |
|------|----------------|-------------------|-----------|---------|
| [`src/custom_listview_control/clv_control.h`](../src/custom_listview_control/clv_control.h) | Experimental API extensions | `CLV_INPUT_NAV_*`, `CLV_EVENT_ACTIVATED`, `clv_control_get_selected` | N/A (decls) | **No** |
| [`src/custom_listview_control/clv_control_input.c`](../src/custom_listview_control/clv_control_input.c) | NAV handlers; selectable walk; reuse make_visible | extend `handle_input`; static find helpers | No new heap | **No** |
| [`src/custom_listview_control/clv_control_internal.h`](../src/custom_listview_control/clv_control_internal.h) | Unchanged focus policy | Do **not** add active-control field | — | **No** |
| [`src/custom_listview_control/clv_control_scroll.c`](../src/custom_listview_control/clv_control_scroll.c) | Unchanged unless tiny helper needed | Existing clamp | — | **No** |
| [`src/custom_listview_control/clv_control_render.c`](../src/custom_listview_control/clv_control_render.c) | Unchanged paint rules | Existing partial/smart/full | — | **No** |
| [`src/custom_listview_control/backends/clv_backend_amiga_v36.*`](../src/custom_listview_control/backends/clv_backend_amiga_v36.c) | Unchanged (draw only) | — | — | **No** |
| [`examples/custom_control_demo/main.c`](../examples/custom_control_demo/main.c) | RAWKEY, translate, focus pointer, EXERCISE, activate print | `demo_translate_rawkey`, `demo_run_exercise` | App-owned `active_control` | **No** |
| [`examples/custom_control_demo/README.md`](../examples/custom_control_demo/README.md) | Document keys, EXERCISE, sizes | — | — | **No** |
| Living design doc | Phase 5.5 completion record after impl | — | — | **No** |
| `src/custom_listview/**` (v1) | Untouched | — | — | Preserve |

**Size / modularity:**

| Piece | Est. impact | Link policy |
|-------|-------------|-------------|
| Core NAV logic | ~0.5–1.5 KB object | Standard custom-control build |
| Demo RAWKEY + focus | ~0.3–0.8 KB | Demo only |
| EXERCISE driver | ~0.5 KB | Demo only |
| Activation | Negligible | Core event enum |

Raw-key translation stays **out of** control core (matches Phase 4 IDCMP-in-demo architecture). Optional later omit-at-link helper only if measurement shows duplication pain.

---

## 8. Deterministic profiling exercise design

### 8.1 Goals

- Phase 6 can invoke a fixed navigation/scroll workload without synthesising RAWKEY.
- Same neutral ops as real keyboard input.
- Consistent on Workbench 2.x and 3.x; independent of key-repeat timing.

### 8.2 Trigger

- **CLI argument:** `EXERCISE` (e.g. `custom-control-demo EXERCISE`).
- Default: run sequence after first paint, then **remain open** for visual inspection.
- Do **not** use fake `IDCMP_RAWKEY` injection.
- Do **not** rely on holding keys.

### 8.3 Fixed inputs

- Current demo row set (includes non-selectable category row 3 and wrapped rows).
- Starting state: `scroll_y = 0`, `selected_row = -1` (or explicitly clear selection).
- Fixed window/control bounds from normal demo open (document actual init size in Phase 5.5 README).

### 8.4 Profiling reset point

Immediately **before** the sequence:

1. Reset any demo counters / optional log markers.
2. Ensure start selection/scroll as above.
3. Phase 6 may start timers here later.

### 8.5 Scenario steps

1. 50 × `CLV_INPUT_NAV_NEXT`
2. 25 × `CLV_INPUT_NAV_PREV`
3. 20 × `CLV_INPUT_SCROLL_LINE_DOWN`
4. 20 × `CLV_INPUT_SCROLL_LINE_UP`
5. 5 × `CLV_INPUT_NAV_PAGE_DOWN`
6. 5 × `CLV_INPUT_NAV_PAGE_UP`
7. 1 × `CLV_INPUT_NAV_LAST`
8. 1 × `CLV_INPUT_NAV_FIRST`
9. Short next/prev burst that crosses the non-selectable heading

Each step goes through `demo_apply_input` (or equivalent) so paint + scroller sync match interactive use.

### 8.6 Later result collection (Phase 6)

- Run under smart-scroll **on** and `CLV_ENABLE_SMART_SCROLL=0`.
- Logging build may count selection paints, smart scrolls, full viewport fallbacks.
- Visual check: selection highlight correct; heading skipped; scroller knob matches `scroll_y`.

---

## 9. Test matrix

| # | Case | Expect |
|---|------|--------|
| 1 | Workbench 2.04 | Cursor/Shift/Ctrl/Return map; no crash |
| 2 | Workbench 3.2 | Same |
| 3 | 68000 build (`-cpu=68000`) | Links and runs |
| 4 | One-line rows | Prev/next selection correct |
| 5 | Wrapped multi-line rows | Whole logical row selects; make_visible |
| 6 | Non-selectable rows | Skipped by NAV_*; mouse still rejects |
| 7 | Empty list | NAV_* no-op |
| 8 | All non-selectable | NAV_* no-op |
| 9 | First / last row | Clamp; Ctrl+Up/Down |
| 10 | Page with partially clipped rows | Page target + make_visible; paint policy |
| 11 | Key repeat (hold Up/Down) | OS repeat advances selection |
| 12 | Modifier combinations | Shift=page; Ctrl=first/last; Ctrl wins over Shift |
| 13 | Two controls (doc / optional smoke) | Only active receives keys |
| 14 | Click-to-focus transfer | Click other viewport retargets keys |
| 15 | Outside click | Active preserved |
| 16 | Scroller synchronisation | After each NAV that scrolls |
| 17 | Resize while keyboard-selected and scrolled | Selection preserved if still valid; layout ok |
| 18 | `EXERCISE` CLI | Deterministic; no RAWKEY injection |
| 19 | Return activate | `CLV_EVENT_ACTIVATED` when selected |
| 20 | Space | Ignored |
| 21 | Smart scroll on/off | Exercise + interactive line scroll |

---

## 10. Risks and open questions

| Item | Class | Notes |
|------|-------|-------|
| Shift+cursor vs unusual keymaps | Must resolve during implementation | Stick to raw `CURSOR*` + qualifier bits |
| CapsLock / numeric-pad quirks | Must resolve during implementation | Verify on real WB2.x/3.x |
| Dual-control runtime still untested | Safe to defer | Document routing; single-control demo OK |
| Richer activation (double-click parity) | Safe to defer | Return → `ACTIVATED` only in 5.5 |
| Size delta vs Phase 5 baselines | Must measure in 5.5 | Not a planning blocker |
| True blockers for planning / starting 5.5 | **None** | |

---

## 11. Roadmap amendment

### Phase 5.5 — Keyboard Navigation and Deterministic Exercise API

**Status (this document):** **Implemented** — see living design Phase 5.5
Completion Record  
**Insert between:** Phase 5 (complete) and Phase 6.

#### Objective

Add classic-Amiga keyboard selection and page/first/last navigation to the experimental custom control, plus a neutral deterministic exercise path Phase 6 can use for profiling—without synthesising raw-key messages or changing CLV v1.

#### Scope

- Extend experimental `CLV_InputType` with `NAV_*` and `CLV_EVENT_ACTIVATED`.
- Implement navigation in `clv_control_handle_input` sharing selectability + `make_visible` with mouse.
- Demo: `IDCMP_RAWKEY`, translate with `CURSORUP`/`CURSORDOWN`/`0x44`, app-owned `active_control`, Return activate, `EXERCISE` CLI.
- Reuse Phase 5 partial / smart / full paint rules and scroller sync.
- Record size deltas vs Phase 5 baselines.
- Update living design Phase 5.5 completion record; revise Phase 6 handoff (remove “do not add keyboard”).

#### Non-goals

- Phase 6 general optimisation or V2 recommendation work
- CLV v1 API or example changes
- Home / End / Page Up / Page Down keys
- Space activation
- `IDCMP_INTUITICKS` repeat engine
- Icons, sorting, horizontal scrolling, multi-selection, inline editing, DnD
- Workbench 1.3 backend
- Synthetic RAWKEY injection
- Global mutable active-control state
- `clv_control_navigate()` parallel API

#### Exit criteria

- [x] 68000 build validated (VBCC link)
- [x] Non-selectable rows skipped in implementation; empty / all-non-selectable safe
- [x] Page / first / last behaviour matches this spec (code)
- [x] Scroller sync path reused after keyboard nav
- [x] `EXERCISE` runs without RAWKEY injection (demo CLI)
- [x] Phase 5 refresh/clip/smart-scroll paint policy preserved in demo
- [x] Sizes recorded; living design Phase 5.5 marked complete
- [x] Phase 6 handoff updated to consume exercise API
- [ ] Verified key mapping works on Workbench 2.x and 3.2 (manual Amiga checklist)

#### Phase 6 handoff (after 5.5)

- Baselines include keyboard/exercise code size.
- Phase 6 may drive `EXERCISE` for timing (smart on/off).
- Still no application migration; still no v1 removal.
- Keyboard is no longer a “do not add” item; further keyboard polish is out of Phase 6 unless evidence-backed and tiny.

---

## 12. Implementation handoff prompt

Copy for the next agent:

```text
Implement Phase 5.5 only — Keyboard Navigation and Deterministic Exercise API —
per docs/CLV_CUSTOM_CONTROL_KEYBOARD_NAVIGATION_PLAN.md and the Phase 5.5
section of docs/CLV_CUSTOM_CONTROL_DESIGN_AND_IMPLEMENTATION_PLAN.md.

Do:
- Extend experimental CLV_InputType with NAV_PREV/NEXT/PAGE_*/FIRST/LAST/ACTIVATE
  and CLV_EVENT_ACTIVATED; add clv_control_get_selected.
- Implement navigation in clv_control_handle_input using shared selectability +
  make_visible; selection-centric page step = max(vp_h - line_h, line_h).
- Demo: IDCMP_RAWKEY; translate with CURSORUP/CURSORDOWN and demo-local 0x44;
  app-owned active_control (no globals); Return → ACTIVATE; Space unused;
  EXERCISE CLI calling the same handle_input path (no synthetic RAWKEY).
- Reuse Phase 5 paint rules (partial selection / smart scroll / full on
  selection+make_visible); keep scroller sync.
- Update demo README, measure sizes, write Phase 5.5 completion record.

Do not:
- Begin Phase 6 optimisation or V2 recommendation.
- Change CLV v1 public API or v1 examples.
- Add Home/End/PageUp/PageDown, Space activation, INTUITICKS repeat engine,
  icons, sorting, horizontal scroll, multi-select, WB 1.3, or
  clv_control_navigate().
- Put IntuiMessage or raw-key translation into control core/layout/render.
- Introduce global mutable active-control state.
```

---

## Completion report (investigation)

| Item | Result |
|------|--------|
| Files inspected | Design + Phase 1 audit; `clv_control.h`, `clv_control_internal.h`, `clv_control_input.c`, `clv_control_scroll.c`, `clv_control.c`, `clv_control_render.c` (paint/selection), `clv_backend_amiga_v36.*`, `examples/custom_control_demo/main.c` + README; AutoDocs intuition/input/console/keymap/gadtools; host SDK `intuition.h` / `inputevent.h` / `keymap.h` |
| Files created | This document |
| Files changed (roadmap) | Living design doc — Phase 5.5 amendment + link only |
| Recommended key mapping | Cursor±; Shift+cursor = page; Ctrl+cursor = first/last; Return = activate; Space unused; no Home/End/PgUp/PgDn |
| Recommended API shape | Extend `CLV_InputEvent` / `handle_input`; no `navigate()` |
| Recommended focus model | App-owned `active_control` pointer; click-to-focus; outside click preserves |
| Recommended roadmap position | **Phase 5.5 before Phase 6** |
| Blockers | **None** |
| Production implementation | **Performed** in Phase 5.5 (see living design Completion Record) |
