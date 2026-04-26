#pragma once
#include <cstddef>
#include <cstdint>
#include <span>

namespace replicator {

// Executes a Brainfuck program where the tape IS the program buffer.
// - tape: in/out byte buffer (program == data)
// - max_ops: hard cap on executed instructions (infinite-loop guard)
// - dp/ip both start at 0; ip advances normally and stops when it walks off
//   the buffer or max_ops is reached. dp wraps mod tape.size().
// - `.` and `,` are no-ops. Unmatched brackets are no-ops.
// Returns the number of instructions actually executed.
std::size_t run_bf(std::span<std::uint8_t> tape, std::size_t max_ops);

} // namespace replicator
