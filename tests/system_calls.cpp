#include <catch2/catch.hpp>

#include <beast/beast.hpp>

TEST_CASE("system_calls_provide_datetime_data", "system_calls") {
  const int32_t variable_index_tz_h = 0;
  const int32_t variable_value_tz_h = -100;
  const int32_t variable_index_tz_m = 1;
  const int32_t variable_value_tz_m = -100;
  const int32_t variable_index_sec = 2;
  const int32_t variable_value_sec = -1;
  const int32_t variable_index_min = 3;
  const int32_t variable_value_min = -1;
  const int32_t variable_index_hr = 4;
  const int32_t variable_value_hr = -1;
  const int32_t variable_index_day = 5;
  const int32_t variable_value_day = -1;
  const int32_t variable_index_mon = 6;
  const int32_t variable_value_mon = -1;
  const int32_t variable_index_yr = 7;
  const int32_t variable_value_yr = -1;
  const int32_t variable_index_wk = 8;
  const int32_t variable_value_wk = -1;

  beast::Program prg;
  prg.declareVariable(variable_index_tz_h, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index_tz_h, variable_value_tz_h, true);

  prg.declareVariable(variable_index_tz_m, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index_tz_m, variable_value_tz_m, true);

  prg.declareVariable(variable_index_sec, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index_sec, variable_value_sec, true);

  prg.declareVariable(variable_index_min, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index_min, variable_value_min, true);

  prg.declareVariable(variable_index_hr, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index_hr, variable_value_hr, true);

  prg.declareVariable(variable_index_day, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index_day, variable_value_day, true);

  prg.declareVariable(variable_index_mon, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index_mon, variable_value_mon, true);

  prg.declareVariable(variable_index_yr, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index_yr, variable_value_yr, true);

  prg.declareVariable(variable_index_wk, beast::Program::VariableType::Int32);
  prg.setVariable(variable_index_wk, variable_value_wk, true);

  prg.performSystemCall(0, 0, variable_index_tz_h, true);
  prg.performSystemCall(0, 1, variable_index_tz_m, true);
  prg.performSystemCall(0, 2, variable_index_sec, true);
  prg.performSystemCall(0, 3, variable_index_min, true);
  prg.performSystemCall(0, 4, variable_index_hr, true);
  prg.performSystemCall(0, 5, variable_index_day, true);
  prg.performSystemCall(0, 6, variable_index_mon, true);
  prg.performSystemCall(0, 7, variable_index_yr, true);
  prg.performSystemCall(0, 8, variable_index_wk, true);

  beast::VmSession session(std::move(prg), 500, 100, 50);
  beast::CpuVirtualMachine virtual_machine;
  while (virtual_machine.step(session, false)) {
  }

  REQUIRE(session.getVariableValue(variable_index_tz_h, true) != variable_value_tz_h);
  REQUIRE(session.getVariableValue(variable_index_tz_m, true) != variable_value_tz_m);
  REQUIRE(session.getVariableValue(variable_index_sec, true) != variable_value_sec);
  REQUIRE(session.getVariableValue(variable_index_min, true) != variable_value_min);
  REQUIRE(session.getVariableValue(variable_index_hr, true) != variable_value_hr);
  REQUIRE(session.getVariableValue(variable_index_day, true) != variable_value_day);
  REQUIRE(session.getVariableValue(variable_index_mon, true) != variable_value_mon);
  REQUIRE(session.getVariableValue(variable_index_yr, true) != variable_value_yr);
  REQUIRE(session.getVariableValue(variable_index_wk, true) != variable_value_wk);
}
