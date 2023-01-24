#include <beast/random_program_factory.hpp>

// Standard
#include <random>

namespace beast {

Program RandomProgramFactory::generate(
    uint32_t size, uint32_t memory_size, uint32_t /*string_table_size*/,
    uint32_t /*string_table_item_length*/) {
  Program prg(size);

  std::random_device random_device;
  std::mt19937 mersenne_engine{random_device()};
  std::uniform_int_distribution<int32_t> code_distribution(
      0, static_cast<int32_t>(OpCode::Size) - 1);
  std::uniform_int_distribution<int32_t> var_distribution(0, memory_size - 1);
  std::uniform_int_distribution<int32_t> bool_distribution(0, 1);
  std::uniform_int_distribution<int32_t> int32_distribution(
      std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max());
  std::uniform_int_distribution<int32_t> int8_distribution(
      std::numeric_limits<int8_t>::min(), std::numeric_limits<int8_t>::max());
  std::uniform_int_distribution<int32_t> rel_addr_distribution(
      -static_cast<int32_t>(static_cast<double>(size) * 0.5),
      static_cast<int32_t>(static_cast<double>(size) * 0.5));
  std::uniform_int_distribution<int32_t> abs_addr_distribution(0, size);

  // Random true/false switch.
  const auto bool_generator =
      [&bool_distribution, &mersenne_engine]() {
        return bool_distribution(mersenne_engine) == 1;
      };
  // Select variable indices only from the range that actually fits into variable memory.
  const auto var_generator =
      [&var_distribution, &mersenne_engine]() {
        return var_distribution(mersenne_engine);
      };
  const auto int32_generator =
      [&int32_distribution, &mersenne_engine]() {
        return int32_distribution(mersenne_engine);
      };
  const auto int8_generator =
      [&int8_distribution, &mersenne_engine]() {
        return int8_distribution(mersenne_engine);
      };
  const auto rel_addr_generator =
      [&rel_addr_distribution, &mersenne_engine]() {
        return rel_addr_distribution(mersenne_engine);
      };
  const auto abs_addr_generator =
      [&abs_addr_distribution, &mersenne_engine]() {
        return abs_addr_distribution(mersenne_engine);
      };

  while (true) {
    Program fragment;

    const auto code = static_cast<OpCode>(code_distribution(mersenne_engine));
    switch (code) {
    case OpCode::NoOp: {
      fragment.noop();
    } break;

    case OpCode::LoadMemorySizeIntoVariable: {
      fragment.loadMemorySizeIntoVariable(var_generator(), bool_generator());
    } break;

    case OpCode::LoadCurrentAddressIntoVariable: {
      fragment.loadCurrentAddressIntoVariable(var_generator(), bool_generator());
    } break;

    case OpCode::Terminate: {
      std::uniform_int_distribution<int32_t> return_code_distribution{-127, 128};
      fragment.terminate(return_code_distribution(mersenne_engine));
    } break;

    case OpCode::TerminateWithVariableReturnCode: {
      fragment.terminateWithVariableReturnCode(var_generator(), bool_generator());
    } break;

    case OpCode::PerformSystemCall: {
      fragment.performSystemCall(
          int8_generator(), int8_generator(), var_generator(), bool_generator());
    } break;

    case OpCode::LoadRandomValueIntoVariable: {
      fragment.loadRandomValueIntoVariable(var_generator(), bool_generator());
    } break;

    case OpCode::DeclareVariable: {
      fragment.declareVariable(
          var_generator(),
          bool_generator() ? Program::VariableType::Int32 : Program::VariableType::Link);
    } break;

    case OpCode::SetVariable: {
      fragment.setVariable(var_generator(), int32_generator(), bool_generator());
    } break;

    case OpCode::UndeclareVariable: {
      fragment.undeclareVariable(var_generator());
    } break;

    case OpCode::CopyVariable: {
      fragment.copyVariable(var_generator(), bool_generator(), var_generator(), bool_generator());
    } break;

    case OpCode::SwapVariables: {
      fragment.swapVariables(var_generator(), bool_generator(), var_generator(), bool_generator());
    } break;

    case OpCode::AddConstantToVariable: {
      fragment.addConstantToVariable(var_generator(), int32_generator(), bool_generator());
    } break;

    case OpCode::AddVariableToVariable: {
      fragment.addVariableToVariable(
          var_generator(), bool_generator(), var_generator(), bool_generator());
    } break;

    case OpCode::SubtractConstantFromVariable: {
      fragment.subtractConstantFromVariable(var_generator(), int32_generator(), bool_generator());
    } break;

    case OpCode::SubtractVariableFromVariable: {
      fragment.subtractVariableFromVariable(
          var_generator(), bool_generator(), var_generator(), bool_generator());
    } break;

    case OpCode::CompareIfVariableGtConstant: {
      fragment.compareIfVariableGtConstant(
          var_generator(), bool_generator(), int32_generator(), var_generator(), bool_generator());
    } break;

    case OpCode::CompareIfVariableLtConstant: {
      fragment.compareIfVariableLtConstant(
          var_generator(), bool_generator(), int32_generator(), var_generator(), bool_generator());
    } break;

    case OpCode::CompareIfVariableEqConstant: {
      fragment.compareIfVariableEqConstant(
          var_generator(), bool_generator(), int32_generator(), var_generator(), bool_generator());
    } break;

    case OpCode::CompareIfVariableGtVariable: {
      fragment.compareIfVariableGtVariable(
          var_generator(), bool_generator(), var_generator(), bool_generator(),
          var_generator(), bool_generator());
    } break;

    case OpCode::CompareIfVariableLtVariable: {
      fragment.compareIfVariableLtVariable(
          var_generator(), bool_generator(), var_generator(), bool_generator(),
          var_generator(), bool_generator());
    } break;

    case OpCode::CompareIfVariableEqVariable: {
      fragment.compareIfVariableEqVariable(
          var_generator(), bool_generator(), var_generator(), bool_generator(),
          var_generator(), bool_generator());
    } break;

    case OpCode::GetMaxOfVariableAndConstant: {
      fragment.getMaxOfVariableAndConstant(
          var_generator(), bool_generator(), int32_generator(),
          var_generator(), bool_generator());
    } break;

    case OpCode::GetMinOfVariableAndConstant: {
      fragment.getMinOfVariableAndConstant(
          var_generator(), bool_generator(), int32_generator(),
          var_generator(), bool_generator());
    } break;

    case OpCode::GetMaxOfVariableAndVariable: {
      fragment.getMaxOfVariableAndVariable(
          var_generator(), bool_generator(), var_generator(), bool_generator(),
          var_generator(), bool_generator());
    } break;

    case OpCode::GetMinOfVariableAndVariable: {
      fragment.getMinOfVariableAndVariable(
          var_generator(), bool_generator(), var_generator(), bool_generator(),
          var_generator(), bool_generator());
    } break;

    case OpCode::ModuloVariableByConstant: {
      fragment.moduloVariableByConstant(
          var_generator(), bool_generator(), int32_generator());
    } break;

    case OpCode::ModuloVariableByVariable: {
      fragment.moduloVariableByVariable(
          var_generator(), bool_generator(), var_generator(), bool_generator());
    } break;

    case OpCode::BitShiftVariableLeft: {
      fragment.bitShiftVariableLeft(
          var_generator(), bool_generator(), int8_generator());
    } break;

    case OpCode::BitShiftVariableRight: {
      fragment.bitShiftVariableRight(
          var_generator(), bool_generator(), int8_generator());
    } break;

    case OpCode::BitWiseInvertVariable: {
      fragment.bitWiseInvertVariable(var_generator(), bool_generator());
    } break;

    case OpCode::BitWiseAndTwoVariables: {
      fragment.bitWiseAndTwoVariables(
          var_generator(), bool_generator(), var_generator(), bool_generator());
    } break;

    case OpCode::BitWiseOrTwoVariables: {
      fragment.bitWiseOrTwoVariables(
          var_generator(), bool_generator(), var_generator(), bool_generator());
    } break;

    case OpCode::BitWiseXorTwoVariables: {
      fragment.bitWiseXorTwoVariables(
          var_generator(), bool_generator(), var_generator(), bool_generator());
    } break;

    case OpCode::RotateVariableLeft: {
      fragment.rotateVariableLeft(var_generator(), bool_generator(), int8_generator());
    } break;

    case OpCode::RotateVariableRight: {
      fragment.rotateVariableRight(var_generator(), bool_generator(), int8_generator());
    } break;

    case OpCode::VariableBitShiftVariableLeft: {
      fragment.variableBitShiftVariableLeft(
          var_generator(), bool_generator(), var_generator(), bool_generator());
    } break;

    case OpCode::VariableBitShiftVariableRight: {
      fragment.variableBitShiftVariableRight(
          var_generator(), bool_generator(), var_generator(), bool_generator());
    } break;

    case OpCode::VariableRotateVariableLeft: {
      fragment.variableRotateVariableLeft(
          var_generator(), bool_generator(), var_generator(), bool_generator());
    } break;

    case OpCode::VariableRotateVariableRight: {
      fragment.variableRotateVariableRight(
          var_generator(), bool_generator(), var_generator(), bool_generator());
    } break;

    case OpCode::RelativeJumpToVariableAddressIfVariableGt0: {
      fragment.relativeJumpToVariableAddressIfVariableGreaterThanZero(
          var_generator(), bool_generator(), var_generator(), bool_generator());
    } break;

    case OpCode::RelativeJumpToVariableAddressIfVariableLt0: {
      fragment.relativeJumpToVariableAddressIfVariableLessThanZero(
          var_generator(), bool_generator(), var_generator(), bool_generator());
    } break;

    case OpCode::RelativeJumpToVariableAddressIfVariableEq0: {
      fragment.relativeJumpToVariableAddressIfVariableEqualsZero(
          var_generator(), bool_generator(), var_generator(), bool_generator());
    } break;

    case OpCode::AbsoluteJumpToVariableAddressIfVariableGt0: {
      fragment.absoluteJumpToVariableAddressIfVariableGreaterThanZero(
          var_generator(), bool_generator(), var_generator(), bool_generator());
    } break;

    case OpCode::AbsoluteJumpToVariableAddressIfVariableLt0: {
      fragment.absoluteJumpToVariableAddressIfVariableLessThanZero(
          var_generator(), bool_generator(), var_generator(), bool_generator());
    } break;

    case OpCode::AbsoluteJumpToVariableAddressIfVariableEq0: {
      fragment.absoluteJumpToVariableAddressIfVariableEqualsZero(
          var_generator(), bool_generator(), var_generator(), bool_generator());
    } break;

    case OpCode::RelativeJumpIfVariableGt0: {
      fragment.relativeJumpToAddressIfVariableGreaterThanZero(
          var_generator(), bool_generator(), rel_addr_generator());
    } break;

    case OpCode::RelativeJumpIfVariableLt0: {
      fragment.relativeJumpToAddressIfVariableLessThanZero(
          var_generator(), bool_generator(), rel_addr_generator());
    } break;

    case OpCode::RelativeJumpIfVariableEq0: {
      fragment.relativeJumpToAddressIfVariableEqualsZero(
          var_generator(), bool_generator(), rel_addr_generator());
    } break;

    case OpCode::AbsoluteJumpIfVariableGt0: {
      fragment.absoluteJumpToAddressIfVariableGreaterThanZero(
          var_generator(), bool_generator(), rel_addr_generator());
    } break;

    case OpCode::AbsoluteJumpIfVariableLt0: {
      fragment.absoluteJumpToAddressIfVariableLessThanZero(
          var_generator(), bool_generator(), rel_addr_generator());
    } break;

    case OpCode::AbsoluteJumpIfVariableEq0: {
      fragment.absoluteJumpToAddressIfVariableEqualsZero(
          var_generator(), bool_generator(), rel_addr_generator());
    } break;

    case OpCode::UnconditionalJumpToAbsoluteAddress: {
      fragment.unconditionalJumpToAbsoluteAddress(abs_addr_generator());
    } break;

    case OpCode::UnconditionalJumpToAbsoluteVariableAddress: {
      fragment.unconditionalJumpToAbsoluteVariableAddress(var_generator(), bool_generator());
    } break;

    case OpCode::UnconditionalJumpToRelativeAddress: {
      fragment.unconditionalJumpToRelativeAddress(rel_addr_generator());
    } break;

    case OpCode::UnconditionalJumpToRelativeVariableAddress: {
      fragment.unconditionalJumpToRelativeVariableAddress(var_generator(), bool_generator());
    } break;

    case OpCode::CheckIfVariableIsInput: {
      fragment.checkIfVariableIsInput(
          var_generator(), bool_generator(), var_generator(), bool_generator());
    } break;

    case OpCode::CheckIfVariableIsOutput: {
      fragment.checkIfVariableIsOutput(
          var_generator(), bool_generator(), var_generator(), bool_generator());
    } break;

    case OpCode::LoadInputCountIntoVariable: {
      fragment.loadInputCountIntoVariable(var_generator(), bool_generator());
    } break;

    case OpCode::LoadOutputCountIntoVariable: {
      fragment.loadOutputCountIntoVariable(var_generator(), bool_generator());
    } break;

    case OpCode::CheckIfInputWasSet: {
      fragment.checkIfInputWasSet(
          var_generator(), bool_generator(), var_generator(), bool_generator());
    } break;

    case OpCode::PrintVariable: {
      fragment.printVariable(var_generator(), bool_generator(), bool_generator());
    } break;

    case OpCode::SetStringTableEntry: {
    } break;

    case OpCode::PrintStringFromStringTable: {
    } break;

    case OpCode::LoadStringTableLimitIntoVariable: {
    } break;

    case OpCode::LoadStringTableItemLengthLimitIntoVariable: {
    } break;

    case OpCode::SetVariableStringTableEntry: {
    } break;

    case OpCode::PrintVariableStringFromStringTable: {
    } break;

    case OpCode::LoadVariableStringItemLengthIntoVariable: {
    } break;

    case OpCode::LoadVariableStringItemIntoVariables: {
    } break;

    case OpCode::LoadStringItemLengthIntoVariable: {
    } break;

    case OpCode::LoadStringItemIntoVariables: {
    } break;

    case OpCode::PushVariableOnStack: {
      fragment.pushVariableOnStack(
          var_generator(), bool_generator(), var_generator(), bool_generator());
    } break;

    case OpCode::PushConstantOnStack: {
      fragment.pushConstantOnStack(var_generator(), bool_generator(), int32_generator());
    } break;

    case OpCode::PopVariableFromStack: {
      fragment.popVariableFromStack(
          var_generator(), bool_generator(), var_generator(), bool_generator());
    } break;

    case OpCode::PopTopItemFromStack: {
      fragment.popTopItemFromStack(var_generator(), bool_generator());
    } break;

    case OpCode::CheckIfStackIsEmpty: {
      fragment.checkIfStackIsEmpty(
          var_generator(), bool_generator(), var_generator(), bool_generator());
    } break;

    case OpCode::Size:
      // Do nothing
      break;
    }

    // TODO(fairlight1337): Implement generation of random programs using the full set of available
    // operators. Also, cover this method with tests properly, beyond checking the target size of
    // the generated programs.

    if (fragment.getSize() + prg.getPointer() > prg.getSize()) {
      // The instruction doesn't fit into the program anymore. Generation is done.
      break;
    }
    prg.insertProgram(fragment);
  }

  return prg;
}

}  // namespace beast
