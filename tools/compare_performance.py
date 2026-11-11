"""Compare scheduler and physical-memory metrics across two ToyOS runs."""

import argparse

import matplotlib.pyplot as plt

from performance_data import load_records, performance_series

def print_summary(label, rows):
    rates = [r["switches_per_second"] for r in rows[1:]]
    memory = [r["memory_percent"] for r in rows]

    print(f"\n{label}")
    print(f"  Samples: {len(rows)}")
    print(f"  Average switches/second: {sum(rates) / len(rates):.2f}" if rates else
          "  Average switches/second: N/A")
    print(f"  Peak switches/second: {max(rates):.2f}" if rates else
          "  Peak switches/second: N/A")
    print(f"  Average memory usage: {sum(memory) / len(memory):.2f}%")

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("baseline", help="serial log captured from the baseline kernel")
    parser.add_argument("updated", help="serial log captured from the updated kernel")
    parser.add_argument("-o", "--output", default="performance-comparison.png",
                        help="PNG output path")
    args = parser.parse_args()

    runs = [("Baseline", performance_series(load_records(args.baseline))),
            ("Updated", performance_series(load_records(args.updated)))]
    for label, rows in runs:
        print_summary(label, rows)
    fig, axes = plt.subplots(2, 1, figsize=(10, 7), sharex=False)
    colors = ("#64748b", "#2563eb")
    for (label, rows), color in zip(runs, colors):
        times = [r["seconds"] for r in rows]
        axes[0].plot(times, [r["switches_per_second"] for r in rows],
                     label=label, color=color)
        axes[1].plot(times, [r["memory_percent"] for r in rows],
                     label=label, color=color)
    axes[0].set_ylabel("switches / second")
    axes[1].set_ylabel("physical pages used (%)")
    axes[1].set_xlabel("uptime (seconds)")
    for axis in axes:
        axis.grid(True, alpha=0.25)
        axis.legend()
    fig.suptitle("ToyOS baseline vs updated kernel")
    fig.tight_layout()
    fig.savefig(args.output, dpi=160)
    print(f"Saved {args.output}")


if __name__ == "__main__":
    main()
