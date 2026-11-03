"""Plot scheduler and physical-memory metrics from a ToyOS serial log."""

import argparse

import matplotlib.pyplot as plt

from performance_data import load_records, performance_series


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", help="JSONL serial capture from ToyOS")
    parser.add_argument("-o", "--output", default="performance.png",
                        help="PNG output path (default: performance.png)")
    args = parser.parse_args()

    rows = performance_series(load_records(args.log))
    times = [row["seconds"] for row in rows]
    fig, axes = plt.subplots(3, 1, figsize=(10, 9), sharex=True)
    axes[0].plot(times, [r["switches_per_second"] for r in rows], color="#2563eb")
    axes[0].set_ylabel("switches / second")
    axes[0].set_title("ToyOS performance")
    axes[1].plot(times, [r["memory_percent"] for r in rows], color="#16a34a")
    axes[1].set_ylabel("physical pages used (%)")
    axes[1].set_ylim(bottom=0)
    axes[2].step(times, [r["switches"] for r in rows], where="post", color="#9333ea")
    axes[2].set_ylabel("total task switches")
    axes[2].set_xlabel("uptime (seconds)")
    for axis in axes:
        axis.grid(True, alpha=0.25)
    fig.tight_layout()
    fig.savefig(args.output, dpi=160)
    print(f"Saved {args.output} ({len(rows)} samples, {rows[-1]['seconds']:.1f}s)")


if __name__ == "__main__":
    main()
