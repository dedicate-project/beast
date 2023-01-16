# BEAST - Binary Evolution And Sentience Toolkit

[![CircleCI](https://circleci.com/gh/dedicate-project/beast.svg?style=shield)](https://circleci.com/gh/dedicate-project/beast)
[![CodeQL](https://github.com/dedicate-project/beast/actions/workflows/codeql.yml/badge.svg)](https://github.com/dedicate-project/beast/actions/workflows/codeql.yml)
[![AppVeyor](https://ci.appveyor.com/api/projects/status/0a51i8ax0vg92p6k?svg=true)](https://ci.appveyor.com/project/fairlight1337/beast)
[![Documentation Status](https://readthedocs.org/projects/beast-project/badge/?version=latest)](https://beast-project.readthedocs.io/en/latest/?badge=latest)
[![Coverage Status](https://coveralls.io/repos/github/dedicate-project/beast/badge.svg?branch=main)](https://coveralls.io/github/dedicate-project/beast?branch=main)
[![Maintainability Rating](https://sonarcloud.io/api/project_badges/measure?project=dedicate-project_beast&metric=sqale_rating)](https://sonarcloud.io/summary/new_code?id=dedicate-project_beast)
[![Code Smells](https://sonarcloud.io/api/project_badges/measure?project=dedicate-project_beast&metric=code_smells)](https://sonarcloud.io/summary/new_code?id=dedicate-project_beast)
[![Lines of Code](https://sonarcloud.io/api/project_badges/measure?project=dedicate-project_beast&metric=ncloc)](https://sonarcloud.io/summary/new_code?id=dedicate-project_beast)
[![Duplicated Lines (%)](https://sonarcloud.io/api/project_badges/measure?project=dedicate-project_beast&metric=duplicated_lines_density)](https://sonarcloud.io/summary/new_code?id=dedicate-project_beast)
[![Technical Debt](https://sonarcloud.io/api/project_badges/measure?project=dedicate-project_beast&metric=sqale_index)](https://sonarcloud.io/summary/new_code?id=dedicate-project_beast)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
![Maintenance](https://img.shields.io/maintenance/yes/2023)
![Platforms](https://img.shields.io/badge/platforms-linux%20%7C%20windows-lightgrey)

<p align="center">
 <img src="https://github.com/dedicate-project/beast/blob/main/images/beast_head_logo_small.png" />
</p>

## Synopsis

BEAST (Binary Evolution And Sentience Toolkit) is an open source project that defines and implements a virtual machine with a custom instruction set. The virtual machine operates on a byte level and supports all common low-level machine operations, but functions within an entirely virtual environment. This allows users to experiment with code transformations and custom low-level operators without the need for physical hardware.

One of the main goals of the BEAST project is to provide a platform for researchers and developers to explore the intersection of evolution and computation. The virtual machine's custom instruction set allows users to define their own low-level operators, enabling them to conduct experiments on how these operators impact the evolution and optimization of binary code.

In addition to its use as a research platform, the BEAST virtual machine also has practical applications in the field of computer science education. By providing a virtual environment for students to learn about low-level machine operations and experiment with code transformations, the BEAST project aims to give students a deeper understanding of how computers work at a fundamental level.

The BEAST virtual machine is implemented in a high-level programming language, making it easily accessible to a wide range of users. The project also includes extensive documentation and examples to help users get started with the virtual machine and begin exploring its capabilities.

Overall, the BEAST project provides a powerful and flexible platform for researchers and educators to explore the intersection of evolution and computation, and to gain a deeper understanding of low-level machine operations. By providing a virtual environment for experimentation and education, the BEAST project aims to advance the field of computer science and inspire the next generation of developers and researchers.

You can find the project's documentation [here](https://beast-project.readthedocs.io/en/latest/). The API documentation can be found [here](https://beast-project.readthedocs.io/en/latest/api.html). The current project roadmap can be found [here](https://github.com/dedicate-project/beast/blob/main/ROADMAP.md).

## Architecture

The BEAST system architecture is driven by three main elements: Virtual Machines (VMs), VM Sessions, and Programs. The following diagram shows how these relate to each other:

![BEAST Architecture](images/architecture.png)

A Virtual Machine can be instantiated and be used with one or more VM Sessions. Each VM Session hosts one Program, alongside its state (which consists of a Variable Memory and a String Table). Each Program consists of zero or more operators. The size of a Program is virtually arbitrary (it must fit into the host's system memory).

The Variable Memory's layout is simple:
* It is defined to hold a maximum number of indexed variables of a fixed size of 8 bytes (internally managed as ``int32_t``).
* Variables can be either of type ``int32_t`` or of type ``link``. For ``int32_t`` variables, they store values directly. For ``link`` variables, when they are read, the read process is redirected to the memory address denoted by the stored value. Example:
  * Variable ``0`` of type ``int32_t`` has value ``128``.
  * Variable ``1`` of type ``link`` has value ``0``.
  * Reading variable ``1`` returns the value ``128``.
  * Setting variable ``1`` effectively sets the value of variable ``0``.
  * The redirection can be ignored for each call working with variables to be able to modify/read a link's redirection value.
* The number of supported variables is a property of the respective VmSession instance holding the Variable Memory.

The String Table's layout is also simple:
* It is defined to hold a maximum number of indexed strings of a maximum length.
* For each item, its size can be read.
* The number of supported string table entries and their maximum length are a property of the respective VmSession instance holding the table.


## A Simple Example

Writing BEAST programs is straight forward. Take the following "Hello World!" example:
```cpp
#include <iostream>

#include <beast/beast.hpp>

int main(int /*argc*/, char** /*argv*/) {
  // Define the program to run. This just sets a string table entry and prints it.
  beast::Program prg;
  prg.setStringTableEntry(0, "Hello World!");
  prg.printStringFromStringTable(0);

  // Define the VM session based on `prg`. It has space for 10 variables and can
  // store 5 string table entries, each being 25 characters long at most.
  beast::VmSession session(std::move(prg), 10, 5, 25);
  // This is the CPU based virtual machine to run the program/session in.
  beast::CpuVirtualMachine vm;

  // Run the program for as long as it runs.
  while (vm.step(session)) {
    // Send to output whatever the current step wants to print and clear the internal
    // buffer afterwards.
    std::cout << session.getPrintBuffer();
    session.clearPrintBuffer();
  }

  std::cout << std::endl;

  // Return the program's return (potentially error) code.
  return session.getReturnCode();
}
```

The program that is defined here stores the string "Hello World!" at string table index 0, and then
prints the string stored at string table index 0. The program is then executed in the CPU VM, the
result being that the following output appears on screen:
```bash
Hello World!
```

This could have been achieved in various ways (for exampe through printing individual variable
values), but this example shows the very bare basics of how to achieve a hello world example.


## Building

First, install the dependencies (assuming you're working on a Ubuntu system):
```bash
sudo apt install clang-tidy ccache
```

To build the project, check out the source code:
```bash
git clone https://github.com/dedicate-project/beast/
cd beast
git submodule update
```

Then, inside the repository, perform the following actions to actually build the code:
```bash
mkdir build
cd build
cmake ..
make
```

To run all tests, afterwards run:
```bash
make test
```

To not build the tests (and save some time while developing or you just don't need them) you can disable them in CMake using this when configuring the build:
```bash
cmake -DBEAST_BUILD_TESTS=NO ..
```

If you want to create a coverage report, install these additional dependencies:
```bash
sudo apt install lcov npm
```

And run this:
```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
make coverage
```


## Adding a new Operator

To add a new operator, you need to follow these steps:

1. Add a new opcode in `include/beast/opcodes.hpp`: Append the new opcode with the next higher hex value at the end of the enum class `OpCode`.
1. Add a signature for this opcode in `include/beast/program.hpp`: This way, client programs can add the operator to a program.
1. Add the program implementation into `src/program.cpp`: This is the implementation of the signature you added in the previous step. Follow the example of existing operators, but in general: Use `appendCode1(...)` with the opcode you added in the first step, and then add any bytes you deem necessary for your operator.
1. Extend the `switch` in `src/cpu_virtual_machine.cpp` in the function `step()`: Add a case for your new opcode, read all parameters (no need to read the opcode, that's done automatically), print a `debug` statement with the operator signature, and call a `VmSession` function that matches your operator.
1. If there is no suitable `VmSession` function, define its signature in `include/beast/vm_session.hpp`: This is a function that explicitly accepts the parameters for your operator. See the existing ones for how to define them.
1. Implement the `VmSession` function you just defined in `src/vm_session.cpp`: Perform the actual logic that should be executed if the VM encounters your operator. Again, see existing operators for how to do it.


## Defined Operators

The content in this section is held for the time being while everything is moved to the [ReadTheDocs
API documentation](https://beast-project.readthedocs.io/en/latest/api.html). Once parts are
documented in the RTD docs, they are removed here. That means the list below is not exhaustive, and
will eventually be removed entirely.

| Operator                                           | Code | Description                                                                                                                                                                                                                                                                                                                                                                                                                                                    |
|----------------------------------------------------|------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Set String Table Entry                             | 0x1b | ``set_string_table_entry(i, s)``: Set the string table entry at position ``i`` to the value stored in the string ``s``. ``s`` must be null-terminated. ``i`` must be within the string table's available number of entries, and ``s``'s length must be within the string table entries' maximum length. Both are defined by the VM executing the instruction.                                                                                                  |
| Print String from String Table                     | 0x1c | ``print_string_table_entry(i)``: Print the string table entry at position ``i`` to screen. The position must have been set before this instruction.                                                                                                                                                                                                                                                                                                            |
| Load String Table Limit into Variable              | 0x1d | ``load_string_table_limit_into_variable(v, fv)``: Load the session's limit on how many string table entries can be used into the variable ``v`` (resolve its links if ``fv`` is ``True``).                                                                                                                                                                                                                                                                     |
| Terminate                                          | 0x1e | ``terminate(c)``: Terminate the program immediately and set the return code to the value ``c``.                                                                                                                                                                                                                                                                                                                                                                |
| Copy Variable                                      | 0x1f | ``copy_variable(s, fs, d, fd)``: Copy the contents of variable ``s`` (resolve links if ``fs`` is ``True``) into the variable ``d`` (resolve links if ``fd`` is ``True``). Only the value is copied, no other flags or characteristics.                                                                                                                                                                                                                         |
| Load String Item Length into Variable              | 0x20 | ``load_string_item_length_into_variable(s, v, fv)``: Loads the length of the string table item at index ``s`` into variable ``v`` (resolve its links if ``fv`` is ``True``).                                                                                                                                                                                                                                                                                   |
| Load String Item into Variables                    | 0x21 | ``load_string_item_into_variables(s, v, fv)``: Consecutively loads the individual characters of the string item at string table index ``s`` into the variables starting at ``v`` (resolve its links first if ``fv`` is ``True``, this is re-evaluated for each character's target consecutive variable). This call expects enough consecutive variables to be declared in the current variable space to house the string item to load into them entirely.      |
| Perform System Call                                | 0x22 |                                                                                                                                                                                                                                                                                                                                                                                                                                                                |
| Bit-shift Variable Left                            | 0x23 | ``bit_shift_variable_left(v, fv, n)``: Bit-shift the variable ``v`` (resolve its links of ``fv`` is ``True``) by ``n`` places to the left.                                                                                                                                                                                                                                                                                                                     |
| Bit-shift Variable Right                           | 0x24 | ``bit_shift_variable_right(v, fv, n)``: Bit-shift the variable ``v`` (resolve its links of ``fv`` is ``True``) by ``n`` places to the right.                                                                                                                                                                                                                                                                                                                   |
| Bit-wise Invert Variable                           | 0x25 | ``bit_wise_invert_variable(v, fv)``: Bit-wise invert the variable ``v`` (resolve its links if ``fv`` is ``True``).                                                                                                                                                                                                                                                                                                                                             |
| Bit-wise AND two Variables                         | 0x26 | ``bit_wise_and_two_variables(va, fva, vb, fvb)``: Perform a bit-wise AND operation on the variables ``va`` (resolve its links if ``fva`` is ``True``) and ``vb`` (resolve its links if ``fvb`` is ``True``) and store the result in ``vb`` (resolve its links if ``fvb`` is ``True``).                                                                                                                                                                         |
| Bit-wise OR two Variables                          | 0x27 | ``bit_wise_or_two_variables(va, fva, vb, fvb)``: Perform a bit-wise OR operation on the variables ``va`` (resolve its links if ``fva`` is ``True``) and ``vb`` (resolve its links if ``fvb`` is ``True``) and store the result in ``vb`` (resolve its links if ``fvb`` is ``True``).                                                                                                                                                                           |
| Bit-wise XOR two Variables                         | 0x28 | ``bit_wise_xor_two_variables(va, fva, vb, fvb)``: Perform a bit-wise XOR operation on the variables ``va`` (resolve its links if ``fva`` is ``True``) and ``vb`` (resolve its links if ``fvb`` is ``True``) and store the result in ``vb`` (resolve its links if ``fvb`` is ``True``).                                                                                                                                                                         |
| Load Random Value into Variable                    | 0x29 | ``load_random_value_into_variable(v, fv)``: Generate a random value in the value range of int32_t and store it in the variable ``v`` (resolve its links if ``fv`` is ``True``).                                                                                                                                                                                                                                                                                |
| Modulo Variable by Constant                        | 0x2a | ``modulo_variable_by_constant(v, fv, c)``: Apply the modulo operator on the variable ``v`` (resolve its links if ``fv`` is ``True``) with the constant ``c`` and store the result in ``v`` (resolve its links if ``fv`` is ``True``).                                                                                                                                                                                                                          |
| Modulo Variable by Variable                        | 0x2b | ``modulo_variable_by_variable(v, fv, vc, fvc)``: Apply the modulo operator on the variable ``v`` (resolve its links if ``fv`` is ``True``) with the value in the variable ``vc`` (resolve its links if ``fvc`` is ``True``) and store the result in ``v`` (resolve its links if ``fv`` is ``True``).                                                                                                                                                           |
| Rotate Variable Left                               | 0x2c | ``rotate_variable_left(v, fv, n)``: Bit-wise rotate the variable ``v`` (resolve its links of ``fv`` is ``True``) by ``n`` places to the left.                                                                                                                                                                                                                                                                                                                  |
| Rotate Variable Right                              | 0x2d | ``rotate_variable_left(v, fv, n)``: Bit-wise rotate the variable ``v`` (resolve its links of ``fv`` is ``True``) by ``n`` places to the right.                                                                                                                                                                                                                                                                                                                 |
| Unconditional Jump to Absolute Address             | 0x2e | ``unconditional_jump_to_absolute_address(a)``: Perform a jump to the absolute address ``addr``.                                                                                                                                                                                                                                                                                                                                                                |
| Unconditional Jump to Absolute Variable Address    | 0x2f | ``unconditional_jump_to_absolute_variable_address(v, fv)``: Perform a jump to the absolute address stored in the variable ``v`` (resolve its links of ``fv`` is ``True``).                                                                                                                                                                                                                                                                                     |
| Unconditional Jump to Relative Address             | 0x30 | ``unconditional_jump_to_relative_address(a)``: Perform a jump to the relative address ``addr``.                                                                                                                                                                                                                                                                                                                                                                |
| Unconditional Jump to Relative Variable Address    | 0x31 | ``unconditional_jump_to_relative_variable_address(v, fv)``: Perform a jump to the relative address stored in the variable ``v`` (resolve its links of ``fv`` is ``True``).                                                                                                                                                                                                                                                                                     |
| Check if input variable was set                    | 0x32 | ``check_if_input_was_set(v, fv, d, fd)``: Check if the input variable ``v`` (resolve links if ``fv`` is ``True``) was set since the last read interaction. If yes, set ``d`` (resolve links if ``fd`` is ``True``) to the value ``0x1``, else set it to ``0x0``. Calling this function resets the flag denoting whether it was read since it was last set.                                                                                                     |
| Load string table item length limit into variable  | 0x33 | ``load_string_table_item_length_limit_into_variable(v, fv)``: Load the length limit of individual string table entries into the variable ``v`` (resolve its links if ``fv`` is ``True``).                                                                                                                                                                                                                                                                      |
| Push Variable on Stack                             | 0x34 | ``push_variable_on_stack(sv, fsv, v, fv)``: Push the contents of variable ``v`` (resolve its links if ``fv`` is ``True``) on the stack starting at variable ``sv`` (resolve its links if ``fsv`` is ``True``).                                                                                                                                                                                                                                                 |
| Push Constant on Stack                             | 0x35 | ``push_constant_on_stack(sv, fsv, c)``: Push the constant ``c`` on the stack starting at variable ``sv`` (resolve its links if ``fsv`` is ``True``).                                                                                                                                                                                                                                                                                                           |
| Pop Variable from Stack                            | 0x36 | ``pop_variable_from_stack(sv, fsv, v, fv)``: Pop the top-most stack item from the stack stack starting at variable ``sv`` (resolve its links if ``fsv`` is ``True``) and store it in variable ``v`` (resolve its links if ``fv`` is ``True``).                                                                                                                                                                                                                 |
| Pop top item from Stack                            | 0x37 | ``pop_variable_from_stack(sv, fsv)``: Pop the top-most stack item from the stack stack starting at variable ``sv`` (resolve its links if ``fsv`` is ``True``) and discard it.                                                                                                                                                                                                                                                                                  |
| Check if stack is empty                            | 0x38 | ``check_if_stack_is_empty(sv, fsv, v, fv)``: Check if the stack starting at variable ``sv`` (resolve its links if ``fsv`` is ``True``) is empty. If it is, store the value ``0x1`` in the variable ``v`` (resolve its links if ``fv`` i ``True``), store ``0x0`` otherwise.                                                                                                                                                                                    |
| Swap Variables                                     | 0x39 | ``swap_variables(va, fva, vb, fvb)``: Swap the contents of variable ``va`` (resolve its links if ``fva`` is ``True``) and variable ``vb`` (resolve its links if ``fvb`` is ``True``).                                                                                                                                                                                                                                                                          |
| Set Variable String Table Entry                    | 0x3a | ``set_variable_string_table_entry(v, fv, s)``: Set the string table entry at the position stored in the variable ``v`` (resolve its links if ``fv`` is ``True``) to the value ``s``.                                                                                                                                                                                                                                                                           |
| Print Variable String From String Table            | 0x3b | ``print_variable_string_from_string_table(v, fv)``: Print the content of the string item entry at the position stored in the variable ``v`` (resolve its links if ``fv`` is ``True``).                                                                                                                                                                                                                                                                         |
| Load Variable String Item Length Into Variable     | 0x3c | ``load_variable_string_item_length_into_variable(sv, fsv, v, fv)``: Loads the length of the string table item at index stored in the variable ``sv`` (resolve its links if ``fsv`` is ``True``) into variable ``v`` (resolve its links if ``fv`` is ``True``).                                                                                                                                                                                                 |
| Load Variable String Item Into Variables           | 0x3d | ``load_variable_string_item_into_variables(vs, fvs, v, fv)``: Consecutively loads the individual characters of the string item at the variable string table index ``vs`` (resolve its links if ``fvs`` is ``True``) into the variables starting at ``v`` (resolve its links first if ``fv`` is ``True``). This call expects for enough consecutive variables to be declared in the current variable space to house the string item to load into them entirely. |
| Terminate with Variable Return Code                | 0x3e | ``terminate_with_variable_return_code(v, fv)``: Immediately terminate the program and set the return code to the value stored in the variable ``v`` (resolve its links if ``fv`` is ``True``).                                                                                                                                                                                                                                                                 |
| Variable Bit-shift Variable Left                   | 0x3f | ``variable_bit_shift_variable_left(v, fv, vn, fvn)``: Bit-shift the variable ``v`` (resolve its links of ``fv`` is ``True``) by as many places as the value in the variable ``vn`` (resolve its links if ``fvn`` is ``True``) places to the left.                                                                                                                                                                                                              |
| Variable Bit-shift Variable Right                  | 0x40 | ``variable_bit_shift_variable_right(v, fv, vn, fvn)``: Bit-shift the variable ``v`` (resolve its links of ``fv`` is ``True``) by as many places as the value in the variable ``vn`` (resolve its links if ``fvn`` is ``True``) places to the right.                                                                                                                                                                                                            |
| Variable Rotate Variable Left                      | 0x41 | ``variable_rotate_variable_left(v, fv, vn, fvn)``: Bit-wise rotate the variable ``v`` (resolve its links of ``fv`` is ``True``) by as many places as the value in the variable ``vn`` (resolve its links if ``fvn`` is ``True``) places to the left.                                                                                                                                                                                                           |
| Variable Rotate Variable Right                     | 0x42 | ``variable_rotate_variable_right(v, fv, vn, fvn)``: Bit-wise rotate the variable ``v`` (resolve its links of ``fv`` is ``True``) by as many places as the value in the variable ``vn`` (resolve its links if ``fvn`` is ``True``) places to the right.                                                                                                                                                                                                         |
| Compare If Variable Is Greater Than Constant       | 0x43 | ``compare_if_variable_gt_constant(v, fv, vr, fvr, c)``: Compares variable ``v`` (resolve its links of ``fv`` is ``True``) to constant ``c``. If ``v`` is greater than ``c``, ``vr`` (resolve its links if ``fvr`` is ``True``) is set to ``0x1``, ``0x00`` otherwise.                                                                                                                                                                                          |
| Compare If Variable Is Less Than Constant          | 0x44 | ``compare_if_variable_lt_constant(v, fv, vr, fvr, c)``: Compares variable ``v`` (resolve its links of ``fv`` is ``True``) to constant ``c``. If ``v`` is less than ``c``, ``vr`` (resolve its links if ``fvr`` is ``True``) is set to ``0x1``, ``0x00`` otherwise.                                                                                                                                                                                             |
| Compare If Variable Is Equal To Constant           | 0x45 | ``compare_if_variable_eq_constant(v, fv, vr, fvr, c)``: Compares variable ``v`` (resolve its links of ``fv`` is ``True``) to constant ``c``. If ``v`` is equal to ``c``, ``vr`` (resolve its links if ``fvr`` is ``True``) is set to ``0x1``, ``0x00`` otherwise.                                                                                                                                                                                              |
| Compare If Variable Is Greater Than Other Variable | 0x46 | ``compare_if_variable_gt_variable(v, fv, vr, fvr, vb, fvb)``: Compares variable ``v`` (resolve its links of ``fv`` is ``True``) to variable ``vb`` (resolve its links if ``fvb`` is ``True``). If ``v`` is greater than ``vb``, ``vr`` (resolve its links if ``fvr`` is ``True``) is set to ``0x1``, ``0x00`` otherwise.                                                                                                                                       |
| Compare If Variable Is Less Than Other Variable    | 0x47 | ``compare_if_variable_lt_variable(v, fv, vr, fvr, vb, fvb)``: Compares variable ``v`` (resolve its links of ``fv`` is ``True``) to variable ``vb`` (resolve its links if ``fvb`` is ``True``). If ``v`` is less than ``vb``, ``vr`` (resolve its links if ``fvr`` is ``True``) is set to ``0x1``, ``0x00`` otherwise.                                                                                                                                          |
| Compare If Variable Is Equal To Other Variable     | 0x48 | ``compare_if_variable_eq_variable(v, fv, vr, fvr, vb, fvb)``: Compares variable ``v`` (resolve its links of ``fv`` is ``True``) to variable ``vb`` (resolve its links if ``fvb`` is ``True``). If ``v`` is equal to ``vb``, ``vr`` (resolve its links if ``fvr`` is ``True``) is set to ``0x1``, ``0x00`` otherwise.                                                                                                                                           |
| Get Maximum Of Variable And Constant               | 0x49 | ``get_max_of_variable_and_constant(v, fv, vr, fvr, c)``: Compares variable ``v`` (resolve its links of ``fv`` is ``True``) to constant ``c`` and stores the larger value in ``vr`` (resolve its links if ``fvr`` is ``True``)                                                                                                                                                                                                                                  |
| Get Minimum Of Variable And Constant               | 0x4a | ``get_min_of_variable_and_constant(v, fv, vr, fvr, c)``: Compares variable ``v`` (resolve its links of ``fv`` is ``True``) to constant ``c`` and stores the smaller value in ``vr`` (resolve its links if ``fvr`` is ``True``)                                                                                                                                                                                                                                 |
| Get Maximum Of Variable And Variable               | 0x4b | ``get_max_of_variable_and_variable(v, fv, vr, fvr, vb, fvb)``: Compares variable ``v`` (resolve its links of ``fv`` is ``True``) to variable ``vb`` (resolve its links if ``fvb`` is ``True``) and stores the larger value in ``vr`` (resolve its links if ``fvr`` is ``True``)                                                                                                                                                                                |
| Get Minimum Of Variable And Variable               | 0x4c | ``get_min_of_variable_and_variable(v, fv, vr, fvr, vb, fvb)``: Compares variable ``v`` (resolve its links of ``fv`` is ``True``) to variable ``vb`` (resolve its links if ``fvb`` is ``True``) and stores the smaller value in ``vr`` (resolve its links if ``fvr`` is ``True``)                                                                                                                                                                               |
