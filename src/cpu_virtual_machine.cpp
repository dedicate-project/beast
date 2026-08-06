#include <beast/cpu_virtual_machine.hpp>

// Standard
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <vector>

// Internal
#include <beast/opcodes.hpp>
#include <beast/program.hpp>
#include <beast/subroutine_library.hpp>
#include <beast/time_functions.hpp>
#include <beast/vm_session.hpp>

namespace {
/**
 * @brief Converts a boolean flag to a string representation
 *
 * A value of `true` will be converted to "true", `false` will become "false".
 *
 * @param flag The boolean flag to convert to a string value
 * @return A string representing the passed in boolean state
 */
std::string to_string(bool flag) noexcept { return flag ? "true" : "false"; }
} // namespace

namespace beast {

namespace {

struct CallerArg {
  int32_t variable_index;
  bool follow_links;
};

/// Read the operand bytes of a `CallSubroutine` instruction off the session stream.
///
/// On the dry-run path we still need to advance the instruction pointer so the next
/// `step` sees the following instruction; on the live path we additionally need the
/// decoded values to feed the callee VM.
struct CallSubroutineOperands {
  uint8_t subroutine_id;
  uint8_t input_arity;
  uint8_t output_arity;
  std::vector<CallerArg> inputs;
  std::vector<CallerArg> outputs;
};

CallSubroutineOperands readCallSubroutineOperands(VmSession& session) {
  CallSubroutineOperands ops;
  ops.subroutine_id = static_cast<uint8_t>(session.getData1());
  ops.input_arity = static_cast<uint8_t>(session.getData1());
  ops.output_arity = static_cast<uint8_t>(session.getData1());
  ops.inputs.reserve(ops.input_arity);
  for (uint8_t i = 0; i < ops.input_arity; ++i) {
    const int32_t variable_index = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    ops.inputs.push_back({variable_index, follow_links});
  }
  ops.outputs.reserve(ops.output_arity);
  for (uint8_t i = 0; i < ops.output_arity; ++i) {
    const int32_t variable_index = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    ops.outputs.push_back({variable_index, follow_links});
  }
  return ops;
}

} // namespace

bool CpuVirtualMachine::dispatchCallSubroutine(VmSession& session, bool dry_run) {
  // Wire format matches `Program::callSubroutine` and `ProgramParser::parse`. We always
  // drain the operand bytes from the session stream -- even on `dry_run` paths or
  // hard-fail validation paths -- so the instruction pointer ends up positioned at the
  // next instruction. Doing otherwise would leave the VM stuck re-reading the same
  // operand bytes as an opcode on the next step.
  const CallSubroutineOperands ops = readCallSubroutineOperands(session);

  debug("call_subroutine(id=" + std::to_string(ops.subroutine_id) +
        ", in_arity=" + std::to_string(ops.input_arity) +
        ", out_arity=" + std::to_string(ops.output_arity) + ")");

  if (dry_run) {
    return true;
  }

  // Validate against the mounted library. Any kind of mismatch (no library, id out of
  // range, arity mismatch, oversized arity) is fatal for this trial; the session is
  // marked abnormal so the evaluator scores it as zero and the GA discards it. This is
  // intentionally strict: a `CallSubroutine` that smuggles a no-op past the parser
  // would otherwise reward the GA for nonsense.
  auto library = session.getSubroutineLibrary();
  if (!library) {
    panic("CallSubroutine dispatched with no library mounted.");
    session.setExitedAbnormally();
    return false;
  }
  if (ops.input_arity > kMaxSubroutineArity || ops.output_arity > kMaxSubroutineArity) {
    panic("CallSubroutine arity exceeds kMaxSubroutineArity.");
    session.setExitedAbnormally();
    return false;
  }
  if (ops.subroutine_id >= library->size()) {
    panic("CallSubroutine id " + std::to_string(ops.subroutine_id) +
          " out of range for library of size " + std::to_string(library->size()) + ".");
    session.setExitedAbnormally();
    return false;
  }
  const SubroutineEntry& entry = (*library)[ops.subroutine_id];
  if (entry.input_arity != ops.input_arity || entry.output_arity != ops.output_arity) {
    panic("CallSubroutine arity (" + std::to_string(ops.input_arity) + "/" +
          std::to_string(ops.output_arity) + ") does not match registered arity (" +
          std::to_string(entry.input_arity) + "/" + std::to_string(entry.output_arity) +
          ").");
    session.setExitedAbnormally();
    return false;
  }

  try {
    // Snapshot the caller's input values *first*, before the callee VM mutates anything.
    // The reads can throw on undeclared variables; we surface that as a normal
    // abnormal-exit rather than a hard crash so the evaluator can continue with the
    // next trial.
    std::vector<int32_t> input_values;
    input_values.reserve(ops.input_arity);
    for (const auto& arg : ops.inputs) {
      input_values.push_back(session.getVariableValue(arg.variable_index, arg.follow_links));
    }

    // The callee session reuses the caller's variable/string-table parameters so a
    // subroutine genome harvested from a standalone evaluator (whose own `variable_count`
    // was sized for the same I/O shape) keeps working. The library itself is intentionally
    // NOT propagated -- v1 explicitly disallows recursion, and an empty library on the
    // callee guarantees that any stray CallSubroutine in the callee bytecode fails closed.
    VmSession callee_session(Program(entry.bytecode), session.getVariableCount(),
                             session.getStringTableCount(), session.getMaxStringSize());

    for (uint8_t i = 0; i < ops.input_arity; ++i) {
      callee_session.setVariableBehavior(static_cast<int32_t>(i),
                                         VmSession::VariableIoBehavior::Input);
      callee_session.setVariableValue(static_cast<int32_t>(i), true, input_values.at(i));
    }
    // Trial-id slot stays at 0 by default behaviour; mark it as input for layout
    // consistency with `BitDistanceEvaluator`.
    callee_session.setVariableBehavior(static_cast<int32_t>(ops.input_arity),
                                       VmSession::VariableIoBehavior::Input);
    const int32_t output_offset = static_cast<int32_t>(ops.input_arity) + 1;
    for (uint8_t i = 0; i < ops.output_arity; ++i) {
      callee_session.setVariableBehavior(output_offset + static_cast<int32_t>(i),
                                         VmSession::VariableIoBehavior::Output);
    }

    // Step the callee until it terminates on its own OR exhausts its per-call step
    // budget. Unlike `BitDistanceEvaluator`, we deliberately do NOT bail out as soon
    // as every output is "available" -- a subroutine often writes to an output and
    // then refines it (e.g. copy-input-to-output, then add-something-to-output), so
    // an early bail-out would clip the last instruction. Per-call step budget is the
    // dispatcher's only upper bound; the evaluator that mounts the library is
    // responsible for sizing it sensibly.
    const uint32_t max_steps = entry.max_steps_per_call == 0 ? 1U : entry.max_steps_per_call;
    uint32_t steps = 0;
    while (steps < max_steps) {
      if (!step(callee_session, false)) {
        break;
      }
      ++steps;
    }

    // Propagate output writes back to the caller. Slots that the callee never wrote to
    // are read back as 0 (the default value), which scores via bit-distance against the
    // expected output and gives the GA partial credit -- preferable to a hard zero that
    // flattens the gradient.
    for (uint8_t i = 0; i < ops.output_arity; ++i) {
      const int32_t callee_value =
          callee_session.getVariableValue(output_offset + static_cast<int32_t>(i), true);
      session.setVariableValue(ops.outputs.at(i).variable_index, ops.outputs.at(i).follow_links,
                               callee_value);
    }
  } catch (...) {
    // Bad variable references on either side, library inconsistencies that somehow slip
    // past the up-front checks, or callee-side exceptions all get funnelled to the same
    // place: mark the caller abnormal and let the outer evaluator handle scoring.
    panic("CallSubroutine raised an exception during execution.");
    session.setExitedAbnormally();
    return false;
  }
  return true;
}

bool CpuVirtualMachine::step(VmSession& session, bool dry_run) {
  OpCode instruction = OpCode::NoOp;
  try {
    // Try to get next major instruction symbol.
    instruction = static_cast<OpCode>(session.getData1());
  } catch (...) {
    // Unable to get more data; the program came to an unexpected end.
    panic("Program ended unexpectedly.");
    // Mark the program session as exited abnormally.
    session.setExitedAbnormally();
    return false;
  }

  session.informAboutStep(instruction);

  switch (instruction) {
  case OpCode::NoOp:
    break;

  case OpCode::DeclareVariable: {
    const int32_t variable_index = session.getData4();
    const auto variable_type = static_cast<Program::VariableType>(session.getData1());
    debug("register_variable(" + std::to_string(variable_index) + ", " +
          std::to_string(static_cast<int32_t>(variable_type)) + ")");
    if (!dry_run) {
      session.registerVariable(variable_index, variable_type);
    }
  } break;

  case OpCode::SetVariable: {
    const int32_t variable_index = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    const int32_t variable_content = session.getData4();
    debug("set_variable(" + std::to_string(variable_index) + ", " +
          std::to_string(variable_content) + ", " + to_string(follow_links) + ")");
    if (!dry_run) {
      session.setVariable(variable_index, variable_content, follow_links);
    }
  } break;

  case OpCode::UndeclareVariable: {
    const int32_t variable_index = session.getData4();
    debug("undeclare_variable(" + std::to_string(variable_index) + ")");
    if (!dry_run) {
      session.unregisterVariable(variable_index);
    }
  } break;

  case OpCode::AddConstantToVariable: {
    const int32_t variable_index = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    const int32_t constant = session.getData4();
    debug("add_constant_to_variable(" + std::to_string(variable_index) + ", " +
          to_string(follow_links) + ", " + std::to_string(constant) + ")");
    if (!dry_run) {
      session.addConstantToVariable(variable_index, constant, follow_links);
    }
  } break;

  case OpCode::AddVariableToVariable: {
    const int32_t source_variable = session.getData4();
    const bool follow_source_links = session.getData1() != 0x0;
    const int32_t destination_variable = session.getData4();
    const bool follow_destination_links = session.getData1() != 0x0;
    debug("add_variable_to_variable(" + std::to_string(source_variable) + ", " +
          to_string(follow_source_links) + ", " + std::to_string(destination_variable) + ", " +
          to_string(follow_destination_links) + ")");
    if (!dry_run) {
      session.addVariableToVariable(
          source_variable, destination_variable, follow_source_links, follow_destination_links);
    }
  } break;

  case OpCode::SubtractConstantFromVariable: {
    const int32_t variable_index = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    const int32_t constant = session.getData4();
    debug("subtract_constant_from_variable(" + std::to_string(variable_index) + ", " +
          to_string(follow_links) + ", " + std::to_string(constant) + ")");
    if (!dry_run) {
      session.subtractConstantFromVariable(variable_index, constant, follow_links);
    }
  } break;

  case OpCode::SubtractVariableFromVariable: {
    const int32_t source_variable = session.getData4();
    const bool follow_source_links = session.getData1() != 0x0;
    const int32_t destination_variable = session.getData4();
    const bool follow_destination_links = session.getData1() != 0x0;
    debug("subtract_variable_from_variable(" + std::to_string(source_variable) + ", " +
          to_string(follow_source_links) + ", " + std::to_string(destination_variable) + ", " +
          to_string(follow_destination_links) + ")");
    if (!dry_run) {
      session.subtractVariableFromVariable(
          source_variable, destination_variable, follow_source_links, follow_destination_links);
    }
  } break;

  case OpCode::RelativeJumpToVariableAddressIfVariableGt0: {
    const int32_t condition_variable = session.getData4();
    const bool follow_condition_links = session.getData1() != 0x0;
    const int32_t addr_variable = session.getData4();
    const bool follow_addr_links = session.getData1() != 0x0;
    debug("relative_jump_to_variable_address_if_variable_gt_0(" +
          std::to_string(condition_variable) + ", " + to_string(follow_condition_links) + ", " +
          std::to_string(addr_variable) + ", " + to_string(follow_addr_links) + ")");
    if (!dry_run) {
      session.relativeJumpToVariableAddressIfVariableGt0(
          condition_variable, follow_condition_links, addr_variable, follow_addr_links);
    }
  } break;

  case OpCode::RelativeJumpToVariableAddressIfVariableLt0: {
    const int32_t condition_variable = session.getData4();
    const bool follow_condition_links = session.getData1() != 0x0;
    const int32_t addr_variable = session.getData4();
    const bool follow_addr_links = session.getData1() != 0x0;
    debug("relative_jump_to_variable_address_if_variable_lt_0(" +
          std::to_string(condition_variable) + ", " + to_string(follow_condition_links) + ", " +
          std::to_string(addr_variable) + ", " + to_string(follow_addr_links) + ")");
    if (!dry_run) {
      session.relativeJumpToVariableAddressIfVariableLt0(
          condition_variable, follow_condition_links, addr_variable, follow_addr_links);
    }
  } break;

  case OpCode::RelativeJumpToVariableAddressIfVariableEq0: {
    const int32_t condition_variable = session.getData4();
    const bool follow_condition_links = session.getData1() != 0x0;
    const int32_t addr_variable = session.getData4();
    const bool follow_addr_links = session.getData1() != 0x0;
    debug("relative_jump_to_variable_address_if_variable_eq_0(" +
          std::to_string(condition_variable) + ", " + to_string(follow_condition_links) + ", " +
          std::to_string(addr_variable) + ", " + to_string(follow_addr_links) + ")");
    if (!dry_run) {
      session.relativeJumpToVariableAddressIfVariableEq0(
          condition_variable, follow_condition_links, addr_variable, follow_addr_links);
    }
  } break;

  case OpCode::AbsoluteJumpToVariableAddressIfVariableGt0: {
    const int32_t condition_variable = session.getData4();
    const bool follow_condition_links = session.getData1() != 0x0;
    const int32_t addr_variable = session.getData4();
    const bool follow_addr_links = session.getData1() != 0x0;
    debug("absolute_jump_to_variable_address_if_variable_gt_0(" +
          std::to_string(condition_variable) + ", " + to_string(follow_condition_links) + ", " +
          std::to_string(addr_variable) + ", " + to_string(follow_addr_links) + ")");
    if (!dry_run) {
      session.absoluteJumpToVariableAddressIfVariableGt0(
          condition_variable, follow_condition_links, addr_variable, follow_addr_links);
    }
  } break;

  case OpCode::AbsoluteJumpToVariableAddressIfVariableLt0: {
    const int32_t condition_variable = session.getData4();
    const bool follow_condition_links = session.getData1() != 0x0;
    const int32_t addr_variable = session.getData4();
    const bool follow_addr_links = session.getData1() != 0x0;
    debug("absolute_jump_to_variable_address_if_variable_lt_0(" +
          std::to_string(condition_variable) + ", " + to_string(follow_condition_links) + ", " +
          std::to_string(addr_variable) + ", " + to_string(follow_addr_links) + ")");
    if (!dry_run) {
      session.absoluteJumpToVariableAddressIfVariableLt0(
          condition_variable, follow_condition_links, addr_variable, follow_addr_links);
    }
  } break;

  case OpCode::AbsoluteJumpToVariableAddressIfVariableEq0: {
    const int32_t condition_variable = session.getData4();
    const bool follow_condition_links = session.getData1() != 0x0;
    const int32_t addr_variable = session.getData4();
    const bool follow_addr_links = session.getData1() != 0x0;
    debug("absolute_jump_to_variable_address_if_variable_eq_0(" +
          std::to_string(condition_variable) + ", " + to_string(follow_condition_links) + ", " +
          std::to_string(addr_variable) + ", " + to_string(follow_addr_links) + ")");
    if (!dry_run) {
      session.absoluteJumpToVariableAddressIfVariableEq0(
          condition_variable, follow_condition_links, addr_variable, follow_addr_links);
    }
  } break;

  case OpCode::RelativeJumpIfVariableGt0: {
    const int32_t condition_variable = session.getData4();
    const bool follow_condition_links = session.getData1() != 0x0;
    const int32_t addr = session.getData4();
    debug("relative_jump_to_address_if_variable_gt_0(" + std::to_string(condition_variable) + ", " +
          to_string(follow_condition_links) + ", " + std::to_string(addr) + ")");
    if (!dry_run) {
      session.relativeJumpToAddressIfVariableGt0(condition_variable, follow_condition_links, addr);
    }
  } break;

  case OpCode::RelativeJumpIfVariableLt0: {
    const int32_t condition_variable = session.getData4();
    const bool follow_condition_links = session.getData1() != 0x0;
    const int32_t addr = session.getData4();
    debug("relative_jump_to_address_if_variable_lt_0(" + std::to_string(condition_variable) + ", " +
          to_string(follow_condition_links) + ", " + std::to_string(addr) + ")");
    if (!dry_run) {
      session.relativeJumpToAddressIfVariableLt0(condition_variable, follow_condition_links, addr);
    }
  } break;

  case OpCode::RelativeJumpIfVariableEq0: {
    const int32_t condition_variable = session.getData4();
    const bool follow_condition_links = session.getData1() != 0x0;
    const int32_t addr = session.getData4();
    debug("relative_jump_to_address_if_variable_eq_0(" + std::to_string(condition_variable) + ", " +
          to_string(follow_condition_links) + ", " + std::to_string(addr) + ")");
    if (!dry_run) {
      session.relativeJumpToAddressIfVariableEq0(condition_variable, follow_condition_links, addr);
    }
  } break;

  case OpCode::AbsoluteJumpIfVariableGt0: {
    const int32_t condition_variable = session.getData4();
    const bool follow_condition_links = session.getData1() != 0x0;
    const int32_t addr = session.getData4();
    debug("absolute_jump_to_address_if_variable_gt_0(" + std::to_string(condition_variable) + ", " +
          to_string(follow_condition_links) + ", " + std::to_string(addr) + ")");
    if (!dry_run) {
      session.absoluteJumpToAddressIfVariableGt0(condition_variable, follow_condition_links, addr);
    }
  } break;

  case OpCode::AbsoluteJumpIfVariableLt0: {
    const int32_t condition_variable = session.getData4();
    const bool follow_condition_links = session.getData1() != 0x0;
    const int32_t addr = session.getData4();
    debug("absolute_jump_to_address_if_variable_lt_0(" + std::to_string(condition_variable) + ", " +
          to_string(follow_condition_links) + ", " + std::to_string(addr) + ")");
    if (!dry_run) {
      session.absoluteJumpToAddressIfVariableLt0(condition_variable, follow_condition_links, addr);
    }
  } break;

  case OpCode::AbsoluteJumpIfVariableEq0: {
    const int32_t condition_variable = session.getData4();
    const bool follow_condition_links = session.getData1() != 0x0;
    const int32_t addr = session.getData4();
    debug("absolute_jump_to_address_if_variable_eq_0(" + std::to_string(condition_variable) + ", " +
          to_string(follow_condition_links) + ", " + std::to_string(addr) + ")");
    if (!dry_run) {
      session.absoluteJumpToAddressIfVariableEq0(condition_variable, follow_condition_links, addr);
    }
  } break;

  case OpCode::LoadMemorySizeIntoVariable: {
    const int32_t variable = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    debug("load_memory_size_into_variable(" + std::to_string(variable) + ", " +
          to_string(follow_links) + ")");
    if (!dry_run) {
      session.loadMemorySizeIntoVariable(variable, follow_links);
    }
  } break;

  case OpCode::CheckIfVariableIsInput: {
    const int32_t source_variable = session.getData4();
    const bool follow_source_links = session.getData1() != 0x0;
    const int32_t destination_variable = session.getData4();
    const bool follow_destination_links = session.getData1() != 0x0;
    debug("check_if_variable_is_input(" + std::to_string(source_variable) + ", " +
          to_string(follow_source_links) + ", " + std::to_string(destination_variable) + ", " +
          to_string(follow_destination_links) + ")");
    if (!dry_run) {
      session.checkIfVariableIsInput(
          source_variable, follow_source_links, destination_variable, follow_destination_links);
    }
  } break;

  case OpCode::CheckIfVariableIsOutput: {
    const int32_t source_variable = session.getData4();
    const bool follow_source_links = session.getData1() != 0x0;
    const int32_t destination_variable = session.getData4();
    const bool follow_destination_links = session.getData1() != 0x0;
    debug("check_if_variable_is_output(" + std::to_string(source_variable) + ", " +
          to_string(follow_source_links) + ", " + std::to_string(destination_variable) + ", " +
          to_string(follow_destination_links) + ")");
    if (!dry_run) {
      session.checkIfVariableIsOutput(
          source_variable, follow_source_links, destination_variable, follow_destination_links);
    }
  } break;

  case OpCode::LoadInputCountIntoVariable: {
    const int32_t variable = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    debug("load_input_count_into_variable(" + std::to_string(variable) + ", " +
          to_string(follow_links) + ")");
    if (!dry_run) {
      session.loadInputCountIntoVariable(variable, follow_links);
    }
  } break;

  case OpCode::LoadOutputCountIntoVariable: {
    const int32_t variable = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    debug("load_output_count_into_variable(" + std::to_string(variable) + ", " +
          to_string(follow_links) + ")");
    if (!dry_run) {
      session.loadOutputCountIntoVariable(variable, follow_links);
    }
  } break;

  case OpCode::LoadCurrentAddressIntoVariable: {
    const int32_t variable = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    debug("load_current_address_into_variable(" + std::to_string(variable) + ", " +
          to_string(follow_links) + ")");
    if (!dry_run) {
      session.loadCurrentAddressIntoVariable(variable, follow_links);
    }
  } break;

  case OpCode::PrintVariable: {
    const int32_t variable_index = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    const bool as_char = session.getData1() != 0x0;
    debug("print_variable(" + std::to_string(variable_index) + ", " + to_string(follow_links) +
          ", " + to_string(as_char) + ")");
    if (!dry_run) {
      session.printVariable(variable_index, follow_links, as_char);
    }
  } break;

  case OpCode::SetStringTableEntry: {
    const int32_t string_table_index = session.getData4();
    const int16_t string_length = session.getData2();
    std::vector<char> buffer(string_length);
    for (int32_t idx = 0; idx < string_length; ++idx) {
      buffer[idx] = session.getData1();
    }
    const auto string_content = std::string(buffer.data(), string_length);
    debug("set_string_table_entry(" + std::to_string(string_table_index) + ", " +
          std::to_string(string_length) + ", '" + string_content + "')");
    if (!dry_run) {
      session.setStringTableEntry(string_table_index, string_content);
    }
  } break;

  case OpCode::PrintStringFromStringTable: {
    const int32_t string_table_index = session.getData4();
    debug("print_string_from_string_table(" + std::to_string(string_table_index) + ")");
    if (!dry_run) {
      session.printStringFromStringTable(string_table_index);
    }
  } break;

  case OpCode::LoadStringTableLimitIntoVariable: {
    const int32_t variable_index = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    debug("load_string_table_limit_into_variable(" + std::to_string(variable_index) + ", " +
          to_string(follow_links) + ")");
    if (!dry_run) {
      session.loadStringTableLimitIntoVariable(variable_index, follow_links);
    }
  } break;

  case OpCode::Terminate: {
    const int8_t return_code = session.getData1();
    debug("terminate(" + std::to_string(return_code) + ")");
    if (!dry_run) {
      session.terminate(return_code);
    }
  } break;

  case OpCode::CopyVariable: {
    const int32_t source_variable = session.getData4();
    const bool follow_source_links = session.getData1() != 0x0;
    const int32_t destination_variable = session.getData4();
    const bool follow_destination_links = session.getData1() != 0x0;
    debug("copy_variable(" + std::to_string(source_variable) + ", " +
          to_string(follow_source_links) + ", " + std::to_string(destination_variable) + ", " +
          to_string(follow_destination_links) + ")");
    if (!dry_run) {
      session.copyVariable(
          source_variable, follow_source_links, destination_variable, follow_destination_links);
    }
  } break;

  case OpCode::LoadStringItemLengthIntoVariable: {
    const int32_t string_table_index = session.getData4();
    const int32_t variable_index = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    debug("load_string_item_length_into_variable(" + std::to_string(string_table_index) + ", " +
          std::to_string(variable_index) + ", " + to_string(follow_links) + ")");
    if (!dry_run) {
      session.loadStringItemLengthIntoVariable(string_table_index, variable_index, follow_links);
    }
  } break;

  case OpCode::LoadStringItemIntoVariables: {
    const int32_t string_table_index = session.getData4();
    const int32_t start_variable_index = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    debug("load_string_item_into_variables(" + std::to_string(string_table_index) + ", " +
          std::to_string(start_variable_index) + ", " + to_string(follow_links) + ")");
    if (!dry_run) {
      session.loadStringItemIntoVariables(string_table_index, start_variable_index, follow_links);
    }
  } break;

  case OpCode::PerformSystemCall: {
    const int8_t major_code = session.getData1();
    const int8_t minor_code = session.getData1();
    const int32_t variable_index = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    debug("perform_system_call(" + std::to_string(major_code) + ", " + std::to_string(minor_code) +
          ", " + std::to_string(variable_index) + ", " + to_string(follow_links) + ")");
    if (!dry_run) {
      session.performSystemCall(major_code, minor_code, variable_index, follow_links);
    }
  } break;

  case OpCode::BitShiftVariableLeft: {
    const int32_t variable_index = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    const int8_t places = session.getData1();
    debug("bit_shift_variable_left(" + std::to_string(variable_index) + ", " +
          to_string(follow_links) + ", " + std::to_string(places) + ")");
    if (!dry_run) {
      session.bitShiftVariable(variable_index, follow_links, places);
    }
  } break;

  case OpCode::BitShiftVariableRight: {
    const int32_t variable_index = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    const auto places = static_cast<int8_t>(-session.getData1());
    debug("bit_shift_variable_right(" + std::to_string(variable_index) + ", " +
          to_string(follow_links) + ", " + std::to_string(places) + ")");
    if (!dry_run) {
      session.bitShiftVariable(variable_index, follow_links, places);
    }
  } break;

  case OpCode::BitWiseInvertVariable: {
    const int32_t variable_index = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    debug("bit_wise_invert_variable(" + std::to_string(variable_index) + ", " +
          to_string(follow_links) + ")");
    if (!dry_run) {
      session.bitWiseInvertVariable(variable_index, follow_links);
    }
  } break;

  case OpCode::BitWiseAndTwoVariables: {
    const int32_t variable_index_a = session.getData4();
    const bool follow_links_a = session.getData1() != 0x0;
    const int32_t variable_index_b = session.getData4();
    const bool follow_links_b = session.getData1() != 0x0;
    debug("bit_wise_and_two_variables(" + std::to_string(variable_index_a) + ", " +
          to_string(follow_links_a) + ", " + std::to_string(variable_index_b) + ", " +
          to_string(follow_links_b) + ")");
    if (!dry_run) {
      session.bitWiseAndTwoVariables(
          variable_index_a, follow_links_a, variable_index_b, follow_links_b);
    }
  } break;

  case OpCode::BitWiseOrTwoVariables: {
    const int32_t variable_index_a = session.getData4();
    const bool follow_links_a = session.getData1() != 0x0;
    const int32_t variable_index_b = session.getData4();
    const bool follow_links_b = session.getData1() != 0x0;
    debug("bit_wise_or_two_variables(" + std::to_string(variable_index_a) + ", " +
          to_string(follow_links_a) + ", " + std::to_string(variable_index_b) + ", " +
          to_string(follow_links_b) + ")");
    if (!dry_run) {
      session.bitWiseOrTwoVariables(
          variable_index_a, follow_links_a, variable_index_b, follow_links_b);
    }
  } break;

  case OpCode::BitWiseXorTwoVariables: {
    const int32_t variable_index_a = session.getData4();
    const bool follow_links_a = session.getData1() != 0x0;
    const int32_t variable_index_b = session.getData4();
    const bool follow_links_b = session.getData1() != 0x0;
    debug("bit_wise_xor_two_variables(" + std::to_string(variable_index_a) + ", " +
          to_string(follow_links_a) + ", " + std::to_string(variable_index_b) + ", " +
          to_string(follow_links_b) + ")");
    if (!dry_run) {
      session.bitWiseXorTwoVariables(
          variable_index_a, follow_links_a, variable_index_b, follow_links_b);
    }
  } break;

  case OpCode::LoadRandomValueIntoVariable: {
    const int32_t variable = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    debug("load_random_value_into_variable(" + std::to_string(variable) + ", " +
          to_string(follow_links) + ")");
    if (!dry_run) {
      session.loadRandomValueIntoVariable(variable, follow_links);
    }
  } break;

  case OpCode::ModuloVariableByConstant: {
    const int32_t variable = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    const int32_t modulo_constant = session.getData4();
    debug("modulo_variable_by_constant(" + std::to_string(variable) + ", " +
          to_string(follow_links) + ", " + std::to_string(modulo_constant) + ")");
    if (!dry_run) {
      session.moduloVariableByConstant(variable, follow_links, modulo_constant);
    }
  } break;

  case OpCode::ModuloVariableByVariable: {
    const int32_t variable = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    const int32_t modulo_variable = session.getData4();
    const bool modulo_follow_links = session.getData1() != 0x0;
    debug("modulo_variable_by_variable(" + std::to_string(variable) + ", " +
          to_string(follow_links) + ", " + std::to_string(modulo_variable) + ", " +
          to_string(modulo_follow_links) + ")");
    if (!dry_run) {
      session.moduloVariableByVariable(
          variable, follow_links, modulo_variable, modulo_follow_links);
    }
  } break;

  case OpCode::RotateVariableLeft: {
    const int32_t variable = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    const int8_t places = session.getData1();
    debug("rotate_variable_left(" + std::to_string(variable) + ", " + to_string(follow_links) +
          ", " + std::to_string(places) + ")");
    if (!dry_run) {
      session.rotateVariable(variable, follow_links, places);
    }
  } break;

  case OpCode::RotateVariableRight: {
    const int32_t variable = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    const auto places = static_cast<int8_t>(-session.getData1());
    debug("rotate_variable_right(" + std::to_string(variable) + ", " + to_string(follow_links) +
          ", " + std::to_string(places) + ")");
    if (!dry_run) {
      session.rotateVariable(variable, follow_links, places);
    }
  } break;

  case OpCode::UnconditionalJumpToAbsoluteAddress: {
    const int32_t addr = session.getData4();
    debug("unconditional_jump_to_absolute_address(" + std::to_string(addr) + ")");
    if (!dry_run) {
      session.unconditionalJumpToAbsoluteAddress(addr);
    }
  } break;

  case OpCode::UnconditionalJumpToAbsoluteVariableAddress: {
    const int32_t variable = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    debug("unconditional_jump_to_absolute_variable_address(" + std::to_string(variable) + ", " +
          to_string(follow_links) + ")");
    if (!dry_run) {
      session.unconditionalJumpToAbsoluteVariableAddress(variable, follow_links);
    }
  } break;

  case OpCode::UnconditionalJumpToRelativeAddress: {
    const int32_t addr = session.getData4();
    debug("unconditional_jump_to_relative_address(" + std::to_string(addr) + ")");
    if (!dry_run) {
      session.unconditionalJumpToRelativeAddress(addr);
    }
  } break;

  case OpCode::UnconditionalJumpToRelativeVariableAddress: {
    const int32_t variable = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    if (!dry_run) {
      session.unconditionalJumpToRelativeVariableAddress(variable, follow_links);
    }
    debug("unconditional_jump_to_relative_variable_address(" + std::to_string(variable) + ", " +
          to_string(follow_links) + ")");
  } break;

  case OpCode::CheckIfInputWasSet: {
    const int32_t variable = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    const int32_t destination_variable = session.getData4();
    const bool follow_destination_links = session.getData1() != 0x0;
    debug("check_if_input_was_set(" + std::to_string(variable) + ", " + to_string(follow_links) +
          ", " + std::to_string(destination_variable) + ", " + to_string(follow_destination_links) +
          ")");
    if (!dry_run) {
      session.checkIfInputWasSet(
          variable, follow_links, destination_variable, follow_destination_links);
    }
  } break;

  case OpCode::LoadStringTableItemLengthLimitIntoVariable: {
    const int32_t variable = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    if (!dry_run) {
      session.loadStringTableItemLengthLimitIntoVariable(variable, follow_links);
    }
    debug("load_string_table_item_length_limit_into_variable(" + std::to_string(variable) + ", " +
          to_string(follow_links) + ")");
  } break;

  case OpCode::PushVariableOnStack: {
    const int32_t stack_variable = session.getData4();
    const bool stack_follow_links = session.getData1() != 0x0;
    const int32_t variable = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    debug("push_variable_on_stack(" + std::to_string(stack_variable) + ", " +
          to_string(stack_follow_links) + ", " + std::to_string(variable) + ", " +
          to_string(follow_links) + ")");
    if (!dry_run) {
      session.pushVariableOnStack(stack_variable, stack_follow_links, variable, follow_links);
    }
  } break;

  case OpCode::PushConstantOnStack: {
    const int32_t stack_variable = session.getData4();
    const bool stack_follow_links = session.getData1() != 0x0;
    const int32_t constant = session.getData4();
    debug("push_constant_on_stack(" + std::to_string(stack_variable) + ", " +
          to_string(stack_follow_links) + ", " + std::to_string(constant) + ")");
    if (!dry_run) {
      session.pushConstantOnStack(stack_variable, stack_follow_links, constant);
    }
  } break;

  case OpCode::PopVariableFromStack: {
    const int32_t stack_variable = session.getData4();
    const bool stack_follow_links = session.getData1() != 0x0;
    const int32_t variable = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    debug("pop_variable_from_stack(" + std::to_string(stack_variable) + ", " +
          to_string(stack_follow_links) + ", " + std::to_string(variable) + ", " +
          to_string(follow_links) + ")");
    if (!dry_run) {
      session.popVariableFromStack(stack_variable, stack_follow_links, variable, follow_links);
    }
  } break;

  case OpCode::PopTopItemFromStack: {
    const int32_t stack_variable = session.getData4();
    const bool stack_follow_links = session.getData1() != 0x0;
    debug("pop_top_item_from_stack(" + std::to_string(stack_variable) + ", " +
          to_string(stack_follow_links) + ")");
    if (!dry_run) {
      session.popTopItemFromStack(stack_variable, stack_follow_links);
    }
  } break;

  case OpCode::CheckIfStackIsEmpty: {
    const int32_t stack_variable = session.getData4();
    const bool stack_follow_links = session.getData1() != 0x0;
    const int32_t variable = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    debug("check_if_stack_is_empty(" + std::to_string(stack_variable) + ", " +
          to_string(stack_follow_links) + ", " + std::to_string(variable) + ", " +
          to_string(follow_links) + ")");
    if (!dry_run) {
      session.checkIfStackIsEmpty(stack_variable, stack_follow_links, variable, follow_links);
    }
  } break;

  case OpCode::SwapVariables: {
    const int32_t variable_a = session.getData4();
    const bool follow_links_a = session.getData1() != 0x0;
    const int32_t variable_b = session.getData4();
    const bool follow_links_b = session.getData1() != 0x0;
    debug("swap_variables(" + std::to_string(variable_a) + ", " + to_string(follow_links_a) + ", " +
          std::to_string(variable_b) + ", " + to_string(follow_links_b) + ")");
    if (!dry_run) {
      session.swapVariables(variable_a, follow_links_a, variable_b, follow_links_b);
    }
  } break;

  case OpCode::SetVariableStringTableEntry: {
    const int32_t string_table_variable_index = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    const int16_t string_length = session.getData2();
    std::vector<char> buffer(string_length);
    for (int32_t idx = 0; idx < string_length; ++idx) {
      buffer[idx] = session.getData1();
    }
    const auto string_content = std::string(buffer.data(), string_length);
    debug("set_variable_string_table_entry(" + std::to_string(string_table_variable_index) + ", " +
          to_string(follow_links) + ", " + std::to_string(string_length) + ", '" + string_content +
          "')");
    if (!dry_run) {
      session.setVariableStringTableEntry(
          string_table_variable_index, follow_links, string_content);
    }
  } break;

  case OpCode::PrintVariableStringFromStringTable: {
    const int32_t variable = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    debug("print_variable_string_from_string_table(" + std::to_string(variable) + ", " +
          to_string(follow_links) + ")");
    if (!dry_run) {
      session.printVariableStringFromStringTable(variable, follow_links);
    }
  } break;

  case OpCode::LoadVariableStringItemLengthIntoVariable: {
    const int32_t string_item_variable = session.getData4();
    const bool string_item_follow_links = session.getData1() != 0x0;
    const int32_t variable = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    debug("load_variable_string_item_length_into_variable(" + std::to_string(string_item_variable) +
          ", " + to_string(string_item_follow_links) + ", " + std::to_string(variable) + ", " +
          to_string(follow_links) + ")");
    if (!dry_run) {
      session.loadVariableStringItemLengthIntoVariable(
          string_item_variable, string_item_follow_links, variable, follow_links);
    }
  } break;

  case OpCode::LoadVariableStringItemIntoVariables: {
    const int32_t string_item_variable = session.getData4();
    const bool string_item_follow_links = session.getData1() != 0x0;
    const int32_t variable = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    debug("load_variable_string_item_into_variable(" + std::to_string(string_item_variable) + ", " +
          to_string(string_item_follow_links) + ", " + std::to_string(variable) + ", " +
          to_string(follow_links) + ")");
    if (!dry_run) {
      session.loadVariableStringItemIntoVariables(
          string_item_variable, string_item_follow_links, variable, follow_links);
    }
  } break;

  case OpCode::TerminateWithVariableReturnCode: {
    const int32_t variable = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    debug("terminate_with_variable_return_code(" + std::to_string(variable) + ", " +
          to_string(follow_links) + ")");
    if (!dry_run) {
      session.terminateWithVariableReturnCode(variable, follow_links);
    }
  } break;

  case OpCode::VariableBitShiftVariableLeft: {
    const int32_t variable = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    const int32_t places_variable = session.getData4();
    const bool places_follow_links = session.getData1() != 0x0;
    debug("variable_bit_shift_variable_left(" + std::to_string(variable) + ", " +
          to_string(follow_links) + ", " + std::to_string(places_variable) + ", " +
          to_string(places_follow_links) + ")");
    if (!dry_run) {
      session.variableBitShiftVariableLeft(
          variable, follow_links, places_variable, places_follow_links);
    }
  } break;

  case OpCode::VariableBitShiftVariableRight: {
    const int32_t variable = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    const int32_t places_variable = session.getData4();
    const bool places_follow_links = session.getData1() != 0x0;
    debug("variable_bit_shift_variable_right(" + std::to_string(variable) + ", " +
          to_string(follow_links) + ", " + std::to_string(places_variable) + ", " +
          to_string(places_follow_links) + ")");
    if (!dry_run) {
      session.variableBitShiftVariableRight(
          variable, follow_links, places_variable, places_follow_links);
    }
  } break;

  case OpCode::VariableRotateVariableLeft: {
    const int32_t variable = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    const int32_t places_variable = session.getData4();
    const bool places_follow_links = session.getData1() != 0x0;
    debug("variable_rotate_variable_left(" + std::to_string(variable) + ", " +
          to_string(follow_links) + ", " + std::to_string(places_variable) + ", " +
          to_string(places_follow_links) + ")");
    if (!dry_run) {
      session.variableRotateVariableLeft(
          variable, follow_links, places_variable, places_follow_links);
    }
  } break;

  case OpCode::VariableRotateVariableRight: {
    const int32_t variable = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    const int32_t places_variable = session.getData4();
    const bool places_follow_links = session.getData1() != 0x0;
    debug("variable_rotate_variable_right(" + std::to_string(variable) + ", " +
          to_string(follow_links) + ", " + std::to_string(places_variable) + ", " +
          to_string(places_follow_links) + ")");
    if (!dry_run) {
      session.variableRotateVariableRight(
          variable, follow_links, places_variable, places_follow_links);
    }
  } break;

  case OpCode::CompareIfVariableGtConstant: {
    const int32_t variable = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    const int32_t constant = session.getData4();
    const int32_t target_variable = session.getData4();
    const bool target_follow_links = session.getData1() != 0x0;
    debug("compare_if_variable_gt_constant(" + std::to_string(variable) + ", " +
          to_string(follow_links) + ", " + std::to_string(constant) + ", " +
          std::to_string(target_variable) + ", " + to_string(target_follow_links));
    if (!dry_run) {
      session.compareIfVariableGtConstant(
          variable, follow_links, constant, target_variable, target_follow_links);
    }
  } break;

  case OpCode::CompareIfVariableLtConstant: {
    const int32_t variable = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    const int32_t constant = session.getData4();
    const int32_t target_variable = session.getData4();
    const bool target_follow_links = session.getData1() != 0x0;
    debug("compare_if_variable_lt_constant(" + std::to_string(variable) + ", " +
          to_string(follow_links) + ", " + std::to_string(constant) + ", " +
          std::to_string(target_variable) + ", " + to_string(target_follow_links));
    if (!dry_run) {
      session.compareIfVariableLtConstant(
          variable, follow_links, constant, target_variable, target_follow_links);
    }
  } break;

  case OpCode::CompareIfVariableEqConstant: {
    const int32_t variable = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    const int32_t constant = session.getData4();
    const int32_t target_variable = session.getData4();
    const bool target_follow_links = session.getData1() != 0x0;
    debug("compare_if_variable_eq_constant(" + std::to_string(variable) + ", " +
          to_string(follow_links) + ", " + std::to_string(constant) + ", " +
          std::to_string(target_variable) + ", " + to_string(target_follow_links));
    if (!dry_run) {
      session.compareIfVariableEqConstant(
          variable, follow_links, constant, target_variable, target_follow_links);
    }
  } break;

  case OpCode::CompareIfVariableGtVariable: {
    const int32_t variable_a = session.getData4();
    const bool follow_links_a = session.getData1() != 0x0;
    const int32_t variable_b = session.getData4();
    const bool follow_links_b = session.getData1() != 0x0;
    const int32_t target_variable = session.getData4();
    const bool target_follow_links = session.getData1() != 0x0;
    debug("compare_if_variable_gt_variable(" + std::to_string(variable_a) + ", " +
          to_string(follow_links_a) + ", " + std::to_string(variable_b) + ", " +
          to_string(follow_links_b) + ", " + std::to_string(target_variable) + ", " +
          to_string(target_follow_links));
    if (!dry_run) {
      session.compareIfVariableGtVariable(variable_a,
                                          follow_links_a,
                                          variable_b,
                                          follow_links_b,
                                          target_variable,
                                          target_follow_links);
    }
  } break;

  case OpCode::CompareIfVariableLtVariable: {
    const int32_t variable_a = session.getData4();
    const bool follow_links_a = session.getData1() != 0x0;
    const int32_t variable_b = session.getData4();
    const bool follow_links_b = session.getData1() != 0x0;
    const int32_t target_variable = session.getData4();
    const bool target_follow_links = session.getData1() != 0x0;
    debug("compare_if_variable_lt_variable(" + std::to_string(variable_a) + ", " +
          to_string(follow_links_a) + ", " + std::to_string(variable_b) + ", " +
          to_string(follow_links_b) + ", " + std::to_string(target_variable) + ", " +
          to_string(target_follow_links));
    if (!dry_run) {
      session.compareIfVariableLtVariable(variable_a,
                                          follow_links_a,
                                          variable_b,
                                          follow_links_b,
                                          target_variable,
                                          target_follow_links);
    }
  } break;

  case OpCode::CompareIfVariableEqVariable: {
    const int32_t variable_a = session.getData4();
    const bool follow_links_a = session.getData1() != 0x0;
    const int32_t variable_b = session.getData4();
    const bool follow_links_b = session.getData1() != 0x0;
    const int32_t target_variable = session.getData4();
    const bool target_follow_links = session.getData1() != 0x0;
    debug("compare_if_variable_eq_variable(" + std::to_string(variable_a) + ", " +
          to_string(follow_links_a) + ", " + std::to_string(variable_b) + ", " +
          to_string(follow_links_b) + ", " + std::to_string(target_variable) + ", " +
          to_string(target_follow_links));
    if (!dry_run) {
      session.compareIfVariableEqVariable(variable_a,
                                          follow_links_a,
                                          variable_b,
                                          follow_links_b,
                                          target_variable,
                                          target_follow_links);
    }
  } break;

  case OpCode::GetMaxOfVariableAndConstant: {
    const int32_t variable = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    const int32_t constant = session.getData4();
    const int32_t target_variable = session.getData4();
    const bool target_follow_links = session.getData1() != 0x0;
    debug("get_max_of_variable_and_constant(" + std::to_string(variable) + ", " +
          to_string(follow_links) + ", " + std::to_string(constant) + ", " +
          std::to_string(target_variable) + ", " + to_string(target_follow_links));
    if (!dry_run) {
      session.getMaxOfVariableAndConstant(
          variable, follow_links, constant, target_variable, target_follow_links);
    }
  } break;

  case OpCode::GetMinOfVariableAndConstant: {
    const int32_t variable = session.getData4();
    const bool follow_links = session.getData1() != 0x0;
    const int32_t constant = session.getData4();
    const int32_t target_variable = session.getData4();
    const bool target_follow_links = session.getData1() != 0x0;
    debug("get_min_of_variable_and_constant(" + std::to_string(variable) + ", " +
          to_string(follow_links) + ", " + std::to_string(constant) + ", " +
          std::to_string(target_variable) + ", " + to_string(target_follow_links));
    if (!dry_run) {
      session.getMinOfVariableAndConstant(
          variable, follow_links, constant, target_variable, target_follow_links);
    }
  } break;

  case OpCode::GetMaxOfVariableAndVariable: {
    const int32_t variable_a = session.getData4();
    const bool follow_links_a = session.getData1() != 0x0;
    const int32_t variable_b = session.getData4();
    const bool follow_links_b = session.getData1() != 0x0;
    const int32_t target_variable = session.getData4();
    const bool target_follow_links = session.getData1() != 0x0;
    debug("get_max_of_variable_and_variable(" + std::to_string(variable_a) + ", " +
          to_string(follow_links_a) + ", " + std::to_string(variable_b) + ", " +
          to_string(follow_links_b) + ", " + std::to_string(target_variable) + ", " +
          to_string(target_follow_links));
    if (!dry_run) {
      session.getMaxOfVariableAndVariable(variable_a,
                                          follow_links_a,
                                          variable_b,
                                          follow_links_b,
                                          target_variable,
                                          target_follow_links);
    }
  } break;

  case OpCode::GetMinOfVariableAndVariable: {
    const int32_t variable_a = session.getData4();
    const bool follow_links_a = session.getData1() != 0x0;
    const int32_t variable_b = session.getData4();
    const bool follow_links_b = session.getData1() != 0x0;
    const int32_t target_variable = session.getData4();
    const bool target_follow_links = session.getData1() != 0x0;
    debug("get_min_of_variable_and_variable(" + std::to_string(variable_a) + ", " +
          to_string(follow_links_a) + ", " + std::to_string(variable_b) + ", " +
          to_string(follow_links_b) + ", " + std::to_string(target_variable) + ", " +
          to_string(target_follow_links));
    if (!dry_run) {
      session.getMinOfVariableAndVariable(variable_a,
                                          follow_links_a,
                                          variable_b,
                                          follow_links_b,
                                          target_variable,
                                          target_follow_links);
    }
  } break;

  case OpCode::CallSubroutine: {
    if (!dispatchCallSubroutine(session, dry_run)) {
      return false;
    }
  } break;

  default: {
    throw std::invalid_argument("Undefined instruction reached.");
  }
  }

  return !session.isAtEnd();
}

void CpuVirtualMachine::message(MessageSeverity severity, const std::string& message) noexcept {
  std::ostream& out_stream = std::cout;
  uint32_t color_fg = 0;
  uint32_t color_bg = 0;
  std::string prefix;

  switch (severity) {
  case MessageSeverity::Debug: {
    color_fg = 90;
    prefix = "DBG";
  } break;

  case MessageSeverity::Info: {
    color_fg = 97;
    prefix = "INF";
  } break;

  case MessageSeverity::Warning: {
    color_fg = 33;
    prefix = "WRN";
  } break;

  case MessageSeverity::Error: {
    color_fg = 31;
    prefix = "ERR";
  } break;

  case MessageSeverity::Panic: {
    color_fg = 31;
    color_bg = 107;
    prefix = "PNC";
  } break;
  }

  const std::time_t time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  std::vector<char> timestamp_buffer(100);

  struct tm result {};
  const size_t timestamp_length =
      std::strftime(timestamp_buffer.data(), 100, "%F %T", localtime_r(&time, &result));
  const std::string timestamp(timestamp_buffer.data(), timestamp_length);
  out_stream << "\033[1;" << color_fg << (color_bg != 0 ? ";" + std::to_string(color_bg) : "")
             << "m"
             << "[" << timestamp << " " << prefix << "] " << message << "\033[0m" << std::endl;
}

} // namespace beast
