#include <cstdlib>
#include <iostream>

#include <beast/cpu_virtual_machine.hpp>

int main(int /*argc*/, char** /*argv*/) {
  beast::Program prg(500);
  prg.declareVariable(0, beast::Program::VariableType::Int32);
  prg.undeclareVariable(0);
  prg.declareVariable(0, beast::Program::VariableType::Int32);
  prg.setVariable(0, 0x100, true);
  prg.undeclareVariable(0);
  prg.declareVariable(1, beast::Program::VariableType::Link);
  prg.setVariable(1, 0x2, false);
  prg.declareVariable(2, beast::Program::VariableType::Int32);
  prg.setVariable(2, 0x120, true);
  // prg.addConstantToVariable(0, 1, 2);
  // prg.addVariableToVariable(0, 1, 2);
  // prg.subtractConstantFromVariable(0, 1, 2);
  // prg.subtractVariableFromVariable(0, 1, 2);
  // prg.relativeJumpToVariableAddressIfVariableGreaterThanZero(0, 1);
  // prg.relativeJumpToVariableAddressIfVariableLessThanZero(0, 1);
  // prg.relativeJumpToVariableAddressIfVariableEqualsZero(0, 1);
  // prg.absoluteJumpToVariableAddressIfVariableGreaterThanZero(0, 1);
  // prg.absoluteJumpToVariableAddressIfVariableLessThanZero(0, 1);
  // prg.absoluteJumpToVariableAddressIfVariableEqualsZero(0, 1);
  // prg.relativeJumpToAddressIfVariableGreaterThanZero(0, 1);
  // prg.relativeJumpToAddressIfVariableLessThanZero(0, 1);
  // prg.relativeJumpToAddressIfVariableEqualsZero(0, 1);
  // prg.absoluteJumpToAddressIfVariableGreaterThanZero(0, 1);
  // prg.absoluteJumpToAddressIfVariableLessThanZero(0, 1);
  // prg.absoluteJumpToAddressIfVariableEqualsZero(0, 1);
  // prg.loadMemorySizeIntoVariable(0);
  // prg.checkIfVariableIsInput(0, 1);
  // prg.checkIfVariableIsOutput(0, 1);
  // prg.loadInputCountIntoVariable(0);
  // prg.loadOutputCountIntoVariable(1);
  // prg.loadCurrentAddressIntoVariable(0);
  prg.printVariable(1, true);
  prg.setStringTableEntry(0, "test");
  prg.printStringFromStringTable(0);
  prg.printStringFromStringTable(0);
  // prg.loadStringTableLimitIntoVariable(0);
  // prg.terminate(0);
  // prg.copyVariable(0, 1);

  beast::CpuVirtualMachine vm;
  beast::VmSession session(std::move(prg), 500, 100, 50);

  while (vm.step(session)) {
    std::cout << session.getPrintBuffer();
    session.clearPrintBuffer();
  }

  std::cout << std::endl;

  return EXIT_SUCCESS;
}
