# CLV Benchmark Implementation Report

## Summary

This change adds a private compile-time benchmark facility for the custom
ListView codebase and wires a deterministic benchmark driver into
`examples/custom_control_demo/main.c`.

The benchmark code is enabled only in benchmark-specific builds via
`CLV_ENABLE_BENCHMARKS`. Normal builds do not define that macro and do not link
the benchmark implementation object.

## Files Changed

- `Makefile`
- `examples/custom_control_demo/main.c`
- `examples/custom_control_demo/README.md`
- `src/custom_listview/clv_bench_internal.h`
- `src/custom_listview/clv_bench.c`
- `src/custom_listview/clv_platform.c`
- `src/custom_listview/clv_columns.c`
- `src/custom_listview/clv_prepared_display_map.c`
- `src/custom_listview/clv_selection.c`
- `src/custom_listview/clv_char_wrap.c`
- `src/custom_listview/clv_renderer_core.c`
- `src/custom_listview/clv_renderer_internal.h`
- `src/custom_listview_control/clv_control.c`
- `src/custom_listview_control/clv_control_layout.c`
- `src/custom_listview_control/clv_control_wrap.c`
- `src/custom_listview_control/clv_control_input.c`
- `src/custom_listview_control/clv_control_render.c`
- `src/custom_listview_control/clv_control_internal.h`
- `src/custom_listview_control/backends/clv_backend_amiga_v36.c`
- `docs/samples/clv_benchmark_sample.txt`

## Timer Source

Primary timer source: `ReadEClock()`.

Why this source was chosen:

- the local AutoDocs describe it as a low-overhead V36 timer intended for short
  intervals;
- it returns both the raw counter value and the timer frequency;
- it works on the target Amiga platform instead of relying on host clocks.

The benchmark stores raw tick totals and the timer frequency in the report.
Converted times are emitted as integer milliseconds and microseconds in the
final formatter.

## Timing Resolution And Limitations

- Resolution depends on the platform E-Clock frequency returned at runtime.
- Timer-read overhead is measured during benchmark initialization and stored in
  the report as `timer_overhead_ticks`.
- The formatter uses integer arithmetic only.
- This environment could cross-build the binaries, but it could not execute the
  Amiga benchmark binary, so runtime values have not been validated here on a
  real Amiga or WinUAE.
- The benchmark falls back to count-only mode if `ReadEClock()` cannot provide a
  non-zero frequency.

## Instrumented Regions

Implemented timing regions include:

- lifecycle: `TOTAL_CREATE`, `TOTAL_PREPARE`, `TOTAL_INITIAL_RENDER`,
  `TOTAL_NAVIGATION_RUN`, `TOTAL_SHUTDOWN`
- layout and prepare: `COLUMN_LAYOUT`, `ROW_HEIGHT_CALC`,
  `PREPARED_LIST_BUILD`, `RENDERER_CREATE`, `PIXEL_WRAP`
- event and navigation: `KEY_EVENT_TOTAL`, `NAV_SELECTION_UPDATE`,
  `NAV_MAKE_VISIBLE`, `SCROLLER_UPDATE`
- rendering: `FULL_REDRAW`, `PARTIAL_REDRAW`, `VIEWPORT_SCROLL`,
  `DRAW_HOOK_TOTAL`, `DRAW_ROW_TOTAL`

## Counters Added

Implemented counters include:

- logical/physical row counts
- wrapped continuation rows
- processed columns and prepared cells
- drawn rows and cells
- draw-hook invocations
- full redraws, partial redraws, selection-only redraws
- viewport scrolls
- scroll-copy attempts, successes, and fallbacks
- text-fit and text-length calls
- wrap decisions
- display-map growth and entry counts
- allocations, reallocations, frees, and requested bytes
- horizontal/vertical line draws
- background/highlight fills
- scrollbar updates
- keyboard events, accepted/rejected moves, skipped non-selectable rows,
  boundary hits, and prepare rebuilds during navigation

## Scripted Navigation Tests

The benchmark driver lives in `examples/custom_control_demo/main.c` and reuses
the existing `demo_apply_input()` path instead of duplicating navigation logic.

Implemented benchmark-only tests:

- `selection_only_down`
- `selection_only_up`
- `steady_scroll_down`
- `steady_scroll_up`
- `large_movement`
- `end_to_end_traversal`
- `redraw_baseline`
- `prepare_baseline`

The benchmark suite writes one report after all tests finish:

- `PROGDIR:clv_benchmark.txt`

### Per-test 68k layout fix

A real A500+ run exposed a two-byte field-offset failure limited to the
per-test records: labels lost two leading characters, `ULONG` values were
shifted left 16 bits, and elapsed times saturated.

The benchmark now owns test labels and requested-step counts in
`clv_bench.c`. The demo begins a test with only its `CLV_BenchTestId`.
Per-test runtime records are fixed named state members selected through one
`UWORD` switch rather than an indexed mixed-width record array. This avoids
both the cross-translation-unit spec layout and the affected vbcc/68000 array
addressing path. Step notifications accumulate so multi-part tests report the
total completed work.

`tools/validate_benchmark_report.ps1` validates the report schema and the
per-test corruption signatures. The source has been cross-built; fresh
Amiga/WinUAE runtime validation remains required before recording measured
replacement values.

## Build Commands

Benchmark build:

```text
make custom-control-demo-bench
```

Run benchmark:

```text
custom-control-demo-bench BENCH
```

Normal build:

```text
make custom-control-demo
```

## Release-Build Exclusion

The build integrates through a separate benchmark object tree:

- `build/custom_listview_control_bench/...`

Benchmark-only objects:

- `build/custom_listview_control_bench/clv_bench.o`
- benchmark-compiled copies of `clv_platform.o` and the custom-control objects

Normal build objects remain in:

- `build/custom_listview_control/...`
- `build/clv_platform.o`

Compile-time exclusion proof from this session:

- `make custom-control-demo-bench` linked successfully.
- `make custom-control-demo` linked successfully.
- Benchmark strings such as `PROGDIR:clv_benchmark.txt`,
  `TOTAL_NAVIGATION_RUN`, and `count_only` were found in
  `bin/custom-control-demo-bench`.
- Those benchmark strings were not found in `bin/custom-control-demo`.

Observed binary sizes from this session:

- `bin/custom-control-demo-bench`: `48156` bytes
- `bin/custom-control-demo`: `33500` bytes

## Code Paths That Are Not Reliably Measured Yet

- The current benchmark driver exercises the experimental custom control path.
- Reusable CLV renderer/prepare modules were instrumented privately, but this
  session did not add a second scripted benchmark harness for the older
  GadTools `LISTVIEW_KIND` examples.
- No runtime benchmark file was generated in this environment because the Amiga
  binary was not executed here.

## Surprising Repeated Work Found During Validation

Inspection while placing counters confirmed several existing repeated-work areas:

- custom control repaint still performs text measurement during drawing;
  wrapping is cached, but fitting/truncation remains paint-time work
- `clv_prepared_display_map.c` can grow geometrically during prepared-list build
- wrapped/fragmented prepare paths still allocate many small strings

These are not optimization changes in this task; they are now instrumented so
the benchmark report can quantify them.

## Recommended First Optimization Targets

No ranked optimization list based on measured runtime data is included yet,
because the benchmark binary was not executed on Amiga hardware or WinUAE in
this environment.

The intended next step is:

1. Run `custom-control-demo-bench BENCH` on the target runtime.
2. Inspect `PROGDIR:clv_benchmark.txt`.
3. Rank optimization candidates from measured totals and counters, especially:
   paint-time text measurement, scroll-copy fallback rate, and repeated prepare
   allocations.
