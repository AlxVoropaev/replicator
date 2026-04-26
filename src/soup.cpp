#include "soup.hpp"
#include "bf.hpp"
#include <stdexcept>

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

void Soup::step(std::size_t max_ops) {
    auto [i, j] = pick_pair();
    std::vector<std::uint8_t> tape(2 * tape_size_);
    std::copy(cells_[i].begin(), cells_[i].end(), tape.begin());
    std::copy(cells_[j].begin(), cells_[j].end(), tape.begin() + tape_size_);
    run_bf(std::span<std::uint8_t>(tape), max_ops);
    std::copy(tape.begin(), tape.begin() + tape_size_, cells_[i].begin());
    std::copy(tape.begin() + tape_size_, tape.end(), cells_[j].begin());
}

void Soup::run(std::size_t n_steps, std::size_t max_ops) {
    for (std::size_t k = 0; k < n_steps; ++k) step(max_ops);
}

} // namespace replicator
