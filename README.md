# C++ Runtime Performance Benchmark Suite

A small C++17 benchmark project for comparing runtime behavior of algorithms and data structures under increasing workloads. It measures execution time, approximate resident memory usage on Linux, and throughput, then exports both CSV and JSON results.

This project is designed to demonstrate internship-relevant evidence for performance testing, technical investigation, and data-driven engineering.

## What it benchmarks

- `std::sort` on random integer vectors
- Linear search over vectors
- Binary search over sorted vectors
- `std::unordered_set` membership lookup
- `std::priority_queue` push and drain workload

## Metrics captured

- Average runtime in milliseconds
- Minimum runtime
- Maximum runtime
- P95 runtime
- Approximate resident memory in MB on Linux
- Operations per second
- CSV and JSON output for analysis or dashboards

## Build

Using Make:

```bash
make
```

Or using CMake:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Run

```bash
./runtime_benchmark --sizes 10000,50000,100000,250000 --iterations 5 --out-dir results
# If you built with CMake, use: ./build/runtime_benchmark ...
```

Outputs:

```text
results/benchmark_results.csv
results/benchmark_results.json
```

## Analyze results

```bash
python3 scripts/analyze_results.py results/benchmark_results.csv
```

## Example resume bullet after you run it

Do not claim numbers until you run the project locally and verify your own output.

> Built a C++17 runtime benchmark suite comparing sorting, vector search, binary search, hash lookup, and priority queue workloads, exporting timing, memory, P95 latency, and throughput metrics to CSV/JSON for performance analysis.

## Possible extensions

- Add CPU cache-friendly vs cache-unfriendly workloads.
- Add matrix multiplication or spatial partitioning workloads.
- Add command-line export directly into Prometheus text format.
- Feed the CSV/JSON output into the Grafana performance dashboard project.
