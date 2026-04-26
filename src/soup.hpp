#pragma once
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

    // Pick two distinct uniform indices.
    std::pair<std::size_t, std::size_t> pick_pair();

    // One simulation step: pick pair, concat, run, split back.
    void step(std::size_t max_ops);

    // Run N steps.
    void run(std::size_t n_steps, std::size_t max_ops);

    // Snapshot copies of all cells (for stats).
    std::vector<std::vector<std::uint8_t>> snapshot() const { return cells_; }

private:
    std::size_t tape_size_;
    std::vector<std::vector<std::uint8_t>> cells_;
    std::mt19937_64 rng_;
};

} // namespace replicator
