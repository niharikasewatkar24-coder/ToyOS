"""Read structured serial logs emitted by ToyOS."""

import json
from pathlib import Path


def load_records(path):
    records = []
    with Path(path).open(encoding="utf-8", errors="replace") as stream:
        for line_number, line in enumerate(stream, 1):
            line = line.strip()
            if not line:
                continue
            try:
                record = json.loads(line)
            except json.JSONDecodeError:
                # QEMU or an older kernel may have emitted non-JSON console text.
                continue
            if isinstance(record, dict) and "event" in record:
                record["_line"] = line_number
                records.append(record)
    if not records:
        raise ValueError(f"No structured ToyOS records found in {path}")
    return records


def performance_series(records):
    samples = [r for r in records if r.get("event") == "performance.sample"]
    if not samples:
        raise ValueError("No performance.sample records found; capture a run of the updated kernel")

    rows = []
    previous = None
    for sample in samples:
        t = float(sample.get("uptime_ms", sample.get("tick", 0) * 10)) / 1000.0
        switches = int(sample.get("task_switches_total", 0))
        used = int(sample.get("memory_used_pages", 0))
        total = int(sample.get("memory_total_pages", 0))
        rate = 0.0
        if previous and t > previous[0]:
            rate = max(0, switches - previous[1]) / (t - previous[0])
        rows.append({"seconds": t, "switches": switches,
                     "switches_per_second": rate, "used_pages": used,
                     "memory_percent": (100.0 * used / total) if total else 0.0,
                     "running_task": sample.get("running_task")})
        previous = (t, switches)
    return rows
