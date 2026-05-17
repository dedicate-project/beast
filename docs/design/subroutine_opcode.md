# Design: Subroutine opcode

Status: **Proposed**.
Owner: open.
Tracking branch: TBD.

## Problem

Every evolved BEAST program today is monolithic. A program that needs to compute
`Sha256RoundEvaluator` from scratch has to *also* contain working implementations of
`BigSigma0`, `BigSigma1`, `Ch`, `Maj`, three or four `rotr` patterns, a handful of
32-bit adds, and the 8-way state shuffle -- all rediscovered inside one genome. Even
with a perfectly biased opcode distribution and a giant `max_genome_bytes`, the
combinatorial cost of evolving all those skills *simultaneously* dwarfs the cost of
evolving them in sequence and composing them.

The SHA-256 curriculum (`IdentityEvaluator` → `RotateEvaluator` → `Sha256SigmaEvaluator`
→ `Sha256ChEvaluator` / `Sha256MajEvaluator` → `Sha256RoundEvaluator`) gives us a
gradient that lets each stage be solved on its own. What it does **not** give us is a
way for the round evaluator to *call* the already-solved sigma genome instead of
re-deriving it. Today the only way for an upstream stage to influence a downstream one
is through `ProgramStorageSink` → `ProgramStorageSource` seeding, which only carries
the survivors' bytecode in as raw starting material -- nothing prevents downstream
evolution from immediately mutating that bytecode beyond recognition.

A **Subroutine opcode** would change that: the downstream evaluator pre-loads its
ancestor stages' top-K genomes as named, immutable subroutines, and the evolving
program is given a single opcode that invokes one of them with arbitrary input and
output variable indices. The GA only has to learn the *wiring*: which subroutines to
call, in what order, on which variables.

This document scopes that opcode end-to-end so we know what we're signing up for
before any code is written.

## Goals

1. **Bytecode reuse across pipeline stages.** A downstream `EvaluatorPipe` should be
   able to mount survivors of an upstream stage as immutable subroutines that the
   evolving population can call.
2. **Negligible cost to existing programs.** The opcode must be optional -- programs
   that don't use it must keep working unchanged. No mandatory header/footer, no
   forced session-level configuration if no subroutines are mounted.
3. **GA-safe encoding.** The mutator must understand the new opcode's byte length
   exactly the same way it understands the existing ones; subroutine ID operands
   must be bounded so a random mutation can't deref a non-existent subroutine.
4. **Bounded execution cost.** A call must consume a predictable share of the
   caller's step budget so a maliciously-evolved program can't trap the evaluator
   in an unkillable loop.
5. **No regressions.** All existing tests must still pass. The new opcode lands
   behind a feature-flag (default ON) so the rollout can be reverted cleanly.

## Non-goals (for the first cut)

- Self-modifying subroutines (mutating subroutine bytecode at runtime).
- Recursion. A subroutine cannot call another subroutine; calls are flat.
- Variadic argument lists. Every subroutine has a fixed input arity and a fixed
  output arity declared at registration time.
- Cross-evaluator sharing. The subroutine library lives on the `EvaluatorPipe`; two
  pipes in the same pipeline have independent libraries. (We could lift this later
  via a shared `SubroutineLibrary` pipe; not in v1.)

## Proposed encoding

New opcode in `include/beast/opcodes.hpp`:

```
CallSubroutine = 0x4d, ///< Call a registered subroutine with mapped I/O variables
```

`OpCode::Size` shifts from `0x4d` to `0x4e`. (Note: this is technically a wire-format
change for any pre-existing pipeline JSON that uses `opcode_weights["77"]` -- previously
out-of-range and silently dropped, now refers to `CallSubroutine`. We accept this; the
weights map's silent-drop behaviour already documents that out-of-range keys are
ignored, so existing files just gain access to a new opcode rather than breaking.)

Instruction layout (variable byte length, to keep arity flexible):

```
byte 0       : 0x4d                                                      (opcode)
byte 1       : subroutine_id           (uint8, [0, library_size))        (which routine)
byte 2       : input_arity             (uint8, [0, kMaxArity])           (how many inputs)
byte 3..N    : input variable indices  (int16 each, follow_links bit 15) (caller-side)
byte N+1     : output_arity            (uint8, [0, kMaxArity])           (how many outputs)
byte N+2..M  : output variable indices (int16 each, follow_links bit 15) (caller-side)
```

Total length: `4 + 2*(input_arity + output_arity)`. Max arity is hardcoded at 8 to keep
the worst-case instruction at 36 bytes, comparable to other multi-operand opcodes; the
SHA-256 round's biggest building block (the round itself, 8 in / 8 out) hits this cap
exactly. If a downstream stage ever needs >8, this is the knob to raise.

The mutator (`bytesToGenome` / span parser) needs to learn this variable-length
encoding. The simplest implementation is a tiny "encoded-length-from-bytes" helper
called from the span parser, parameterised on the opcode -- which is the same shape we
already have for fixed-length opcodes; we just extend it to read `input_arity` and
`output_arity` from the bytes themselves.

## Calling convention

When the VM dispatches a `CallSubroutine`:

1. Resolve subroutine bytecode + declared I/O arity by id from the session-attached
   library. If `subroutine_id >= library.size()` OR the declared arity doesn't match
   the instruction's arity, **throw `OutOfBoundsCall`**. The VM catches and marks the
   session as `exited_abnormally`; the evaluator scores 0 for that trial.
2. Allocate a fresh `VmSession` for the subroutine (the *callee* session), using:
   - The subroutine's bytecode.
   - The same `variable_count` / `string_table_count` / `max_string_size` as the
     caller. (Keeps the address space familiar to programs reused across stages.)
3. Copy `input_arity` words from the caller's input variable indices into the
   callee's vars `0 .. input_arity - 1`, then mark those as `VariableIoBehavior::Input`.
4. Mark the callee's vars `input_arity + 1 .. input_arity + output_arity` as
   `VariableIoBehavior::Output`. (The slot at `input_arity` is the trial-id
   convention from `BitDistanceEvaluator`; the callee can ignore it, but we leave the
   layout consistent so the same subroutine genome works for both standalone
   evaluation and subroutine invocation.)
5. Run the callee VM up to `kMaxStepsPerCall` steps OR until all output variables
   have new data available, whichever comes first.
6. Copy the callee's output variables back into the caller's output variable indices.
   If the callee timed out without writing all outputs, the unwritten slots end up as
   the default value (0) in the caller -- not a hard failure, but the partial-credit
   scoring will reflect the bad result.
7. Charge `actual_callee_steps` against the caller's step budget. The caller's outer
   step counter (the one the evaluator decrements per-step in its main loop) is the
   single source of truth for "did this evaluation time out", so a runaway callee
   can't extend its own life by hiding inside a `CallSubroutine`.

### Step budget accounting

`kMaxStepsPerCall` is hardcoded at first (say, `caller_step_budget / 4`) to keep
behaviour bounded. A future iteration could expose it via the `CallSubroutine`
operand, but a fixed bound keeps the GA's search space smaller and avoids the
"evolve a bigger budget" exploit path.

### Recursion

Disallowed in v1. The subroutine library at registration time is statically checked
to not contain a `CallSubroutine` opcode anywhere in its bytecode -- if it does, that
subroutine fails to register and the host pipe surfaces the error in its config-load
path. This is enforced by walking the bytecode at library-build time using the
existing program parser (the parser already understands every opcode's byte length;
we just look for `0x4d` and reject).

This restriction can be lifted later by adding a per-VM call stack with a small
recursion-depth cap (e.g., 4). The cap matters: without it, mutual recursion between
two subroutines combined with non-terminating call patterns deadlocks the evaluator.

## Subroutine library

A `SubroutineLibrary` lives on the `EvaluatorPipe` (or on the host pipe in general --
`ProgramFactoryPipe` would also benefit from it once we want random programs to
already-be-calling subroutines):

```cpp
struct SubroutineEntry {
  std::vector<unsigned char> bytecode;
  uint8_t  input_arity;
  uint8_t  output_arity;
  uint32_t max_steps_per_call;
};

using SubroutineLibrary = std::vector<SubroutineEntry>;
```

The library is **immutable** for the lifetime of a single evaluation. It's installed
on every `VmSession` the evaluator constructs via a new `VmSession::setSubroutineLibrary(...)`
method called from the evaluator's `evaluate()` setup. From the VM dispatch side, the
session exposes `getSubroutine(id) -> const SubroutineEntry&` to support `CallSubroutine`.

### Library construction

The new `EvaluatorPipe` constructor accepts a list of `SubroutineSource`s in JSON:

```json
"subroutines": [
  {
    "ledger_path": "/tmp/beast-sha-sigma.json",
    "top_k": 1,
    "input_arity": 1,
    "output_arity": 1,
    "max_steps_per_call": 800
  },
  {
    "ledger_path": "/tmp/beast-sha-ch.json",
    "top_k": 1,
    "input_arity": 3,
    "output_arity": 1,
    "max_steps_per_call": 600
  }
]
```

Each source loads the top-K genomes from a `ProgramStorageSinkPipe`-format ledger and
mounts them as the next N subroutines (so the order in the array determines the IDs).
`top_k` defaults to 1 in v1 -- "use the best genome from this ledger". Loading more
than one per source is a quick follow-up that gives the GA a choice ("call any of
these three competing sigma implementations"), at the cost of slot churn in the ID
space.

## Random program factory integration

`RandomProgramFactory` needs to know that `CallSubroutine` is variable-length and that
its `subroutine_id` operand has to be in `[0, library_size)` for the program to be
valid. The constructor gains a `library_size` parameter (default 0 = no subroutines,
in which case the factory never emits `CallSubroutine`). When `library_size > 0` AND
`opcode_weights` doesn't explicitly suppress `CallSubroutine`, the factory emits it
at the default uniform rate; the user can bias for / against via the same
`opcode_weights` mechanism as every other opcode.

The factory needs the per-subroutine arity to generate valid operands. The simplest
fix is to make the `ProgramFactoryPipe` accept the same `subroutines` JSON shape as
the evaluator pipe, build a parallel `SubroutineLibrary`, and pass arities through to
the factory.

## Mutation safety

`evolution_pipe.cpp`'s span-based mutator currently classifies bytes by opcode and
treats each operator as a single mutation unit. Two changes:

1. The "what's the next operator's length?" helper learns the variable-length encoding
   for `CallSubroutine` (reads `input_arity` and `output_arity` from the bytes
   immediately after the opcode).
2. The "insert a fresh operator" path, when picking `CallSubroutine`, must pick a
   `subroutine_id` from the in-scope library and use that subroutine's declared arity
   to size the operand bytes correctly.

There's no way for byte-level mutation to land *inside* the `subroutine_id` field and
produce an out-of-bounds ID without the span parser noticing -- because the parser
detects an invalid `CallSubroutine` (`subroutine_id >= library_size`) and treats the
whole span as garbage, the next mutation pass cleans it up the same way it cleans up
any other garbled byte sequence. The runtime `OutOfBoundsCall` throw is the
belt-and-suspenders fallback for the case where the parser has been bypassed
(direct genome-bytes injection, ledger-loaded survivors from before the schema change,
etc.).

## Implementation phases

The work splits cleanly into 4 PRs that can land independently:

### Phase 1: VM-side plumbing (no behaviour change yet)

- Add `OpCode::CallSubroutine = 0x4d`; bump `OpCode::Size` to `0x4e`.
- Add `Program::callSubroutine(...)` byte-encoder.
- Add `SubroutineEntry`, `SubroutineLibrary`, `VmSession::setSubroutineLibrary(...)`,
  `VmSession::getSubroutine(...)`.
- Add `CpuVirtualMachine` dispatch case for `CallSubroutine` that throws
  `OutOfBoundsCall` (no library mounted yet, so all calls fail). Pure dead-code in
  practice.
- Tests: encode/decode round-trip, throws-when-no-library, throws-on-id-overflow,
  throws-on-arity-mismatch.

**Out**: no `EvaluatorPipe` integration, no `ProgramFactory` integration, no GA
mutator integration. The opcode exists but nobody emits it.

### Phase 2: Mutator + factory

- `RandomProgramFactory` accepts a `SubroutineLibrary` reference and learns the
  variable-length encoding.
- `evolution_pipe.cpp` span parser learns the variable-length encoding.
- Tests: random factory with a 2-entry library emits valid calls; mutator's
  insertion / replacement paths produce in-bounds IDs; a deliberately-corrupted
  `CallSubroutine` span gets cleaned up by the next mutation pass.

**Out**: still no `EvaluatorPipe` JSON shape; the library is hand-constructed in C++
for tests.

### Phase 3: EvaluatorPipe integration

- `EvaluatorPipe` accepts a `subroutines` JSON array; `PipelineManager`
  construct/deconstruct round-trips it.
- `ProgramStorageSinkPipe` ledgers already store full bytecode; we just read it.
- `EvaluatorPipe::execute()` builds the library once per cycle and attaches it to
  each per-genome `VmSession`.
- UI: `PipeTypes.js` gets a `subroutines` form field on `EvaluatorPipe`. Probably
  rendered as an editable table of `{ledger_path, top_k, input_arity, output_arity,
  max_steps_per_call}` rows.
- Tests: end-to-end pipeline with two stages where stage 2 mounts stage 1's
  ledger and the score gradient is measurably better than without the library.

### Phase 4: Polish + curriculum integration

- Update `sha256-curriculum.json` example to mount the upstream survivors as
  subroutines.
- Cookbook section in `examples/compose-pipelines/README.md` showing the wiring.
- Optional: lift the "no recursion" rule to "recursion-depth ≤ 4" if there's a
  compelling use case.

## Open questions

1. **Subroutine I/O variable layout vs. evaluator I/O variable layout.** Today
   `BitDistanceEvaluator` puts inputs at `0..N-1`, trial_id at `N`, outputs at
   `N+1..N+M`. The callee VM session uses the same convention so a subroutine
   genome works in both modes. *Is this the right layout for subroutines, or
   should subroutines use a simpler `inputs at 0..N-1, outputs at N..N+M-1`
   layout without the trial-id slot?* Conservative choice: keep the layout
   consistent, the cost is one wasted variable.

2. **Should `Subroutine` calls cost extra in fitness scoring?** Some GP literature
   uses a "calls are expensive" penalty so the GA prefers compact programs that
   inline simple operations and only call out for complex ones. Default v1
   behaviour is no penalty; revisit if we see pathological "wrap everything in a
   subroutine call" populations.

3. **Top-K vs. top-1 per ledger.** v1 ships top-1; bumping to top-K means the
   subroutine ID encoding has to use more than 8 bits for libraries with >256
   slots. Probably never an issue in practice, but worth noting before someone
   tries to mount the whole top-1000 of a ledger.

4. **Cross-evaluator library sharing.** Currently each `EvaluatorPipe` owns its
   library. The natural next abstraction is a "Library Pipe" that hosts a shared
   library and broadcasts it to downstream pipes. Out of scope for v1; the
   single-pipe model is plenty for the SHA-256 curriculum.

## Effort estimate

Rough sizing, in commit-sized chunks:

| Phase | Files touched | LoC delta (incl tests) | Risk |
|---|---|---|---|
| 1 | ~6 | ~400 | Low; pure new code, no existing tests change. |
| 2 | ~4 | ~300 | Medium; mutator changes need careful coverage. |
| 3 | ~6 + UI | ~600 | Medium; JSON schema + UI form bring scope. |
| 4 | ~3 (docs + example) | ~150 | Low. |
| **Total** | **~19** | **~1500** | |

The whole feature is ~1500 lines of code and 4 weeks of part-time work. Each phase
ships independently and provides incremental value: Phase 1 is dead code but unlocks
the encoding test; Phase 2 lets the factory emit `CallSubroutine` against a fixture
library; Phase 3 makes it useful end-to-end; Phase 4 lights up the curriculum.
