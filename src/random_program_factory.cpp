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
    } break;

    case OpCode::LoadRandomValueIntoVariable: {
      fragment.loadRandomValueIntoVariable(var_generator(), bool_generator());
    } break;

    case OpCode::DeclareVariable: {
    } break;

    case OpCode::SetVariable: {
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
    } break;

    case OpCode::AddVariableToVariable: {
      fragment.addVariableToVariable(
          var_generator(), bool_generator(), var_generator(), bool_generator());
    } break;

    case OpCode::SubtractConstantFromVariable: {
    } break;

    case OpCode::SubtractVariableFromVariable: {
      fragment.subtractVariableFromVariable(
          var_generator(), bool_generator(), var_generator(), bool_generator());
    } break;

    case OpCode::CompareIfVariableGtConstant: {
    } break;

    case OpCode::CompareIfVariableLtConstant: {
    } break;

    case OpCode::CompareIfVariableEqConstant: {
    } break;

    case OpCode::CompareIfVariableGtVariable: {
      fragment.compareIfVariableGtVariable(
          var_generator(), bool_generator(),
          var_generator(), bool_generator(),
          var_generator(), bool_generator());
    } break;

    case OpCode::CompareIfVariableLtVariable: {
      fragment.compareIfVariableLtVariable(
          var_generator(), bool_generator(),
          var_generator(), bool_generator(),
          var_generator(), bool_generator());
    } break;

    case OpCode::CompareIfVariableEqVariable: {
      fragment.compareIfVariableEqVariable(
          var_generator(), bool_generator(),
          var_generator(), bool_generator(),
          var_generator(), bool_generator());
    } break;

    case OpCode::GetMaxOfVariableAndConstant: {
    } break;

    case OpCode::GetMinOfVariableAndConstant: {
    } break;

    case OpCode::GetMaxOfVariableAndVariable: {
      fragment.getMaxOfVariableAndVariable(
          var_generator(), bool_generator(),
          var_generator(), bool_generator(),
          var_generator(), bool_generator());
    } break;

    case OpCode::GetMinOfVariableAndVariable: {
      fragment.getMinOfVariableAndVariable(
          var_generator(), bool_generator(),
          var_generator(), bool_generator(),
          var_generator(), bool_generator());
    } break;

    case OpCode::ModuloVariableByConstant: {
    } break;

    case OpCode::ModuloVariableByVariable: {
      fragment.moduloVariableByVariable(
          var_generator(), bool_generator(),
          var_generator(), bool_generator());
    } break;

    case OpCode::BitShiftVariableLeft: {
    } break;

    case OpCode::BitShiftVariableRight: {
    } break;

    case OpCode::BitWiseInvertVariable: {
    } break;

    case OpCode::BitWiseAndTwoVariables: {
    } break;

    case OpCode::BitWiseOrTwoVariables: {
    } break;

    case OpCode::BitWiseXorTwoVariables: {
    } break;

    case OpCode::RotateVariableLeft: {
    } break;

    case OpCode::RotateVariableRight: {
    } break;

    case OpCode::VariableBitShiftVariableLeft: {
    } break;

    case OpCode::VariableBitShiftVariableRight: {
    } break;

    case OpCode::VariableRotateVariableLeft: {
    } break;

    case OpCode::VariableRotateVariableRight: {
    } break;

    case OpCode::RelativeJumpToVariableAddressIfVariableGt0: {
    } break;

    case OpCode::RelativeJumpToVariableAddressIfVariableLt0: {
    } break;

    case OpCode::RelativeJumpToVariableAddressIfVariableEq0: {
    } break;

    case OpCode::AbsoluteJumpToVariableAddressIfVariableGt0: {
    } break;

    case OpCode::AbsoluteJumpToVariableAddressIfVariableLt0: {
    } break;

    case OpCode::AbsoluteJumpToVariableAddressIfVariableEq0: {
    } break;

    case OpCode::RelativeJumpIfVariableGt0: {
    } break;

    case OpCode::RelativeJumpIfVariableLt0: {
    } break;

    case OpCode::RelativeJumpIfVariableEq0: {
    } break;

    case OpCode::AbsoluteJumpIfVariableGt0: {
    } break;

    case OpCode::AbsoluteJumpIfVariableLt0: {
    } break;

    case OpCode::AbsoluteJumpIfVariableEq0: {
    } break;

    case OpCode::UnconditionalJumpToAbsoluteAddress: {
    } break;

    case OpCode::UnconditionalJumpToAbsoluteVariableAddress: {
    } break;

    case OpCode::UnconditionalJumpToRelativeAddress: {
    } break;

    case OpCode::UnconditionalJumpToRelativeVariableAddress: {
    } break;

    case OpCode::CheckIfVariableIsInput: {
    } break;

    case OpCode::CheckIfVariableIsOutput: {
    } break;

    case OpCode::LoadInputCountIntoVariable: {
    } break;

    case OpCode::LoadOutputCountIntoVariable: {
    } break;

    case OpCode::CheckIfInputWasSet: {
    } break;

    case OpCode::PrintVariable: {
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
    } break;

    case OpCode::PushConstantOnStack: {
    } break;

    case OpCode::PopVariableFromStack: {
    } break;

    case OpCode::PopTopItemFromStack: {
    } break;

    case OpCode::CheckIfStackIsEmpty: {
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
