// Standard
#include <algorithm>
#include <iostream>
#include <numeric>
#include <random>
#include <vector>

// BEAST
#include <beast/beast.hpp>

int main(int /*argc*/, char** /*argv*/) {
  /* Print BEAST library version. */
  const auto version = beast::getVersion();
  std::cout << "Using BEAST library version " << static_cast<uint32_t>(version[0]) << "."
            << static_cast<uint32_t>(version[1]) << "." << static_cast<uint32_t>(version[2]) << "." << std::endl;

  /* This program illustrates how to implement the bubblesort algorithm using a
   * BEAST program. */

  /* Initialize a set of variables for generating the random input. These are
     not necessarily
     required for the sorting algorithm, but help to illustrate its practical
     functionality. */
  std::random_device random_device;
  std::mt19937 mersenne_engine{random_device()};
  std::uniform_int_distribution<int32_t> distribution{1, 100};

  const auto generator = [&distribution, &mersenne_engine]() { return distribution(mersenne_engine); };

  /* The input/output variables, but also the algorithm itself can be scaled to
     any number of numbers to sort. In this example, we're sorting 10 random
     numbers using the function objects from above. */
  const int32_t numbers = 10;
  std::vector<int32_t> input(numbers);
  std::generate(input.begin(), input.end(), generator);

  /* Print the unsorted input. */
  std::cout << "Input: ";
  for (int32_t number : input) {
    std::cout << number << " ";
  }
  std::cout << std::endl;

  /* Define the input and output indices used in the program. These are not only
     used internally, but are also used to feed the unsorted input data into the
     program. There are ways to stream this through fewer variables and then use
     arbitrary amounts of variables to sort (see the
     `feedloop` example), but in the scope of this example this is not done. */
  std::vector<int32_t> input_variables(numbers);
  std::vector<int32_t> output_variables(numbers);
  std::iota(input_variables.begin(), input_variables.end(), 0);
  std::iota(output_variables.begin(), output_variables.end(), numbers);

  /* Define bookkeeping variables for the algorithm's loops and operations. */
  const int32_t var_i = 2 * numbers;
  const int32_t var_j = 2 * numbers + 1;
  const int32_t var_temp = 2 * numbers + 2;
  const int32_t var_l1 = 2 * numbers + 3;
  const int32_t var_l2 = 2 * numbers + 4;

  /* Define the actual program. First, declare the algorithm's working
   * variables. */
  beast::Program prg;
  prg.declareVariable(var_i, beast::Program::VariableType::Int32);
  prg.declareVariable(var_j, beast::Program::VariableType::Int32);
  prg.declareVariable(var_temp, beast::Program::VariableType::Int32);
  prg.declareVariable(var_l1, beast::Program::VariableType::Link);
  prg.declareVariable(var_l2, beast::Program::VariableType::Link);

  /* Bubblesort uses two cascaded loops; store their starting addresses here to
     be able to conditionally jump back. */
  prg.setVariable(var_i, 0x0, false);
  const auto outer_loop_start = static_cast<int32_t>(prg.getPointer());
  prg.setVariable(var_j, 0x0, false);
  const auto inner_loop_start = static_cast<int32_t>(prg.getPointer());

  /* Here we use the Link variable type for an indirection operation. `var_l1`
     and `var_l2` are links, so their value is used as a variable address when
     reading/writing to them. This way, we can access variables by knowing their
     index in another variable, allowing array iterations and alike. For
     bubblesort, we need this to iterate through all known numbers of the array
     to sort. */
  prg.copyVariable(var_j, true, var_l1, false);
  prg.copyVariable(var_j, true, var_l2, false);
  prg.addConstantToVariable(var_l2, 1, false);
  prg.compareIfVariableGtVariable(var_l1, true, var_l2, true, var_temp, true);
  beast::Program swap;
  swap.swapVariables(var_l1, true, var_l2, true);
  prg.relativeJumpToAddressIfVariableEqualsZero(var_temp, true, static_cast<int32_t>(swap.getSize()));
  prg.insertProgram(swap);

  /* The next two blocks are the cascaded loop tails; `var_j` controls the inner
     loop, `var_i` controls the outer loop. */
  prg.addConstantToVariable(var_j, 1, false);
  prg.compareIfVariableLtConstant(var_j, false, numbers - 1, var_temp, true);
  prg.absoluteJumpToAddressIfVariableGreaterThanZero(var_temp, true, inner_loop_start);

  prg.addConstantToVariable(var_i, 1, false);
  prg.compareIfVariableLtConstant(var_i, false, numbers - 1, var_temp, true);
  prg.absoluteJumpToAddressIfVariableGreaterThanZero(var_temp, true, outer_loop_start);

  /* Copy working variables to output after execution. */
  for (int32_t idx : output_variables) {
    prg.copyVariable(idx - numbers, true, idx, true);
  }

  /* for informational purposes only, print the sort program's total length. */
  std::cout << "Program length: " << prg.getSize() << " bytes" << std::endl;

  /* Define the session to use, and register input and output variables. */
  beast::VmSession session(std::move(prg), 2 * numbers + 5, 0, 0);
  for (int32_t idx : input_variables) {
    session.setVariableBehavior(idx, beast::VmSession::VariableIoBehavior::Input);
    session.setVariableValue(idx, true, input[idx]);
  }
  for (int32_t idx : output_variables) {
    session.setVariableBehavior(idx, beast::VmSession::VariableIoBehavior::Output);
  }

  /* The CpuVirtualMachine class is used for execution. */
  beast::CpuVirtualMachine virtual_machine;
  /* If desired, the minimum message severity can be adjusted here to print the
     executed byte code operators and their operands. Just uncomment the next
     line to see them during execution. */
  // virtual_machine.setMinimumMessageSeverity(beast::VirtualMachine::MessageSeverity::Debug);
  while (virtual_machine.step(session, false)) {
  }

  /* Print the sorted output. */
  std::cout << "Output: ";
  for (int32_t idx : output_variables) {
    std::cout << session.getVariableValue(idx, true) << " ";
  }
  std::cout << std::endl;

  /* Return the program's return code. This defaults to `0x0`, but could be
     different if the program terminates with an error code. */
  return session.getRuntimeStatistics().return_code;
}
