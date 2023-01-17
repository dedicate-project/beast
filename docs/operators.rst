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

TODO(fairlight1337): Add references to actual operator documentation.

Math
----

These operators perform mathematical operations such as adding or subtracting constants or
variables, comparing variables for greater than, less than, or equality, getting the maximum or
minimum of variables or constants, and performing modulo operations on variables.

TODO(fairlight1337): Add references to actual operator documentation.

Bit manipulation
----------------

These operators perform bit manipulation operations on variables such as bit shifting left or right,
bitwise AND, OR, and NOT operations, and checking bit values.

TODO(fairlight1337): Add references to actual operator documentation.

Stacks
------

These operators manage the content of virtual stacks used for storing and retrieving data items in a
specific order.

TODO(fairlight1337): Add references to actual operator documentation.

Jumps
-----

These operators control the flow of a program such as performing absolute and relative jumps
optionally based on various different conditions.

TODO(fairlight1337): Add references to actual operator documentation.

I/O
---

These operators handle input and output operations such as identifying input/output characteristics
of variables, and finding out which and how many I/O variables there are.

TODO(fairlight1337): Add references to actual operator documentation.

Misc
----

These operators perform general-purpose actions such as loading memory size into a variable, loading
the current execution pointer address into a variable, terminating the program with a fixed or
variable return code, performing system calls, and loading random values into variables.

TODO(fairlight1337): Add references to actual operator documentation.
