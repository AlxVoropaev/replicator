#include "soup.hpp"
#include "bf.hpp"

namespace replicator {

Soup::Soup(std::size_t population, std::size_t tape_size, std::uint64_t seed)
    : tape_size_(tape_size), cells_(population), rng_(seed) {
    // stub
    (void)population;
}

std::pair<std::size_t, std::size_t> Soup::pick_pair() {
    return {0, 0}; // stub
}

void Soup::step(std::size_t /*max_ops*/) {
    // stub
}

void Soup::run(std::size_t /*n_steps*/, std::size_t /*max_ops*/) {
    // stub
}

} // namespace replicator
