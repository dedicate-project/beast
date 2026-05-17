#include <beast/random_program_factory.hpp>

// Standard
#include <algorithm>
#include <limits>
#include <random>
#include <vector>

namespace beast {

namespace {

/// Build a `discrete_distribution` over [0, OpCode::Size) according to a weight map.
///
/// Opcodes missing from the map get weight 1.0 (uniform default). An empty map therefore
/// produces a uniform distribution. Weights of 0.0 cleanly exclude an opcode from sampling.
std::discrete_distribution<int32_t> makeOpcodeDistribution(const OpcodeWeights& weights) {
  const auto count = static_cast<size_t>(OpCode::Size);
  std::vector<double> weight_vector(count, 1.0);
  for (const auto& [opcode, weight] : weights) {
    const auto idx = static_cast<size_t>(opcode);
    if (idx < count) {
      weight_vector[idx] = weight;
    }
  }
  return std::discrete_distribution<int32_t>(weight_vector.begin(), weight_vector.end());
}

/// Bundle of random distributions sized for a particular VM environment.
///
/// Centralises RNG state so the streaming `generate()` path and the one-shot
/// `generateRandomOperator()` helper can share the same distributions instead of rebuilding
/// them on every operator emission.
struct RandomToolkit {
  std::mt19937& engine;
  std::discrete_distribution<int32_t> opcode_dist;
  std::uniform_int_distribution<int32_t> var_dist;
  std::uniform_int_distribution<int32_t> bool_dist;
  std::uniform_int_distribution<int32_t> int32_dist;
  // `std::uniform_int_distribution<int8_t>` is undefined behaviour per the standard
  // (`int8_t` is typically `signed char`, which isn't a valid template parameter). Always use
  // `int32_t` as the distribution type and narrow afterwards.
  std::uniform_int_distribution<int32_t> int8_dist;
  std::uniform_int_distribution<int32_t> rel_addr_dist;
  std::uniform_int_distribution<int32_t> abs_addr_dist;
  std::uniform_int_distribution<int32_t> string_idx_dist;
  std::uniform_int_distribution<int32_t> str_len_dist;
  std::uniform_int_distribution<int32_t> char_dist;

  RandomToolkit(std::mt19937& engine_ref, uint32_t program_size_for_addresses,
                uint32_t memory_size, uint32_t string_table_size,
                uint32_t string_table_item_length, const OpcodeWeights& opcode_weights = {})
      : engine{engine_ref},
        opcode_dist{makeOpcodeDistribution(opcode_weights)},
        var_dist{0, std::max<int32_t>(0, static_cast<int32_t>(memory_size) - 1)},
        bool_dist{0, 1},
        int32_dist{std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max()},
        int8_dist{std::numeric_limits<int8_t>::min(), std::numeric_limits<int8_t>::max()},
        rel_addr_dist{
            -static_cast<int32_t>(static_cast<double>(program_size_for_addresses) * 0.5),
            static_cast<int32_t>(static_cast<double>(program_size_for_addresses) * 0.5)},
        abs_addr_dist{0, static_cast<int32_t>(program_size_for_addresses)},
        string_idx_dist{0, std::max<int32_t>(0, static_cast<int32_t>(string_table_size))},
        str_len_dist{0, static_cast<int32_t>(string_table_item_length)},
        char_dist{33, 126} {}

  bool boolean() { return bool_dist(engine) == 1; }
  int32_t variable() { return var_dist(engine); }
  int32_t int32() { return int32_dist(engine); }
  int8_t int8() { return static_cast<int8_t>(int8_dist(engine)); }
  int32_t relAddr() { return rel_addr_dist(engine); }
  int32_t absAddr() { return abs_addr_dist(engine); }
  int32_t stringIdx() { return string_idx_dist(engine); }
  OpCode opcode() { return static_cast<OpCode>(opcode_dist(engine)); }

  std::string string() {
    std::string out;
    const int32_t length = str_len_dist(engine);
    out.reserve(length);
    for (int32_t i = 0; i < length; ++i) {
      out += static_cast<char>(char_dist(engine));
    }
    return out;
  }
};

/// Emit one randomly-chosen operator into `fragment`. Returns the OpCode that was emitted.
///
/// Centralises the giant switch over the OpCode enum so both `generate()` and
/// `generateRandomOperator()` go through the same path. Updating the instruction set means
/// updating this one place.
OpCode appendRandomOperator(Program& fragment, RandomToolkit& toolkit) {
  const OpCode opcode = toolkit.opcode();
  switch (opcode) {
  case OpCode::NoOp: fragment.noop(); break;
  case OpCode::LoadMemorySizeIntoVariable:
    fragment.loadMemorySizeIntoVariable(toolkit.variable(), toolkit.boolean());
    break;
  case OpCode::LoadCurrentAddressIntoVariable:
    fragment.loadCurrentAddressIntoVariable(toolkit.variable(), toolkit.boolean());
    break;
  case OpCode::Terminate:
    // Return code is int8_t; the toolkit's int8 distribution already excludes the off-by-one
    // {-127, 128} bug that used to live here.
    fragment.terminate(toolkit.int8());
    break;
  case OpCode::TerminateWithVariableReturnCode:
    fragment.terminateWithVariableReturnCode(toolkit.variable(), toolkit.boolean());
    break;
  case OpCode::PerformSystemCall:
    fragment.performSystemCall(
        toolkit.int8(), toolkit.int8(), toolkit.variable(), toolkit.boolean());
    break;
  case OpCode::LoadRandomValueIntoVariable:
    fragment.loadRandomValueIntoVariable(toolkit.variable(), toolkit.boolean());
    break;
  case OpCode::DeclareVariable:
    fragment.declareVariable(toolkit.variable(),
                             toolkit.boolean() ? Program::VariableType::Int32
                                               : Program::VariableType::Link);
    break;
  case OpCode::SetVariable:
    fragment.setVariable(toolkit.variable(), toolkit.int32(), toolkit.boolean());
    break;
  case OpCode::UndeclareVariable: fragment.undeclareVariable(toolkit.variable()); break;
  case OpCode::CopyVariable:
    fragment.copyVariable(
        toolkit.variable(), toolkit.boolean(), toolkit.variable(), toolkit.boolean());
    break;
  case OpCode::SwapVariables:
    fragment.swapVariables(
        toolkit.variable(), toolkit.boolean(), toolkit.variable(), toolkit.boolean());
    break;
  case OpCode::AddConstantToVariable:
    fragment.addConstantToVariable(toolkit.variable(), toolkit.int32(), toolkit.boolean());
    break;
  case OpCode::AddVariableToVariable:
    fragment.addVariableToVariable(
        toolkit.variable(), toolkit.boolean(), toolkit.variable(), toolkit.boolean());
    break;
  case OpCode::SubtractConstantFromVariable:
    fragment.subtractConstantFromVariable(
        toolkit.variable(), toolkit.int32(), toolkit.boolean());
    break;
  case OpCode::SubtractVariableFromVariable:
    fragment.subtractVariableFromVariable(
        toolkit.variable(), toolkit.boolean(), toolkit.variable(), toolkit.boolean());
    break;
  case OpCode::CompareIfVariableGtConstant:
    fragment.compareIfVariableGtConstant(toolkit.variable(),
                                         toolkit.boolean(),
                                         toolkit.int32(),
                                         toolkit.variable(),
                                         toolkit.boolean());
    break;
  case OpCode::CompareIfVariableLtConstant:
    fragment.compareIfVariableLtConstant(toolkit.variable(),
                                         toolkit.boolean(),
                                         toolkit.int32(),
                                         toolkit.variable(),
                                         toolkit.boolean());
    break;
  case OpCode::CompareIfVariableEqConstant:
    fragment.compareIfVariableEqConstant(toolkit.variable(),
                                         toolkit.boolean(),
                                         toolkit.int32(),
                                         toolkit.variable(),
                                         toolkit.boolean());
    break;
  case OpCode::CompareIfVariableGtVariable:
    fragment.compareIfVariableGtVariable(toolkit.variable(),
                                         toolkit.boolean(),
                                         toolkit.variable(),
                                         toolkit.boolean(),
                                         toolkit.variable(),
                                         toolkit.boolean());
    break;
  case OpCode::CompareIfVariableLtVariable:
    fragment.compareIfVariableLtVariable(toolkit.variable(),
                                         toolkit.boolean(),
                                         toolkit.variable(),
                                         toolkit.boolean(),
                                         toolkit.variable(),
                                         toolkit.boolean());
    break;
  case OpCode::CompareIfVariableEqVariable:
    fragment.compareIfVariableEqVariable(toolkit.variable(),
                                         toolkit.boolean(),
                                         toolkit.variable(),
                                         toolkit.boolean(),
                                         toolkit.variable(),
                                         toolkit.boolean());
    break;
  case OpCode::GetMaxOfVariableAndConstant:
    fragment.getMaxOfVariableAndConstant(toolkit.variable(),
                                         toolkit.boolean(),
                                         toolkit.int32(),
                                         toolkit.variable(),
                                         toolkit.boolean());
    break;
  case OpCode::GetMinOfVariableAndConstant:
    fragment.getMinOfVariableAndConstant(toolkit.variable(),
                                         toolkit.boolean(),
                                         toolkit.int32(),
                                         toolkit.variable(),
                                         toolkit.boolean());
    break;
  case OpCode::GetMaxOfVariableAndVariable:
    fragment.getMaxOfVariableAndVariable(toolkit.variable(),
                                         toolkit.boolean(),
                                         toolkit.variable(),
                                         toolkit.boolean(),
                                         toolkit.variable(),
                                         toolkit.boolean());
    break;
  case OpCode::GetMinOfVariableAndVariable:
    fragment.getMinOfVariableAndVariable(toolkit.variable(),
                                         toolkit.boolean(),
                                         toolkit.variable(),
                                         toolkit.boolean(),
                                         toolkit.variable(),
                                         toolkit.boolean());
    break;
  case OpCode::ModuloVariableByConstant:
    fragment.moduloVariableByConstant(toolkit.variable(), toolkit.boolean(), toolkit.int32());
    break;
  case OpCode::ModuloVariableByVariable:
    fragment.moduloVariableByVariable(
        toolkit.variable(), toolkit.boolean(), toolkit.variable(), toolkit.boolean());
    break;
  case OpCode::BitShiftVariableLeft:
    fragment.bitShiftVariableLeft(toolkit.variable(), toolkit.boolean(), toolkit.int8());
    break;
  case OpCode::BitShiftVariableRight:
    fragment.bitShiftVariableRight(toolkit.variable(), toolkit.boolean(), toolkit.int8());
    break;
  case OpCode::BitWiseInvertVariable:
    fragment.bitWiseInvertVariable(toolkit.variable(), toolkit.boolean());
    break;
  case OpCode::BitWiseAndTwoVariables:
    fragment.bitWiseAndTwoVariables(
        toolkit.variable(), toolkit.boolean(), toolkit.variable(), toolkit.boolean());
    break;
  case OpCode::BitWiseOrTwoVariables:
    fragment.bitWiseOrTwoVariables(
        toolkit.variable(), toolkit.boolean(), toolkit.variable(), toolkit.boolean());
    break;
  case OpCode::BitWiseXorTwoVariables:
    fragment.bitWiseXorTwoVariables(
        toolkit.variable(), toolkit.boolean(), toolkit.variable(), toolkit.boolean());
    break;
  case OpCode::RotateVariableLeft:
    fragment.rotateVariableLeft(toolkit.variable(), toolkit.boolean(), toolkit.int8());
    break;
  case OpCode::RotateVariableRight:
    fragment.rotateVariableRight(toolkit.variable(), toolkit.boolean(), toolkit.int8());
    break;
  case OpCode::VariableBitShiftVariableLeft:
    fragment.variableBitShiftVariableLeft(
        toolkit.variable(), toolkit.boolean(), toolkit.variable(), toolkit.boolean());
    break;
  case OpCode::VariableBitShiftVariableRight:
    fragment.variableBitShiftVariableRight(
        toolkit.variable(), toolkit.boolean(), toolkit.variable(), toolkit.boolean());
    break;
  case OpCode::VariableRotateVariableLeft:
    fragment.variableRotateVariableLeft(
        toolkit.variable(), toolkit.boolean(), toolkit.variable(), toolkit.boolean());
    break;
  case OpCode::VariableRotateVariableRight:
    fragment.variableRotateVariableRight(
        toolkit.variable(), toolkit.boolean(), toolkit.variable(), toolkit.boolean());
    break;
  case OpCode::RelativeJumpToVariableAddressIfVariableGt0:
    fragment.relativeJumpToVariableAddressIfVariableGreaterThanZero(
        toolkit.variable(), toolkit.boolean(), toolkit.variable(), toolkit.boolean());
    break;
  case OpCode::RelativeJumpToVariableAddressIfVariableLt0:
    fragment.relativeJumpToVariableAddressIfVariableLessThanZero(
        toolkit.variable(), toolkit.boolean(), toolkit.variable(), toolkit.boolean());
    break;
  case OpCode::RelativeJumpToVariableAddressIfVariableEq0:
    fragment.relativeJumpToVariableAddressIfVariableEqualsZero(
        toolkit.variable(), toolkit.boolean(), toolkit.variable(), toolkit.boolean());
    break;
  case OpCode::AbsoluteJumpToVariableAddressIfVariableGt0:
    fragment.absoluteJumpToVariableAddressIfVariableGreaterThanZero(
        toolkit.variable(), toolkit.boolean(), toolkit.variable(), toolkit.boolean());
    break;
  case OpCode::AbsoluteJumpToVariableAddressIfVariableLt0:
    fragment.absoluteJumpToVariableAddressIfVariableLessThanZero(
        toolkit.variable(), toolkit.boolean(), toolkit.variable(), toolkit.boolean());
    break;
  case OpCode::AbsoluteJumpToVariableAddressIfVariableEq0:
    fragment.absoluteJumpToVariableAddressIfVariableEqualsZero(
        toolkit.variable(), toolkit.boolean(), toolkit.variable(), toolkit.boolean());
    break;
  case OpCode::RelativeJumpIfVariableGt0:
    fragment.relativeJumpToAddressIfVariableGreaterThanZero(
        toolkit.variable(), toolkit.boolean(), toolkit.relAddr());
    break;
  case OpCode::RelativeJumpIfVariableLt0:
    fragment.relativeJumpToAddressIfVariableLessThanZero(
        toolkit.variable(), toolkit.boolean(), toolkit.relAddr());
    break;
  case OpCode::RelativeJumpIfVariableEq0:
    fragment.relativeJumpToAddressIfVariableEqualsZero(
        toolkit.variable(), toolkit.boolean(), toolkit.relAddr());
    break;
  case OpCode::AbsoluteJumpIfVariableGt0:
    fragment.absoluteJumpToAddressIfVariableGreaterThanZero(
        toolkit.variable(), toolkit.boolean(), toolkit.relAddr());
    break;
  case OpCode::AbsoluteJumpIfVariableLt0:
    fragment.absoluteJumpToAddressIfVariableLessThanZero(
        toolkit.variable(), toolkit.boolean(), toolkit.relAddr());
    break;
  case OpCode::AbsoluteJumpIfVariableEq0:
    fragment.absoluteJumpToAddressIfVariableEqualsZero(
        toolkit.variable(), toolkit.boolean(), toolkit.relAddr());
    break;
  case OpCode::UnconditionalJumpToAbsoluteAddress:
    fragment.unconditionalJumpToAbsoluteAddress(toolkit.absAddr());
    break;
  case OpCode::UnconditionalJumpToAbsoluteVariableAddress:
    fragment.unconditionalJumpToAbsoluteVariableAddress(toolkit.variable(), toolkit.boolean());
    break;
  case OpCode::UnconditionalJumpToRelativeAddress:
    fragment.unconditionalJumpToRelativeAddress(toolkit.relAddr());
    break;
  case OpCode::UnconditionalJumpToRelativeVariableAddress:
    fragment.unconditionalJumpToRelativeVariableAddress(toolkit.variable(), toolkit.boolean());
    break;
  case OpCode::CheckIfVariableIsInput:
    fragment.checkIfVariableIsInput(
        toolkit.variable(), toolkit.boolean(), toolkit.variable(), toolkit.boolean());
    break;
  case OpCode::CheckIfVariableIsOutput:
    fragment.checkIfVariableIsOutput(
        toolkit.variable(), toolkit.boolean(), toolkit.variable(), toolkit.boolean());
    break;
  case OpCode::LoadInputCountIntoVariable:
    fragment.loadInputCountIntoVariable(toolkit.variable(), toolkit.boolean());
    break;
  case OpCode::LoadOutputCountIntoVariable:
    fragment.loadOutputCountIntoVariable(toolkit.variable(), toolkit.boolean());
    break;
  case OpCode::CheckIfInputWasSet:
    fragment.checkIfInputWasSet(
        toolkit.variable(), toolkit.boolean(), toolkit.variable(), toolkit.boolean());
    break;
  case OpCode::PrintVariable:
    fragment.printVariable(toolkit.variable(), toolkit.boolean(), toolkit.boolean());
    break;
  case OpCode::SetStringTableEntry:
    fragment.setStringTableEntry(toolkit.stringIdx(), toolkit.string());
    break;
  case OpCode::PrintStringFromStringTable:
    fragment.printStringFromStringTable(toolkit.stringIdx());
    break;
  case OpCode::LoadStringTableLimitIntoVariable:
    fragment.loadStringTableLimitIntoVariable(toolkit.variable(), toolkit.boolean());
    break;
  case OpCode::LoadStringTableItemLengthLimitIntoVariable:
    fragment.loadStringTableItemLengthLimitIntoVariable(toolkit.variable(), toolkit.boolean());
    break;
  case OpCode::SetVariableStringTableEntry:
    fragment.setVariableStringTableEntry(
        toolkit.variable(), toolkit.boolean(), toolkit.string());
    break;
  case OpCode::PrintVariableStringFromStringTable:
    fragment.printVariableStringFromStringTable(toolkit.variable(), toolkit.boolean());
    break;
  case OpCode::LoadVariableStringItemLengthIntoVariable:
    fragment.loadVariableStringItemLengthIntoVariable(
        toolkit.variable(), toolkit.boolean(), toolkit.variable(), toolkit.boolean());
    break;
  case OpCode::LoadVariableStringItemIntoVariables:
    fragment.loadVariableStringItemIntoVariables(
        toolkit.variable(), toolkit.boolean(), toolkit.variable(), toolkit.boolean());
    break;
  case OpCode::LoadStringItemLengthIntoVariable:
    fragment.loadStringItemLengthIntoVariable(
        toolkit.stringIdx(), toolkit.variable(), toolkit.boolean());
    break;
  case OpCode::LoadStringItemIntoVariables:
    fragment.loadStringItemIntoVariables(
        toolkit.stringIdx(), toolkit.variable(), toolkit.boolean());
    break;
  case OpCode::PushVariableOnStack:
    fragment.pushVariableOnStack(
        toolkit.variable(), toolkit.boolean(), toolkit.variable(), toolkit.boolean());
    break;
  case OpCode::PushConstantOnStack:
    fragment.pushConstantOnStack(toolkit.variable(), toolkit.boolean(), toolkit.int32());
    break;
  case OpCode::PopVariableFromStack:
    fragment.popVariableFromStack(
        toolkit.variable(), toolkit.boolean(), toolkit.variable(), toolkit.boolean());
    break;
  case OpCode::PopTopItemFromStack:
    fragment.popTopItemFromStack(toolkit.variable(), toolkit.boolean());
    break;
  case OpCode::CheckIfStackIsEmpty:
    fragment.checkIfStackIsEmpty(
        toolkit.variable(), toolkit.boolean(), toolkit.variable(), toolkit.boolean());
    break;
  case OpCode::Size:
    // Sentinel; the opcode distribution excludes it. Reaching here means somebody passed a
    // weight map with the sentinel set to non-zero, so just emit a NoOp as a safe fallback.
    fragment.noop();
    break;
  }
  return opcode;
}

} // namespace

RandomProgramFactory::RandomProgramFactory() : mersenne_engine_{std::random_device{}()} {}

Program RandomProgramFactory::generate(uint32_t size, uint32_t memory_size,
                                       uint32_t string_table_size,
                                       uint32_t string_table_item_length) {
  // Uniform sampling is just weighted sampling with an empty weights map. Implementing it in
  // one place avoids drifting between two giant switch statements.
  return generate(size, memory_size, string_table_size, string_table_item_length, OpcodeWeights{});
}

Program RandomProgramFactory::generate(uint32_t size, uint32_t memory_size,
                                       uint32_t string_table_size,
                                       uint32_t string_table_item_length,
                                       const OpcodeWeights& weights) {
  Program prg(size);
  RandomToolkit toolkit(mersenne_engine_, size, memory_size, string_table_size,
                        string_table_item_length, weights);

  while (true) {
    Program fragment;
    appendRandomOperator(fragment, toolkit);
    if (fragment.getSize() + prg.getPointer() > prg.getSize()) {
      // The instruction doesn't fit into the program anymore. Generation is done.
      break;
    }
    prg.insertProgram(fragment);
  }
  return prg;
}

std::vector<unsigned char>
RandomProgramFactory::generateRandomOperator(uint32_t memory_size, uint32_t string_table_size,
                                             uint32_t string_table_item_length,
                                             uint32_t max_bytes) {
  return generateRandomOperator(memory_size, string_table_size, string_table_item_length,
                                max_bytes, OpcodeWeights{});
}

std::vector<unsigned char>
RandomProgramFactory::generateRandomOperator(uint32_t memory_size, uint32_t string_table_size,
                                             uint32_t string_table_item_length,
                                             uint32_t max_bytes, const OpcodeWeights& weights) {
  // Static factory function: own a thread-local engine so we don't pay for random_device
  // construction on every call (the GA mutator calls this *per operator span*).
  thread_local std::mt19937 engine{std::random_device{}()};

  RandomToolkit toolkit(engine,
                        /*program_size_for_addresses=*/max_bytes, memory_size, string_table_size,
                        string_table_item_length, weights);

  // Try a handful of times to land on an operator that fits within `max_bytes`. Only the
  // variable-length string operators can overshoot in practice.
  for (int attempt = 0; attempt < 8; ++attempt) {
    Program fragment(max_bytes);
    appendRandomOperator(fragment, toolkit);
    if (fragment.getPointer() > 0 && fragment.getPointer() <= max_bytes) {
      std::vector<unsigned char> bytes = fragment.getData();
      bytes.resize(fragment.getPointer());
      return bytes;
    }
  }

  // Last resort: a no-op fits everywhere and is always a valid 1-byte operator.
  Program fallback(max_bytes);
  fallback.noop();
  std::vector<unsigned char> bytes = fallback.getData();
  bytes.resize(fallback.getPointer());
  return bytes;
}

} // namespace beast
