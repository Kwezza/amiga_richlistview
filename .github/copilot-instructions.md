# GitHub Copilot Instructions — RichListview

## Project scope

RichListview is a standalone full custom ListView control for classic 68k AmigaOS.

It owns:

- viewport rendering;
- row and cell layout;
- scrolling;
- selection;
- mouse and keyboard input;
- wrapped rows;
- embedded cell controls;
- application-facing control events.

It does **not** use GadTools `LISTVIEW_KIND`, `GTLV_Labels`, `GTLV_CallBack`, `LVDrawMsg`, or the legacy physical-row selection adapter. A GadTools `SCROLLER_KIND` gadget may be used as a companion scrollbar.

## Architecture boundary

Product implementation code belongs under:

```text
src/rich_listview/
```

Supporting changes may be made to the repository Makefile, examples, tests, and current documentation when required by the task.

Do not add source or build dependencies on the retired ListView repository, and do not import or recreate its retired systems:

- ASCII ListView formatting;
- GadTools draw-hook renderers;
- renderer binders;
- `clv_selection_*`;
- `clv_cellctl_*`;
- legacy prepared-label or physical-row maps.

For RichListview checkbox or cell-control work, use `rlv_checkbox.c` and the control event path. Do not patch or restore the old GadTools cell-control implementation.

A small architecture-neutral algorithm may be copied or rewritten when this keeps RichListview independent. Do not copy legacy data structures, lifecycle rules, selection adapters, prepared-row models, renderer hooks, or other architecture-specific behaviour.

## Target platform

Target classic 68k Amiga hardware and AmigaOS 3.x.

Preserve the existing V36/V37-compatible implementation paths. Do not raise the minimum required version of intuition, graphics, layers, gadtools, dos, or another system library without an explicit design decision, documented requirement, and compatible fallback or runtime gate where appropriate.

Prefer:

- 68000-compatible code;
- conservative C89-style source;
- Amiga SDK types and APIs;
- explicit ownership and cleanup;
- small stack and heap usage;
- bounded operations;
- predictable performance on low-end hardware.

Do not introduce POSIX dependencies, modern desktop assumptions, threading requirements, or a dependency on AmigaOS 4, MorphOS, AROS, MUI, or ReAction.

The public API uses `RLV_*` types/macros and `rlv_*` functions via
`#include "rich_listview/rich_listview.h"`.

## Language and compiler compatibility

Build with the repository's current VBCC compiler mode, but write conservative C89-style source unless an existing module already requires a later construct.

Do not introduce C99-only language or library dependencies merely because the compiler accepts them.

In particular:

- place declarations before statements;
- do not declare loop variables inside `for` statements;
- avoid compound literals and designated initialisers unless already required locally;
- avoid variable-length arrays;
- do not assume modern C-library functions are available.

## Core invariants

- Keep logical rows distinct from wrapped display lines and visible viewport slots.
- Selection is owned by RichListview and refers to the control's logical data model, not a GadTools physical row.
- Rebuild layout and wrapping when data, font, columns, or viewport width changes—not during every repaint.
- Keep hit testing, rendering, scrolling, and keyboard navigation consistent with the same layout data.
- Embedded controls must report structured events containing enough information to identify the row, column, control type, and value.
- Multiple control instances must not depend on shared mutable instance state.
- Partial-construction failure and normal disposal must release every owned resource safely.

## Stack, memory, and 68000 safety

Classic Amiga programs may run with a small default stack. Treat stack use as a constrained resource.

- avoid recursion;
- avoid large automatic arrays and large temporary structures;
- do not allocate full-row, full-viewport, wrapping, or render buffers on the stack;
- do not pass large structures by value;
- pass large or shared objects by pointer, using `const` where appropriate;
- keep callback and event structures compact;
- avoid deeply nested call chains in hot paths where practical;
- move sizeable temporary storage into owned heap objects or reusable control workspace;
- do not increase per-row or per-cell memory without measuring the cost;
- avoid floating-point and 64-bit arithmetic in render, input, layout, and scrolling paths;
- do not perform unaligned `UWORD`, `WORD`, `ULONG`, or `LONG` accesses;
- do not cast arbitrary byte buffers to structures or wider integer pointers;
- use sufficiently wide intermediate values for pixel positions and content heights, then range-check before narrowing to `WORD` or `UWORD`;
- use the existing platform allocator and its matching free operation;
- do not mix `malloc/free`, `AllocMem/FreeMem`, and `AllocVec/FreeVec` for the same ownership path.

## Rendering and performance

Rendering, scrolling, layout, and input handling are performance-sensitive.

Inside render, scroll, layout, and input paths:

- avoid heap allocation;
- avoid reparsing or rewrapping unchanged text;
- avoid large stack temporaries;
- clip all drawing to the supplied viewport or cell bounds;
- preserve or deliberately set required `RastPort` state;
- redraw only the affected control or region where practical;
- preserve smart scrolling unless a change is specifically intended to replace it;
- keep tight loops simple and bounded.

The core renderer uses configured semantic pen values. Only the Amiga drawing backend or host setup code may obtain those values from `DrawInfo`. Do not introduce a `DrawInfo` dependency into platform-neutral control, layout, wrapping, or input code.

Optional logging and benchmark instrumentation must compile out of normal builds.

## Amiga event, refresh, and drawing rules

- Interpret `IntuiMessage.IAddress` only when the message class defines its meaning.
- Never dereference `IAddress` as a gadget for arbitrary IDCMP classes.
- Preserve balanced `GT_BeginRefresh()` and `GT_EndRefresh()` handling.
- Restore every installed clip region, layer hook, layer lock, and modified `RastPort` state on every return path.
- During `LAYERUPDATING`, do not replace Intuition's damage clipping with `InstallClipRegion()`. Preserve the established software-clipping refresh path.
- Preserve the established scroller event path.
- Do not replace message `Code` handling with a newer gadget-query API unless compatibility has been reviewed and tested.
- Do not retain pointers to temporary IDCMP messages or stack-owned callback data.
- Keep application event delivery separate from internal input translation.

## Source and API discipline

- Public declarations belong in public headers; implementation details belong in internal headers.
- Do not expose internal layout, cache, workspace, or backend structures without a documented API need.
- Keep the Amiga V36 drawing backend behind `RLV_DrawOps`.
- Do not make generic control code call backend-specific drawing functions directly.
- Use `static` for private functions and file-local data.
- Avoid unnecessary global or shared mutable state.
- Avoid unrelated renames, formatting churn, or architecture rewrites in behavioural fixes.
- Do not add dependencies merely to reuse a small helper.
- Keep application-specific behaviour out of the RichListview core.

## String handling

Use existing repository helpers or explicitly checked lengths for string formatting and copying.

Do not assume that `snprintf()`, `strlcpy()`, or other modern C-library functions are available on the target toolchain.

When copying or formatting strings:

- know the destination capacity;
- reserve space for the terminating null byte;
- guarantee termination;
- detect or deliberately handle truncation;
- do not treat `strncpy()` as a universal safe-copy solution;
- do not build large temporary strings on the stack.

## Build discipline

Use explicit source and object lists. Do not replace them with wildcard compilation.

Preserve the existing build variants where applicable:

```text
rich-listview-demo
rich-listview-demo-log
rich-listview-demo-bench
rich-listview-demo-nosmart
rich-listview-demo-console
```

A change is not validated merely because it compiles. Report separately whether it was:

- compiled;
- linked;
- run under emulation;
- run on physical Amiga hardware;
- visually verified;
- behaviourally verified.

Investigate unexpected executable-size, memory-use, stack-use, or performance regressions.

## Before changing behaviour

Read the relevant implementation, demo, current design notes, and repository creation report. Treat historical legacy ListView documents as background only.

For any behavioural change:

1. identify the owning control module;
2. preserve the existing public contract unless the task explicitly changes it;
3. update the demo or focused tests where useful;
4. build the affected variants;
5. document ownership, event, lifecycle, compatibility, or API changes.

Prefer the smallest correct change that keeps RichListview independent, understandable, and usable on stock low-end Amiga hardware.

## Coding style

Follow the style already established by the extracted RichListview code unless the task explicitly includes a style change.

General rules:

- use explicit, descriptive names;
- preserve compatibility with the repository's VBCC build;
- keep declarations before statements;
- avoid variable-length arrays;
- use `static` for private functions and file-local data;
- place public declarations in the relevant public header;
- avoid unnecessary global or shared mutable state;
- check allocation results and Amiga API failures;
- keep Amiga library opening and closing balanced;
- keep allocation, ownership, and cleanup paths clear;
- do not suppress compiler warnings without documenting the reason;
- do not include unrelated refactoring in behavioural fixes or bug-fix commits.

Add a short comment at the beginning of each new source module describing its purpose.

Use comments where they clarify:

- the purpose of a section or processing stage;
- non-obvious Amiga-specific behaviour;
- ownership or lifetime requirements;
- important invariants;
- performance-sensitive decisions;
- unusual workarounds or compatibility constraints.

Do not comment every line or restate code that is already clear. Prefer a brief comment above a logical section rather than repeated comments beside individual statements.

## Diagnostic logging

RichListview includes optional diagnostic logging for inspecting control behaviour on real or emulated Amiga systems.

Use the existing logger instead of adding temporary `printf()` calls.

### Availability

- Header: `rich_listview/rlv_log.h`
- Implementation: `src/rich_listview/rlv_log.c`
- Enable with `RLV_ENABLE_LOGGING`
- Logging build: `make rich-listview-demo-log`
- Output file: `PROGDIR:rlv.log`

The logger opens, seeks, writes, and closes the file for each line to improve the chance that useful information survives a crash.

When `RLV_ENABLE_LOGGING` is not defined, the logging macros and lifecycle functions compile to no-ops. Logging calls may therefore remain in shared production code without affecting normal builds.

Logging arguments must be free of side effects. Correct behaviour must never depend on an expression being evaluated inside `RLV_LOG` or `RLV_LOGF`.

The logger uses `rlv_log_*` / `RLV_LOG*` and is compile-time gated by
`RLV_ENABLE_LOGGING`. Do not rename it as part of unrelated work.

### Diagnostic executable and demo usage

Include:

```c
#include "rich_listview/rlv_log.h"
```

Call `rlv_log_init()` early during diagnostic application startup.

Write messages using:

```c
RLV_LOG("literal message");
RLV_LOGF("value=%ld index=%u", (long)value, (unsigned)index);
```

Call `rlv_log_shutdown()` on every normal and failure exit path after logging has been initialised.

The demo or embedding application may initialise the logger for diagnostic builds, but applications must not treat these functions as a stable supported RichListview API. Do not place the logger in a public umbrella header.

### Logging policy

Add diagnostic logging to relevant new or modified code where it helps explain the internal behaviour of the control.

Useful logging locations include:

- control creation and disposal;
- layout or wrapping rebuilds;
- viewport and column-size changes;
- selection changes;
- scroll requests and smart-scroll decisions;
- mouse and keyboard input translation;
- cell-control activation and event delivery;
- gadget and IDCMP processing;
- allocation or Amiga API failures;
- rejected operations and invariant violations;
- important fallback paths.

Use logging to record significant state transitions, decisions, inputs, and results. Include identifiers and values that would help reproduce or understand a problem.

Do not add noisy logging for:

- every rendered character;
- every pixel operation;
- every row during a normal full redraw;
- tight inner loops;
- routine getters or trivial successful operations.

High-frequency paths may log summaries, exceptional conditions, or phase boundaries rather than every iteration.

### Message conventions

Follow the existing message style:

- `FAIL ...` — an operation or required API call failed;
- `INVARIANT ...` — an unexpected state or invalid internal condition;
- `... begin` and `... end` — bracket a significant processing phase;
- domain prefixes such as `RESIZE`, `SMART_SCROLL`, `LAYOUT`, `INPUT`, `CELL_CONTROL`, or `IDCMP`.

Examples:

```c
RLV_LOGF("LAYOUT begin rows=%lu width=%u",
         (unsigned long)row_count,
         (unsigned)viewport_width);

RLV_LOGF("SMART_SCROLL delta=%ld exposed=%u",
         (long)delta,
         (unsigned)exposed_rows);

RLV_LOGF("CELL_CONTROL row=%lu column=%u type=%u value=%ld",
         (unsigned long)row,
         (unsigned)column,
         (unsigned)control_type,
         (long)value);

RLV_LOG("FAIL layout allocation");
RLV_LOG("INVARIANT selected row outside logical row count");
```

Keep the logging system internal and diagnostic. It is not part of the stable public RichListview API.

## Decision principle

Prefer the smallest correct AmigaOS implementation that:

- follows the repository's local patterns and relevant Amiga AutoDocs;
- preserves documented RichListview behaviour;
- makes ownership and lifetime explicit;
- allows diagnostic or optional functionality to compile out when unused;
- remains understandable to a developer following the examples;
- keeps code size, stack use, memory use, and performance measurable;
- avoids unnecessary dependencies and abstraction;
- works reliably on classic low-end Amiga hardware.

When several designs are correct, prefer the one with the clearest lifecycle, the lowest runtime cost, the smallest stack and memory requirement, and the smallest impact on existing code.

RichListview exists to provide a reusable and dependable full custom ListView control without requiring each Amiga application to reimplement rendering, scrolling, selection, input handling, wrapped rows, and embedded cell controls.
