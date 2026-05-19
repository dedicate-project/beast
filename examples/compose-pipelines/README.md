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

**Memorisation knobs.** Each evaluation runs `trial_count` trials with different random
inputs, but the trial sequence is deterministic across evaluations (otherwise the GA
couldn't distinguish improvement from noise). That makes the trial set a fixed *target*
the candidate could in principle memorise as a lookup table instead of computing the
real round function. Two parameters defend against that:

- **`trial_count`** -- raise this to widen the target. The default is `8` for back-compat
  with older pipelines; bump it to `16`-`64` for serious runs. Each extra trial adds
  roughly 544 bits of answer the candidate would have to encode in its genome.
- **`round_constants_mode`** -- `"fixed"` (default, single K every trial), `"cycle_all"`
  (trial `t` uses `K[(round_constant_index + t) mod 64]`), or `"random_per_trial"`
  (K drawn from the same deterministic per-evaluation RNG that picks the input state).
  Non-`fixed` modes force the candidate to actually *read* the K input variable each
  trial, which biases the search toward the real round formula rather than hard-coding
  a single round constant.

For a memorisation-resistant baseline pick `trial_count = 32` with
`round_constants_mode = "cycle_all"` -- the search target then covers 32 distinct
`(state, K, W)` triples drawn from 32 of the 64 SHA-256 rounds, which is well beyond
what a 1024-byte genome can encode as a lookup table.

**Multi-round chaining (`rounds_per_trial`).** A second axis: by default each trial
runs *one* round transformation. Set `rounds_per_trial = R >= 2` to ask the candidate
to internally apply the round transformation `R` times to a single input, with the
same K and W, and write the *final* state. The reference applies the same R-round
transformation and only the final state is scored. The rounds-count is exposed in
variable 19, so the program can use it as a literal loop trip count. The step budget
scales with `R` (effective steps = `max_steps_per_trial * R`) so a genome that
actually loops the round body has room to do the work. R turns the search target
from "compute one transformation" into "compute one transformation AND wrap it in a
loop" -- a state-rotation-only program drops sharply from ~0.875 (R=1) toward ~0.5
as R grows, while a program that genuinely implements both the round formula and the
loop stays at 1.0 regardless of R. Use it to widen the fitness gap between
"implements the formula plus loop" and "exploits the rotation ceiling".

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

### `sha256-round-multi-k.json`

Same wiring as `sha256-round.json` but with the memorisation-resistant settings called
out above pre-baked: `trial_count = 32` and `round_constants_mode = "cycle_all"`. Use
this in preference to `sha256-round.json` when you actually want to *learn* the round
function instead of just measuring how far a small genome can stretch a lookup table.
Identical layout, scoring, and opcode bias -- only the evaluator parameters differ, so
A/B comparisons against the single-K baseline are clean.

### `sha256-staged-multi-round.json`

The recommended template for the multi-round (`rounds_per_trial >= 2`) experiment. The
naive "feed a random factory into a multi-round evaluator" wiring fails for two
compounding reasons:

1. **No gradient.** Multi-round mode requires the candidate program to *loop* the round
   body internally (it reads the trip count from variable 19). A 256-byte random program
   from `RandomProgramFactory` will never implement a loop; it runs once, writes the
   first round's intermediate state to the output slots, and is scored against the
   N-round reference -- which is almost-random bit soup compared to a one-round
   advancement. Every candidate scores ~0.5 (popcount-of-XOR averaged against a uniform
   reference), the GA sees no signal, and the population walks randomly.
2. **Per-cycle cost.** Multi-round multiplies the per-evaluation VM step budget by
   `rounds_per_trial`, which combined with high `trial_count` / `generations` /
   `populationSize` easily pushes one `evolve()` cycle into the multi-minute range. A
   pipeline that takes 20 minutes between visible outputs looks deadlocked even when
   it's just slow.

The staged template fixes both:

```
Stage A (cheap, single-round, fresh exploration):
factory_a -> eval_a_single_round -> stats_a -> fan_a -> sink_a_survivors
                                                            │
                                                            ▼   (file-based handoff)
Stage B (cheap, multi-round, seeded from Stage A's survivors):
source_b_seed -> eval_b_multi_round -> filter_b_above_random ─┬─> discard_b_noise
                                                              └─> stats_b -> graph_b -> fan_b -> sink_b_best
```

Key design choices:

- **Stage A and Stage B run in parallel within the same pipeline.** Stage A keeps
  pushing fresh single-round survivors into a ledger file (`top_k = 16`); Stage B reads
  that same ledger on every evaluation cycle, so its seed pool gets better over time
  without any explicit hand-off.
- **Stage B never sees a random program.** Its only `ProgramStorageSourcePipe` is Stage
  A's ledger -- if Stage A hasn't produced anything yet, Stage B simply waits.
- **Stage B is cheap by construction.** `trial_count = 4`, `generations = 10`,
  `max_steps_per_trial = 2000`, `max_candidates = 16`; that's roughly 30x faster per
  `evolve()` cycle than the "default everything" multi-round wiring, so cycles complete
  in seconds and you actually see whether the GA is making progress.
- **`FilterPipe` (threshold 0.55) drops noise.** Stage B's `cut_off_score = 0.55` keeps
  random-baseline candidates out of the output slot in the first place; the
  `filter_b_above_random` block is the second line of defence, routing anything still at
  the random-walk floor to `discard_b_noise` instead of letting it pollute the best-of
  ledger.
- **A `ScoreGraphPipe` (`graph_b`, 120-second window) sits in Stage B's promotion path**
  so you can watch the score climb in real time and tell apart "GA is climbing slowly"
  from "GA is genuinely stuck at the floor".

If you need to *visualise* the staged hand-off without restructuring the pipeline (e.g.
you want Stage A's survivors to be seen by Stage B *and* logged separately), wire a
`DemultiplexerPipe` between `fan_a` and `sink_a_survivors` with
`strategy = "least_loaded"` (see below) -- that way a slow logging sink can't back up
into Stage A.

### `DemultiplexerPipe` strategies, quick reference

- `round_robin` (default): strict back-pressure -- if the next output slot is full, the
  whole pipe stalls. Correct for symmetric downstream branches; **wrong** for
  asymmetric ones (the slow branch stalls the fast one).
- `broadcast`: copy each candidate to every output slot. Correct when every branch
  evaluates the same population against a different task.
- `least_loaded`: pick the output with the shortest queue, breaking ties round-robin.
  Correct when downstream branches have asymmetric throughput -- the slow branch's
  buffer fills up and the demux just stops sending to it, instead of stalling the whole
  upstream. Use this whenever one downstream evaluator is dramatically slower than the
  others (multi-round vs. single-round, large maze vs. small maze, etc.).

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

## Cognitive scaffolding: primitives, mazes, and the transfer-learning experiment

These three pipelines load and run together to ask a single research question:

> Does evolving a set of low-level algorithmic primitives first, then mounting them
> as immutable subroutines, accelerate evolution on a more complex downstream task
> -- even when the primitives and the downstream task come from different domains?

The honest answer at the time of writing: **we don't know yet**. The infrastructure to
run the experiment is what we just shipped; the experiment itself is something you
run yourself and watch unfold in the popovers. The pipelines below are sized so a
laptop can finish a meaningful run within a single evening.

### `primitives-gym.json`

Four parallel "primitive evolution" stages, each evolving one small,
self-contained algorithmic function with a clean numeric gradient. Each stage runs
independently and writes its top-K survivors to a JSON ledger that downstream
consumers will mount as a subroutine.

```
popcount_factory → popcount_stage → popcount_stats → popcount_sink  (/tmp/beast-popcount.json)
parity_factory   → parity_stage   → parity_stats   → parity_sink    (/tmp/beast-parity.json)
bitrev_factory   → bitrev_stage   → bitrev_stats   → bitrev_sink    (/tmp/beast-bitrev.json)
min4_factory     → min4_stage     → min4_stats     → min4_sink      (/tmp/beast-min4.json)
```

| Stage | Reference function | Difficulty | Scoring | Typical convergence |
|---|---|---|---|---|
| `popcount` | `output = popcount(input[0])` | Easy-medium (~8 ops) | Numeric distance, ceiling 32 | minutes to >0.8 |
| `parity` | `output = input[0] ^ ... ^ input[3]` | Trivial (~3 ops) | Bit-Hamming | seconds to >0.9 |
| `bitrev` | `output = reverse_bits(input[0])` | Hard (~15-25 ops) | Bit-Hamming | tens of minutes to ~0.8 |
| `min4` | `output = min(input[0..3])` | Medium (~6 ops with min opcode) | Log-scale numeric distance | minutes to >0.7 |

Each stage's `opcode_weights` biases the random factory toward opcodes that the
reference function actually needs (XOR for parity, shift+AND for popcount, the
dedicated `GetMinOfVariableAndVariable` opcode for min4, etc.). Without those
biases the GA's exploration budget gets wasted on jumps, prints, and system calls
that contribute nothing to any of the four tasks.

`parity` and `popcount` should be visibly converging within the first minute. If they
aren't, the GA stack itself has a problem -- they're your canary primitives.

### `maze-ladder-with-subroutines.json`

The within-domain transfer experiment: small mazes (6×6) feed a medium-maze stage
(12×12) via population seeding, which in turn feeds a large-maze stage (24×24) via
the same mechanism. The large stage also mounts `popcount` and `min4` from the
primitives gym as subroutines.

```
small_factory ───────────────→ small_maze   → small_stats   → small_sink   (/tmp/beast-maze-small.json)

medium_factory ─┐
                ├→ medium_mux → medium_maze → medium_stats → medium_sink  (/tmp/beast-maze-medium.json)
small_seed   ───┘   (loops the small-maze ledger back as seed material)

large_factory ─┐
                ├→ large_mux → large_maze   → large_stats → large_sink    (/tmp/beast-maze-large.json)
medium_seed  ──┘   (loops the medium-maze ledger back as seed material)
                    PLUS subroutines: popcount, min4
```

**Why seeding instead of subroutines for the maze→maze step?** The maze evaluator
runs the candidate as a *control loop*: read perception → decide a move → step the
maze → repeat. A "small-maze winner" is therefore a control loop, not a pure
function of inputs. The subroutine mechanism (v1) calls the callee with a fixed set
of input variables, runs it once, and reads back outputs -- which is the right shape
for the bit-twiddling primitives but the wrong shape for a maze-navigation control
loop. Seeding the population from the small-maze ledger via `ProgramStorageSourcePipe`
+ `MultiplexerPipe` is the right composition pattern for that case, and it's what
the existing curriculum-learning examples use.

`popcount` and `min4` are mounted as subroutines on the large stage because they
*are* pure functions and *could* plausibly help (popcount over a perception window
gives a "how cluttered is this direction" feature; min4 picks the smallest of four
distance hints). Whether the GA actually discovers a use for them is the
interesting question.

### `cognitive-scaffolding-ab.json`

The cross-domain transfer experiment, structured as a clean A/B comparison.

```
control_factory  → control_maze   → control_stats   → control_fan   → control_sink
                   (16×16 maze, NO subroutines)

treatment_factory → treatment_maze → treatment_stats → treatment_fan → treatment_sink
                   (16×16 maze, SAME config + 4 primitives mounted as subroutines)
```

Both branches run the identical `MazeEvaluator(16×16, difficulty=0.30)` with the
identical evolution-parameter shape. The only difference is the `subroutines` block
on the treatment branch, which mounts the top-1 winner from each of
`popcount`/`parity`/`min4`/`bitrev` as subroutine IDs 0..3, and biases the opcode
distribution toward `CallSubroutine` (`0x4d` = `77`, weight 3.5).

Watch the two `ResultsSummary` popovers side by side. If the cognitive-scaffolding
hypothesis is right, the treatment popover's best-ever score should pull ahead and
stay ahead. If the bit-twiddling primitives turn out to be irrelevant to maze
navigation (the honest null hypothesis), the two should track each other within noise.

### Recommended run order

1. **Load and start `primitives-gym.json`.** Watch `parity` and `popcount` converge
   first (seconds-minutes), then `min4` and `bitrev`. Once each stage has scored at
   least once at >0.7 the ledgers contain useful subroutine material.
2. **Load and start `cognitive-scaffolding-ab.json`.** This is the A/B experiment.
   Both branches now have the same starting conditions but the treatment branch
   has the primitives mounted as callable subroutines. Compare best-ever scores in
   the two popovers.
3. **(Optional) Load `maze-ladder-with-subroutines.json`** alongside the above for
   the within-domain curriculum demonstration.

All three pipelines coexist with each other and with the SHA-256 family -- they
write to disjoint ledger paths and don't share any pipes. The
`galibSerialisationMutex` releases during the evaluator callback, so multi-pipeline
throughput stays high even with seven or eight evolution stages running concurrently
(see `src/pipes/evolution_pipe.cpp` for the locking strategy).

### What success looks like

Honest expectations for what you'll see at convergence after, say, half an hour of
wall-clock with the default sizing:

- `parity`: ~0.95+ (essentially solved).
- `popcount`: ~0.80-0.90 (close to solved; the genome encodes the loop pattern).
- `min4`: ~0.75-0.90 (genome has discovered the `GetMinOfVariableAndVariable` opcode
  and uses it three times in sequence).
- `bitrev`: ~0.70-0.85 (genome has discovered shift+mask but probably not the full
  5-stage swap pattern; bit-Hamming gives it a noisy gradient regardless).
- `small_maze`: ~0.40-0.60 (small mazes are mostly luck-of-the-perception-window
  for the easy ones).
- `medium_maze`: ~0.15-0.35 (large gap from `small_maze`; the seeding helps but
  the evaluator's exponential-with-overshoot scoring is harsh).
- `large_maze`: ~0.05-0.20 (24×24 is properly hard; even a hand-written A\* would
  need to make every move count).
- `control_maze` vs. `treatment_maze`: this is the actual experiment. If they track
  each other within ±0.05, the primitives don't transfer. If `treatment_maze`
  pulls ahead by more than ~0.10 sustained for tens of cycles, the primitives are
  doing something real.

A negative result is still a valid experimental outcome. The cleanest thing the
infrastructure does is *make the question askable* -- swap in different primitives,
different downstream tasks, different opcode biases, and the A/B comparison
generalises to any "does priming with X help with Y" investigation.

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
