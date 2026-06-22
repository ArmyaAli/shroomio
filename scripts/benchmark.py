#!/usr/bin/env python3
"""Run repeatable local shroomio server benchmark scenarios."""

from __future__ import annotations

import argparse
import csv
import os
import resource
import subprocess
import sys
import time
from pathlib import Path


def parse_counts(value: str) -> list[int]:
    counts: list[int] = []
    for part in value.split(","):
        part = part.strip()
        if not part:
            continue
        counts.append(int(part))
    if not counts:
        raise argparse.ArgumentTypeError("at least one player count is required")
    return counts


def read_single_row(path: Path) -> dict[str, str]:
    with path.open(newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))
    if len(rows) != 1:
        raise RuntimeError(f"expected one benchmark row in {path}, got {len(rows)}")
    return rows[0]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--server", default="dist/server/linux/shroomio-server", help="server binary to execute"
    )
    parser.add_argument("--out-dir", default="build/benchmarks", help="artifact output directory")
    parser.add_argument("--ticks", type=int, default=600, help="ticks per scenario")
    parser.add_argument(
        "--players",
        type=parse_counts,
        default=parse_counts("1,8,32"),
        help="comma-separated player/bot counts",
    )
    args = parser.parse_args()

    server = Path(args.server)
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    if not server.exists():
        print(f"server binary not found: {server}; run make server first", file=sys.stderr)
        return 1

    summary_path = out_dir / "summary.csv"
    rows: list[dict[str, str]] = []
    for players in args.players:
        scenario_path = out_dir / f"server_bots_{players}.csv"
        command = [
            str(server),
            "--benchmark",
            "--benchmark-ticks",
            str(args.ticks),
            "--benchmark-bots",
            str(players),
        ]
        started = time.perf_counter()
        result = subprocess.run(command, check=True, text=True, capture_output=True)
        elapsed_ms = (time.perf_counter() - started) * 1000.0
        scenario_path.write_text(result.stdout, encoding="utf-8")
        row = read_single_row(scenario_path)
        row["harness_elapsed_ms"] = f"{elapsed_ms:.3f}"
        row["max_rss_kb"] = str(resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss)
        rows.append(row)

    fieldnames = list(rows[0].keys())
    with summary_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)

    print(f"benchmark summary: {summary_path}")
    for row in rows:
        print(
            "players={players} ticks={ticks} avg_tick_ms={avg_tick_ms} "
            "worst_tick_ms={worst_tick_ms} estimated_bytes={estimated_bytes}".format(**row)
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
