Operators
=========

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
