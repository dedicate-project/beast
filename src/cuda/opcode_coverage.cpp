#include <beast/cuda/opcode_coverage.hpp>

namespace beast::cuda {

DeviceCoverage deviceCoverage(beast::OpCode opcode) noexcept {
  // Switch over the entire OpCode enum -- `-Wswitch` will scream the day someone
  // adds an opcode without recording its device decision here. The unit test in
  // `tests/cuda_opcode_coverage.cpp` enforces the same invariant at runtime as a
  // belt-and-braces second layer.
  //
  // When adding a new opcode: pick exactly ONE of:
  //   * DeviceImpl            -- if you ported it (commit the kernel + parity test)
  //   * DeviceNoOpEquivalent  -- if its CPU side-effect is not observable through
  //                              variable scoring (rare; PrintVariable / SysCall family)
  //   * DeviceFallbackToCpu   -- if it cannot be ported with bounded per-thread state
  //                              (extremely rare; today reserved for Link var follow-up)
  switch (opcode) {
    // ----- Misc -----------------------------------------------------------------------
    case beast::OpCode::NoOp:                            return DeviceCoverage::DeviceImpl;
    case beast::OpCode::LoadMemorySizeIntoVariable:      return DeviceCoverage::DeviceImpl;
    case beast::OpCode::LoadCurrentAddressIntoVariable:  return DeviceCoverage::DeviceImpl;
    case beast::OpCode::Terminate:                       return DeviceCoverage::DeviceImpl;
    case beast::OpCode::TerminateWithVariableReturnCode: return DeviceCoverage::DeviceImpl;
    // CPU `PerformSystemCall` writes to a panic log and (depending on syscall id) sets
    // exited-abnormally. The evaluator scores by reading output variables; the panic
    // log is host-only. The closest honest device semantic is a no-op -- same scoring
    // outcome by construction. Documented in README's GPU section.
    case beast::OpCode::PerformSystemCall:               return DeviceCoverage::DeviceNoOpEquivalent;
    case beast::OpCode::LoadRandomValueIntoVariable:     return DeviceCoverage::DeviceImpl;

    // ----- Variable management --------------------------------------------------------
    // Declare/Undeclare are managed by a per-thread 64-bit `declared_mask` in the
    // kernel. Access to an undeclared var is silently skipped (matches the CPU
    // evaluator's outer try/catch policy of swallowing variable-access throws).
    case beast::OpCode::DeclareVariable:                 return DeviceCoverage::DeviceImpl;
    case beast::OpCode::UndeclareVariable:               return DeviceCoverage::DeviceImpl;
    case beast::OpCode::SetVariable:                     return DeviceCoverage::DeviceImpl;
    case beast::OpCode::CopyVariable:                    return DeviceCoverage::DeviceImpl;
    case beast::OpCode::SwapVariables:                   return DeviceCoverage::DeviceImpl;

    // ----- Math -----------------------------------------------------------------------
    case beast::OpCode::AddConstantToVariable:           return DeviceCoverage::DeviceImpl;
    case beast::OpCode::AddVariableToVariable:           return DeviceCoverage::DeviceImpl;
    case beast::OpCode::SubtractConstantFromVariable:    return DeviceCoverage::DeviceImpl;
    case beast::OpCode::SubtractVariableFromVariable:    return DeviceCoverage::DeviceImpl;
    case beast::OpCode::CompareIfVariableGtConstant:     return DeviceCoverage::DeviceImpl;
    case beast::OpCode::CompareIfVariableLtConstant:     return DeviceCoverage::DeviceImpl;
    case beast::OpCode::CompareIfVariableEqConstant:     return DeviceCoverage::DeviceImpl;
    case beast::OpCode::CompareIfVariableGtVariable:     return DeviceCoverage::DeviceImpl;
    case beast::OpCode::CompareIfVariableLtVariable:     return DeviceCoverage::DeviceImpl;
    case beast::OpCode::CompareIfVariableEqVariable:     return DeviceCoverage::DeviceImpl;
    case beast::OpCode::GetMaxOfVariableAndConstant:     return DeviceCoverage::DeviceImpl;
    case beast::OpCode::GetMinOfVariableAndConstant:     return DeviceCoverage::DeviceImpl;
    case beast::OpCode::GetMaxOfVariableAndVariable:     return DeviceCoverage::DeviceImpl;
    case beast::OpCode::GetMinOfVariableAndVariable:     return DeviceCoverage::DeviceImpl;
    case beast::OpCode::ModuloVariableByConstant:        return DeviceCoverage::DeviceImpl;
    case beast::OpCode::ModuloVariableByVariable:        return DeviceCoverage::DeviceImpl;

    // ----- Bit manipulation -----------------------------------------------------------
    case beast::OpCode::BitShiftVariableLeft:            return DeviceCoverage::DeviceImpl;
    case beast::OpCode::BitShiftVariableRight:           return DeviceCoverage::DeviceImpl;
    case beast::OpCode::BitWiseInvertVariable:           return DeviceCoverage::DeviceImpl;
    case beast::OpCode::BitWiseAndTwoVariables:          return DeviceCoverage::DeviceImpl;
    case beast::OpCode::BitWiseOrTwoVariables:           return DeviceCoverage::DeviceImpl;
    case beast::OpCode::BitWiseXorTwoVariables:          return DeviceCoverage::DeviceImpl;
    case beast::OpCode::RotateVariableLeft:              return DeviceCoverage::DeviceImpl;
    case beast::OpCode::RotateVariableRight:             return DeviceCoverage::DeviceImpl;
    case beast::OpCode::VariableBitShiftVariableLeft:    return DeviceCoverage::DeviceImpl;
    case beast::OpCode::VariableBitShiftVariableRight:   return DeviceCoverage::DeviceImpl;
    case beast::OpCode::VariableRotateVariableLeft:      return DeviceCoverage::DeviceImpl;
    case beast::OpCode::VariableRotateVariableRight:     return DeviceCoverage::DeviceImpl;

    // ----- Jumps ----------------------------------------------------------------------
    // Constant-target jumps were always supported. The variable-target jumps now
    // resolve `vars[op0]` at runtime against the byte_to_insn table (uploaded to the
    // device alongside the program), and fold to NoOp if the byte target doesn't land
    // on a known instruction boundary. Adds divergence proportional to the program's
    // control flow, but no false scoring.
    case beast::OpCode::RelativeJumpToVariableAddressIfVariableGt0:
    case beast::OpCode::RelativeJumpToVariableAddressIfVariableLt0:
    case beast::OpCode::RelativeJumpToVariableAddressIfVariableEq0:
    case beast::OpCode::AbsoluteJumpToVariableAddressIfVariableGt0:
    case beast::OpCode::AbsoluteJumpToVariableAddressIfVariableLt0:
    case beast::OpCode::AbsoluteJumpToVariableAddressIfVariableEq0:
    case beast::OpCode::RelativeJumpIfVariableGt0:
    case beast::OpCode::RelativeJumpIfVariableLt0:
    case beast::OpCode::RelativeJumpIfVariableEq0:
    case beast::OpCode::AbsoluteJumpIfVariableGt0:
    case beast::OpCode::AbsoluteJumpIfVariableLt0:
    case beast::OpCode::AbsoluteJumpIfVariableEq0:
    case beast::OpCode::UnconditionalJumpToAbsoluteAddress:
    case beast::OpCode::UnconditionalJumpToAbsoluteVariableAddress:
    case beast::OpCode::UnconditionalJumpToRelativeAddress:
    case beast::OpCode::UnconditionalJumpToRelativeVariableAddress:
      return DeviceCoverage::DeviceImpl;

    // ----- I/O ------------------------------------------------------------------------
    case beast::OpCode::CheckIfVariableIsInput:          return DeviceCoverage::DeviceImpl;
    case beast::OpCode::CheckIfVariableIsOutput:         return DeviceCoverage::DeviceImpl;
    case beast::OpCode::LoadInputCountIntoVariable:      return DeviceCoverage::DeviceImpl;
    case beast::OpCode::LoadOutputCountIntoVariable:     return DeviceCoverage::DeviceImpl;
    case beast::OpCode::CheckIfInputWasSet:              return DeviceCoverage::DeviceImpl;

    // ----- Printing and string table --------------------------------------------------
    // Print* are NoOpEquivalent: scoring never reads the print buffer. The string-table
    // entries themselves ARE observable (LoadStringItemLength*, LoadStringItem* feed
    // values back into variables), so the string-table state itself is implemented
    // bit-exactly with bounded per-thread storage.
    case beast::OpCode::PrintVariable:                   return DeviceCoverage::DeviceNoOpEquivalent;
    case beast::OpCode::PrintStringFromStringTable:      return DeviceCoverage::DeviceNoOpEquivalent;
    case beast::OpCode::PrintVariableStringFromStringTable: return DeviceCoverage::DeviceNoOpEquivalent;
    case beast::OpCode::SetStringTableEntry:             return DeviceCoverage::DeviceImpl;
    case beast::OpCode::LoadStringTableLimitIntoVariable: return DeviceCoverage::DeviceImpl;
    case beast::OpCode::LoadStringTableItemLengthLimitIntoVariable: return DeviceCoverage::DeviceImpl;
    case beast::OpCode::SetVariableStringTableEntry:     return DeviceCoverage::DeviceImpl;
    case beast::OpCode::LoadVariableStringItemLengthIntoVariable: return DeviceCoverage::DeviceImpl;
    case beast::OpCode::LoadVariableStringItemIntoVariables: return DeviceCoverage::DeviceImpl;
    case beast::OpCode::LoadStringItemLengthIntoVariable: return DeviceCoverage::DeviceImpl;
    case beast::OpCode::LoadStringItemIntoVariables:     return DeviceCoverage::DeviceImpl;

    // ----- Stack ----------------------------------------------------------------------
    // BEAST stacks aren't real stack objects -- they're encoded inside the variable
    // file (`vars[base]` is the stack pointer, `vars[base+1..]` is the storage). The 5
    // stack opcodes compile to 2-4 ordinary variable reads/writes, all of which the
    // device VM already supports. No new per-thread state needed.
    case beast::OpCode::PushVariableOnStack:             return DeviceCoverage::DeviceImpl;
    case beast::OpCode::PushConstantOnStack:             return DeviceCoverage::DeviceImpl;
    case beast::OpCode::PopVariableFromStack:            return DeviceCoverage::DeviceImpl;
    case beast::OpCode::PopTopItemFromStack:             return DeviceCoverage::DeviceImpl;
    case beast::OpCode::CheckIfStackIsEmpty:             return DeviceCoverage::DeviceImpl;

    // ----- Subroutines ----------------------------------------------------------------
    // Per-thread callee scratch space + bounded call depth (kMaxCallDepth in the
    // kernel). The subroutine library is uploaded once per evaluate() call; arity
    // mismatches abort the trial bit-exactly with CPU.
    case beast::OpCode::CallSubroutine:                  return DeviceCoverage::DeviceImpl;

    // ----- Sentinel -------------------------------------------------------------------
    case beast::OpCode::Size:
      // Never a real opcode; this branch exists only to satisfy `-Wswitch`.
      return DeviceCoverage::DeviceFallbackToCpu;
  }
  // Unreachable for valid `OpCode` values; the switch above is exhaustive. Returning
  // a conservative fallback so a corrupted byte value still routes to CPU rather
  // than silently scoring wrong.
  return DeviceCoverage::DeviceFallbackToCpu;
}

} // namespace beast::cuda
