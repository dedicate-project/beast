#ifndef BEAST_PROGRAM_PARSER_HPP_
#define BEAST_PROGRAM_PARSER_HPP_

// Standard
#include <cstdint>
#include <vector>

// Internal
#include <beast/opcodes.hpp>

namespace beast {

/**
 * @class ProgramParser
 * @brief Stateless tokenizer that splits raw bytecode into operator-aligned spans
 *
 * The genetic algorithm needs to perform mutation and crossover at instruction boundaries
 * rather than at random byte offsets. Otherwise random byte flips almost always corrupt
 * subsequent operands and turn each generation into a sea of trapping programs.
 *
 * `ProgramParser::parse` walks a byte vector, decodes each `OpCode`, computes the operator's
 * byte length (including variable-length operands like inline strings), and emits an
 * `OperatorSpan` per recognized instruction. If the parser encounters bytes it cannot decode
 * (unknown opcode, truncated operand, etc.), it stops cleanly and records the remaining bytes
 * as `trailing_garbage_bytes`.
 *
 * The parser is "best effort": it never throws. Callers can use the `clean` flag to decide
 * whether to discard a genome, keep it for byte-level mutation only, or report it as
 * malformed in diagnostics.
 *
 * Length calculation is centralized here so adding new opcodes only requires updating one
 * switch statement (see `getOperatorLength`).
 */
class ProgramParser {
 public:
  /**
   * @brief A single decoded instruction within a byte stream
   */
  struct OperatorSpan {
    OpCode opcode;   ///< Decoded operator
    uint32_t offset; ///< Byte offset of the opcode byte within the parent stream
    uint32_t length; ///< Total length in bytes (opcode byte + all operands)
  };

  /**
   * @brief Output of `parse`
   */
  struct ParseResult {
    std::vector<OperatorSpan> spans;     ///< One entry per recognized operator
    uint32_t trailing_garbage_bytes = 0; ///< Bytes that could not be decoded (always at the
                                         ///  end of the stream by construction)
    bool clean = true;                   ///< True iff the entire stream parsed cleanly
  };

  /**
   * @brief Whether an opcode has a variable-length encoding (string payload)
   *
   * Currently only `SetStringTableEntry` and `SetVariableStringTableEntry` are variable
   * length; both have a 2-byte string length field followed by that many character bytes.
   */
  [[nodiscard]] static bool isVariableLengthOperator(OpCode opcode) noexcept;

  /**
   * @brief Returns the byte length of a fixed-length operator (opcode + all operands)
   *
   * For variable-length operators, returns the length of the *fixed prefix* (opcode + leading
   * operands up to and including the 2-byte length field). The caller must add the payload
   * length separately.
   *
   * Returns 0 for unknown opcodes; treat that as a parse failure.
   */
  [[nodiscard]] static uint32_t getOperatorLength(OpCode opcode) noexcept;

  /**
   * @brief Walk a byte stream and emit operator spans
   *
   * Never throws. The returned `ParseResult::clean` flag tells the caller whether every byte
   * was consumed; if not, `trailing_garbage_bytes` indicates how many bytes were left over.
   */
  [[nodiscard]] static ParseResult parse(const std::vector<unsigned char>& data) noexcept;
};

} // namespace beast

#endif // BEAST_PROGRAM_PARSER_HPP_
