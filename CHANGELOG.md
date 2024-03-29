# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.2]

### Added

- RuntimeStatistics object to store metadata about program execution
- Evaluator base class for scoring VmSession data and static program data
- RuntimeStatisticsEvaluator class scoring the dynamic runtime behavior and
  the static structure of a program
- AggregationEvaluator class for aggregating different evaluator scores
- OperatorUsageEvaluator class for evaluating how much a specific operator was used
  in a program
- Updated documentation for evaluator classes
- Added nothrow and noexcept tags to relevant methods
- Corrected tests accordingly
- Corrected ROADMAP.md by adding more detail to the second project phase
- Added an evaluator example

## [0.1.1]

### Added

- A base class for Program Factories: ProgramFactoryBase
- A concrete implementation of the base class for random programs: RandomProgramFactory
- Covered the random programs factory via tests
- Updated the Sphinx documentation to cover program factories
- Added an end-of-list size entry to the beast::OpCode enum
- Fixed long-standing AppVeyor signedness warnings
- Updated roadmap for program generators

### Fixed

- Fixed size program initialization is done via an unsigned value, not signed

## [0.1.0]

### Added

- Base class for Virtual Machine implementation.
- Concrete CPU-based implementation of a Virtual Machine.
- Definition of 77 operators to use in BEAST byte code programs.
- Classes for storing byte code programs and their execution state in a VM.
- Added examples (feedloop, adder, hello_world, bubblesort) for implementing
  programs in byte code.
- Added tests to cover all vital functions and operators.
- Wrote documentation for all APIs.
- Configured Doxygen, Sphinx, CircleCI, GitHub Actions / CodeQL, SonarQube,
  Coveralls.io, and a few minor badges for the README.md
