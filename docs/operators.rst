Operators
=========

Operators are the building blocks of BEAST programs. They are uniquely identified by an opcode
(operator code) each (see `this list
<https://github.com/dedicate-project/beast/blob/main/include/beast/opcodes.hpp>`_) and have varying
numbers of parameters. In this document, instructions are given for adding new operator, and an
overview of what operators are available already.

Adding a new operator
+++++++++++++++++++++

To add a new operator to the BEAST library, you will need to follow these steps:

1. **Add a new opcode:** Append the new opcode with the next higher hex value in an appropriate
   category in the `enum class OpCode` in `include/beast/opcodes.hpp`.

2. **Add a signature for this opcode:** Add a new function signature in `include/beast/program.hpp`
   that corresponds to your new opcode. This will allow client programs to add the operator to a
   program.

3. **Implement the program:** Add the implementation for the new function you added in step 2 in
   `src/program.cpp`. Follow the example of existing operators, but in general: Use
   `appendCode1(...)` with the opcode you added in step 1, and then add any bytes you deem necessary
   for your operator.

4. **Add a VM case for your new opcode:** In `src/cpu_virtual_machine.cpp`, extend the `switch`
   statement in the `step()` function. Add a new case for your new opcode, read all parameters (no
   need to read the opcode, that's done automatically), print a debug statement with the operator
   signature, and call a `VmSession` function that matches your operator.

5. **Define the VmSession function:** If there is no suitable `VmSession` function, define its
   signature in `include/beast/vm_session.hpp`. This is a function that explicitly accepts the
   parameters for your operator. See the existing ones for how to define them.

6. **Implement the VmSession function:** In `src/vm_session.cpp`, implement the actual logic that
   should be executed if the VM encounters your operator. Again, see existing operators for how to
   do it.

Already implemented operators
+++++++++++++++++++++++++++++

The BEAST byte code is using a defined set of operators that are documented `in this OpCodes header
file <https://github.com/dedicate-project/beast/blob/main/include/beast/opcodes.hpp>`_. These
available operator codes are organized in ... distinct categories:

Variable management
-------------------

These operators are used to manage variables such as declaring variables with index and type,
setting variable values, undeclaring variables, copying variable values, and swapping variable
values.

.. doxygenfunction:: beast::Program::declareVariable

.. doxygenfunction:: beast::Program::undeclareVariable

.. doxygenfunction:: beast::Program::setVariable

.. doxygenfunction:: beast::Program::copyVariable

.. doxygenfunction:: beast::Program::swapVariables

Math
----

These operators perform mathematical operations such as adding or subtracting constants or
variables, comparing variables for greater than, less than, or equality, getting the maximum or
minimum of variables or constants, and performing modulo operations on variables.

.. doxygenfunction:: beast::Program::addConstantToVariable

.. doxygenfunction:: beast::Program::addVariableToVariable

.. doxygenfunction:: beast::Program::subtractConstantFromVariable

.. doxygenfunction:: beast::Program::subtractVariableFromVariable

.. doxygenfunction:: beast::Program::compareIfVariableGtConstant

.. doxygenfunction:: beast::Program::compareIfVariableLtConstant

.. doxygenfunction:: beast::Program::compareIfVariableEqConstant

.. doxygenfunction:: beast::Program::compareIfVariableGtVariable

.. doxygenfunction:: beast::Program::compareIfVariableLtVariable

.. doxygenfunction:: beast::Program::compareIfVariableEqVariable

.. doxygenfunction:: beast::Program::getMaxOfVariableAndConstant

.. doxygenfunction:: beast::Program::getMinOfVariableAndConstant

.. doxygenfunction:: beast::Program::getMaxOfVariableAndVariable

.. doxygenfunction:: beast::Program::getMinOfVariableAndVariable

.. doxygenfunction:: beast::Program::moduloVariableByConstant

.. doxygenfunction:: beast::Program::moduloVariableByVariable

Bit manipulation
----------------

These operators perform bit manipulation operations on variables such as bit shifting left or right,
bitwise AND, OR, and NOT operations, and checking bit values.

.. doxygenfunction:: beast::Program::bitShiftVariableLeft

.. doxygenfunction:: beast::Program::bitShiftVariableRight

.. doxygenfunction:: beast::Program::bitWiseInvertVariable

.. doxygenfunction:: beast::Program::bitWiseAndTwoVariables

.. doxygenfunction:: beast::Program::bitWiseOrTwoVariables

.. doxygenfunction:: beast::Program::bitWiseXorTwoVariables

.. doxygenfunction:: beast::Program::rotateVariableLeft

.. doxygenfunction:: beast::Program::rotateVariableRight

.. doxygenfunction:: beast::Program::variableBitShiftVariableLeft

.. doxygenfunction:: beast::Program::variableBitShiftVariableRight

.. doxygenfunction:: beast::Program::variableRotateVariableLeft

.. doxygenfunction:: beast::Program::variableRotateVariableRight

Stacks
------

These operators manage the content of virtual stacks used for storing and retrieving data items in a
specific order.

.. doxygenfunction:: beast::Program::pushVariableOnStack

.. doxygenfunction:: beast::Program::pushConstantOnStack

.. doxygenfunction:: beast::Program::popVariableFromStack

.. doxygenfunction:: beast::Program::popTopItemFromStack

.. doxygenfunction:: beast::Program::checkIfStackIsEmpty

Jumps
-----

These operators control the flow of a program such as performing absolute and relative jumps
optionally based on various different conditions.

.. doxygenfunction:: beast::Program::relativeJumpToVariableAddressIfVariableGreaterThanZero

.. doxygenfunction:: beast::Program::relativeJumpToVariableAddressIfVariableLessThanZero

.. doxygenfunction:: beast::Program::relativeJumpToVariableAddressIfVariableEqualsZero

.. doxygenfunction:: beast::Program::absoluteJumpToVariableAddressIfVariableGreaterThanZero

.. doxygenfunction:: beast::Program::absoluteJumpToVariableAddressIfVariableLessThanZero

.. doxygenfunction:: beast::Program::absoluteJumpToVariableAddressIfVariableEqualsZero

.. doxygenfunction:: beast::Program::relativeJumpToAddressIfVariableGreaterThanZero

.. doxygenfunction:: beast::Program::relativeJumpToAddressIfVariableLessThanZero

.. doxygenfunction:: beast::Program::relativeJumpToAddressIfVariableEqualsZero

.. doxygenfunction:: beast::Program::absoluteJumpToAddressIfVariableGreaterThanZero

.. doxygenfunction:: beast::Program::absoluteJumpToAddressIfVariableLessThanZero

.. doxygenfunction:: beast::Program::absoluteJumpToAddressIfVariableEqualsZero

.. doxygenfunction:: beast::Program::unconditionalJumpToAbsoluteAddress

.. doxygenfunction:: beast::Program::unconditionalJumpToAbsoluteVariableAddress

.. doxygenfunction:: beast::Program::unconditionalJumpToRelativeAddress

.. doxygenfunction:: beast::Program::unconditionalJumpToRelativeVariableAddress

I/O
---

These operators handle input and output operations such as identifying input/output characteristics
of variables, and finding out which and how many I/O variables there are.

.. doxygenfunction:: beast::Program::checkIfVariableIsInput

.. doxygenfunction:: beast::Program::checkIfVariableIsOutput

.. doxygenfunction:: beast::Program::loadInputCountIntoVariable

.. doxygenfunction:: beast::Program::loadOutputCountIntoVariable

.. doxygenfunction:: beast::Program::checkIfInputWasSet

Printing and string table
-------------------------

These operators produce screen output and manage the content of the string table, such as storing or
retrieving items, but also providing information about them and the string table in general.

.. doxygenfunction:: beast::Program::printVariable

.. doxygenfunction:: beast::Program::setStringTableEntry

.. doxygenfunction:: beast::Program::printStringFromStringTable

.. doxygenfunction:: beast::Program::loadStringTableLimitIntoVariable

.. doxygenfunction:: beast::Program::loadStringTableItemLengthLimitIntoVariable

.. doxygenfunction:: beast::Program::setVariableStringTableEntry

.. doxygenfunction:: beast::Program::printVariableStringFromStringTable

.. doxygenfunction:: beast::Program::loadVariableStringItemLengthIntoVariable

.. doxygenfunction:: beast::Program::loadVariableStringItemIntoVariables

.. doxygenfunction:: beast::Program::loadStringItemLengthIntoVariable

.. doxygenfunction:: beast::Program::loadStringItemIntoVariables

Misc
----

These operators perform general-purpose actions such as loading memory size into a variable, loading
the current execution pointer address into a variable, terminating the program with a fixed or
variable return code, performing system calls, and loading random values into variables.

.. doxygenfunction:: beast::Program::noop

.. doxygenfunction:: beast::Program::loadMemorySizeIntoVariable

.. doxygenfunction:: beast::Program::loadCurrentAddressIntoVariable

.. doxygenfunction:: beast::Program::terminate

.. doxygenfunction:: beast::Program::terminateWithVariableReturnCode

.. doxygenfunction:: beast::Program::performSystemCall

.. doxygenfunction:: beast::Program::loadRandomValueIntoVariable

