#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

#include <beast/cpu_virtual_machine.hpp>

int main(int /*argc*/, char** /*argv*/) {
  /* Declare the variable indices and values to use in this program. */
  const int32_t input_variable = 0;
  const int32_t count_variable = 1;
  const int32_t count_start_value = 5;
  const int32_t input_changed_variable = 2;

  /* Declare the actual program.

     The main function of this program is the following:
     1. Declare and initialize a set of variables, used for input, counting, and status.
     2. Check in a loop whether the input was set from outside.
     3. For each change of the input from outside, decrease the counting variable by 1.
     4. Once the counting variable reaches 0, terminate the program.
  */
  beast::Program prg;
  prg.declareVariable(count_variable, beast::Program::VariableType::Int32);
  prg.setVariable(count_variable, count_start_value, true);
  prg.declareVariable(input_changed_variable, beast::Program::VariableType::Int32);
  prg.setVariable(input_changed_variable, 0, true);

  const auto loop_start_address = static_cast<int32_t>(prg.getPointer());
  prg.checkIfInputWasSet(input_variable, true, input_changed_variable, true);
  prg.absoluteJumpToAddressIfVariableEqualsZero(input_changed_variable, true, loop_start_address);
  prg.subtractConstantFromVariable(count_variable, 1, true);
  prg.printVariable(count_variable, true, false);
  prg.absoluteJumpToAddressIfVariableGreaterThanZero(count_variable, true, loop_start_address);
  prg.terminate(0);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  session.setVariableBehavior(input_variable, beast::VmSession::VariableIoBehavior::Input);

  beast::CpuVirtualMachine virtual_machine;

  using namespace std::chrono_literals;
  auto last_timepoint = std::chrono::high_resolution_clock::now();
  while (virtual_machine.step(session)) {
    const auto now = std::chrono::high_resolution_clock::now();
    const std::chrono::duration<double, std::milli> elapsed = now - last_timepoint;
    if (elapsed.count() > 1000) {  // 1s has passed
      session.setVariableValue(input_variable, true, 0x0);
      last_timepoint = now;
    }

    std::this_thread::sleep_for(100ms);

    std::cout << session.getPrintBuffer();
    session.clearPrintBuffer();
  }

  return session.getReturnCode();
}
