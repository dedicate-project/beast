API
===

.. toctree::
   :maxdepth: 2
   :caption: Content

   program.rst
   cpu_virtual_machine.rst
   vm_session.rst
   virtual_machine.rst

This document contains the API available to integrate BEAST into other projects. The API spans over
multiple different classes that each have their own use-case in projects applying the BEAST library:

* :ref:`The Program Class`: The central entity holding BEAST programs. It has a
  high level programming interface to populate byte code programs without knowledge of their actual
  representation.

* :ref:`The CpuVirtualMachine Class`: The main execution engine for byte code programs. Is able to
  interpret the entire BEAST byte code operator set and makes use of a VmSession instance to
  maintain the state of a program.

* :ref:`The VmSession Class`: Holds the state of a program, including current instruction pointer,
  variable memory, and string table.

* :ref:`The VirtualMachine Class`: Acts as a base class for implementing custom virtual machines.
