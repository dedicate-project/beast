#ifndef BEAST_CUDA_COMPILER_HPP_
#define BEAST_CUDA_COMPILER_HPP_

// Standard
#include <cstdint>
#include <limits>
#include <vector>

// Internal
#include <beast/cuda/instruction.hpp>

namespace beast::cuda {

/**
 * @brief Sentinel for "this source byte offset doesn't correspond to a known
 *        instruction boundary" in the byte_to_insn table.
 *
 * Exposed in the header so the kernel and the host compiler agree on the sentinel
 * without duplicating the literal.
 */
inline constexpr uint32_t kNoInstructionMapping = std::numeric_limits<uint32_t>::max();

/**
 * @brief One CallSubroutine argument pair (variable_index, follow_flag).
 *
 * Mirrors `Program::CallSubroutineArg` on the host side; copied verbatim into device
 * memory so the kernel can resolve caller-side input/output variables without
 * re-parsing the byte stream. Lives in `CompiledProgram::call_subroutine_args` as a
 * flat array indexed by `Instruction::aux` (see compiler.cpp for the encoding).
 */
struct CompiledCallArg {
  int32_t variable_index = 0;
  uint8_t follow_flag = 0;
  uint8_t pad0 = 0;
  uint16_t pad1 = 0;
};

static_assert(sizeof(CompiledCallArg) == 8,
              "CompiledCallArg must be 8 bytes for stable device-side indexing");

/**
 * @brief Host-side compilation product: a flat instruction stream ready for upload.
 *
 * Note that even an "untranslatable" BEAST program -- one made entirely of opcodes the
 * device VM doesn't implement -- still produces a `CompiledProgram` (every instruction
 * just compiles to `Op_NoOp`). The kernel then runs to completion and the evaluator
 * scores the all-zero outputs against the expected values. This is intentional: we
 * never refuse to evaluate a candidate just because the device doesn't speak every
 * opcode, because that would silently bias the GA against perfectly-valid CPU programs.
 */
struct CompiledProgram {
  std::vector<Instruction> instructions; ///< Decoded instruction stream, indexed by PC.
  uint32_t max_variable_index = 0;       ///< Highest variable index actually referenced.
                                         ///< Lets the device kernel know how much of the
                                         ///< per-thread register file to bother saving
                                         ///< back at trial end (though for SHA-256 the
                                         ///< I/O layout is fixed and this is unused).

  /// Source byte offset -> instruction index. Required by the device VM to resolve
  /// variable-target jumps at runtime: when the program executes
  /// `UnconditionalJumpToAbsoluteVariableAddress`, it reads `vars[op0]`, treats the
  /// value as a byte offset, looks it up here, and jumps to the corresponding
  /// instruction. `kNoInstructionMapping` for byte offsets that don't land on an
  /// instruction boundary -- the kernel folds those to NoOp.
  std::vector<uint32_t> byte_to_insn;

  /// Per-variable "is this an input?" / "is this an output?" bitmasks, used by
  /// `CheckIfVariableIs*` and `LoadInput/OutputCount` opcodes. Sized to
  /// `variable_count` bits each, packed into 64-bit words. The host evaluator sets
  /// these before each launch from its variable-behavior configuration.
  uint64_t is_input_mask = 0;
  uint64_t is_output_mask = 0;

  /// Flat operand pool for `CallSubroutine` instructions. Each `Op_CallSubroutine`
  /// instruction's `aux` field is an index into this array, pointing at the first
  /// of `(input_arity + output_arity)` `CompiledCallArg`s. Empty when the program
  /// doesn't use subroutines.
  std::vector<CompiledCallArg> call_subroutine_args;

  /// Set when the program contains at least one `LoadRandomValueIntoVariable`. The
  /// kernel allocates `curand_state` per thread only when this is true, so genomes
  /// that don't use random pay no register / occupancy cost for the feature.
  bool uses_random = false;

  /// Set when the program contains at least one `CallSubroutine`. Used to elide
  /// the call-frame allocation pass for the common case where no subroutines are
  /// used.
  bool uses_subroutines = false;
};

/**
 * @brief Compile one BEAST byte program into the device instruction stream.
 *
 * Walks `program_data` through `ProgramParser::parse`, decodes each parsed span into a
 * `cuda::Instruction`, then makes one back-patch pass to convert any constant-immediate
 * jump offsets into instruction-index targets.
 *
 * Translation policy:
 *   - Supported opcodes (arithmetic, bit ops, comparison, constant-target relative
 *     jumps, set/copy/swap) translate one-for-one to their `DeviceOpcode` counterpart.
 *   - Unsupported opcodes (printing, string tables, stacks, system calls, variable-
 *     target jumps, subroutine calls, dynamic variable declaration) compile to
 *     `Op_NoOp`. The PC still advances one instruction per source span so jump
 *     resolution stays correct.
 *   - Variable-target jumps cannot have their target known until runtime; the device
 *     VM doesn't implement runtime PC lookup tables, so these are also folded to NoOp.
 *
 * Variable index validation:
 *   - Any operand that names a variable is clamped to `[0, variable_count)`. An
 *     out-of-range index in the source program is replaced with index 0 (a benign
 *     register that's always present). Same clamping policy as the CPU VM.
 *
 * @param program_data Raw BEAST bytecode (typically a genome from the GA).
 * @param variable_count Total number of variables the device VM should allocate
 *                       registers for. Must match the value the SHA-256 evaluator
 *                       (or whatever consumer) expects.
 * @return The compiled program. Empty `instructions` vector indicates the source was
 *         empty or entirely unparseable.
 */
[[nodiscard]] CompiledProgram compileProgramForGpu(const std::vector<uint8_t>& program_data,
                                                   uint32_t variable_count);

} // namespace beast::cuda

#endif // BEAST_CUDA_COMPILER_HPP_
