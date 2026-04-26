#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>
#include <utility>

namespace replicator {

class Soup {
public:
    Soup(std::size_t population, std::size_t tape_size, std::uint64_t seed);

    std::size_t population() const { return cells_.size(); }
    std::size_t tape_size() const { return tape_size_; }

    const std::vector<std::uint8_t>& cell(std::size_t i) const { return cells_[i]; }
    const std::vector<std::vector<std::uint8_t>>& cells() const { return cells_; }

    // Pick two distinct uniform indices.
    std::pair<std::size_t, std::size_t> pick_pair();

    // One simulation step: pick pair, concat, run, split back.
    // Returns true if the BF program halted naturally (ops < max_ops).
    bool step(std::size_t max_ops);

    // Run N steps single-threaded; deterministic for a given seed.
    // Returns count of runs that halted naturally.
    std::size_t run(std::size_t n_steps, std::size_t max_ops);

    // Run N steps across `n_threads` workers. Each worker has its own RNG
    // seeded from the soup's RNG (so RNG state is consumed deterministically
    // at construction, but step ordering is not deterministic). Cells are
    // claimed atomically; concurrent threads never operate on the same cell.
    // Stops early if `stop` becomes true. n_threads <= 1 falls back to run().
    // Returns count of runs that halted naturally.
    std::size_t run_parallel(std::size_t n_steps,
                             std::size_t max_ops,
                             std::size_t n_threads,
                             const std::atomic<bool>& stop);

    // Snapshot copies of all cells.
    std::vector<std::vector<std::uint8_t>> snapshot() const { return cells_; }

private:
    std::size_t tape_size_;
    std::vector<std::vector<std::uint8_t>> cells_;
    std::mt19937_64 rng_;
};

} // namespace replicator
