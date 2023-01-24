Bytecode Virtual Machine
========================

The foundation of the BEAST framework is a bytecode virtual machine environment. It provides two
essential elements: A bytecode definition for Turing complete programs, and a parameterizable
virtual machine that can execute those programs.

.. toctree::
   :maxdepth: 1
   :caption: Table of Contents

   api.rst
   operators.rst
   real_world_applications.rst
   example_programs.rst


Bytecode Programs
-----------------

The bytecode that is used in BEAST programs is identified by a custom set of operators that each
execute a well-defined action within a virtualized environment.

The operators supported by BEAST are listed in the :ref:`Operators` document, complete with a
programming interface. For examples how to use these operators, please refer to the :ref:`Example
Programs` document.


Virtual Machine
---------------

The virtual machine concept provided within the BEAST environment allows to execute bytecode
programs. The individual classes and their programming interface used in the virtual machine are
described in the :ref:`API` document.
