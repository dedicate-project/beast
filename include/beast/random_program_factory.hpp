#ifndef BEAST_RANDOM_PROGRAM_FACTORY_HPP_
#define BEAST_RANDOM_PROGRAM_FACTORY_HPP_

// Standard
#include <cstdint>
#include <map>
#include <random>

// Internal
#include <beast/opcodes.hpp>
#include <beast/program_factory_base.hpp>

namespace beast {

/**
 * @brief Per-opcode sampling weights used when generating random programs
 *
 * Maps an `OpCode` to a relative weight. A weight of 0 excludes the opcode entirely;
 * positive weights bias the random sampling distribution proportionally. Opcodes that are
 * not present in the map fall back to a default weight of 1 (i.e. uniform), so a caller
 * can override just a handful of opcodes without enumerating the whole instruction set.
 *
 * Tasks typically supply a hand-tuned distribution to restrict the search space to
 * relevant instructions - e.g. a simple "forward input to output" task has no use for
 * jump or string-table opcodes, and biasing them to zero dramatically improves
 * convergence.
 */
using OpcodeWeights = std::map<OpCode, double>;

/**
 * @brief Minimal description of the subroutine library available to the factory
 *
 * Holds the `(input_arity, output_arity)` of every subroutine in the mounted
 * library, indexed by `subroutine_id`. The factory only needs the arities (not
 * the bytecode) to mint valid `CallSubroutine` instructions: arities determine
 * the operand byte layout, the bytecode determines what the call *does* at runtime
 * which is none of the factory's business.
 *
 * Empty table (the default) means "no library is mounted; `CallSubroutine` is
 * dropped from the random distribution even if weighted explicitly".
 */
using SubroutineArityTable = std::vector<std::pair<uint8_t, uint8_t>>;

/**
 * @class RandomProgramFactory
 * @brief Generates random programs with a valid structure
 *
 * This implementation of the abstract ProgramFactoryBase class can be used to generate Program
 * instances that have a random, albeit valid structure. Only valid operators are used in the
 * respective programs, the operators are aligned correctly, and variable indices, string table
 * indices, and string lengths are all within bounds of the passed in runtime environment
 * parameters.
 */
class RandomProgramFactory : public ProgramFactoryBase {
 public:
  /**
   * @brief Construct with a seeded Mersenne Twister
   *
   * The RNG is owned by the factory so we don't pay for `std::random_device` + Mersenne Twister
   * construction on every call to `generate` (it dominated factory cost in profiling).
   */
  RandomProgramFactory();

  /**
   * @brief Destructor added for vtable consistency
   */
  ~RandomProgramFactory() override = default;

  /**
   * @fn RandomProgramFactory::generate
   * @brief Generates a program consisting of random but valid operators and operands
   *
   * Uses a uniform distribution over all opcodes. Equivalent to calling the weighted overload
   * with an empty `OpcodeWeights` map (which falls back to uniform).
   */
  [[nodiscard]] Program generate(uint32_t size, uint32_t memory_size, uint32_t string_table_size,
                                 uint32_t string_table_item_length) override;

  /**
   * @brief Generates a random program biased by per-opcode weights
   *
   * Programs continue to be syntactically valid. The weights map is consulted on every
   * fragment-generation step to decide which opcode to emit next; entries with a weight of
   * 0 are never sampled. Opcodes missing from the map default to weight 1.
   *
   * @param size The maximum size of the program to generate, in bytes
   * @param memory_size The memory size the generated program would be executed with
   * @param string_table_size The string table size the generated program would be executed with
   * @param string_table_item_length The string table item length the generated program would be
   *        executed with
   * @param weights Per-opcode sampling weights (empty map = uniform)
   * @return A randomly generated, but valid program
   */
  [[nodiscard]] Program generate(uint32_t size, uint32_t memory_size, uint32_t string_table_size,
                                 uint32_t string_table_item_length, const OpcodeWeights& weights);

  /**
   * @brief Subroutine-aware overload of `generate`
   *
   * Same semantics as the weighted overload, but additionally makes a
   * `SubroutineArityTable` available to the factory. When the table is non-empty
   * AND `CallSubroutine` is not weighted to zero, the factory emits valid
   * `CallSubroutine` instructions (with in-bounds ids and correct operand byte
   * layouts). When the table is empty, the factory drops `CallSubroutine` from
   * its output even if the caller weighted it non-zero -- there's no library to
   * point at, so any call would be guaranteed-invalid bytecode.
   */
  [[nodiscard]] Program generate(uint32_t size, uint32_t memory_size, uint32_t string_table_size,
                                 uint32_t string_table_item_length, const OpcodeWeights& weights,
                                 const SubroutineArityTable& subroutines);

  /**
   * @brief Generates a single random operator, returned as raw bytecode
   *
   * Used by operator-aware mutation in `EvolutionPipe::execute()` (and any other caller that
   * needs to splice fresh operators into existing bytecode). Honors the same parameter ranges
   * as `generate()` so the produced byte sequence is always a valid, fully-formed operator.
   *
   * If the randomly chosen operator wouldn't fit within `max_bytes` (only possible for the
   * variable-length string operators when the payload is long), the helper retries up to a
   * small number of times before falling back to a `NoOp` instruction so the caller always
   * gets something valid back.
   *
   * Static because callers (GA mutators) don't own a factory instance and instead need a
   * standalone byte producer; the per-call cost of constructing the internal RNG is small
   * enough to be acceptable for the operator-mutation use case.
   */
  [[nodiscard]] static std::vector<unsigned char>
  generateRandomOperator(uint32_t memory_size, uint32_t string_table_size,
                         uint32_t string_table_item_length, uint32_t max_bytes = 64);

  /**
   * @brief Weighted variant of `generateRandomOperator`
   *
   * Same retry/fallback behavior as the uniform overload, but samples opcodes according to
   * the given weight map.
   */
  [[nodiscard]] static std::vector<unsigned char>
  generateRandomOperator(uint32_t memory_size, uint32_t string_table_size,
                         uint32_t string_table_item_length, uint32_t max_bytes,
                         const OpcodeWeights& weights);

  /**
   * @brief Subroutine-aware variant of `generateRandomOperator`
   *
   * Same retry/fallback behavior as the other overloads, plus the ability to
   * emit valid `CallSubroutine` instructions when `subroutines` is non-empty.
   */
  [[nodiscard]] static std::vector<unsigned char>
  generateRandomOperator(uint32_t memory_size, uint32_t string_table_size,
                         uint32_t string_table_item_length, uint32_t max_bytes,
                         const OpcodeWeights& weights,
                         const SubroutineArityTable& subroutines);

 private:
  std::mt19937 mersenne_engine_;
};

} // namespace beast

#endif // BEAST_RANDOM_PROGRAM_FACTORY_HPP_
