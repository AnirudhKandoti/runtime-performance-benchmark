#!/usr/bin/env python3
"""Create a compact summary from benchmark_results.csv.

Usage:
    python scripts/analyze_results.py results/benchmark_results.csv
"""
from __future__ import annotations

import csv
import sys
from collections import defaultdict
from pathlib import Path


def load_rows(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def main() -> int:
    if len(sys.argv) != 2:
        print("Usage: python scripts/analyze_results.py results/benchmark_results.csv")
        return 1

    path = Path(sys.argv[1])
    rows = load_rows(path)
    if not rows:
        print("No benchmark rows found.")
        return 1

    by_size: dict[int, list[dict[str, str]]] = defaultdict(list)
    for row in rows:
        by_size[int(row["input_size"])].append(row)

    print("Runtime benchmark summary")
    print("=" * 72)
    for size in sorted(by_size):
        fastest = min(by_size[size], key=lambda r: float(r["average_ms"]))
        slowest = max(by_size[size], key=lambda r: float(r["average_ms"]))
        print(f"Input size: {size:,}")
        print(f"  Fastest: {fastest['algorithm']} ({float(fastest['average_ms']):.3f} ms avg)")
        print(f"  Slowest: {slowest['algorithm']} ({float(slowest['average_ms']):.3f} ms avg)")
        print()

    print("Tip: use benchmark_results.json or CSV in dashboards, reports, or resume screenshots.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
