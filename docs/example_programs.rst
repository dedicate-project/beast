Example Programs
================

To illustrate how to implement BEAST programs, a couple of examples are provided. The following
example programs can be found as part of the BEAST environment in `the repository
<relativeJumpToAddressIfVariableGreaterThanZero>`_, with each featuring explanations of how the
programs work in their respective source code:


Hello World
-----------

This program defines the very basics for setting up, declaring, and executing a BEAST program. It
shows how to define an entry in a program's string table, and how to print it.

Its source can be found in `example/hello_world.cpp
<https://github.com/dedicate-project/beast/blob/main/examples/hello_world.cpp>`_.


Adder
-----

This program shows how to stream a series of input pairs into a program, perform a calculation (an
addition in this case), and signal that the output is available. The specific value of this program
is that it shows how external logic can communicate with program-internal logic.

Its source can be found in `examples/adder.cpp
<https://github.com/dedicate-project/beast/blob/main/examples/adder.cpp>`_.


Feedloop
--------

Similar to the `Adder` example from above, this program accepts streaming input from outside. In
addition to that, operations aren't executed in a straight batch, but are timed. This specifically
shows how to make BEAST programs reactive and execute logic upon specific conditions.

Its source can be found in `examples/feedloop.cpp
<https://github.com/dedicate-project/beast/blob/main/examples/feedloop.cpp>`_.


Bubblesort
----------

In this example program, the bubblesort sorting algorithm is implemented. This program does not use
streaming input, but accepts a fixed number of random integers that it then sorts and makes
available as output. The input and output arrays are printed for comparison.

In addition to that, this example illustrates how to use variable indirection - a technique where
one variable contains not a regular value, but the address to another variable (much like a pointer
or a refernence in other languages). This way, transparent array accesses can be implemented.

Its source can be found in `examples/bubblesort.cpp
<https://github.com/dedicate-project/beast/blob/main/examples/bubblesort.cpp>`_.
