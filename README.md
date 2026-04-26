# replicator

A primordial-soup experiment in C++. A population of random byte strings is
repeatedly paired up, concatenated, and run as a Brainfuck program where the
**tape *is* the program**. Over many interactions, self-reproducing byte
patterns can emerge from nothing but random collisions — life from non-life
building blocks.

Inspired by Google's [*Computational Life: How Well-formed, Self-Replicating
Programs Emerge from Simple Interaction*](https://arxiv.org/abs/2406.19108).

---

## How it works

### The substrate

- A **soup** of `N` byte-strings, each `L` bytes long. Defaults: `N = 256`,
  `L = 32`.
- Initial bytes are uniform random over `0..255`.

### One step

1. Pick two **distinct** strings uniformly at random from the soup.
2. Concatenate them into a `2L`-byte tape.
3. Execute the tape as a Brainfuck program where the tape is *also* the data
   memory. `+` / `-` mutate the bytes (which include the program's own
   instruction bytes), and `[` / `]` jump within the same buffer.
4. Split the (possibly modified) tape back into two halves and write them
   back to the same two slots in the soup.

The interesting consequence: a `+` instruction can rewrite the next opcode.
A loop can copy bytes from one half of the tape into the other. Replicators
are byte sequences that arrange to copy themselves, end-to-end, into the
neighbour they happen to be paired with.

### Brainfuck variant

| op | meaning                                                           |
|----|-------------------------------------------------------------------|
| `>` | move data pointer right (wrap mod tape size)                     |
| `<` | move data pointer left  (wrap mod tape size)                     |
| `+` | increment cell at data pointer (wrap 255→0)                      |
| `-` | decrement cell at data pointer (wrap 0→255)                      |
| `[` | if cell == 0, jump past matching `]`                             |
| `]` | if cell != 0, jump back to matching `[`                          |
| `.` | **no-op** (no I/O in this substrate)                             |
| `,` | **no-op**                                                        |
| any other byte | no-op (advances ip)                                   |

- Cells are 8-bit unsigned with wrap.
- Two pointers — instruction pointer (`ip`) and data pointer (`dp`) — start
  at 0. `dp` wraps; `ip` halts when it walks off the end.
- Bracket jumps are **precomputed once** from the initial tape. Brackets
  introduced by self-modification mid-run are NOT honoured (a deliberate
  simplification).
- Unmatched brackets are no-ops.
- Each run is capped at `MAX_OPS` instructions (default 128) to bound
  infinite loops.

### Stats

Every `--report-every` steps the program prints a snapshot of the soup:

- `unique` — number of distinct strings present
- `H` — Shannon entropy in bits over the multiset of strings
- `speed` — measured steps/sec
- top-N most-frequent strings (hex), only those with count ≥ 2

When a replicator emerges, you'll see one row dominate the top-N list with a
high count.

---

## Build & run

### Docker (recommended)

```bash
docker build -t replicator .
docker run --rm replicator --seed 42
# Ctrl-C to stop
```

The Dockerfile uses a multi-stage build: `gcc:15` builder image runs the full
test suite during build; final runtime image is `debian:trixie-slim` and is
~79 MB.

### Native

Requirements: GCC ≥ 13 (C++20), CMake ≥ 3.20, Ninja, an internet connection
the first time (CMake fetches GoogleTest).

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/replicator --seed 42
```

Incremental rebuild after a code change:

```bash
cmake --build build --parallel
```

---

## CLI

```
--seed N           PRNG seed (default: random)
--population N     number of strings in the soup (default: 256)
--tape-size N      bytes per string (default: 32)
--max-ops N        per-step instruction cap (default: 128)
--report-every N   print stats every N steps (default: 1000)
--top-n N          how many top strings to print (default: 10)
-h, --help         show this help
```

Example output:

```
replicator: pop=256 tape=32 max_ops=128 report=500000 seed=42

=== step 500000  (unique=256  H=8.000 bits  speed=3881144 steps/s) ===

=== step 1000000  (unique=256  H=8.000 bits  speed=3977805 steps/s) ===
...
```

`H = 8.000 bits` means all 256 strings are distinct — no replicators yet.
The number drops, and entries with count > 1 appear at the top, when
replicators take hold.

---

## Project layout

```
replicator/
├── CMakeLists.txt       # build + GoogleTest via FetchContent
├── Dockerfile           # gcc:15 builder -> debian:trixie-slim runtime
├── src/
│   ├── bf.{hpp,cpp}     # Brainfuck interpreter (tape == program)
│   ├── soup.{hpp,cpp}   # population, pairing, step loop
│   └── main.cpp         # CLI, stats, signal handling
├── tests/
│   ├── test_bf.cpp      # opcodes, wrap, brackets, max_ops, self-mod
│   └── test_soup.cpp    # invariants, reproducibility, distinct pairs
└── README.md
```

---

## Tests

16 unit tests covering:

- every BF opcode, including wrap behaviour on `+`/`-`/`>`/`<`
- bracket matching, skip-when-zero, jump-back-when-nonzero
- unmatched brackets become no-ops
- `MAX_OPS` actually caps infinite loops
- `.` and `,` are no-ops
- self-modification: a `+` mutates the next instruction byte
- soup population stays constant
- same seed → identical end state (reproducibility)
- different seeds diverge
- `pick_pair` always returns distinct indices
- initial population has 256 unique strings

```bash
ctest --test-dir build --output-on-failure
```

---

## Performance

Roughly **3–4 million steps per second** on a modern x86 laptop core (single
thread). At default settings that's ~12 million BF ops/sec and a full
top-N stats sweep over the 256-string population takes <1 ms.

---

## What "emergence" looks like

The `Computational Life` paper reports replicators emerging in BF substrates
after roughly 10⁶–10⁸ interactions, depending on tape length, population
size, and the per-run instruction cap. With this implementation's defaults
that's ~1–30 minutes of wall-clock time. If your run isn't producing
replicators, things to try:

- Raise `--max-ops` (programs need enough budget to actually copy themselves).
- Lower `--population` (more pair collisions per replicator candidate).
- Run for longer — emergence is stochastic.
