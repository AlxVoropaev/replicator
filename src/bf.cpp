#include "bf.hpp"
#include <vector>

namespace replicator {

namespace {

// Build jump table: for every '[' in tape, the index of its matching ']' (and
// vice versa). Unmatched brackets get -1 -> treated as no-ops.
std::vector<int> build_jumps(std::span<const std::uint8_t> tape) {
    const int n = static_cast<int>(tape.size());
    std::vector<int> jumps(n, -1);
    std::vector<int> stack;
    stack.reserve(n);
    for (int i = 0; i < n; ++i) {
        if (tape[i] == '[') {
            stack.push_back(i);
        } else if (tape[i] == ']') {
            if (!stack.empty()) {
                int open = stack.back();
                stack.pop_back();
                jumps[open] = i;
                jumps[i] = open;
            }
        }
    }
    return jumps;
}

} // namespace

std::size_t run_bf(std::span<std::uint8_t> tape, std::size_t max_ops) {
    const std::size_t n = tape.size();
    if (n == 0) return 0;

    // NOTE: the jump table is computed once from the initial tape. Self-
    // modifying writes that introduce new brackets won't be honored mid-run;
    // this is the standard simplification used in this kind of substrate.
    auto jumps = build_jumps(std::span<const std::uint8_t>(tape.data(), n));

    std::size_t ip = 0;
    std::size_t dp = 0;
    std::size_t ops = 0;

    while (ip < n && ops < max_ops) {
        const std::uint8_t op = tape[ip];
        switch (op) {
            case '>':
                dp = (dp + 1) % n;
                ++ip;
                break;
            case '<':
                dp = (dp + n - 1) % n;
                ++ip;
                break;
            case '+':
                ++tape[dp];
                ++ip;
                break;
            case '-':
                --tape[dp];
                ++ip;
                break;
            case '[': {
                const int match = jumps[ip];
                if (tape[dp] == 0 && match >= 0) {
                    ip = static_cast<std::size_t>(match) + 1;
                } else {
                    ++ip;
                }
                break;
            }
            case ']': {
                const int match = jumps[ip];
                if (tape[dp] != 0 && match >= 0) {
                    ip = static_cast<std::size_t>(match) + 1;
                } else {
                    ++ip;
                }
                break;
            }
            default:
                ++ip;
                break;
        }
        ++ops;
    }
    return ops;
}

} // namespace replicator
