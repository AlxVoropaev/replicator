#include "soup.hpp"
#include "bf.hpp"
#include <algorithm>
#include <stdexcept>
#include <thread>

namespace replicator {

Soup::Soup(std::size_t population, std::size_t tape_size, std::uint64_t seed)
    : tape_size_(tape_size), cells_(population), rng_(seed) {
    if (population < 2) {
        throw std::invalid_argument("population must be >= 2");
    }
    if (tape_size == 0) {
        throw std::invalid_argument("tape_size must be > 0");
    }
    static constexpr std::uint8_t bf_alphabet[] = {'+', '-', '>', '<', '[', ']', '.', ','};
    std::uniform_int_distribution<int> sym_dist(0, sizeof(bf_alphabet) - 1);
    for (auto& c : cells_) {
        c.resize(tape_size);
        for (auto& b : c) b = bf_alphabet[sym_dist(rng_)];
    }
}

std::pair<std::size_t, std::size_t> Soup::pick_pair() {
    const std::size_t n = cells_.size();
    std::uniform_int_distribution<std::size_t> d(0, n - 1);
    const std::size_t i = d(rng_);
    // pick j uniformly from the remaining n-1 indices
    std::uniform_int_distribution<std::size_t> d2(0, n - 2);
    std::size_t j = d2(rng_);
    if (j >= i) ++j;
    return {i, j};
}

bool Soup::step(std::size_t max_ops) {
    auto [i, j] = pick_pair();
    std::vector<std::uint8_t> tape(2 * tape_size_);
    std::copy(cells_[i].begin(), cells_[i].end(), tape.begin());
    std::copy(cells_[j].begin(), cells_[j].end(), tape.begin() + tape_size_);
    const std::size_t ops = run_bf(std::span<std::uint8_t>(tape), max_ops);
    std::copy(tape.begin(), tape.begin() + tape_size_, cells_[i].begin());
    std::copy(tape.begin() + tape_size_, tape.end(), cells_[j].begin());
    return ops < max_ops;
}

std::size_t Soup::run(std::size_t n_steps, std::size_t max_ops) {
    std::size_t ok = 0;
    for (std::size_t k = 0; k < n_steps; ++k) ok += step(max_ops) ? 1 : 0;
    return ok;
}

std::size_t Soup::run_parallel(std::size_t n_steps,
                               std::size_t max_ops,
                               std::size_t n_threads,
                               const std::atomic<bool>& stop) {
    if (n_threads <= 1) {
        std::size_t ok = 0;
        for (std::size_t k = 0; k < n_steps && !stop.load(std::memory_order_relaxed); ++k) {
            ok += step(max_ops) ? 1 : 0;
        }
        return ok;
    }

    const std::size_t pop = cells_.size();
    std::vector<std::atomic<std::uint8_t>> busy(pop);
    for (auto& b : busy) b.store(0, std::memory_order_relaxed);

    std::atomic<std::size_t> done{0};
    std::atomic<std::size_t> ok_runs{0};

    std::vector<std::uint64_t> seeds(n_threads);
    for (auto& s : seeds) s = rng_();

    auto worker = [&](std::uint64_t seed) {
        std::mt19937_64 rng(seed);
        std::uniform_int_distribution<std::size_t> d(0, pop - 1);
        std::uniform_int_distribution<std::size_t> d2(0, pop - 2);
        std::vector<std::uint8_t> tape(2 * tape_size_);
        std::size_t local_ok = 0;

        while (!stop.load(std::memory_order_relaxed)) {
            const std::size_t cur = done.fetch_add(1, std::memory_order_relaxed);
            if (cur >= n_steps) break;

            // Try to claim two distinct cells. Lower index first to keep
            // an arbitrary (but consistent) acquisition order.
            for (;;) {
                std::size_t i = d(rng);
                std::size_t j = d2(rng);
                if (j >= i) ++j;
                std::size_t lo = i, hi = j;
                if (lo > hi) std::swap(lo, hi);

                std::uint8_t expected = 0;
                if (!busy[lo].compare_exchange_strong(
                        expected, 1, std::memory_order_acquire, std::memory_order_relaxed)) {
                    continue;
                }
                expected = 0;
                if (!busy[hi].compare_exchange_strong(
                        expected, 1, std::memory_order_acquire, std::memory_order_relaxed)) {
                    busy[lo].store(0, std::memory_order_release);
                    continue;
                }

                std::copy(cells_[i].begin(), cells_[i].end(), tape.begin());
                std::copy(cells_[j].begin(), cells_[j].end(), tape.begin() + tape_size_);
                const std::size_t ops = run_bf(std::span<std::uint8_t>(tape), max_ops);
                std::copy(tape.begin(), tape.begin() + tape_size_, cells_[i].begin());
                std::copy(tape.begin() + tape_size_, tape.end(), cells_[j].begin());
                if (ops < max_ops) ++local_ok;

                busy[hi].store(0, std::memory_order_release);
                busy[lo].store(0, std::memory_order_release);
                break;
            }
        }
        ok_runs.fetch_add(local_ok, std::memory_order_relaxed);
    };

    std::vector<std::thread> ts;
    ts.reserve(n_threads);
    for (std::size_t t = 0; t < n_threads; ++t) ts.emplace_back(worker, seeds[t]);
    for (auto& th : ts) th.join();
    return ok_runs.load(std::memory_order_relaxed);
}

} // namespace replicator
