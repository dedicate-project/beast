#include <catch2/catch.hpp>

#include <beast/beast.hpp>

TEST_CASE("when_marked_exited_abnormally_that_status_can_be_retrieved", "vm_session") {
  beast::Program prg(2);
  prg.noop();
  prg.noop();

  beast::VmSession session(std::move(prg), 500, 100, 50);
  session.setExitedAbnormally();

  REQUIRE(session.getRuntimeStatistics().abnormal_exit == true);
}

TEST_CASE("when_not_marked_exited_abnormally_that_status_can_be_retrieved", "vm_session") {
  beast::Program prg(2);
  prg.noop();
  prg.noop();

  beast::VmSession session(std::move(prg), 500, 100, 50);

  REQUIRE(session.getRuntimeStatistics().abnormal_exit == false);
}

TEST_CASE("getting_variable_behavior_of_non_existent_variable_index_throws", "vm_session") {
  beast::Program prg;
  beast::VmSession session(std::move(prg), 3, 0, 0);

  bool threw = false;
  try {
    static_cast<void>(session.getVariableBehavior(0, false));
  } catch (...) {
    threw = true;
  }

  REQUIRE(threw == true);
}

TEST_CASE("checking_for_output_on_non_existent_variable_index_throws", "vm_session") {
  beast::Program prg;
  beast::VmSession session(std::move(prg), 3, 0, 0);

  bool threw = false;
  try {
    static_cast<void>(session.hasOutputDataAvailable(0, false));
  } catch (...) {
    threw = true;
  }

  REQUIRE(threw == true);
}

TEST_CASE("clearing_print_buffer_empties_it", "vm_session") {
  const std::string test_string = "Entry";

  beast::Program prg;
  beast::VmSession session(std::move(prg), 3, 0, 0);

  session.appendToPrintBuffer(test_string);
  REQUIRE(session.getPrintBuffer() == test_string);

  session.clearPrintBuffer();
  REQUIRE(session.getPrintBuffer().empty());
}

TEST_CASE("setting_a_too_long_string_table_entry_throws", "vm_session") {
  const std::string test_string = "Entry";
  const int32_t max_string_length = test_string.size() - 1;
  const int32_t variable_index = 0;
  const int32_t string_table_index = 0;

  beast::Program prg;
  beast::VmSession session(std::move(prg), 1, 1, max_string_length);

  session.registerVariable(variable_index, beast::Program::VariableType::Int32);
  session.setVariable(variable_index, string_table_index, true);

  bool threw = false;
  try {
    session.setVariableStringTableEntry(variable_index, true, test_string);
  } catch (...) {
    threw = true;
  }

  REQUIRE(threw == true);
}

TEST_CASE("set_io_behaviors_can_be_retrieved", "vm_session") {
  const int32_t variable_index_store = 0;
  const int32_t variable_index_input = 1;
  const int32_t variable_index_output = 2;

  beast::Program prg;
  beast::VmSession session(std::move(prg), 3, 0, 0);

  session.setVariableBehavior(variable_index_store, beast::VmSession::VariableIoBehavior::Store);
  session.setVariableBehavior(variable_index_input, beast::VmSession::VariableIoBehavior::Input);
  session.setVariableBehavior(variable_index_output, beast::VmSession::VariableIoBehavior::Output);

  REQUIRE(session.getVariableBehavior(variable_index_store, true) ==
          beast::VmSession::VariableIoBehavior::Store);
  REQUIRE(session.getVariableBehavior(variable_index_input, true) ==
          beast::VmSession::VariableIoBehavior::Input);
  REQUIRE(session.getVariableBehavior(variable_index_output, true) ==
          beast::VmSession::VariableIoBehavior::Output);
}

TEST_CASE("getting_io_behavior_for_not_registered_variable_throws", "vm_session") {
  const int32_t variable_index = 0;

  beast::Program prg;
  beast::VmSession session(std::move(prg), 1, 0, 0);

  bool threw = false;
  try {
    (void)session.getVariableBehavior(variable_index, true);
  } catch (...) {
    threw = true;
  }

  REQUIRE(threw == true);
}

TEST_CASE("output_data_availability_can_be_determined", "vm_session") {
  const int32_t variable_index_output = 0;

  beast::Program prg;
  prg.setVariable(variable_index_output, 0x0, true);

  beast::VmSession session(std::move(prg), 1, 0, 0);
  session.setVariableBehavior(variable_index_output, beast::VmSession::VariableIoBehavior::Output);

  beast::CpuVirtualMachine virtual_machine;
  (void)virtual_machine.step(session, false);

  REQUIRE(session.hasOutputDataAvailable(variable_index_output, true) == true);
}

TEST_CASE("retrieving_output_data_availability_for_invalid_index_throws", "vm_session") {
  const int32_t variable_index_output = 0;

  beast::Program prg;
  beast::VmSession session(std::move(prg), 1, 0, 0);

  bool threw = false;
  try {
    (void)session.hasOutputDataAvailable(variable_index_output, true);
  } catch (...) {
    threw = true;
  }

  REQUIRE(threw == true);
}

TEST_CASE("retrieving_output_data_availability_for_input_variable_throws", "vm_session") {
  const int32_t variable_index = 0;

  beast::Program prg;
  beast::VmSession session(std::move(prg), 1, 0, 0);
  session.setVariableBehavior(variable_index, beast::VmSession::VariableIoBehavior::Input);

  bool threw = false;
  try {
    (void)session.hasOutputDataAvailable(variable_index, true);
  } catch (...) {
    threw = true;
  }

  REQUIRE(threw == true);
}

TEST_CASE("retrieving_output_data_availability_for_store_variable_throws", "vm_session") {
  const int32_t variable_index = 0;

  beast::Program prg;
  beast::VmSession session(std::move(prg), 1, 0, 0);
  session.setVariableBehavior(variable_index, beast::VmSession::VariableIoBehavior::Store);

  bool threw = false;
  try {
    (void)session.hasOutputDataAvailable(variable_index, true);
  } catch (...) {
    threw = true;
  }

  REQUIRE(threw == true);
}

TEST_CASE("appending_to_print_buffer_beyond_buffer_limit_throws", "vm_session") {
  const int32_t string_table_index = 0;
  const std::string string_table_item = "The string table item.";

  beast::Program prg;
  prg.setStringTableEntry(string_table_index, string_table_item);
  prg.printStringFromStringTable(string_table_index);

  beast::VmSession session(std::move(prg), 0, 1, 100);
  session.setMaximumPrintBufferLength(10);
  beast::CpuVirtualMachine virtual_machine;
  (void)virtual_machine.step(session, false);
  bool threw = false;
  try {
    (void)virtual_machine.step(session, false);
  } catch (...) {
    threw = true;
  }

  REQUIRE(threw == true);
}

TEST_CASE("double_registering_variable_throws", "vm_session") {
  const int32_t variable_index = 0;

  beast::Program prg;
  prg.declareVariable(variable_index, beast::Program::VariableType::Int32);
  prg.declareVariable(variable_index, beast::Program::VariableType::Int32);

  beast::VmSession session(std::move(prg), 1, 0, 0);
  beast::CpuVirtualMachine virtual_machine;

  (void)virtual_machine.step(session, false);
  bool threw = false;
  try {
    (void)virtual_machine.step(session, false);
  } catch (...) {
    threw = true;
  }

  REQUIRE(threw == true);
}

TEST_CASE("registering_a_negative_variable_index_throws", "vm_session") {
  const int32_t variable_index = -1;

  beast::Program prg;
  prg.declareVariable(variable_index, beast::Program::VariableType::Int32);

  beast::VmSession session(std::move(prg), 1, 0, 0);
  beast::CpuVirtualMachine virtual_machine;

  bool threw = false;
  try {
    (void)virtual_machine.step(session, false);
  } catch (...) {
    threw = true;
  }

  REQUIRE(threw == true);
}

TEST_CASE("registering_a_variable_beyond_memory_limit_throws", "vm_session") {
  const int32_t variable_index = 1;

  beast::Program prg;
  prg.declareVariable(variable_index, beast::Program::VariableType::Int32);

  beast::VmSession session(std::move(prg), 1, 0, 0);
  beast::CpuVirtualMachine virtual_machine;

  bool threw = false;
  try {
    (void)virtual_machine.step(session, false);
  } catch (...) {
    threw = true;
  }

  REQUIRE(threw == true);
}

TEST_CASE("runtime_statistics_correctly_record_operator_execution", "vm_session") {
  beast::Program prg;
  beast::VmSession session(std::move(prg), 0, 0, 0);
  session.informAboutStep(beast::OpCode::LoadCurrentAddressIntoVariable);
  session.informAboutStep(beast::OpCode::LoadCurrentAddressIntoVariable);
  session.informAboutStep(beast::OpCode::CopyVariable);
  session.informAboutStep(beast::OpCode::PerformSystemCall);
  session.informAboutStep(beast::OpCode::PerformSystemCall);
  session.informAboutStep(beast::OpCode::ModuloVariableByVariable);
  session.informAboutStep(beast::OpCode::LoadCurrentAddressIntoVariable);

  beast::VmSession::RuntimeStatistics statistics = session.getRuntimeStatistics();
  REQUIRE(statistics.steps_executed == 7);
  REQUIRE(statistics.operator_executions[beast::OpCode::LoadCurrentAddressIntoVariable] == 3);
  REQUIRE(statistics.operator_executions[beast::OpCode::CopyVariable] == 1);
  REQUIRE(statistics.operator_executions[beast::OpCode::PerformSystemCall] == 2);
  REQUIRE(statistics.operator_executions[beast::OpCode::ModuloVariableByVariable] == 1);
}

TEST_CASE("runtime_statistics_reset_correctly", "vm_session") {
  beast::Program prg;
  beast::VmSession session(std::move(prg), 0, 0, 0);
  session.informAboutStep(beast::OpCode::LoadCurrentAddressIntoVariable);
  session.informAboutStep(beast::OpCode::LoadCurrentAddressIntoVariable);
  session.informAboutStep(beast::OpCode::CopyVariable);
  session.informAboutStep(beast::OpCode::PerformSystemCall);
  session.informAboutStep(beast::OpCode::PerformSystemCall);
  session.informAboutStep(beast::OpCode::ModuloVariableByVariable);
  session.informAboutStep(beast::OpCode::LoadCurrentAddressIntoVariable);

  session.resetRuntimeStatistics();
  beast::VmSession::RuntimeStatistics statistics = session.getRuntimeStatistics();
  REQUIRE(statistics.steps_executed == 0);
  REQUIRE(statistics.operator_executions[beast::OpCode::LoadCurrentAddressIntoVariable] == 0);
  REQUIRE(statistics.operator_executions[beast::OpCode::CopyVariable] == 0);
  REQUIRE(statistics.operator_executions[beast::OpCode::PerformSystemCall] == 0);
  REQUIRE(statistics.operator_executions[beast::OpCode::ModuloVariableByVariable] == 0);
}
