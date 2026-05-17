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

## Roadmap toward the full SHA-256 hash

The plan for evolving a complete SHA-256 hasher decomposes the algorithm into stages
that match the natural reusability of its components. Each stage gets its own
evaluator (`Sha256RoundEvaluator` is the first one in tree) and can be evolved on its
own pipeline. The downstream stage composes the upstream stage as a building block,
either by reusing the bytecode (when we add a `Subroutine` opcode) or by re-running
the genome inline today:

1. **One round** -- this pipeline. `(a..h, K, W) -> (a'..h')`.
2. **Message-schedule expansion** -- evolve `W[0..15] -> W[0..63]` via σ0/σ1 the same
   way the round above evolves the compression step. Smaller search space than a round
   because the transformation is more local.
3. **Block compression** -- given `H[0..7]` and a 64-byte message block, run 64 rounds
   plus the schedule and emit the new running hash. Once stages 1 and 2 exist this
   stage is mostly bookkeeping: rotate a shift-register of 8 words 64 times and add at
   the end.
4. **Padding** -- given a byte stream plus length, emit padded 64-byte blocks per
   FIPS 180-4 §5.1.1. Trivial compared to the round, but needs its own evaluator so
   the end-to-end stage doesn't have to also learn padding from scratch.
5. **End-to-end hash** -- given an arbitrary byte stream, emit the 256-bit digest.
   Composes 3+4 with an iteration loop over blocks.

All five stages can run side-by-side as independent pipelines feeding into a shared
`ProgramStorageSinkPipe` ledger; once a stage starts hitting high scores, its best
genomes get seeded into the next stage's pipeline via a `ProgramStorageSourcePipe`.
