#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <random>
#include <span>
#include <vector>
#include <utility>

namespace replicator {

class Soup {
public:
    Soup(std::size_t population, std::size_t tape_size, std::uint64_t seed);

    std::size_t population() const { return population_; }
    std::size_t tape_size() const { return tape_size_; }

    // Cells live in one contiguous buffer (population * tape_size bytes) for
    // cache locality. cell(i) returns a view; bytes_data() exposes the whole
    // buffer for callers that want to iterate it in one sweep.
    std::span<const std::uint8_t> cell(std::size_t i) const {
        return {bytes_.data() + i * tape_size_, tape_size_};
    }
    const std::uint8_t* bytes_data() const { return bytes_.data(); }

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

    // Snapshot copies of all cells (used by tests for equality comparisons).
    std::vector<std::vector<std::uint8_t>> snapshot() const {
        std::vector<std::vector<std::uint8_t>> out(population_);
        for (std::size_t i = 0; i < population_; ++i) {
            const auto* p = bytes_.data() + i * tape_size_;
            out[i].assign(p, p + tape_size_);
        }
        return out;
    }

private:
    std::size_t population_;
    std::size_t tape_size_;
    std::vector<std::uint8_t> bytes_; // population_ * tape_size_ bytes, row-major
    std::mt19937_64 rng_;
};

} // namespace replicator
