#!/usr/bin/env python3
import argparse
import csv
import math
import os
from pathlib import Path


def to_float(value: str) -> float:
    try:
        return float(value)
    except ValueError:
        return math.nan


def main() -> int:
    parser = argparse.ArgumentParser(description="Plot testbench_torque_sweep CSV.")
    parser.add_argument("csv", nargs="?", default="torque_sweep.csv")
    parser.add_argument("--out", default="torque_sweep.png")
    args = parser.parse_args()

    x = []
    motor = []
    dyn200 = []
    with open(args.csv, newline="") as f:
        for row in csv.DictReader(f):
            x.append(to_float(row["command_torque_Nm"]))
            motor.append(to_float(row["motor_torque_mean_Nm"]))
            dyn200.append(to_float(row["dyn200_torque_mean_Nm"]))

    if not x:
        raise SystemExit(f"No rows in {args.csv}")

    cache_dir = Path(__file__).resolve().parents[1] / ".venv" / "matplotlib-cache"
    cache_dir.mkdir(parents=True, exist_ok=True)
    os.environ.setdefault("MPLCONFIGDIR", str(cache_dir))

    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, ax = plt.subplots(figsize=(10, 6))
    ax_dyn = ax.twinx()

    command_line, = ax.plot(x, x, "k--", linewidth=1.5, label="commanded torque")
    motor_line, = ax.plot(x, motor, marker="o", label="motor telemetry torque")
    dyn_line, = ax_dyn.plot(x, dyn200, color="tab:orange", marker="s", label="DYN-200 torque")

    ax.set_xlabel("Commanded torque, Nm")
    ax.set_ylabel("Motor/command torque, Nm")
    ax_dyn.set_ylabel("DYN-200 torque, Nm")
    ax.set_title("FOC torque sweep response")
    ax.grid(True, alpha=0.3)
    lines = [command_line, motor_line, dyn_line]
    ax.legend(lines, [line.get_label() for line in lines], loc="best")
    fig.tight_layout()
    fig.savefig(args.out, dpi=160)
    print(f"OK: wrote {Path(args.out)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
