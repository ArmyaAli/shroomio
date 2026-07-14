#!/usr/bin/env python3
"""Run real ENet loopback networking benchmark scenarios."""

from __future__ import annotations

import argparse
import csv
import subprocess
from pathlib import Path


def parse_counts(value: str) -> list[int]:
    counts = [int(part.strip()) for part in value.split(",") if part.strip()]
    if not counts or any(count < 1 or count > 256 for count in counts):
        raise argparse.ArgumentTypeError("client counts must be between 1 and 256")
    return counts


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", default="build/benchmarks/network-benchmark")
    parser.add_argument("--out-dir", default="build/benchmarks/network")
    parser.add_argument("--clients", type=parse_counts, default=parse_counts("1,64,256"))
    parser.add_argument("--duration-ms", type=int, default=1500)
    parser.add_argument("--split-pieces", type=int, default=1)
    parser.add_argument("--min-input-hz", type=float, default=20.0)
    parser.add_argument("--min-snapshot-hz", type=float, default=10.0)
    parser.add_argument("--max-drop-percent", type=float, default=1.0)
    parser.add_argument("--max-deadline-failures", type=int, default=5)
    args = parser.parse_args()

    binary = Path(args.binary)
    output = Path(args.out_dir)
    output.mkdir(parents=True, exist_ok=True)
    rows: list[dict[str, str]] = []
    for offset, clients in enumerate(args.clients):
        participants = clients
        command = [
            str(binary),
            "--clients",
            str(clients),
            "--participants",
            str(participants),
            "--split-pieces",
            str(args.split_pieces),
            "--duration-ms",
            str(args.duration_ms),
            "--port",
            str(39777 + offset),
            "--min-input-hz",
            str(args.min_input_hz),
            "--min-snapshot-hz",
            str(args.min_snapshot_hz),
            "--max-drop-percent",
            str(args.max_drop_percent),
            "--max-deadline-failures",
            str(args.max_deadline_failures),
        ]
        result = subprocess.run(command, text=True, capture_output=True, check=False)
        artifact = output / f"enet_loopback_{clients}.csv"
        artifact.write_text(result.stdout, encoding="utf-8")
        if result.returncode != 0:
            print(result.stderr, end="")
            return result.returncode
        with artifact.open(newline="", encoding="utf-8") as handle:
            scenario_rows = list(csv.DictReader(handle))
        if len(scenario_rows) != 1:
            raise RuntimeError(f"expected one row from {binary}, got {len(scenario_rows)}")
        rows.append(scenario_rows[0])

    summary = output / "summary.csv"
    with summary.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    print(f"network benchmark summary: {summary}")
    for row in rows:
        print(
            "clients={clients} input/s={input_messages_per_sec} "
            "snapshot/s={snapshot_messages_per_sec} "
            "snapshot_reduction={snapshot_byte_reduction_percent}% "
            "deadlines={tick_deadline_failures}".format(
                **row
            )
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
