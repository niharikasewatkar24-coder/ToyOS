# Performance instrumentation comparison

| Area | Supplied baseline | Updated project |
|---|---|---|
| Kernel log format | Human-readable `[LEVEL] message` text | One JSON object per serial line, with tick and uptime on normal log records |
| Scheduler | Logs `TASK SWITCH` without task IDs or timing | Counts context switches and reports the cumulative count in periodic snapshots |
| Runtime sampling | No periodic scheduler or memory samples | Emits periodic uptime, cumulative switches, running task, and PMM page usage |
| Analysis | Manual inspection of console output | Python chart for one run and a side-by-side chart for two updated runs |

The original serial output contains no clock or memory counters, so it cannot support a valid numerical before/after performance comparison after the fact. The comparison script therefore compares two instrumented captures (for example, two scheduler or memory-management revisions). The supplied baseline remains available as the starting point in this ZIP; no performance improvement is claimed by adding observability alone.

All rates are based on the kernel's configured PIT frequency of 100 Hz. Physical-memory measurements are the PMM's current page counters, not a whole-machine RAM measurement.
