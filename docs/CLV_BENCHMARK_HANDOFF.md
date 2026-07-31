# CLV Benchmark Handoff

## Purpose

This repository now includes a private, benchmark-only instrumentation path for
the custom ListView control demo. The benchmark support is intended to measure
real runtime behavior on classic Amiga hardware, especially low-end 68k
systems such as a 7 MHz 68000, without changing the normal library ABI or
adding overhead to non-benchmark builds.

The benchmark code is compiled only into the dedicated benchmark target:

- `custom-control-demo-bench`

Normal builds should not carry benchmark logic, timing calls, strings, or
report output.

## Aim

The benchmark system was added to answer a practical question:

- where does time go during custom ListView preparation, selection movement,
  redraw, and smart-scroll activity on real Amiga hardware?

The benchmark is designed to:

- use an Amiga-appropriate timer source;
- accumulate timings and counters in memory;
- emit one plain-text report at the end of a run;
- avoid hot-path console/file logging during the measurement itself;
- provide deterministic scripted input so runs are repeatable;
- make it possible to compare smart scrolling against redraw-heavy paths.

## What It Measures

The benchmark currently measures a mix of coarse timings and event counters.

Examples include:

- total benchmark run time;
- key/input handling time;
- selection update time;
- make-visible time;
- scroller update time;
- viewport scroll time;
- partial redraw time;
- full redraw time;
- draw-row total time;
- timer-read overhead;
- counts of redraw types;
- counts of viewport scrolls;
- counts of scroll-copy attempts/successes/fallbacks;
- counts of rows/cells drawn and scrollbar updates.

The benchmark output is written to:

- `PROGDIR:clv_benchmark.txt`
- `PROGDIR:clv_benchmark_debug.log`

The demo now deletes both files at the start of each benchmark run so every run
starts with clean artifacts.

## Current Execution Model

Benchmark mode is launched with:

```text
custom-control-demo-bench BENCH
```

Behavior:

1. The demo opens normally and does not auto-run the benchmark immediately.
2. The user can move overlapping windows out of the way first.
3. The bottom button becomes `Start Benchmark` in benchmark mode.
4. Clicking that button runs the deterministic benchmark suite.
5. The report is written once at the end.

This replaced the earlier extra-button approach because the extra benchmark
button caused a Workbench 2.x / A500+ window drawing lockup. Reusing a single
bottom button avoided the multi-gadget issue and restored stable behavior on
older systems.

## Timer Source

The benchmark uses:

- `ReadEClock(timer.device)`

Important implementation detail:

- explicit `timer.device` setup is required on the Amiga target used here

The working path opens `timer.device`, records the frequency, uses
`ReadEClock()`, and closes the device cleanly on shutdown.

This resolved an earlier startup failure where the benchmark build exited with
return code `20`.

## What Works

The following is believed to be working correctly now.

### 1. Benchmark build separation

- Normal builds do not require benchmark support.
- Benchmark support is linked only into `custom-control-demo-bench`.

### 2. Benchmark startup and shutdown

- `timer.device` initialization succeeds.
- Clean debug logs show a single init/open/configure/write/shutdown sequence.

### 3. Deterministic manual benchmark start

- The benchmark no longer runs too early while windows are still overlapping.
- The user can visually prepare the screen before starting the run.

### 4. Smart-scroll path is being exercised

Fresh real-hardware reports currently show:

- `smart_scroll_enabled=1`
- `scroll_copy_attempts=200`
- `scroll_copy_successes=200`
- `scroll_copy_fallbacks=0`

This strongly suggests the smart-scroll copy path is active and succeeding on
the tested A500+ run.

### 5. Aggregate benchmark summary is usable

The following parts of `clv_benchmark.txt` appear credible:

- `[HEADER]`
- `[TIMING]`
- `[COUNTERS]`

The environment data reported for the A500+ run is also plausible, for example:

- `screen_depth=2`
- `font_name=topaz.font`
- `font_height=8`
- `visible_physical_rows=18`
- `logical_rows=80`

## What Was Fixed Along the Way

Already-resolved issues include:

- benchmark build returning `20` on startup;
- missing explicit `timer.device` setup;
- benchmark output/debug logs accumulating old content across runs;
- benchmark starting too early before the listview window was visually stable;
- Workbench 2.x lockup caused by adding a second bottom-row benchmark button;
- hot-path debug logging contaminating timing runs.

## Per-Test Reporting Fix

The per-test corruption fix is now implemented in source. It still requires a
fresh Amiga or WinUAE runtime report before the per-test timings can be declared
runtime-validated.

The observed corruption was a consistent two-byte record-field slip:

- labels were read two bytes late;
- `ULONG` fields were read two bytes late on big-endian 68k, turning `6` into
  `0x00060000` (`393216`);
- shifted E-Clock fields caused converted times to saturate.

The fix removes both layout-sensitive paths:

- test metadata now belongs to `clv_bench.c` and is selected by test ID;
- test records are fixed named members selected by a `UWORD` switch, not an
  indexed array of mixed-width records;
- the demo calls `clv_bench_test_begin(test_id)` and no longer passes a
  cross-translation-unit `CLV_BenchTestSpec`;
- completed-step notifications accumulate, so the two halves of
  `large_movement` report 10 completed steps.

## Known Broken Output

The per-test sections in `clv_benchmark.txt` are still not trustworthy.

Examples of bad output:

- test names lose their first character:
  - `selection_only_down` becomes `lection_only_down`
  - `steady_scroll_down` becomes `eady_scroll_down`
- step counts are inflated by `65536`
  - `6` becomes `393216`
  - `50` becomes `3276800`
- many `elapsed_us` values saturate at:
  - `4294967295`

Because of this, the individual `[TEST ...]` sections should currently be
treated as corrupted.

## Trust Status

Until a fresh target run passes validation:

- trust the aggregate header/timing/counter sections;
- treat the checked-in old `bin/clv_benchmark.txt` per-test blocks as a
  negative fixture, not fixed output;
- trust new individual per-test blocks only after running
  `tools/validate_benchmark_report.ps1` on the new report.

## Validation

Host-side format check:

```text
powershell -NoProfile -ExecutionPolicy Bypass -File tools/validate_benchmark_report.ps1 -ReportPath docs/samples/clv_benchmark_sample.txt
```

After copying back a fresh target report, replace the path with that report.
The validator requires all eight exact names, expected requested steps, sane
completed-step bounds, non-saturated elapsed times in timed mode, and per-test
counter deltas no greater than their aggregate counters.

Target acceptance still requires:

1. clean-build `custom-control-demo-bench`;
2. run `custom-control-demo-bench BENCH` and click `Start Benchmark`;
3. validate the resulting `PROGDIR:clv_benchmark.txt`;
4. confirm the aggregate smart-scroll evidence remains 200 attempts,
   200 successes, and 0 fallbacks for the current workload.

## Practical Notes for the Next Agent

- `bin/clv_control.log` is not expected in the benchmark build unless a logging
  build is being used.
- `bin/clv_benchmark_debug.log` is now intentionally a small lifecycle log, not
  a hot-path trace.
- The current benchmark run path on A500+ is stable enough to use for further
  investigation.
- Do not regress the Workbench 2.x fix by reintroducing the extra benchmark
  gadget/button arrangement without strong evidence it is safe.

## Current Bottom Line

The benchmark system is now good enough to:

- launch on real hardware;
- run after manual user start;
- capture aggregate timing/counter evidence;
- confirm smart-scroll copy behavior is active.

The source fix and host report validator are present. A fresh A500+/WinUAE run
is the remaining step before declaring the per-test breakdown runtime-tested.
