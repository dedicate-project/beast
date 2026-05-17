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
