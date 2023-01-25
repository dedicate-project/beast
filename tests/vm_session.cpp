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

TEST_CASE("set_io_behaviors_can_be_retrieved", "vm_session") {
  const int32_t variable_index_store = 0;
  const int32_t variable_index_input = 1;
  const int32_t variable_index_output = 2;

  beast::Program prg;
  beast::VmSession session(std::move(prg), 3, 0, 0);

  session.setVariableBehavior(variable_index_store, beast::VmSession::VariableIoBehavior::Store);
  session.setVariableBehavior(variable_index_input, beast::VmSession::VariableIoBehavior::Input);
  session.setVariableBehavior(variable_index_output, beast::VmSession::VariableIoBehavior::Output);

  REQUIRE(session.getVariableBehavior(variable_index_store, true)
          == beast::VmSession::VariableIoBehavior::Store);
  REQUIRE(session.getVariableBehavior(variable_index_input, true)
          == beast::VmSession::VariableIoBehavior::Input);
  REQUIRE(session.getVariableBehavior(variable_index_output, true)
          == beast::VmSession::VariableIoBehavior::Output);
}

TEST_CASE("getting_io_behavior_for_not_registered_variable_throws", "vm_session") {
  const int32_t variable_index = 0;

  beast::Program prg;
  beast::VmSession session(std::move(prg), 1, 0, 0);

  bool threw = false;
  try {
    session.getVariableBehavior(variable_index, true);
  } catch(...) {
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

  beast::CpuVirtualMachine vm;
  vm.step(session);

  REQUIRE(session.hasOutputDataAvailable(variable_index_output, true) == true);
}

TEST_CASE("retrieving_output_data_availability_for_invalid_index_throws", "vm_session") {
  const int32_t variable_index_output = 0;

  beast::Program prg;
  beast::VmSession session(std::move(prg), 1, 0, 0);

  bool threw = false;
  try {
    session.hasOutputDataAvailable(variable_index_output, true);
  } catch(...) {
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
    session.hasOutputDataAvailable(variable_index, true);
  } catch(...) {
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
    session.hasOutputDataAvailable(variable_index, true);
  } catch(...) {
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
  beast::CpuVirtualMachine vm;
  vm.step(session);
  bool threw = false;
  try {
    vm.step(session);
  } catch(...) {
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
  beast::CpuVirtualMachine vm;

  vm.step(session);
  bool threw = false;
  try {
    vm.step(session);
  } catch(...) {
    threw = true;
  }

  REQUIRE(threw == true);
}

TEST_CASE("registering_a_negative_variable_index_throws", "vm_session") {
  const int32_t variable_index = -1;

  beast::Program prg;
  prg.declareVariable(variable_index, beast::Program::VariableType::Int32);

  beast::VmSession session(std::move(prg), 1, 0, 0);
  beast::CpuVirtualMachine vm;

  bool threw = false;
  try {
    vm.step(session);
  } catch(...) {
    threw = true;
  }

  REQUIRE(threw == true);
}

TEST_CASE("registering_a_variable_beyond_memory_limit_throws", "vm_session") {
  const int32_t variable_index = 1;

  beast::Program prg;
  prg.declareVariable(variable_index, beast::Program::VariableType::Int32);

  beast::VmSession session(std::move(prg), 1, 0, 0);
  beast::CpuVirtualMachine vm;

  bool threw = false;
  try {
    vm.step(session);
  } catch(...) {
    threw = true;
  }

  REQUIRE(threw == true);
}
