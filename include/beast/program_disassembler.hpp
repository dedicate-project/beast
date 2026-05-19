#ifndef BEAST_PROGRAM_DISASSEMBLER_HPP_
#define BEAST_PROGRAM_DISASSEMBLER_HPP_

// Standard
#include <cstdint>
#include <string>
#include <vector>

// Internal
#include <beast/opcodes.hpp>

namespace beast {

/**
 * @class ProgramDisassembler
 * @brief Render a BEAST bytecode sequence into a human-readable operator list
 *
 * Used by the Program Collection web UI to show the user what's inside a stored
 * candidate genome. Wraps `ProgramParser` to walk the bytes and decodes each
 * operator's operands so the UI can render them inline rather than just showing
 * raw hex.
 *
 * The disassembler never throws; malformed bytes produce a `trailing_garbage_bytes`
 * count instead of an exception. Same best-effort contract as `ProgramParser`.
 */
class ProgramDisassembler {
 public:
  /// One decoded operator. Operands are stored as raw decoded integers (signed 64-bit
  /// to fit any int32 / int8 / int16 value the bytecode produces); the human-readable
  /// rendering is the `text` field, which the UI displays verbatim.
  struct Instruction {
    OpCode opcode = OpCode::NoOp;
    std::string mnemonic; ///< e.g. "SetVariable"
    uint32_t offset = 0;  ///< Byte offset of the opcode byte within the parent stream
    uint32_t length = 0;  ///< Total byte length (opcode + all operands)
    std::vector<int64_t> operands;
    std::string bytes_hex; ///< Hex representation of every byte in the span (lowercase, no separator)
    std::string text;      ///< Pretty-printed mnemonic + operands (e.g. "SetVariable v=0 value=42")
  };

  struct Result {
    std::vector<Instruction> instructions;
    uint32_t trailing_garbage_bytes = 0;
    bool clean = true;
  };

  /// Walk the byte stream and emit a `Result`. Always returns a value -- caller
  /// inspects `clean` / `trailing_garbage_bytes` to detect malformed input.
  [[nodiscard]] static Result disassemble(const std::vector<unsigned char>& bytecode) noexcept;

  /// Canonical name string for an opcode. Returns `"opcode_0xNN"` for unrecognised
  /// values so the UI never has to render an empty cell.
  [[nodiscard]] static std::string mnemonicFor(OpCode opcode);
};

} // namespace beast

#endif // BEAST_PROGRAM_DISASSEMBLER_HPP_
