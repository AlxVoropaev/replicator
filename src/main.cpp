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
#include <span>
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

Stats compute_stats(const std::uint8_t* bytes,
                    std::size_t n,
                    std::size_t tape_size,
                    std::size_t top_n,
                    std::size_t n_threads) {
    // Sort-based stats: build string_views into the (read-only) bytes buffer,
    // sort, then walk to count consecutive equals. Faster than unordered_map
    // at pop>=10^6 because it does no per-entry node allocation; std::sort is
    // O(n log n) but with cheap 32-byte memcmp comparisons.
    Stats st;
    if (n == 0) return st;

    std::vector<std::string_view> views(n);
    if (n_threads < 1) n_threads = 1;
    if (n_threads > n) n_threads = n;

    auto build_chunk = [&](std::size_t lo, std::size_t hi) {
        for (std::size_t i = lo; i < hi; ++i) {
            views[i] = std::string_view(
                reinterpret_cast<const char*>(bytes + i * tape_size), tape_size);
        }
    };

    if (n_threads <= 1) {
        build_chunk(0, n);
    } else {
        std::vector<std::thread> ts;
        ts.reserve(n_threads);
        const std::size_t chunk = (n + n_threads - 1) / n_threads;
        for (std::size_t t = 0; t < n_threads; ++t) {
            const std::size_t lo = t * chunk;
            const std::size_t hi = std::min(lo + chunk, n);
            if (lo < hi) ts.emplace_back(build_chunk, lo, hi);
        }
        for (auto& th : ts) th.join();
    }

    std::sort(views.begin(), views.end());

    // Walk runs of equal views, accumulating count + entropy. Keep only the
    // top-N candidates via a min-heap to avoid materializing all uniques.
    using Cand = std::pair<std::size_t, std::string_view>; // (count, key)
    auto heap_cmp = [](const Cand& a, const Cand& b) { return a.first > b.first; };
    std::vector<Cand> heap;
    heap.reserve(top_n + 1);

    const double total = static_cast<double>(n);
    std::size_t unique = 0;
    double entropy = 0.0;
    std::size_t i = 0;
    while (i < n) {
        std::size_t j = i + 1;
        while (j < n && views[j] == views[i]) ++j;
        const std::size_t cnt = j - i;
        ++unique;
        const double p = static_cast<double>(cnt) / total;
        entropy -= p * std::log2(p);

        if (top_n > 0) {
            if (heap.size() < top_n) {
                heap.emplace_back(cnt, views[i]);
                std::push_heap(heap.begin(), heap.end(), heap_cmp);
            } else if (cnt > heap.front().first) {
                std::pop_heap(heap.begin(), heap.end(), heap_cmp);
                heap.back() = {cnt, views[i]};
                std::push_heap(heap.begin(), heap.end(), heap_cmp);
            }
        }
        i = j;
    }

    std::sort(heap.begin(), heap.end(),
              [](const Cand& a, const Cand& b) { return a.first > b.first; });

    st.unique = unique;
    st.entropy_bits = entropy;
    st.top.reserve(heap.size());
    for (auto& [cnt, sv] : heap) {
        st.top.emplace_back(cnt, std::vector<std::uint8_t>(sv.begin(), sv.end()));
    }
    return st;
}

void print_report(std::size_t step,
                  const Stats& st,
                  double steps_per_sec,
                  std::size_t ok_runs,
                  std::size_t total_runs,
                  std::span<const std::uint8_t> cell0) {
    std::printf("\n=== step %zu  (unique=%zu  H=%.3f bits  speed=%.0f steps/s  done=%zu/%zu) ===\n",
                step, st.unique, st.entropy_bits, steps_per_sec, ok_runs, total_runs);
    for (std::size_t i = 0; i < st.top.size(); ++i) {
        const auto& [cnt, bytes] = st.top[i];
        if (i > 0 && cnt < 2) break; // always show #1; skip the rest if singletons
        std::printf("  %2zu) x%-4zu  %s\n", i + 1, cnt, bf_text_of(bytes).c_str());
    }
    if (!st.top.empty() && st.top.front().first == 1) {
        std::vector<std::uint8_t> c0(cell0.begin(), cell0.end());
        std::printf("  c0) x1     %s\n", bf_text_of(c0).c_str());
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
        const std::size_t ok = soup.run_parallel(cfg.report_every, cfg.max_ops, cfg.threads, g_stop);
        auto t_run1 = std::chrono::steady_clock::now();
        step += cfg.report_every;
        double dt = std::chrono::duration<double>(t_run1 - t_run0).count();
        double sps = dt > 0 ? static_cast<double>(cfg.report_every) / dt : 0.0;
        auto st = compute_stats(soup.bytes_data(), soup.population(), soup.tape_size(),
                                cfg.top_n, cfg.threads);
        print_report(step, st, sps, ok, cfg.report_every, soup.cell(0));
    }

    std::printf("\nstopped after %zu steps\n", step);
    return 0;
}
