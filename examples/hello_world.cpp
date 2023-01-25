// Standard
#include <iostream>
#include <vector>

// BEAST
#include <beast/beast.hpp>

int main(int /*argc*/, char** /*argv*/) {
  /* Print BEAST library version. */
  const auto version = beast::getVersion();
  std::cout << "Using BEAST library version "
            << static_cast<uint32_t>(version[0]) << "."
            << static_cast<uint32_t>(version[1]) << "."
            << static_cast<uint32_t>(version[2]) << "." << std::endl;

  /* Define the constants used in this program.
     These are:
     * The "Hello World!" message to be displayed
     * The string table index to store the message at */
  const std::string hello_world_message = "Hello World!";
  const int32_t string_table_index = 0;

  /* Here, the actual program is declared. It consists of only two operators: Setting the above
     "Hello World!" message string in the string table index, and printing from that index. */
  beast::Program prg;
  prg.setStringTableEntry(string_table_index, hello_world_message);
  prg.printStringFromStringTable(string_table_index);

  /* A VmSession instance is required for execution. This one has no variable space, and can hold
     exactly one string table item with maximum length of 12 characters, matching the string
     above. */
  beast::VmSession session(std::move(prg), 0, 1, 12);

  /* The CpuVirtualMachine class is used for execution. */
  beast::CpuVirtualMachine virtual_machine;
  /* Here, the program is executed through its session step by step. In each step, the print buffer
     is read and printed, and the internal session print buffer variable is cleared. This is
     important, as otherwise the VmSession may throw an exception due to an overflow. */
  while (virtual_machine.step(session)) {
    std::cout << session.getPrintBuffer();
    session.clearPrintBuffer();
  }

  /* Print an endline as that is not part of the program's output. */
  std::cout << std::endl;

  /* Return the program's return code. This defaults to `0x0`, but could be different if the program
     terminates with an error code. */
  return session.getRuntimeStatistics().return_code;
}
