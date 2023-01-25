// Standard
#include <iostream>
#include <vector>

// BEAST
#include <beast/beast.hpp>

struct Calculation {
  int32_t op_a;
  int32_t op_b;
  int32_t result;
};

enum class CalculationState {
  WaitingForPrgReady,
  SettingOperands,
  WaitingForResult,
  WaitingForQuit
};

int main(int /*argc*/, char** /*argv*/) {
  /* Print BEAST library version. */
  const auto version = beast::getVersion();
  std::cout << "Using BEAST library version "
            << static_cast<uint32_t>(version[0]) << "."
            << static_cast<uint32_t>(version[1]) << "."
            << static_cast<uint32_t>(version[2]) << "." << std::endl;

  /* These `+` calculations are supposed to be performed (operands and expected result) */
  const std::vector<Calculation> add_calculations
      {{1, 1, 2}, {7, 2, 9}, {100, 1000, 1100}, {1, -1, 0}, {-10000, -81, -10081}};

  /* Declare the variable indices and values to use in this program. */
  /* These are the I/O variables */
  const int32_t operand_variable_a = 0;
  const int32_t operand_variable_b = 1;
  const int32_t trigger_calc_variable = 2;
  const int32_t trigger_quit_variable = 3;
  const int32_t result_variable = 4;
  const int32_t prg_ready = 5;
  /* These are the internal variables */
  const int32_t calc_triggered_variable = 6;
  const int32_t quit_triggered_variable = 7;

  /* The individual program parts are defined separately here in order to know their size. This is
     used to perform relative jumps based on conditions and allows to calculate jump addresses
     without knowledge of the exact operator sizes. The main program `prg` further down then
     includes the `add_prg` and `quit_prg` programs using the `insertProgram` method. */

  /* Define the actual addition program part */
  beast::Program add_prg;
  add_prg.addVariableToVariable(operand_variable_a, true, operand_variable_b, true);
  add_prg.copyVariable(operand_variable_b, true, result_variable, true);

  /* Define the quit program part */
  beast::Program quit_prg;
  quit_prg.terminate(0);

  /* Define the main program */
  beast::Program prg;
  prg.declareVariable(calc_triggered_variable, beast::Program::VariableType::Int32);
  prg.setVariable(calc_triggered_variable, 0x0, true);
  prg.declareVariable(quit_triggered_variable, beast::Program::VariableType::Int32);
  prg.setVariable(quit_triggered_variable, 0x0, true);
  prg.setVariable(prg_ready, 0x1, true);

  const auto loop_start_address = static_cast<int32_t>(prg.getPointer());
  prg.checkIfInputWasSet(trigger_calc_variable, true, calc_triggered_variable, true);
  prg.checkIfInputWasSet(trigger_quit_variable, true, quit_triggered_variable, true);
  prg.relativeJumpToAddressIfVariableEqualsZero(
      quit_triggered_variable, true, static_cast<int32_t>(quit_prg.getSize()));
  prg.insertProgram(quit_prg);
  prg.relativeJumpToAddressIfVariableEqualsZero(
      calc_triggered_variable, true, static_cast<int32_t>(add_prg.getSize()));
  prg.insertProgram(add_prg);
  prg.unconditionalJumpToAbsoluteAddress(loop_start_address);

  /* Several I/O variables are registered here, used for feeding operands into the program and
     triggering the calculation, or an intentional program quit. The interesting part in this
     program is that it is not invoked for each operand pair separately, but stays active and
     receives a stream of input, providing results for each once the add operation is complete. */
  beast::VmSession session(std::move(prg), 500, 100, 50);
  session.setVariableBehavior(operand_variable_a, beast::VmSession::VariableIoBehavior::Input);
  session.setVariableBehavior(operand_variable_b, beast::VmSession::VariableIoBehavior::Input);
  session.setVariableBehavior(trigger_calc_variable, beast::VmSession::VariableIoBehavior::Input);
  session.setVariableBehavior(trigger_quit_variable, beast::VmSession::VariableIoBehavior::Input);
  session.setVariableBehavior(result_variable, beast::VmSession::VariableIoBehavior::Output);
  session.setVariableBehavior(prg_ready, beast::VmSession::VariableIoBehavior::Output);

  beast::CpuVirtualMachine virtual_machine;
  uint32_t calculation_index = 0;
  CalculationState state = CalculationState::WaitingForPrgReady;
  while (virtual_machine.step(session)) {
    switch (state) {
    case CalculationState::WaitingForPrgReady: {
      if (session.getVariableValue(prg_ready, true) == 0x1) {
        state = CalculationState::SettingOperands;
      }
    } break;

    case CalculationState::SettingOperands: {
      std::cout << "Setting operands for calculation " << calculation_index
                << " of " << add_calculations.size()
                << ": " << add_calculations[calculation_index].op_a
                << ", " << add_calculations[calculation_index].op_b
                << std::endl;
      session.setVariableValue(operand_variable_a, true, add_calculations[calculation_index].op_a);
      session.setVariableValue(operand_variable_b, true, add_calculations[calculation_index].op_b);
      std::cout << "Triggering calculation" << std::endl;
      session.setVariableValue(trigger_calc_variable, true, 0x0);

      state = CalculationState::WaitingForResult;
    } break;

    case CalculationState::WaitingForResult: {
      if (session.hasOutputDataAvailable(result_variable, true)) {
        const int32_t result = session.getVariableValue(result_variable, true);
        std::cout << "Got result: " << result
                  << " (expected: " << add_calculations[calculation_index].result
                  << ")" << std::endl;

        calculation_index++;
        if (calculation_index >= add_calculations.size()) {
          session.setVariableValue(trigger_quit_variable, true, 0x0);
          state = CalculationState::WaitingForQuit;
        } else {
          state = CalculationState::SettingOperands;
        }
      }
    } break;

    case CalculationState::WaitingForQuit: {
      /* Do nothing, waiting for the program to end. */
    } break;
    }
  }

  return session.getRuntimeStatistics().return_code;
}
