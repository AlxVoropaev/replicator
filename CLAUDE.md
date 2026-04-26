
<!-- From https://github.com/forrestchang/andrej-karpathy-skills/blob/main/CLAUDE.md -->


# Replicator


## 1. Think Before Coding

**Don't assume. Don't hide confusion. Surface tradeoffs.**

Before implementing:
- State your assumptions explicitly. If uncertain, ask.
- If multiple interpretations exist, present them - don't pick silently.
- If a simpler approach exists, say so. Push back when warranted.
- If something is unclear, stop. Name what's confusing. Ask.

## 2. Simplicity First

**Minimum code that solves the problem. Nothing speculative.**

- No features beyond what was asked.
- No abstractions for single-use code.
- No "flexibility" or "configurability" that wasn't requested.
- No error handling for impossible scenarios.
- If you write 200 lines and it could be 50, rewrite it.

Ask yourself: "Would a senior engineer say this is overcomplicated?" If yes, simplify.

## 3. Surgical Changes

**Touch only what you must. Clean up only your own mess.**

When editing existing code:
- Don't "improve" adjacent code, comments, or formatting.
- Don't refactor things that aren't broken.
- Match existing style, even if you'd do it differently.
- If you notice unrelated dead code, mention it - don't delete it.

When your changes create orphans:
- Remove imports/variables/functions that YOUR changes made unused.
- Don't remove pre-existing dead code unless asked.

The test: Every changed line should trace directly to the user's request.

## 4. Goal-Driven Execution

**Define success criteria. Loop until verified.**

Transform tasks into verifiable goals:
- "Add validation" → "Write tests for invalid inputs, then make them pass"
- "Fix the bug" → "Write a test that reproduces it, then make it pass"
- "Refactor X" → "Ensure tests pass before and after"

For multi-step tasks, state a brief plan:
```
1. [Step] → verify: [check]
2. [Step] → verify: [check]
3. [Step] → verify: [check]
```

Strong success criteria let you loop independently. Weak criteria ("make it work") require constant clarification.

---

**These guidelines are working if:** fewer unnecessary changes in diffs, fewer rewrites due to overcomplication, and clarifying questions come before implementation rather than after mistakes.

---

# Project context for development

## What this project is

A primordial-soup simulator: 256 byte-strings of length 32 are repeatedly paired,
concatenated, run as a Brainfuck program where **the tape IS the program**, then
split back. The goal is to observe self-replicating byte sequences emerge from
random interactions. Inspired by Google's *Computational Life* paper.

## Tech stack

- **Language:** C++20 (`std::span`, structured bindings, `[[nodiscard]]`-friendly).
- **Compiler:** GCC 15 in Docker; native build needs GCC ≥ 13.
- **Build:** CMake ≥ 3.20 with Ninja generator. `Release` is the default.
- **Tests:** GoogleTest 1.15.2, fetched via `FetchContent` (no system install).
- **Container:** multi-stage Dockerfile, `gcc:15` builder → `debian:trixie-slim`
  runtime. **Do not downgrade the runtime to `bookworm-slim`** — gcc:15 links
  against glibc 2.38+, which bookworm doesn't have.

## Layout

```
src/bf.{hpp,cpp}    Brainfuck interpreter, single function: run_bf(span, max_ops)
src/soup.{hpp,cpp}  class Soup: pick_pair, step, run, snapshot
src/main.cpp        CLI parsing, stats (entropy, top-N), SIGINT handling
tests/test_bf.cpp   11 interpreter tests
tests/test_soup.cpp 5 soup-level invariants
```

## Build commands

```bash
# native, first build
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure

# native, incremental
cmake --build build --parallel

# Docker (also runs ctest during build)
docker build -t replicator:dev .
docker run --rm replicator:dev --seed 42
```

## Interpreter semantics (don't silently change these)

- `+` `-` `>` `<` `[` `]` are the only opcodes; `.` and `,` are no-ops; any
  other byte is a no-op.
- 8-bit cells with wrap; `dp` wraps mod `tape.size()`; `ip` halts when it
  walks off the end (no `ip` wrap).
- Bracket jump table is **precomputed once** from the initial tape. Brackets
  introduced by self-modification are NOT honoured. This is intentional and
  matches the Computational Life paper.
- Unmatched brackets are no-ops (don't crash, don't seek to end).
- `run_bf` returns the count of executed instructions.

## Performance baseline

~3–4M steps/sec single-thread at default config. Hot path is `run_bf`'s switch.
If touching it: keep the `tape[ip]` read in a local, avoid `std::function`,
avoid heap allocation per step (the only per-`run_bf` allocation today is the
`jumps` vector — it could be reused via a thread-local buffer if profiling
shows it matters).

## Stats hot spot

`compute_stats` builds an `unordered_map<string, count>` over the snapshot
every `--report-every` steps. At pop=256 this is trivial; at pop=10⁶ it
dominates. If we ever scale up, switch to incremental stats (update map on
each `step` rather than re-snapshotting).

## Testing discipline

- Tests-first was used originally (RED → GREEN milestones).
- Every BF opcode has a dedicated test with an observable side effect, NOT
  just a no-throw assertion (except where unavoidable, e.g. `>` wrap).
- Soup tests assert reproducibility under the same seed, which means RNG
  changes are breaking changes — discuss before altering RNG ordering.

## CLI conventions

Long flags only (`--seed`, `--population`, ...). No short forms except `-h`.
Adding a new flag: extend `Config`, the parser table in `parse_args`, and
the `usage()` string. Keep defaults sane — the program runs with no flags.

## Git / repo

- Default branch: `main`. No `dev` branch is used.
- Remote: `origin` -> https://github.com/AlxVoropaev/replicator (public).
- Commits follow a "Milestone N: <one-line>" pattern from the initial build.
  After milestones, just describe the change normally.

## Things to ask the user before doing

- Changing interpreter semantics (would invalidate published runs).
- Adding background mutation — the user explicitly said no mutation.
- Splitting source into more files / introducing new abstractions — the
  user prefers minimum code (see "Simplicity First" above).
- Pushing a non-trivial change to `main` without local test pass.

## Open / known

- No replicator emergence observed in short smoke runs (~6M steps). Paper
  reports emergence at 10⁶–10⁸ depending on `MAX_OPS`. May want to expose
  more knobs (head start patterns, larger `MAX_OPS`) if user asks for it.
- `pick_pair` is O(1) but the `j >= i ? ++j : j` trick assumes `n ≥ 2`;
  the constructor enforces this.
- Stats snapshot copies all cells. Fine at pop=256; revisit if scaled.

