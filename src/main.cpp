#include <cstdlib>
#include <iostream>

#include <beast/cpu_virtual_machine.hpp>

int main(int /*argc*/, char** /*argv*/) {
  beast::Program prg(500);
  prg.declareVariable(0, beast::Program::VariableType::Int32);
  prg.undeclareVariable(0);
  prg.declareVariable(0, beast::Program::VariableType::Int32);
  prg.setVariable(0, 0x10, true);
  prg.declareVariable(1, beast::Program::VariableType::Link);
  prg.setVariable(1, 0x2, false);
  prg.declareVariable(2, beast::Program::VariableType::Int32);
  prg.setVariable(2, 0x120, true);
  prg.addConstantToVariable(2, 1, true);
  prg.addVariableToVariable(0, true, 2, true);
  prg.undeclareVariable(0);
  prg.declareVariable(10, beast::Program::VariableType::Int32);
  prg.setVariable(10, 0x48, true);  // H
  prg.declareVariable(11, beast::Program::VariableType::Int32);
  prg.setVariable(11, 0x65, true);  // e
  prg.declareVariable(12, beast::Program::VariableType::Int32);
  prg.setVariable(12, 0x6c, true);  // l
  prg.declareVariable(13, beast::Program::VariableType::Int32);
  prg.setVariable(13, 0x6c, true);  // l
  prg.declareVariable(14, beast::Program::VariableType::Int32);
  prg.setVariable(14, 0x6f, true);  // o
  prg.declareVariable(15, beast::Program::VariableType::Int32);
  prg.setVariable(15, 0x10, true);  // \n
  prg.subtractConstantFromVariable(10, 1, true);
  prg.subtractVariableFromVariable(1, true, 11, true);
  prg.printVariable(10, true, true);
  prg.printVariable(11, true, true);
  prg.printVariable(12, true, true);
  prg.printVariable(13, true, true);
  prg.printVariable(14, true, true);
  prg.printVariable(15, true, true);
  prg.declareVariable(16, beast::Program::VariableType::Int32);
  prg.setVariable(16, 0, true);
  prg.declareVariable(17, beast::Program::VariableType::Int32);
  prg.setVariable(17, 14, true);
  prg.relativeJumpToVariableAddressIfVariableGreaterThanZero(16, true, 17, true);
  prg.setStringTableEntry(1, "Ok");
  prg.printStringFromStringTable(1);
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
  prg.printVariable(1, true, false);
  prg.setStringTableEntry(0, "test");
  prg.printStringFromStringTable(0);
  prg.terminate(127);
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
  std::cout << ">> Return code = " << static_cast<int32_t>(session.getReturnCode()) << std::endl;

  return EXIT_SUCCESS;
}
