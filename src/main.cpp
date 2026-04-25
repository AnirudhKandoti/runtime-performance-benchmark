#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <queue>
#include <random>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

static volatile std::uint64_t g_sink = 0;

struct Config {
    std::vector<std::size_t> sizes{10000, 50000, 100000, 250000};
    int iterations = 5;
    std::uint32_t seed = 42;
    std::string out_dir = "results";
};

struct BenchmarkResult {
    std::string algorithm;
    std::string workload;
    std::size_t input_size{};
    int iterations{};
    double average_ms{};
    double min_ms{};
    double max_ms{};
    double p95_ms{};
    double memory_mb{};
    double ops_per_second{};
};

static std::vector<std::string> split(const std::string& input, char delimiter) {
    std::vector<std::string> parts;
    std::stringstream ss(input);
    std::string item;
    while (std::getline(ss, item, delimiter)) {
        if (!item.empty()) parts.push_back(item);
    }
    return parts;
}

static std::string csv_escape(const std::string& value) {
    if (value.find_first_of(",\"") == std::string::npos) return value;
    std::string escaped = "\"";
    for (char c : value) {
        if (c == '"') escaped += "\"";
        escaped += c;
    }
    escaped += "\"";
    return escaped;
}

static std::string json_escape(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (char c : value) {
        switch (c) {
            case '\\': escaped += "\\\\"; break;
            case '"': escaped += "\\\""; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default: escaped += c;
        }
    }
    return escaped;
}

static double current_rss_mb() {
#ifdef __linux__
    std::ifstream status("/proc/self/status");
    std::string key;
    while (status >> key) {
        if (key == "VmRSS:") {
            double kb = 0.0;
            status >> kb;
            return kb / 1024.0;
        }
        std::string rest_of_line;
        std::getline(status, rest_of_line);
    }
#endif
    return 0.0; // Unsupported platform fallback. Timing results are still valid.
}

static Config parse_args(int argc, char** argv) {
    Config config;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto need_value = [&](const std::string& name) -> std::string {
            if (i + 1 >= argc) {
                throw std::runtime_error("Missing value for " + name);
            }
            return argv[++i];
        };

        if (arg == "--sizes") {
            config.sizes.clear();
            for (const auto& item : split(need_value(arg), ',')) {
                config.sizes.push_back(static_cast<std::size_t>(std::stoull(item)));
            }
        } else if (arg == "--iterations") {
            config.iterations = std::stoi(need_value(arg));
        } else if (arg == "--seed") {
            config.seed = static_cast<std::uint32_t>(std::stoul(need_value(arg)));
        } else if (arg == "--out-dir") {
            config.out_dir = need_value(arg);
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Runtime Performance Benchmark Suite\n"
                      << "Usage: ./runtime_benchmark [--sizes 10000,50000] [--iterations 5] [--seed 42] [--out-dir results]\n";
            std::exit(0);
        } else {
            throw std::runtime_error("Unknown argument: " + arg);
        }
    }

    if (config.sizes.empty()) throw std::runtime_error("At least one input size is required.");
    if (config.iterations <= 0) throw std::runtime_error("Iterations must be positive.");
    return config;
}

static std::vector<int> make_random_vector(std::size_t n, std::uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> dist(0, static_cast<int>(n * 4 + 100));
    std::vector<int> data(n);
    for (auto& value : data) value = dist(rng);
    return data;
}

static std::vector<int> make_queries(std::size_t n, std::uint32_t seed) {
    std::size_t query_count = std::min<std::size_t>(5000, std::max<std::size_t>(100, n / 20));
    std::mt19937 rng(seed + 1337);
    std::uniform_int_distribution<int> dist(0, static_cast<int>(n * 4 + 100));
    std::vector<int> queries(query_count);
    for (auto& query : queries) query = dist(rng);
    return queries;
}

template <typename Callable>
static BenchmarkResult run_benchmark(
    const std::string& algorithm,
    const std::string& workload,
    std::size_t input_size,
    int iterations,
    Callable&& callable
) {
    std::vector<double> timings_ms;
    timings_ms.reserve(static_cast<std::size_t>(iterations));
    double max_rss_mb = current_rss_mb();

    for (int i = 0; i < iterations; ++i) {
        auto start = std::chrono::steady_clock::now();
        std::uint64_t checksum = callable(i);
        auto stop = std::chrono::steady_clock::now();
        g_sink += checksum;

        std::chrono::duration<double, std::milli> elapsed = stop - start;
        timings_ms.push_back(elapsed.count());
        max_rss_mb = std::max(max_rss_mb, current_rss_mb());
    }

    std::vector<double> sorted = timings_ms;
    std::sort(sorted.begin(), sorted.end());
    const double sum = std::accumulate(sorted.begin(), sorted.end(), 0.0);
    const double average = sum / static_cast<double>(sorted.size());
    const std::size_t p95_index = static_cast<std::size_t>(std::ceil(sorted.size() * 0.95)) - 1;
    const double p95 = sorted[std::min(p95_index, sorted.size() - 1)];
    const double ops_per_sec = average > 0.0 ? (static_cast<double>(input_size) / (average / 1000.0)) : 0.0;

    return BenchmarkResult{
        algorithm,
        workload,
        input_size,
        iterations,
        average,
        sorted.front(),
        sorted.back(),
        p95,
        max_rss_mb,
        ops_per_sec
    };
}

static void write_csv(const fs::path& path, const std::vector<BenchmarkResult>& results) {
    std::ofstream out(path);
    out << "algorithm,workload,input_size,iterations,average_ms,min_ms,max_ms,p95_ms,memory_mb,ops_per_second\n";
    out << std::fixed << std::setprecision(4);
    for (const auto& r : results) {
        out << csv_escape(r.algorithm) << ','
            << csv_escape(r.workload) << ','
            << r.input_size << ','
            << r.iterations << ','
            << r.average_ms << ','
            << r.min_ms << ','
            << r.max_ms << ','
            << r.p95_ms << ','
            << r.memory_mb << ','
            << r.ops_per_second << '\n';
    }
}

static void write_json(const fs::path& path, const std::vector<BenchmarkResult>& results, const Config& config) {
    std::ofstream out(path);
    out << std::fixed << std::setprecision(4);
    out << "{\n";
    out << "  \"benchmark\": \"runtime-performance-benchmark-suite\",\n";
    out << "  \"seed\": " << config.seed << ",\n";
    out << "  \"results\": [\n";
    for (std::size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        out << "    {"
            << "\"algorithm\": \"" << json_escape(r.algorithm) << "\", "
            << "\"workload\": \"" << json_escape(r.workload) << "\", "
            << "\"input_size\": " << r.input_size << ", "
            << "\"iterations\": " << r.iterations << ", "
            << "\"average_ms\": " << r.average_ms << ", "
            << "\"min_ms\": " << r.min_ms << ", "
            << "\"max_ms\": " << r.max_ms << ", "
            << "\"p95_ms\": " << r.p95_ms << ", "
            << "\"memory_mb\": " << r.memory_mb << ", "
            << "\"ops_per_second\": " << r.ops_per_second
            << "}" << (i + 1 == results.size() ? "" : ",") << "\n";
    }
    out << "  ]\n";
    out << "}\n";
}

int main(int argc, char** argv) {
    try {
        const Config config = parse_args(argc, argv);
        fs::create_directories(config.out_dir);

        std::vector<BenchmarkResult> results;

        for (std::size_t n : config.sizes) {
            const auto base_data = make_random_vector(n, config.seed + static_cast<std::uint32_t>(n));
            auto sorted_data = base_data;
            std::sort(sorted_data.begin(), sorted_data.end());
            const auto queries = make_queries(n, config.seed + static_cast<std::uint32_t>(n));

            results.push_back(run_benchmark("std::sort", "random integer vector", n, config.iterations, [&](int iteration) {
                auto data = base_data;
                if (iteration % 2 == 1 && !data.empty()) std::swap(data.front(), data.back());
                std::sort(data.begin(), data.end());
                return static_cast<std::uint64_t>(data[data.size() / 2]);
            }));

            results.push_back(run_benchmark("linear search", "membership queries on vector", n, config.iterations, [&](int iteration) {
                std::uint64_t found = 0;
                for (int q : queries) {
                    found += static_cast<std::uint64_t>(std::find(base_data.begin(), base_data.end(), q + iteration) != base_data.end());
                }
                return found;
            }));

            results.push_back(run_benchmark("binary search", "membership queries on sorted vector", n, config.iterations, [&](int iteration) {
                std::uint64_t found = 0;
                for (int q : queries) {
                    found += static_cast<std::uint64_t>(std::binary_search(sorted_data.begin(), sorted_data.end(), q + iteration));
                }
                return found;
            }));

            results.push_back(run_benchmark("unordered_set lookup", "membership queries on hash set", n, config.iterations, [&](int iteration) {
                std::unordered_set<int> lookup;
                lookup.reserve(base_data.size() * 2);
                lookup.insert(base_data.begin(), base_data.end());

                std::uint64_t found = 0;
                for (int q : queries) {
                    found += static_cast<std::uint64_t>(lookup.find(q + iteration) != lookup.end());
                }
                return found;
            }));

            results.push_back(run_benchmark("priority_queue", "push and drain random integers", n, config.iterations, [&](int iteration) {
                std::priority_queue<int> pq;
                for (int value : base_data) pq.push(value + iteration);
                std::uint64_t checksum = 0;
                std::size_t sampled = 0;
                while (!pq.empty()) {
                    if ((sampled++ % 97) == 0) checksum += static_cast<std::uint64_t>(pq.top());
                    pq.pop();
                }
                return checksum;
            }));
        }

        const fs::path csv_path = fs::path(config.out_dir) / "benchmark_results.csv";
        const fs::path json_path = fs::path(config.out_dir) / "benchmark_results.json";
        write_csv(csv_path, results);
        write_json(json_path, results, config);

        std::cout << "Benchmark complete.\n"
                  << "CSV:  " << csv_path << "\n"
                  << "JSON: " << json_path << "\n"
                  << "Checksum guard: " << g_sink << "\n";

        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}
