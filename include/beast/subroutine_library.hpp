#ifndef BEAST_SUBROUTINE_LIBRARY_HPP_
#define BEAST_SUBROUTINE_LIBRARY_HPP_

// Standard
#include <cstdint>
#include <vector>

namespace beast {

/**
 * @brief Hard upper bound on the input or output arity of a subroutine
 *
 * The opcode encoding stores `input_arity` and `output_arity` as `uint8`s but we cap
 * them more aggressively to keep the worst-case `CallSubroutine` instruction size
 * predictable and bound the per-call setup/teardown cost. A SHA-256 round (8 in / 8
 * out) hits this cap exactly, which is the largest natural subroutine we ship today.
 *
 * Raising this is a one-line change but warrants a deliberate review of the GA-side
 * impact (longer instructions = bigger mutation steps, slower convergence per byte).
 */
inline constexpr uint8_t kMaxSubroutineArity = 8;

/**
 * @brief Hard upper bound on the number of subroutines a single library can hold
 *
 * Bound by the wire format: `subroutine_id` is a `uint8`. Setting this to 255 (not
 * 256) reserves the all-ones byte as a sentinel that the parser can use for
 * "invalid id" diagnostics without colliding with a legitimate slot.
 */
inline constexpr uint16_t kMaxSubroutineLibrarySize = 255;

/**
 * @struct SubroutineEntry
 * @brief A single immutable subroutine mounted in a library
 *
 * The bytecode is the same wire format as a regular `Program` (i.e. it would parse
 * cleanly with `ProgramParser`), with the additional constraint that it must not
 * itself contain a `CallSubroutine` opcode -- recursion is disabled in v1.
 *
 * Variable layout when the callee runs:
 *   - Var 0 .. input_arity-1                       : inputs (set per-call by VM)
 *   - Var input_arity                              : trial-id slot (matches
 *                                                    BitDistanceEvaluator), set to 0
 *   - Var input_arity+1 .. input_arity+output_arity: outputs (read after call)
 *
 * `max_steps_per_call` bounds the VM execution time for a single invocation of this
 * subroutine. It is charged against the caller's outer step budget but capped here
 * so a buggy callee cannot exhaust the caller's budget in a single call.
 */
struct SubroutineEntry {
  std::vector<unsigned char> bytecode; ///< Subroutine bytecode in `Program` wire format
  uint8_t input_arity = 0;             ///< Number of input words the subroutine consumes
  uint8_t output_arity = 0;            ///< Number of output words it produces
  uint32_t max_steps_per_call = 800;   ///< Hard step cap per invocation
};

/**
 * @brief A flat list of subroutines indexed by their `subroutine_id` byte
 *
 * The library is intentionally a plain `std::vector` -- entries are addressed by
 * stable, dense, zero-based ids that match the `subroutine_id` operand in the
 * `CallSubroutine` opcode. Mounting/un-mounting at runtime is not supported; the
 * library is built once per evaluation cycle and is immutable for the lifetime of
 * every `VmSession` that consumes it.
 */
using SubroutineLibrary = std::vector<SubroutineEntry>;

/**
 * @brief True iff `bytecode` contains no `CallSubroutine` opcode anywhere
 *
 * Used by `SubroutineLibrary` builders to enforce the v1 "no recursion" rule. The
 * walk is byte-aligned via `ProgramParser` -- a `0x4d` byte that happens to fall in
 * the middle of another opcode's operand bytes is *not* a `CallSubroutine`, so we
 * need a real parse rather than a naive byte search.
 *
 * Returns true for empty bytecode (vacuously safe).
 */
[[nodiscard]] bool subroutineBodyIsCallFree(const std::vector<unsigned char>& bytecode) noexcept;

} // namespace beast

#endif // BEAST_SUBROUTINE_LIBRARY_HPP_
