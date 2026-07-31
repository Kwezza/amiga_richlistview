# CLV Cell-Control Event Notification and Demo Plan

**Status:** Phase E4 complete — cell-control event notification path closed for application use  
**Target package:** `src/custom_listview_control/`  
**Primary objective:** Generalise the existing checkbox toggle event into a generic application-facing cell-control notification, then prove it in the demo status field  
**Demo objective:** Show the most recent control event in a read-only GadTools status field beneath the custom ListView  
**Working method:** One phase per AI-agent session unless a phase is small enough to complete safely together  
**Implementation authority:** Agents may revise this plan where source evidence or technical blockers justify a change, but every material change must be documented before or alongside implementation

---

## 1. Purpose

The custom ListView already supports drawn checkbox cells that can be toggled by mouse and keyboard.

**E1 audit note:** The host is already notified today via experimental
`CLV_EVENT_CELL_TOGGLED` (row, column, `previous_value`, `cell_value`,
`row_user_data`). The remaining gap is that the event is checkbox-shaped
(name and fields), not a generic cell-control notification suitable for
future buttons/cycle cells, and the demo only prints to the console.

The calling program still needs a clean, generic notification so it can:

- update its own authoritative data;
- trigger application-specific actions;
- save configuration changes;
- refresh other UI;
- support future cell controls such as buttons, cycle controls, or compact action gadgets.

This work must add an event mechanism that feels natural in a classic Amiga event loop.

The preferred model is:

1. the application translates IDCMP input into `CLV_InputEvent`;
2. `clv_control_handle_input()` processes the input;
3. when a completed cell-control action occurs, it fills and returns a neutral `CLV_Event`;
4. the application handles that event in its normal window loop;
5. the demo formats the event details and displays them in a read-only GadTools `TEXT_KIND` gadget beneath the ListView.

This should behave conceptually like receiving an `IDCMP_GADGETUP` notification: the application is informed only after a valid interaction has completed.

---

## 2. Design goals

The implementation must:

- use the existing synchronous `CLV_Event` path where practical;
- avoid callbacks that execute application code from inside the control;
- avoid allocating Exec messages for ordinary local UI notifications;
- identify the affected logical row and column;
- identify the control type and action;
- report previous and new values where meaningful;
- include row `user_data` where safely available;
- work for both mouse and keyboard activation;
- remain suitable for future buttons and other drawn controls;
- preserve existing selection, activation, scrolling, and redraw behaviour;
- keep the control core independent of application-specific meaning.

The demo must:

- place a read-only status field immediately beneath the custom ListView;
- update it without deleting and recreating the gadget;
- show enough event information to prove that the application received the event;
- show friendly names rather than only numeric IDs;
- continue to work on Workbench 2.x and 3.x targets supported by the project.

---

## 3. Locked event model (Phase E1)

> **Locked 2026-07-30.** Section 3 was a design target before E1. After
> source audit it is the ABI E2 must implement. See Design Change EC-001
> for the revision vs the original sketch (`LONG value` clash, deferred
> `control_user_data`, rename of `CELL_TOGGLED`).

### 3.1 Generic event type

Rename the existing checkbox-only top-level event to one generic cell-control event. Keep the same enum ordinal (immediately after `CLV_EVENT_ACTIVATED`):

```c
typedef enum CLV_EventType
{
    CLV_EVENT_NONE = 0,
    CLV_EVENT_SELECTION_CHANGED,
    CLV_EVENT_SCROLL_CHANGED,
    CLV_EVENT_ACTIVATED,       /* NAV_ACTIVATE / Return — row activation */
    CLV_EVENT_CELL_CONTROL     /* was CLV_EVENT_CELL_TOGGLED */
} CLV_EventType;
```

Do **not** keep a parallel checkbox-only event type. Update all call sites
(demo + docs). No compatibility `#define` alias is required: the surface is
still experimental and the only in-tree consumer is
`examples/custom_control_demo/`.

### 3.2 Generic control action

Add a control action identifier separate from the control type:

```c
typedef enum CLV_CellControlAction
{
    CLV_CTRL_ACTION_NONE = 0,
    CLV_CTRL_ACTION_VALUE_CHANGED,
    CLV_CTRL_ACTION_PRESSED
} CLV_CellControlAction;
```

Initial use:

- checkbox: `CLV_CTRL_ACTION_VALUE_CHANGED`;
- future button: `CLV_CTRL_ACTION_PRESSED`;
- future cycle control: likely `CLV_CTRL_ACTION_VALUE_CHANGED`.

`control_type` reuses existing column-type constants
(`CLV_CTRL_COL_TYPE_CHECKBOX`, later button/cycle types in the same mask).
Do not invent a second parallel type enum.

### 3.3 Event payload (locked)

```c
typedef struct CLV_Event
{
    UWORD type;             /* CLV_EventType */
    LONG  row;              /* logical row; -1 if none */
    LONG  previous_row;     /* SELECTION_CHANGED prior selection; else -1 */
    LONG  value;            /* scroll_y on scroll/selection; unused on CELL_CONTROL */
    UWORD column;           /* CELL_CONTROL */
    UWORD control_type;     /* CELL_CONTROL: CLV_CTRL_COL_TYPE_* */
    UWORD control_action;   /* CELL_CONTROL: CLV_CTRL_ACTION_* */
    APTR  row_user_data;    /* CELL_CONTROL: borrowed row user_data at commit */
    UBYTE previous_value;   /* CELL_CONTROL */
    UBYTE cell_value;       /* CELL_CONTROL new value (not LONG value — EC-001) */
} CLV_Event;
```

A control event must expose:

- event type (`CLV_EVENT_CELL_CONTROL`);
- logical row;
- column;
- control type;
- control action;
- previous value;
- new value (`cell_value`);
- row `user_data` (borrowed pointer copied at commit).

`control_user_data` is **deferred** (EC-001). `CLV_ControlCell` is only
`flags` + `value` today; adding per-cell user data would enlarge every
snapshot cell without a current consumer.

### 3.4 Event lifetime and ownership

- Events are filled synchronously into a caller-owned `CLV_Event` on the stack.
- No heap allocation; no Exec messages; no callbacks into app code.
- `row_user_data` is a borrowed pointer valid only while the application’s
  row array remains alive (same rule as `CLV_ControlRow.user_data`).
- The event struct must not be retained past the `handle_input` return /
  immediate app handler.

---

## 4. Event semantics

### 4.1 Checkbox by mouse

```text
SELECT_DOWN inside enabled interactive checkbox
    -> arm that row/column/control

SELECT_UP inside the same checkbox
    -> commit toggle
    -> update internal checkbox state
    -> return CLV_EVENT_CELL_CONTROL
    -> control_type = CLV_CTRL_COL_TYPE_CHECKBOX
    -> action = CLV_CTRL_ACTION_VALUE_CHANGED
    -> previous_value and cell_value populated
```

Release outside, release over a different control, disabled controls, and display-only controls must not generate a change event.

### 4.2 Checkbox by keyboard

A valid keyboard toggle must emit the same generic event shape as the mouse path.

The event must not force the application to care whether activation came from mouse or keyboard unless the existing event model already records input origin cleanly.

### 4.3 Future button

A future button should be able to use the same event structure:

```text
type            = CLV_EVENT_CELL_CONTROL
control_type    = CLV_CTRL_CELL_BUTTON
control_action  = CLV_CTRL_ACTION_PRESSED
row             = affected row
column          = affected column
```

No value transition is required for a stateless button. `previous_value` and `cell_value` may both be zero or reserved.

### 4.4 Selection interaction

Existing selection events must remain intact.

Preferred behaviour:

- row selection occurs through the existing selection path;
- checkbox commit produces the cell-control event;
- neither event silently replaces the other;
- if selection occurs on button-down and toggle commits on button-up, the two input messages may naturally produce two separate events.

The implementing agent must verify the actual event-return behaviour before changing it.

---

## 5. Demo status gadget

### 5.1 Gadget type

Use a GadTools `TEXT_KIND` gadget configured as a read-only bordered status field.

Do not use `STRING_KIND` unless a verified platform limitation prevents `TEXT_KIND` from updating correctly. A string gadget introduces unnecessary editing, cursor, and focus behaviour.

Suggested initial text:

```text
Last control event: none
```

### 5.2 Update method

Update the gadget with `GT_SetGadgetAttrs()`:

```c
GT_SetGadgetAttrs(
    event_text_gadget,
    window,
    NULL,
    GTTX_Text, (ULONG)event_text,
    TAG_DONE);
```

Keep the backing text buffer alive for as long as GadTools may reference it.

Suggested storage:

```c
char event_text[160];
```

The implementation must verify whether the target GadTools version copies or borrows the string. The safe default is to keep a persistent buffer owned by the demo.

### 5.3 Suggested event display

Example checkbox event:

```text
CELL: row=1 (Beta) col=4 (On) CHECKBOX CHANGED 0 -> 1
```

A more compact fallback for low-width windows:

```text
Beta / On: CHECKBOX 0 -> 1
```

The demo should preferably include:

- row index;
- row name or row `user_data` identity where practical;
- column index or title;
- control type name;
- action name;
- old and new values.

### 5.4 Friendly name helpers

Add small demo-local helpers, not core-library dependencies:

```c
static CONST_STRPTR clv_demo_control_type_name(UWORD type);
static CONST_STRPTR clv_demo_control_action_name(UWORD action);
```

The control library should return stable IDs. Formatting them into English text belongs in the demo/application.

---

## 6. Implementation phases

## Phase E1 — Source audit and event design lock

**Status:** Complete (2026-07-30)  
**Objective:** Confirm the current event, input, checkbox state, row data, and demo architecture before changing code.

### Required work

Inspected:

- `src/custom_listview_control/clv_control.h` — public ABI;
- `src/custom_listview_control/clv_control_input.c` — clear/arm/hit/toggle/commit/`handle_input`;
- `src/custom_listview_control/clv_control_internal.h` — snapshot + private arm;
- `src/custom_listview_control/clv_control.c` — snapshot copy / setter;
- `examples/custom_control_demo/main.c` — IDCMP translate, `demo_apply_input`, `CreateContext` gadget chain, `DemoGeom`;
- `docs/AutoDocs/gadtools.doc` — `TEXT_KIND` / `GTTX_Text` / `GT_SetGadgetAttrs` (V36+).

### Audit answers

1. **Can `CLV_Event` be safely extended?**  
   **Yes**, within the experimental control package. The struct is filled by
   `handle_input` into caller stack storage; the only in-tree consumer is the
   custom-control demo. Adding `control_type` / `control_action` and renaming
   `CLV_EVENT_CELL_TOGGLED` → `CLV_EVENT_CELL_CONTROL` is an intentional
   experimental ABI break that E2 must update in the demo and docs. Do not
   reuse `LONG value` for the new cell value (already means scroll_y).

2. **Does one input call return only one event?**  
   **Yes.** Header, §D.11, and `handle_input` enforce at most one
   `CLV_EventType` per call. Mouse selection (SELECT_DOWN) and toggle
   (SELECT_UP) are separate invocations; they never compound.

3. **Do mouse and keyboard paths share a toggle helper?**  
   **Yes.** Both `clv_ctrl_commit_checkbox_toggle` (SELECT_UP) and
   `CLV_INPUT_TOGGLE` call `clv_ctrl_toggle_checkbox_at()`. E2 should
   centralise event field fill there (or extract `clv_control_fill_cell_event`
   and call it from that helper).

4. **Where is the old value still available at commit time?**  
   Read from the owned snapshot **before** mutation:
   `prev = c->cell_snapshot[index].value`, then write `next`, then fill
   `previous_value` / `cell_value`. Available on both mouse and keyboard paths.

5. **Can row `user_data` be copied into the event safely?**  
   **Yes**, as a borrowed pointer:
   `result->row_user_data = c->rows[row].user_data`. Demo keeps row storage
   for the window lifetime and syncs via that pointer on toggle. Lifetime
   equals the app’s row array (synchronous handler only).

6. **Is `control_user_data` justified now?**  
   **Defer.** `CLV_ControlCell` has only `flags` + `value`. No natural
   per-cell user-data slot without enlarging every snapshot cell. Row
   `user_data` already covers the demo’s authoritative Boolean pattern.

7. **Demo gadget context suitable for `TEXT_KIND`?**  
   **Yes.** Persistent `CreateContext` → `CreateGadget` chain (scroller +
   action strip). Resize rebuilds the whole chain (`demo_handle_newsize`).
   E3 can insert a bordered `TEXT_KIND` between the control bottom and the
   Go/cycle strip by shrinking `ctrl_h` / adjusting `go_top` in
   `demo_compute_geom` (today: `go_top = ctrl_top + ctrl_h + DEMO_PAD`).

8. **Is `GT_SetGadgetAttrs(GTTX_Text)` reliable?**  
   **Yes on V36+.** AutoDocs: `GTTX_Text` on `GT_SetGadgetAttrs` is V36;
   subsequent updates **reference by pointer, not copy**. Demo must keep a
   persistent `char event_text[160]` (or similar). `GTTX_CopyText` only
   applies at `CreateGadget` time (V37) and still does not copy later
   `SetGadgetAttrs` updates — persistent buffer remains mandatory.
   Prefer `TEXT_KIND` + `GTTX_Border, TRUE`; do not use `STRING_KIND`.

9. **Is `CLV_EVENT_ACTIVATED` still appropriate?**  
   **Yes.** It means row activation via `CLV_INPUT_NAV_ACTIVATE` (Return)
   only and must not toggle or become a cell-control event. Keep the name
   and semantics unchanged.

### Deliverables (E1)

- Design Change **EC-001** (Accepted) — see §11 below.
- Locked ABI — §3 above.
- Files E2 must change:
  - `src/custom_listview_control/clv_control.h`
  - `src/custom_listview_control/clv_control_input.c`
  - `examples/custom_control_demo/main.c` (event type / field names only in E2;
    status gadget is E3)
  - docs: this plan, developer log, demo README / master-plan mentions of
    `CELL_TOGGLED` as E2/E4 touch as needed
- Compatibility risks:
  - experimental rename `CELL_TOGGLED` → `CELL_CONTROL`;
  - struct grows by two `UWORD`s;
  - any out-of-tree experimental consumers must update;
  - keep `UBYTE cell_value` (not `LONG value`).
- **No behavioural product code changed in E1** (docs only).

### Exit criteria

- [x] mouse and keyboard event paths fully traced;
- [x] event ABI and ownership explicit;
- [x] no application-specific behaviour placed in the control core.

---

## Phase E2 — Generic cell-control event implementation

**Status:** Complete (2026-07-30)  
**Objective:** Replace `CLV_EVENT_CELL_TOGGLED` with locked `CLV_EVENT_CELL_CONTROL` and fill the generic payload from checkbox commits.

### Required work

- [x] rename `CLV_EVENT_CELL_TOGGLED` → `CLV_EVENT_CELL_CONTROL` (same ordinal);
- [x] add `CLV_CellControlAction` / `CLV_CTRL_ACTION_*`;
- [x] add `control_type` and `control_action` to `CLV_Event`;
- [x] keep `UBYTE previous_value` / `UBYTE cell_value` (EC-001);
- [x] initialise all event fields deterministically in `clv_ctrl_clear_event`;
- [x] emit `CLV_EVENT_CELL_CONTROL` + `VALUE_CHANGED` + `CLV_CTRL_COL_TYPE_CHECKBOX`
  from mouse SELECT_UP commit and keyboard `CLV_INPUT_TOGGLE`;
- [x] include previous/new values, logical row, column, `row_user_data`;
- [x] preserve selection / scroll / `ACTIVATED` return conventions;
- [x] update demo handler field/type names (no status gadget yet — E3);
- [x] do not add callbacks or application code inside the control;
- [x] do not add `control_user_data`.

### Implementation notes

Centralised fill via `clv_ctrl_fill_cell_event()` called from
`clv_ctrl_toggle_checkbox_at()` — the single mutation point shared by
SELECT_UP commit and `CLV_INPUT_TOGGLE`. No behavioural change to arm /
cancel / disabled / display-only paths.

Files changed:

| Path | Change |
|------|--------|
| `src/custom_listview_control/clv_control.h` | Rename event; add action enum; extend `CLV_Event` |
| `src/custom_listview_control/clv_control_input.c` | clear + fill helper; emit CELL_CONTROL |
| `examples/custom_control_demo/main.c` | Handler uses `CLV_EVENT_CELL_CONTROL` + new fields |
| `examples/custom_control_demo/README.md` | Live integrator docs rename |
| `docs/CLV_CELL_CONTROL_EVENT_NOTIFICATION_PLAN.md` | This phase record |
| `docs/CLV_CONTROL_CELLS_DEVELOPER_LOG.md` | E2 entry |

### Tests

Code-path / build verification (Amiga runtime UI not re-run this session):

| Check | Result |
|-------|--------|
| Mouse/keyboard share `clv_ctrl_toggle_checkbox_at` | PASS (unchanged call graph) |
| Fill sets type/action/checkbox + prev/new/`row_user_data` | PASS (code review) |
| `clv_ctrl_clear_event` zeros `control_type` / `control_action` | PASS |
| Disabled / display-only / release-outside still no event | PASS (no path change) |
| Selection / scroll / ACTIVATED conventions preserved | PASS (no path change) |
| VBCC `make custom-control-demo` | PASS — `bin/custom-control-demo` **43644** bytes |

Amiga/emulator visual re-check of toggle printing remains recommended before
closing E3 (status gadget will make delivery visible).

### Exit criteria

- [x] host application can identify exactly which cell control changed;
- [x] mouse and keyboard produce equivalent control events;
- [x] no duplicate event is produced for one completed toggle;
- [x] existing input behaviour is not regressed (compile + path audit).

---

## Phase E3 — Demo status field

**Status:** Complete (2026-07-30)  
**Objective:** Prove event delivery through the real application/window loop.

### Required work

- [x] add a bordered read-only `TEXT_KIND` gadget below the custom ListView;
- [x] adjust demo window/list bounds if needed;
- [x] keep the event text buffer persistent;
- [x] listen for `CLV_EVENT_CELL_CONTROL` in the demo window code;
- [x] format a clear message;
- [x] update the gadget with `GT_SetGadgetAttrs()`;
- [x] refresh only what GadTools requires;
- [x] retain the existing custom ListView and button controls;
- [x] ensure resizing, scrolling, and keyboard control continue to work
  (status geom participates in `demo_compute_geom` / gadget recreate).

### Implementation notes

Layout: ListView + scroller, then bordered `TEXT_KIND` spanning
`ctrl_w + scroll_w`, then the existing Go/cycle action strip. `ctrl_h`
shrinks to reserve `status_h + DEMO_PAD + action_h`. Persistent
`g_demo_event_text[160]` initialised to `"Last control event: none"`;
`CreateGadget` and later `GT_SetGadgetAttrs(GTTX_Text)` both borrow it.

Demo-local helpers (not linked into the control core):

- `demo_control_type_name` / `demo_control_action_name`
- `demo_update_event_status` — formats friendly text and updates the gadget

On `CLV_EVENT_CELL_CONTROL`, the demo still syncs the app store and
repaints the logical row; console printf remains as a secondary trace.

Window title set to `CLV Custom Control (Phase E3)`.

Files changed:

| Path | Change |
|------|--------|
| `examples/custom_control_demo/main.c` | Status geom + TEXT_KIND + status update |
| `examples/custom_control_demo/README.md` | Status-field integrator note |
| `docs/CLV_CELL_CONTROL_EVENT_NOTIFICATION_PLAN.md` | This phase record |
| `docs/CLV_CONTROL_CELLS_DEVELOPER_LOG.md` | E3 entry |

### Tests

| Check | Result |
|-------|--------|
| `TEXT_KIND` + `GTTX_Border` in gadget chain | PASS (code) |
| Persistent buffer; no STRING_KIND | PASS |
| `demo_apply_input` calls `demo_update_event_status` on CELL_CONTROL | PASS |
| Resize recreates status gadget with current buffer text | PASS (path) |
| VBCC `make custom-control-demo` | PASS — `bin/custom-control-demo` **44824** bytes
  (+1180 vs E2 43644; demo status gadget + format helpers) |

Amiga/emulator visual confirmation of mouse and keyboard status updates
remains recommended (host cannot run the Amiga UI).

### Exit criteria

- [x] visible proof path that the application receives the event (status gadget);
- [x] status field updates without gadget recreation (on toggle);
- [x] no intentional visual regression in the custom ListView layout path;
- [x] mouse and keyboard share the same status update helper (both via
  `demo_apply_input` → CELL_CONTROL).

---

## Phase E4 — Future-control readiness and closure audit

**Status:** Complete (2026-07-30)  
**Objective:** Confirm that the event design is genuinely generic and documented.

### Required work

- [x] review the event names for checkbox-specific leakage;
- [x] confirm a future button can use `CLV_CTRL_ACTION_PRESSED`;
- [x] confirm a future cycle control can use `CLV_CTRL_ACTION_VALUE_CHANGED`;
- [x] confirm event construction is centralised;
- [x] document value semantics for stateless controls;
- [x] document row and control `user_data` ownership;
- [x] document synchronous delivery and event lifetime;
- [x] update public header comments;
- [x] update integration documentation;
- [x] update the developer log;
- [x] run standard builds and tests;
- [x] record executable-size delta if measurable.

### Audit findings

| Check | Result |
|-------|--------|
| Public event name leakage | PASS — `CLV_EVENT_CELL_CONTROL` is control-neutral (no `TOGGLED`) |
| Action enum leakage | PASS — `VALUE_CHANGED` / `PRESSED` (not checkbox-shaped) |
| Payload field names | PASS — `previous_value` / `cell_value` suit stateful cells; unused on PRESSED |
| Internal helper names | OK — `clv_ctrl_toggle_checkbox_*` remain checkbox implementation detail |
| Checkbox-only public setter | OK — `clv_control_set_checkbox_value` is intentionally type-specific |
| Future button path | PASS — same `CELL_CONTROL` + `PRESSED`; values may be zero |
| Future cycle path | PASS — same `CELL_CONTROL` + `VALUE_CHANGED`; values = old/new indices |
| Centralised fill | PASS — sole writer is `clv_ctrl_fill_cell_event()` from toggle commit |
| No callbacks / Exec msgs | PASS |
| Demo formatting in core | PASS — type/action name helpers + status gadget are demo-local |
| `clv_ctrl_clear_event` zeros all fields | PASS (E2; unchanged) |

No Design Change record required: live ABI already matches §3 / EC-001; E4
is documentation and window-title closure only.

### Implementation notes

Files changed:

| Path | Change |
|------|--------|
| `src/custom_listview_control/clv_control.h` | Integrator comments: generic event, actions, lifetime, ownership, stateless values |
| `src/custom_listview_control/clv_control_input.c` | Fill-helper comment clarifies centralised / future-control use |
| `examples/custom_control_demo/main.c` | Window title `Phase E4`; file banner |
| `examples/custom_control_demo/README.md` | Integrator contract section + E4 size note |
| `docs/CLV_CELL_CONTROL_EVENT_NOTIFICATION_PLAN.md` | This phase record |
| `docs/CLV_CONTROL_CELLS_DEVELOPER_LOG.md` | E4 entry |

Window title set to `CLV Custom Control (Phase E4)`.

### Tests

| Check | Result |
|-------|--------|
| VBCC `make custom-control-demo` | PASS — `bin/custom-control-demo` **44824** bytes (0 vs E3) |
| Product behaviour vs E3 | Unchanged (comments/docs only) |
| Closure checklist below | PASS (code + docs review) |

Amiga/emulator visual re-check of the status gadget remains recommended but
is not an E4 blocker (E3 already shipped the visible proof path).

### Closure checks

- [x] no callback function is required;
- [x] no Exec message allocation is required;
- [x] no event pointers outlive the input call;
- [x] no demo formatting code is linked into the core library;
- [x] baseline custom-control behaviour remains intact;
- [x] generic event fields are initialised for every event type;
- [x] future controls can extend enums without changing the overall delivery model.

### Exit criteria

- [x] event API is documented and stable enough for application use;
- [x] demo proves operation;
- [x] no known correctness blocker remains;
- [x] future button work can proceed without redesigning the notification path.

---

## 7. Agent rules and controlled design changes

Each agent must:

1. read this document and the current custom-control master plan;
2. read the relevant developer log;
3. inspect the repository diff before editing;
4. work only on the next incomplete phase;
5. update the controlling documentation before ending;
6. append a dated developer-log entry;
7. record exact builds and tests;
8. leave the repository buildable.

An agent may revise this plan if source evidence reveals a blocker or a better design.

Material changes must be documented using:

```markdown
### Design Change EC-XXX — <title>

**Date:** YYYY-MM-DD
**Phase:** E1/E2/E3/E4
**Status:** Proposed / Accepted / Rejected / Superseded

**Original design:**
<what this plan required>

**Evidence or blocker:**
<source, compiler, runtime, API, or compatibility finding>

**Revised design:**
<new approach>

**Why it is better or necessary:**
<reasoning>

**Consequences:**
<API, files, tests, compatibility, migration>

**Alternatives considered:**
<other options and why rejected>
```

Examples requiring a design record:

- replacing returned events with callbacks;
- adding an Exec message port;
- changing existing event ABI incompatibly;
- adding per-cell heap allocation;
- changing checkbox state ownership;
- using `STRING_KIND` instead of `TEXT_KIND`;
- omitting old/new values;
- removing keyboard event parity;
- introducing a separate checkbox-only callback.

Small implementation details do not require a design record but must still be logged.

---

## 8. Recommended API behaviour summary

```text
Application receives IDCMP
        |
        v
Translate to CLV_InputEvent
        |
        v
clv_control_handle_input()
        |
        +--> selection event
        |
        +--> scroll event
        |
        +--> row activation event
        |
        +--> generic cell-control event
                  |
                  +--> CHECKBOX + VALUE_CHANGED
                  +--> future BUTTON + PRESSED
                  +--> future CYCLE + VALUE_CHANGED
```

The application remains responsible for deciding what the event means.

Examples:

```text
Checkbox changed
    -> update package enabled state

Button pressed
    -> open row editor

Cycle value changed
    -> update mode field
```

The control library must never perform those application-specific actions itself.

---

## 9. Expected final demo layout

```text
+----------------------------------------------------------+
| Custom ListView                                          |
|                                                          |
| Name       Type       Description        Status      On   |
| Alpha      Tool       ...                Ready       [x]  |
| Beta       Library    ...                Idle        [ ]  |
| ...                                                      |
+----------------------------------------------------------+

+----------------------------------------------------------+
| CELL row=1 col=4 CHECKBOX CHANGED 0 -> 1                 |
+----------------------------------------------------------+

[ Go ] [ Divider ] [ X Pad ] [ Y Pad ] [ Gap ]
```

The exact layout may be adjusted to fit the existing demo and Workbench 2.x screen constraints.

---

## 10. Completion record

**Current status:** Phase E4 complete — generic cell-control event path closed  
**Next phase:** None for this plan (future button/cycle cell types are separate feature work; they reuse this notification path)  
**Implementation freeze:** Do not add callbacks or Exec messages; notification delivery model is locked  
**Primary acceptance test:** Toggling a checkbox by mouse or keyboard updates the read-only demo status gadget with the correct row, column, control type, action, previous value, and new value

---

## 11. Design change records

### Design Change EC-001 — Lock ABI to existing `CLV_Event` layout (not the sketch)

**Date:** 2026-07-30  
**Phase:** E1  
**Status:** Accepted

**Original design:**
Section 3 sketched a fresh `CLV_Event` with `LONG previous_value` /
`LONG value`, optional `control_user_data`, and a new
`CLV_EVENT_CELL_CONTROL` type, without first reconciling the live
experimental ABI.

**Evidence or blocker:**
Live `clv_control.h` already has `CLV_EVENT_CELL_TOGGLED` and:

```c
LONG  value;            /* scroll_y */
UWORD column;
APTR  row_user_data;
UBYTE previous_value;
UBYTE cell_value;       /* deliberately not LONG value */
```

`cell_value` exists specifically to avoid clashing with scroll `value`
(§D.11 / C2). Mouse and keyboard already share
`clv_ctrl_toggle_checkbox_at` and copy `row_user_data`.
`CLV_ControlCell` is only `{ UBYTE flags; UBYTE value; }` — no per-cell
user-data field. GadTools AutoDocs confirm `GTTX_Text` updates borrow
the pointer (V36 `GT_SetGadgetAttrs`).

**Revised design:**
- Rename `CLV_EVENT_CELL_TOGGLED` → `CLV_EVENT_CELL_CONTROL` (same ordinal).
- Add `control_type` (`CLV_CTRL_COL_TYPE_*`) and `control_action`
  (`CLV_CTRL_ACTION_*`).
- Keep `UBYTE previous_value` / `UBYTE cell_value`; do **not** overload
  `LONG value` for cell state.
- Keep copying `row_user_data` at commit.
- Defer `control_user_data` until a cell model has a natural slot.
- Keep `CLV_EVENT_ACTIVATED` as row activation only.
- E3: bordered `TEXT_KIND` + persistent buffer + `GT_SetGadgetAttrs(GTTX_Text)`.

**Why it is better or necessary:**
Preserves the scroll field, the existing toggle paths, and Amiga size
discipline, while still delivering a generic event name/actions for
future button/cycle cells.

**Consequences:**
- E2 is an experimental ABI rename + two new `UWORD` fields, not a greenfield event.
- Demo and docs must drop `CELL_TOGGLED` naming.
- No core behaviour change expected beyond field population.

**Alternatives considered:**
- Keep `CELL_TOGGLED` forever — rejected (not future-control ready).
- Alias both names — rejected for an experimental single-consumer API.
- Put new value in `LONG value` — rejected (scroll_y clash).
- Add `control_user_data` now — rejected (no cell storage; row pointer suffices).
