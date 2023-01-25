Runtime Statistics
==================

Each VmSession object records runtime statistics (in the form of a
`beast::VmSession::RuntimeStatistics` object) for the program that is currently being executed in
its context. These statistics include data about the Virtual Machine's execution behavior, and the
termination status of the program:

* How many steps were executed in total?
* Was the program terminated?
* Was the program termination abnormal (i.e., was an exception thrown)?
* What was the program's return code?
* Which operator was executed how many times?

These statistics can be used to analyze the quality of the program, the fitness for any particular
use-case that depends on this metadata, or simply for statistical purposes.

The statistics object can be retrieved from the executing code outside of a program any time,
independent from whether the program has terminated or not. It will contain the most up to date
information about the execution. The relevant struct and calls can be found in the
`beast::VmSession` class:

.. doxygenstruct:: beast::VmSession::RuntimeStatistics

.. doxygenfunction:: beast::VmSession::informAboutStep

.. doxygenfunction:: beast::VmSession::resetRuntimeStatistics

.. doxygenfunction:: beast::VmSession::getRuntimeStatistics
