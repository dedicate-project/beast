# Compose Pipelines

Ready-to-load JSON pipelines for `beast-compose`. Each file is in the same on-disk
format the server uses for its own persistence, so the simplest way to install one is to
drop it into your storage directory and restart the server:

```bash
# Pick the directory you pass as --storage_folder. If you don't pass one, the server
# uses ./models in its working directory.
cp examples/compose-pipelines/ascending-mazes.json ~/.beast-storage/
beast-compose --storage_folder ~/.beast-storage --http_port 8080 --html_root html_static
```

Then open the web UI at `http://localhost:8080`, click into the Pipelines view, and
you'll see the newly-added pipeline alongside any others you've created.

## Pipelines

### `ascending-mazes.json`

The classic curriculum-learning shape: a single `ProgramFactoryPipe` feeds three
`EvaluatorPipe`s in series, each wrapped around a `MazeEvaluator` of increasing size
and difficulty (5x5 / 8x8 / 12x12). A `ResultsSummaryPipe` sits between each stage so
you can see at a glance how candidates degrade as the difficulty steps up. The tail of
the pipeline persists the top-K best programs to
`/tmp/beast-ascending-mazes-best.json` via a `ProgramStorageSinkPipe`.

```
factory -> easy_maze -> stats_easy -> medium_maze -> stats_medium -> hard_maze -> stats_hard -> best_of_run
```

The three `ResultsSummaryPipe` instances each show up in the floating
"Results summaries" panel in the web UI, so you can watch the rolling win-rate of the
population at each difficulty level in real time, and hit "Reset" between difficulty
bumps if you change the maze size or shape.

### `survivor-recirculation.json`

Demonstrates a loop-back wiring: a `MultiplexerPipe` merges the fresh
`ProgramFactoryPipe` output with the best survivors from a downstream stage, feeding
them both back into the evolution loop. The animated `FanPipe` after the evaluator
makes throughput visible (faster spin = more candidates flowing) and a
`ProgramStorageSinkPipe` checkpoints the best programs to disk.

```
factory ──┐
          ├─> mux -> adder -> fan -> stats -> fanout ──┬─> survivors
fanout ───┘                                            └─> mux (loop-back)
```

The `fanout` `DemultiplexerPipe` is configured in `broadcast` mode so every candidate
that leaves `stats` is sent to both the `survivors` sink (for persistence) and back
into the `mux` (for another round of evolution). The broadcast strategy is intentional
here -- we need a copy of each candidate on both branches; a round-robin demux would
let half of them slip past the sink or the loop, defeating the point.

This is the recommended shape for any task you want to train over many cycles without
losing accumulated progress: editing knobs in the UI no longer wipes the
`ResultsSummaryPipe`'s best-ever score (state is migrated across `mutatePipeline`
calls), and `ProgramStorageSinkPipe` keeps a JSON ledger you can reload via
`ProgramStorageSourcePipe` after a server restart.

### `sha256-round.json`

Starting point for evolving a full SHA-256 hash function. This pipeline targets the
first rung of the curriculum: a single SHA-256 compression round. The candidate program
reads 8 working-state words (`a..h`) from VM variables 0-7, the round constant `K` from
variable 8, and the message-schedule word `W` from variable 9; it has to write the
transformed `(a'..h')` to variables 11-18.

```
factory -> sha256_round -> stats -> fan -> best_of_run
```

The `Sha256RoundEvaluator` is parametrised by which of the 64 round constants to pin
(default: `round_constant_index = 0` ⇒ `K = 0x428a2f98`). Scoring is bit-level
Hamming distance per output word averaged over trials, so the GA has a smooth gradient
to climb (a uniform-random guess scores ~0.5 per word, an exact-match scores 1.0).
Pure exact-match scoring would degenerate into needle-in-a-haystack hash inversion --
that's the cryptographic property of SHA-256 working *against* us.

The GA is heavily biased toward bit-manipulation opcodes (XOR, AND, OR, INVERT,
ROTATE, BitShift, ADD, COPY) via the `opcode_weights` map -- without that bias the
random-program factory produces too many jumps, comparisons, and print instructions
that don't help reproduce the round transformation. `max_genome_bytes` is bumped to
4096 because a hand-written round expressed in BEAST bytecode is a couple of hundred
operators long, and we need room for the GA to discover sub-optimal variants on the
way to the optimum.

Don't expect this single pipeline to actually converge on a perfect SHA-256 round in
any reasonable wall-clock time -- this is a research artefact, not a recipe. What the
pipeline does give you is the wiring on which to iterate: tune the opcode bias, the
mutation rate, the max-genome size; seed the population with a hand-written round via
`ProgramStorageSourcePipe`; or chain several `Sha256RoundEvaluator`s with different
`round_constant_index` values to teach a polymorphic round.

### `sha256-curriculum.json`

A three-stage curriculum that teaches a population the prerequisites of the SHA-256
round body before throwing the round itself at it. Each stage runs in parallel, writes
its top-K survivors to a JSON ledger, and the next stage seeds from that ledger via a
`Multiplexer` mixing fresh exploration with survivor seed material.

```
factory_id    -> id_stage    -> id_stats    -> id_sink      (writes /tmp/beast-sha-id.json)

factory_rot ──┐
              ├─> rot_mux -> rot_stage -> rot_stats -> rot_sink   (writes /tmp/beast-sha-rot.json)
id_seed     ──┘

factory_sigma ─┐
               ├─> sigma_mux -> sigma_stage -> sigma_stats -> sigma_sink   (writes /tmp/beast-sha-sigma.json)
rot_seed    ───┘
```

Stage 0 is `IdentityEvaluator(width=4)` -- teaches the genome to copy inputs to
outputs (the prerequisite skill any downstream stage assumes). Stage 1 is a
`RotateEvaluator(amount=2, direction=right)` seeded from the identity-stage survivors;
it teaches the GA to apply the `RotateVariableRight` opcode at a specific amount on
top of a population that already knows how to address output slots. Stage 2 is a
`Sha256SigmaEvaluator(variant=big0)` seeded from the rotate-stage survivors; that
sigma is itself just three rotations XORed together, so a population that's fluent in
rotation and XOR should converge much faster than starting from byte soup.

Once stage 2 is converging well, extend the curriculum the same way: add a stage that
seeds `sigma_sink` survivors into a `Sha256ChEvaluator` and a `Sha256MajEvaluator`,
then a final stage that seeds *those* survivors into the full `Sha256RoundEvaluator`.
The example file only ships three stages because the file gets unwieldy beyond that,
but the pattern is mechanical -- add a `(factory, ledger-source, mux, evaluator,
stats, sink)` tuple per new stage.

`opcode_weights` in each stage is tuned to bias toward the opcodes that stage's
reference function needs. Stage 0 biases `CopyVariable` (opcode 10) heavily; stage 1
biases `RotateVariableLeft/Right` (opcodes 34/35) plus a smaller `CopyVariable`
weight; stage 2 biases `BitWiseXorTwoVariables` (opcode 33) plus rotations. Without
these biases the random-program factory would produce too many jumps, prints, and
system calls and the early-stage progress would slow to a crawl.

### `sha256-round-with-subroutines.json`

A minimal example of the **subroutine mechanism** in action. It runs a single
`Sha256RoundEvaluator` stage but mounts the best `Sha256SigmaEvaluator(variant=big0)`
genome from `sha256-curriculum.json`'s `sigma_sink` ledger as **subroutine 0**, callable
from the round body via the `CallSubroutine` opcode (`0x4d` = `77`):

```
factory_round -> round_stage -> round_stats -> round_sink
                     │
                     └── subroutine 0 ← /tmp/beast-sha-sigma.json (top-1)
```

To use it, run `sha256-curriculum.json` first (until the sigma stage starts producing
non-trivial winners), then load `sha256-round-with-subroutines.json` alongside it.
Each evolution cycle, `round_stage` reloads the current top-1 entry of the sigma
ledger and re-mounts it. That means the round_stage's view of subroutine 0 *improves
over time* as the sigma stage discovers better candidates -- without any explicit
re-wiring or restart.

The wiring lives in the `subroutines` block of `round_stage.parameters`:

```json
"subroutines": [
  {
    "ledger_path": "/tmp/beast-sha-sigma.json",
    "top_k": 1,
    "input_arity": 1,
    "output_arity": 1,
    "max_steps_per_call": 800
  }
]
```

The order of entries in this list determines each subroutine's ID; the first entry is
ID 0, the second is ID 1, and so on (max library size is 255). At evaluation time
`EvaluatorPipe` reads the ledger, takes the highest-scoring `top_k` genomes, and
mounts them into the callee VM as `SubroutineEntry` objects. The genome itself is the
raw bytecode of the winning program -- the caller passes input variable indices, the
VM forks a fresh `VmSession` for the callee with the inputs copied into its
variables, runs the callee bytecode up to `max_steps_per_call` instructions, and
copies the callee's output variables back to the caller's output slots. **The callee
cannot itself call subroutines** (recursive bodies are rejected at mount time by
`subroutineBodyIsCallFree`); that constraint keeps step accounting bounded and avoids
runaway recursion.

A few constraints that are easy to miss when designing a subroutine source:

- **Arity matches what the producer pipeline wrote.** If the source stage wrote
  programs that read variable 0 and wrote variable 1, set `input_arity: 1` and
  `output_arity: 1` here. Misconfigured arity won't crash, but the caller will pass
  the wrong number of arguments and the callee will see garbage in unwritten input
  slots.
- **`max_steps_per_call` is the only upper bound** on callee execution within one
  call. Pick a value large enough for the callee to actually finish (the example uses
  800, which matches the producer's `max_steps_per_trial: 2000` with margin to spare
  for unaligned starts). Too low and the callee gets clipped mid-write; too high and
  the caller burns its own step budget on a single call.
- **Re-mount on every `execute()` cycle** is automatic. There's no need to restart
  the pipeline after the producer ledger updates -- `EvaluatorPipe::execute()`
  rebuilds its library at the top of every cycle.
- **Ledger must exist before the callee runs**, or the rebuild fails for that source
  and the slot is silently dropped (you'll see a warning in the server log). If you
  load `sha256-round-with-subroutines.json` before `sha256-curriculum.json` has
  produced `/tmp/beast-sha-sigma.json`, the round_stage will run *without* subroutine
  0 available, and the GA will gravitate toward `NoOp` whenever it picks the
  `CallSubroutine` opcode.

The intended composition pattern is: curriculum stages run *continuously* and keep
their ledgers fresh, downstream stages mount those ledgers and get progressively
stronger building blocks. Stage 5+ (message schedule, block compression, full hash)
will follow the same pattern, mounting Stage 4's round_sink as subroutine 0 to
sequence 64 rounds without re-evolving the round body from scratch.

## Roadmap toward the full SHA-256 hash

The plan for evolving a complete SHA-256 hasher decomposes the algorithm into stages
that match the natural reusability of its components. Each stage gets its own
evaluator and can be evolved on its own pipeline. The downstream stage composes the
upstream stage as a building block, either by seeding from its ledger via a
`ProgramStorageSourcePipe`, or by directly invoking the upstream genome via the
`CallSubroutine` opcode -- see `sha256-round-with-subroutines.json` for a worked
example and `docs/design/subroutine_opcode.md` for the full design notes.

| Stage | Evaluator(s) | Status | Notes |
|---|---|---|---|
| 0. Identity | `IdentityEvaluator` | shipped | "Copy N inputs to N outputs." Foundation skill. |
| 1a. Bitwise primitives | `BitwiseEvaluator` (xor/and/or/not) | shipped | One pipeline per op. |
| 1b. Rotation primitives | `RotateEvaluator` | shipped | One pipeline per amount used by SHA-256 (2, 6, 7, 11, 13, 17, 18, 19, 22, 25). |
| 2. Sigma compositions | `Sha256SigmaEvaluator` (big0/big1/small0/small1) | shipped | One pipeline per variant. Composes 1a+1b. |
| 3. Round body atoms | `Sha256ChEvaluator`, `Sha256MajEvaluator` | shipped | One pipeline each; composes 1a. |
| 4. Round | `Sha256RoundEvaluator` | shipped | Composes 2+3 plus a few adds and a state rotate. |
| 5. Message schedule | `Sha256ScheduleEvaluator` (planned) | future | `W[0..15] -> W[0..63]` via small sigmas. |
| 6. Block compression | `Sha256BlockEvaluator` (planned) | future | 64 rounds + schedule + working-state add. |
| 7. Padding | `Sha256PaddingEvaluator` (planned) | future | Trivial vs. the round; needs its own evaluator. |
| 8. End-to-end hash | `Sha256HashEvaluator` (planned) | future | Composes 6+7 with an iteration loop. |

All shipped stages can run side-by-side as independent pipelines feeding into shared
`ProgramStorageSinkPipe` ledgers; downstream stages mix those ledgers in via
`ProgramStorageSourcePipe` + `MultiplexerPipe` (the `sha256-curriculum.json` example
shows the wiring). The longer the curriculum runs, the richer the seed material the
final stages have available.

### Pitfalls when configuring a SHA-256 pipeline

A few foot-guns I've hit while running these pipelines that are easy to miss:

- **`variable_count` must be >= evaluator inputs + 1 + outputs**, otherwise the GA's
  mutator literally cannot pick variable indices for some outputs and the pipeline's
  score ceiling is artificially capped. For `Sha256RoundEvaluator`, that's >= 19;
  bump to 32 or 64 for scratch space. Same constraint applies to `memory_variables`
  on the host `EvaluatorPipe` -- the VM session has to have room for the outputs to
  live in.
- **`max_size` / `starting_program_size` matter more than you'd think.** A hand-written
  SHA-256 round in BEAST bytecode is ~30 operators × ~10 bytes ≈ 300 bytes; starting
  at 80 bytes means the GA has to grow the genome via insertion mutations before it
  can even hold the answer. The example files set `starting_program_size: 256` and
  up.
- **`opcode_weights: {}` (uniform) is roughly the worst default for SHA-256.** The
  random factory has ~70 opcodes; uniform sampling gives every opcode a 1/70 share,
  including jumps, prints, system calls, terminate, none of which help compute the
  round. Bias toward the opcodes the task actually needs and the population's
  exploration efficiency goes up by an order of magnitude.
