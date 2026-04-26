# replicator

A primordial-soup experiment in C++: a population of random byte strings is
repeatedly paired up, concatenated, and run as a Brainfuck program where the
**tape is the program**. Over time, self-reproducing strings can emerge from
nothing but random interactions — life from non-life building blocks.

Inspired by Google's *Computational Life* paper.

## Rules

- Population: 256 strings of 32 bytes each, initialised uniformly at random.
- Each step:
  1. Pick two **distinct** strings uniformly at random.
  2. Concatenate them into a 64-byte tape.
  3. Run the tape as a Brainfuck program (the tape *is* the program — `+`/`-`
     mutate the code, `[`/`]` jump within it).
  4. Split the (possibly modified) tape back into two 32-byte halves and
     return them to the population.
- `.` and `,` are no-ops; unmatched brackets are no-ops.
- Each run is capped at 128 instructions.
- Every 1000 steps (configurable), print: unique-string count, Shannon
  entropy, top-N most frequent strings as hex.
- Runs until Ctrl-C.

## Build & run with Docker

```bash
docker build -t replicator .
docker run --rm replicator --seed 42
```

## Build natively

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/replicator --seed 42
```

## CLI

```
--seed N           PRNG seed (default: random)
--population N     number of strings in the soup (default: 256)
--tape-size N      bytes per string (default: 32)
--max-ops N        per-step instruction cap (default: 128)
--report-every N   print stats every N steps (default: 1000)
--top-n N          how many top strings to print (default: 10)
```

## Layout

```
src/bf.{hpp,cpp}     Brainfuck interpreter (tape == program)
src/soup.{hpp,cpp}   population, step loop
src/main.cpp         CLI, stats, signal handling
tests/               GoogleTest suite
```
