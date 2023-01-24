# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0]

### Added

- Base class for Virtual Machine implementation.
- Concrete CPU-based implementation of a Virtual Machine.
- Definition of 76 operators to use in BEAST byte code programs.
- Classes for storing byte code programs and their execution state in a VM.
- Added examples (feedloop, adder, hello_world, bubblesort) for implementing
  programs in byte code.
- Added tests to cover all vital functions and operators.
- Wrote documentation for all APIs.
- Configured Doxygen, Sphinx, CircleCI, GitHub Actions / CodeQL, SonarQube,
  Coveralls.io, and a few minor badges for the README.md