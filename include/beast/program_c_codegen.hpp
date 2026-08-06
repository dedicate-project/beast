#ifndef BEAST_PROGRAM_C_CODEGEN_HPP_
#define BEAST_PROGRAM_C_CODEGEN_HPP_

// Standard
#include <cstdint>
#include <string>
#include <vector>

namespace beast {

/**
 * @class ProgramCCodeGenerator
 * @brief Emit a self-contained C source file that executes a BEAST genome
 *
 * The output is a single .c file the user can compile with a plain `cc program.c -o
 * program` and run as a standalone CLI tool. The file contains:
 *   - The genome bytecode embedded as a `static const unsigned char PROGRAM[]`.
 *   - A compact in-place BEAST VM in C that walks the bytecode and implements the
 *     common opcodes directly (math, bitwise, jumps, comparisons, stack, I/O probes,
 *     Terminate). Exotic opcodes (string table, system calls, subroutines) are
 *     stubbed -- they print a one-line warning to stderr and continue, so a program
 *     that uses them still runs (just without that opcode's side effects).
 *   - A `main()` that parses `--var IDX=VALUE` / `--var IDX:VALUE` CLI args to
 *     populate the variable space and then steps the VM up to a configurable budget,
 *     printing every variable's final value on exit.
 *
 * The user passes inputs as CLI args (so the same generated program can be re-run
 * with different inputs without recompiling) and reads outputs from the printed
 * variable dump. Which variables matter is evaluator-dependent and the generator
 * does NOT try to guess; documenting the variable layout is left to the caller.
 *
 * The generator is pure / stateless / thread-safe. All required information is
 * encoded in the inputs.
 */
class ProgramCCodeGenerator {
 public:
  struct Options {
    /// Variable count exposed to the program. Mirrors the VmSession's memory variable
    /// pool. 0 -> default (64), which is enough for every evaluator currently shipped.
    uint32_t memory_variables = 0;
    /// Stack capacity exposed to the program. 0 -> default (256).
    uint32_t stack_capacity = 0;
    /// Maximum number of VM steps the generated program executes before bailing out.
    /// 0 -> default (1,000,000). Prevents an infinite loop in an evolved program from
    /// running forever when the user just wanted to "see what it does".
    uint32_t max_steps = 0;
    /// A short header banner the generator inlines at the top of the C file, useful
    /// for the user to know where the genome came from (e.g. "from ledger /tmp/foo,
    /// score 0.886, 430 bytes"). Empty omits the banner.
    std::string source_description;
  };

  /// Generate a complete C source file. Caller owns the returned string.
  [[nodiscard]] static std::string generate(const std::vector<unsigned char>& bytecode,
                                             const Options& options);
};

} // namespace beast

#endif // BEAST_PROGRAM_C_CODEGEN_HPP_
