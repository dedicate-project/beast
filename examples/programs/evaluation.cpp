// Standard
#include <iostream>

// BEAST
#include <beast/beast.hpp>

int main(int /*argc*/, char** /*argv*/) {
  /* Print BEAST library version. */
  const auto version = beast::getVersion();
  std::cout << "Using BEAST library version " << static_cast<uint32_t>(version[0]) << "."
            << static_cast<uint32_t>(version[1]) << "." << static_cast<uint32_t>(version[2]) << "."
            << std::endl;

  /* This example shows how to use program evaluations to generate metrics. */
  const std::string message = "Some message.";

  beast::Program prg;
  prg.noop();
  prg.setStringTableEntry(0, message);
  prg.printStringFromStringTable(0);
  prg.terminate(0);
  prg.noop();

  beast::CpuVirtualMachine virtual_machine;

  beast::VmSession session_static(prg, 0, 0, 0);
  while (virtual_machine.step(session_static, true)) {
  }
  beast::VmSession::RuntimeStatistics static_analysis_data = session_static.getRuntimeStatistics();

  beast::VmSession session_dynamic(prg, 0, 1, 50);
  while (virtual_machine.step(session_dynamic, false)) {
  }
  beast::VmSession::RuntimeStatistics dynamic_analysis_data =
      session_dynamic.getRuntimeStatistics();

  beast::OperatorUsageEvaluator noop_evaluator(beast::OpCode::NoOp);
  const double static_noop_ratio = noop_evaluator.evaluate(session_static);
  const double dynamic_noop_ratio = noop_evaluator.evaluate(session_dynamic);

  std::cout << "Static Analysis  : NoOp operator ratio = " << static_noop_ratio << std::endl;
  std::cout << "Dynamic Analysis : NoOp operator ratio = " << dynamic_noop_ratio << std::endl;

  return dynamic_analysis_data.return_code;
}
