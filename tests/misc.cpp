#include <catch2/catch.hpp>

#include <beast/beast.hpp>

TEST_CASE("noop", "instructions") {
  beast::Program prg(1);
  prg.noop();

  REQUIRE(prg.getData1(0) == 0x00);
}

TEST_CASE("store_variable_memory_size_into_variable", "misc") {
  beast::Program prg(100);
  prg.declareVariable(0, beast::Program::VariableType::Int32);
  prg.setVariable(0, 0, true);
  prg.loadMemorySizeIntoVariable(0, true);

  beast::VmSession session(std::move(prg), 128, 100, 50);
  beast::CpuVirtualMachine virtual_machine;
  while (virtual_machine.step(session, false)) {}

  REQUIRE(session.getVariableValue(0, true) == 128);
}

TEST_CASE("termination_prevents_further_execution_and_sets_return_code", "misc") {
  const int8_t return_code = 127;
  beast::Program prg(100);
  prg.declareVariable(0, beast::Program::VariableType::Int32);
  prg.setVariable(0, 0, true);
  prg.terminate(return_code);
  prg.setVariable(0, 1, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine virtual_machine;
  while (virtual_machine.step(session, false)) {}

  REQUIRE(session.getVariableValue(0, true) == 0);
  REQUIRE(session.getRuntimeStatistics().return_code == return_code);
}

TEST_CASE("current_address_can_be_determined", "misc") {
  beast::Program prg(100);
  prg.declareVariable(0, beast::Program::VariableType::Int32);
  prg.setVariable(0, 0, true);
  prg.loadCurrentAddressIntoVariable(0, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine virtual_machine;
  while (virtual_machine.step(session, false)) {}

  REQUIRE(session.getVariableValue(0, true) == 22);
}

TEST_CASE("termination_prevents_further_execution_and_sets_variable_return_code", "misc") {
  const int32_t return_code_variable_index = 14;
  const int8_t return_code = 52;

  beast::Program prg;
  prg.declareVariable(return_code_variable_index, beast::Program::VariableType::Int32);
  prg.setVariable(return_code_variable_index, return_code, true);
  prg.declareVariable(0, beast::Program::VariableType::Int32);
  prg.setVariable(0, 0, true);
  prg.terminateWithVariableReturnCode(return_code_variable_index, true);
  prg.setVariable(0, 1, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine virtual_machine;
  while (virtual_machine.step(session, false)) {}

  REQUIRE(session.getVariableValue(0, true) == 0);
  REQUIRE(session.getRuntimeStatistics().return_code == return_code);
}

TEST_CASE("load_random_value_into_variable", "misc") {
  const int32_t index = 2;

  // Assuming that at least one of 10 random values is != 0.

  beast::Program prg;
  prg.declareVariable(index, beast::Program::VariableType::Int32);
  prg.setVariable(index, 0, true);
  prg.loadRandomValueIntoVariable(index, true);
  prg.loadRandomValueIntoVariable(index, true);
  prg.loadRandomValueIntoVariable(index, true);
  prg.loadRandomValueIntoVariable(index, true);
  prg.loadRandomValueIntoVariable(index, true);
  prg.loadRandomValueIntoVariable(index, true);
  prg.loadRandomValueIntoVariable(index, true);
  prg.loadRandomValueIntoVariable(index, true);
  prg.loadRandomValueIntoVariable(index, true);
  prg.loadRandomValueIntoVariable(index, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine virtual_machine;
  (void)virtual_machine.step(session, false);
  (void)virtual_machine.step(session, false);

  bool any_is_random = false;
  while (virtual_machine.step(session, false)) {
    (void)virtual_machine.step(session, false);
    if (session.getVariableValue(index, true) != 0) {
      any_is_random = true;
      break;
    }
  }

  REQUIRE(any_is_random == true);
}
