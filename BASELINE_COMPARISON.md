# Performance instrumentation comparison

| Area | Supplied baseline | Updated project |
|---|---|---|
| Kernel log format | Human-readable `[LEVEL] message` text | One JSON object per serial line, with tick and uptime on normal log records |
| Scheduler | Logs `TASK SWITCH` without task IDs or timing | Counts context switches and reports the cumulative count in periodic snapshots |
| Runtime sampling | No periodic scheduler or memory samples | Emits periodic uptime, cumulative switches, running task, and PMM page usage |
| Analysis | Manual inspection of console output | Python chart for one run and a side-by-side chart for two updated runs |

The original serial output contains no clock or memory counters, so it cannot support a valid numerical before/after performance comparison after the fact. The comparison script therefore compares two instrumented captures (for example, two scheduler or memory-management revisions). The supplied baseline remains available as the starting point in this ZIP; no performance improvement is claimed by adding observability alone.

All rates are based on the kernel's configured PIT frequency of 100 Hz. Physical-memory measurements are the PMM's current page counters, not a whole-machine RAM measurement.

## Day 1: Scheduler sampling interval experiment

The scheduler performance snapshot interval was reduced from 100 PIT ticks
to 10 PIT ticks. At the configured 100 Hz PIT frequency, this changes the
sampling interval from approximately 1 second to 100 milliseconds.

### Test environment

- Emulator: QEMU (`qemu-system-i386`)
- Memory: 128 MiB
- Display: GTK
- QEMU timeout: 15 seconds
- Kernel: ToyOS booted from the GRUB ISO

### Captured results

| Metric | Baseline (100 ticks) | Updated (10 ticks) |
|---|---:|---:|
| Performance samples | 3 | 27 |
| Average switches/second | 25.00 | 25.77 |
| Peak switches/second | 25.00 | 40.00 |
| Average physical memory usage | 50.15% | 50.15% |

### Generated artifacts

- Baseline log: `build/performance-100tick-baseline.jsonl`
- Updated log: `build/performance-10tick-gtk-test.jsonl`
- Updated run chart: `build/day1-performance-10tick.png`
- Comparison chart: `build/day1-baseline-vs-10tick.png`

### Interpretation and limitations

The 10-tick interval provides more frequent performance observations:
27 samples compared with 3 in the captured baseline.

The average scheduler switch rates are similar, and memory usage is
unchanged. The different sampling intervals and small number of baseline
samples mean these results do not establish a scheduler performance
improvement. This experiment validates improved monitoring resolution,
not faster context switching.

A controlled benchmark with identical sampling intervals, workloads,
and repeated runs is required for a reliable performance comparison.
