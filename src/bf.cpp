#include "bf.hpp"
#include <vector>

namespace replicator {

namespace {

// Thread-local scratch reused across run_bf calls — eliminates the per-step
// vector allocations that were ~1.4M/sec at the previous hot path.
thread_local std::vector<int> tl_jumps;
thread_local std::vector<int> tl_stack;

template <bool Pow2>
std::size_t run_loop(std::uint8_t* tape,
                     std::size_t n,
                     const int* jumps,
                     std::size_t max_ops) {
    const std::size_t mask = n - 1;
    std::size_t ip = 0;
    std::size_t dp = 0;
    std::size_t ops = 0;

#if defined(__GNUC__) || defined(__clang__)
    // Computed-goto threaded dispatch: each handler jumps directly to the
    // next one based on the opcode byte, avoiding the loop branch and giving
    // the branch predictor a separate site per opcode.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
    void* dispatch_table[256];
    for (auto& p : dispatch_table) p = &&op_default;
    dispatch_table['>'] = &&op_gt;
    dispatch_table['<'] = &&op_lt;
    dispatch_table['+'] = &&op_plus;
    dispatch_table['-'] = &&op_minus;
    dispatch_table['['] = &&op_open;
    dispatch_table[']'] = &&op_close;

    #define DISPATCH() do { \
        if (ip >= n || ops >= max_ops) goto done; \
        ++ops; \
        goto *dispatch_table[tape[ip]]; \
    } while (0)

    DISPATCH();

op_gt:
    if constexpr (Pow2) dp = (dp + 1) & mask;
    else                dp = (dp + 1) % n;
    ++ip;
    DISPATCH();

op_lt:
    if constexpr (Pow2) dp = (dp - 1) & mask;
    else                dp = (dp + n - 1) % n;
    ++ip;
    DISPATCH();

op_plus:
    ++tape[dp];
    ++ip;
    DISPATCH();

op_minus:
    --tape[dp];
    ++ip;
    DISPATCH();

op_open: {
    const int match = jumps[ip];
    if (tape[dp] == 0 && match >= 0) {
        ip = static_cast<std::size_t>(match) + 1;
    } else {
        ++ip;
    }
    DISPATCH();
}

op_close: {
    const int match = jumps[ip];
    if (tape[dp] != 0 && match >= 0) {
        ip = static_cast<std::size_t>(match) + 1;
    } else {
        ++ip;
    }
    DISPATCH();
}

op_default:
    ++ip;
    DISPATCH();

done:
    #undef DISPATCH
#pragma GCC diagnostic pop
    return ops;
#else
    while (ip < n && ops < max_ops) {
        const std::uint8_t op = tape[ip];
        switch (op) {
            case '>':
                if constexpr (Pow2) dp = (dp + 1) & mask;
                else                dp = (dp + 1) % n;
                ++ip; break;
            case '<':
                if constexpr (Pow2) dp = (dp - 1) & mask;
                else                dp = (dp + n - 1) % n;
                ++ip; break;
            case '+': ++tape[dp]; ++ip; break;
            case '-': --tape[dp]; ++ip; break;
            case '[': {
                const int match = jumps[ip];
                ip = (tape[dp] == 0 && match >= 0) ? static_cast<std::size_t>(match) + 1 : ip + 1;
                break;
            }
            case ']': {
                const int match = jumps[ip];
                ip = (tape[dp] != 0 && match >= 0) ? static_cast<std::size_t>(match) + 1 : ip + 1;
                break;
            }
            default: ++ip; break;
        }
        ++ops;
    }
    return ops;
#endif
}

} // namespace

std::size_t run_bf(std::span<std::uint8_t> tape, std::size_t max_ops) {
    const std::size_t n = tape.size();
    if (n == 0) return 0;

    // Build jump table into thread-local scratch (no per-call allocation
    // after the first warm-up call on this thread).
    if (tl_jumps.size() < n) tl_jumps.resize(n);
    int* jumps = tl_jumps.data();
    for (std::size_t i = 0; i < n; ++i) jumps[i] = -1;

    tl_stack.clear();
    for (std::size_t i = 0; i < n; ++i) {
        const std::uint8_t b = tape[i];
        if (b == '[') {
            tl_stack.push_back(static_cast<int>(i));
        } else if (b == ']') {
            if (!tl_stack.empty()) {
                int open = tl_stack.back();
                tl_stack.pop_back();
                jumps[open] = static_cast<int>(i);
                jumps[i] = open;
            }
        }
    }

    const bool pow2 = (n & (n - 1)) == 0;
    if (pow2) return run_loop<true>(tape.data(), n, jumps, max_ops);
    else      return run_loop<false>(tape.data(), n, jumps, max_ops);
}

} // namespace replicator
