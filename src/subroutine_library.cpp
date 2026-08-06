#include <beast/subroutine_library.hpp>

// Standard
#include <algorithm>

// Internal
#include <beast/opcodes.hpp>
#include <beast/program_parser.hpp>

namespace beast {

bool subroutineBodyIsCallFree(const std::vector<unsigned char>& bytecode) noexcept {
  if (bytecode.empty()) {
    return true;
  }
  const auto result = ProgramParser::parse(bytecode);
  return std::all_of(result.spans.begin(), result.spans.end(),
                     [](const ProgramParser::OperatorSpan& span) {
                       return span.opcode != OpCode::CallSubroutine;
                     });
}

} // namespace beast
