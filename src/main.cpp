#include "soup.hpp"
#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>
#include <algorithm>

namespace {

std::atomic<bool> g_stop{false};

void on_sigint(int) { g_stop = true; }

struct Config {
    std::size_t population = std::pow(2, 24);
    std::size_t tape_size = 32;
    std::size_t max_ops = 128;
    std::size_t report_every = 1000000;
    std::size_t top_n = 10;
    std::size_t threads = 1;
    std::uint64_t seed = std::random_device{}();
    bool seed_set = false;
};

void usage(const char* argv0) {
    std::fprintf(stderr,
        "Usage: %s [options]\n"
        "  --seed N           PRNG seed (default: random)\n"
        "  --population N     number of strings in the soup (default: 256)\n"
        "  --tape-size N      bytes per string (default: 32)\n"
        "  --max-ops N        per-step instruction cap (default: 128)\n"
        "  --report-every N   print stats every N steps (default: 1000)\n"
        "  --top-n N          how many top strings to print (default: 10)\n"
        "  --threads N        worker threads for sim and stats (default: 1)\n"
        "  -h, --help         show this help\n",
        argv0);
}

bool parse_size(const char* s, std::size_t& out) {
    char* end = nullptr;
    unsigned long long v = std::strtoull(s, &end, 10);
    if (end == s || *end != '\0') return false;
    out = static_cast<std::size_t>(v);
    return true;
}

bool parse_u64(const char* s, std::uint64_t& out) {
    char* end = nullptr;
    unsigned long long v = std::strtoull(s, &end, 10);
    if (end == s || *end != '\0') return false;
    out = static_cast<std::uint64_t>(v);
    return true;
}

bool parse_args(int argc, char** argv, Config& cfg) {
    for (int i = 1; i < argc; ++i) {
        std::string_view a = argv[i];
        auto need_val = [&](std::size_t& dst) {
            if (i + 1 >= argc || !parse_size(argv[i + 1], dst)) return false;
            ++i;
            return true;
        };
        if (a == "--seed") {
            if (i + 1 >= argc || !parse_u64(argv[i + 1], cfg.seed)) return false;
            cfg.seed_set = true;
            ++i;
        } else if (a == "--population") { if (!need_val(cfg.population)) return false; }
        else if (a == "--tape-size")    { if (!need_val(cfg.tape_size))  return false; }
        else if (a == "--max-ops")      { if (!need_val(cfg.max_ops))    return false; }
        else if (a == "--report-every") { if (!need_val(cfg.report_every)) return false; }
        else if (a == "--top-n")        { if (!need_val(cfg.top_n))      return false; }
        else if (a == "--threads")      { if (!need_val(cfg.threads))    return false; }
        else if (a == "-h" || a == "--help") { usage(argv[0]); std::exit(0); }
        else {
            std::fprintf(stderr, "unknown argument: %s\n", argv[i]);
            return false;
        }
    }
    if (cfg.threads == 0) cfg.threads = 1;
    return true;
}

std::string bf_text_of(const std::vector<std::uint8_t>& v) {
    std::string s;
    s.resize(v.size());
    for (std::size_t i = 0; i < v.size(); ++i) {
        switch (v[i]) {
            case '+': case '-': case '>': case '<': case '[': case ']':
                s[i] = static_cast<char>(v[i]);
                break;
            default:
                s[i] = '.';
                break;
        }
    }
    return s;
}

struct Stats {
    std::size_t unique = 0;
    double entropy_bits = 0.0;
    std::vector<std::pair<std::size_t, std::vector<std::uint8_t>>> top; // (count, bytes)
};

Stats compute_stats(const std::vector<std::vector<std::uint8_t>>& cells,
                    std::size_t top_n,
                    std::size_t n_threads) {
    using Map = std::unordered_map<std::string, std::size_t>;
    const std::size_t n = cells.size();
    if (n_threads < 1) n_threads = 1;
    if (n_threads > n) n_threads = n;

    auto build_chunk = [&](std::size_t lo, std::size_t hi) {
        Map m;
        m.reserve((hi - lo) * 2);
        for (std::size_t i = lo; i < hi; ++i) {
            const auto& c = cells[i];
            ++m[std::string(c.begin(), c.end())];
        }
        return m;
    };

    Map merged;
    if (n_threads <= 1) {
        merged = build_chunk(0, n);
    } else {
        std::vector<Map> partials(n_threads);
        std::vector<std::thread> ts;
        ts.reserve(n_threads);
        const std::size_t chunk = (n + n_threads - 1) / n_threads;
        for (std::size_t t = 0; t < n_threads; ++t) {
            const std::size_t lo = t * chunk;
            const std::size_t hi = std::min(lo + chunk, n);
            ts.emplace_back([&, t, lo, hi] {
                if (lo < hi) partials[t] = build_chunk(lo, hi);
            });
        }
        for (auto& th : ts) th.join();

        // Merge into the largest partial to minimize rehashing.
        std::size_t big = 0;
        for (std::size_t t = 1; t < partials.size(); ++t) {
            if (partials[t].size() > partials[big].size()) big = t;
        }
        merged = std::move(partials[big]);
        merged.reserve(n);
        for (std::size_t t = 0; t < partials.size(); ++t) {
            if (t == big) continue;
            for (auto& [k, v] : partials[t]) merged[k] += v;
        }
    }

    Stats st;
    st.unique = merged.size();
    const double total = static_cast<double>(n);
    for (const auto& [k, v] : merged) {
        double p = static_cast<double>(v) / total;
        st.entropy_bits -= p * std::log2(p);
    }
    std::vector<std::pair<std::size_t, std::vector<std::uint8_t>>> sorted;
    sorted.reserve(merged.size());
    for (auto& [k, v] : merged) {
        sorted.emplace_back(v, std::vector<std::uint8_t>(k.begin(), k.end()));
    }
    std::partial_sort(
        sorted.begin(),
        sorted.begin() + std::min(top_n, sorted.size()),
        sorted.end(),
        [](const auto& a, const auto& b) { return a.first > b.first; });
    sorted.resize(std::min(top_n, sorted.size()));
    st.top = std::move(sorted);
    return st;
}

void print_report(std::size_t step, const Stats& st, double steps_per_sec) {
    std::printf("\n=== step %zu  (unique=%zu  H=%.3f bits  speed=%.0f steps/s) ===\n",
                step, st.unique, st.entropy_bits, steps_per_sec);
    for (std::size_t i = 0; i < st.top.size(); ++i) {
        const auto& [cnt, bytes] = st.top[i];
        if (i > 0 && cnt < 2) break; // always show #1; skip the rest if singletons
        std::printf("  %2zu) x%-4zu  %s\n", i + 1, cnt, bf_text_of(bytes).c_str());
    }
    std::fflush(stdout);
}

} // namespace

int main(int argc, char** argv) {
    Config cfg;
    if (!parse_args(argc, argv, cfg)) {
        usage(argv[0]);
        return 1;
    }

    std::signal(SIGINT, on_sigint);

    std::printf("replicator: pop=%zu tape=%zu max_ops=%zu report=%zu threads=%zu seed=%llu\n",
                cfg.population, cfg.tape_size, cfg.max_ops, cfg.report_every, cfg.threads,
                static_cast<unsigned long long>(cfg.seed));
    std::fflush(stdout);

    replicator::Soup soup(cfg.population, cfg.tape_size, cfg.seed);

    std::size_t step = 0;

    while (!g_stop.load()) {
        auto t_run0 = std::chrono::steady_clock::now();
        soup.run_parallel(cfg.report_every, cfg.max_ops, cfg.threads, g_stop);
        auto t_run1 = std::chrono::steady_clock::now();
        step += cfg.report_every;
        double dt = std::chrono::duration<double>(t_run1 - t_run0).count();
        double sps = dt > 0 ? static_cast<double>(cfg.report_every) / dt : 0.0;
        auto st = compute_stats(soup.cells(), cfg.top_n, cfg.threads);
        print_report(step, st, sps);
    }

    std::printf("\nstopped after %zu steps\n", step);
    return 0;
}
