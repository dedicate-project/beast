# BEAST - Binary Evolution And Sentience Toolkit

[![CircleCI](https://circleci.com/gh/dedicate-project/beast/tree/main.svg?style=shield)](https://circleci.com/gh/dedicate-project/beast/tree/main)
[![CodeQL](https://github.com/dedicate-project/beast/actions/workflows/codeql.yml/badge.svg?branch=main)](https://github.com/dedicate-project/beast/actions/workflows/codeql.yml?branch=main)
[![AppVeyor](https://ci.appveyor.com/api/projects/status/0a51i8ax0vg92p6k/branch/main?svg=true)](https://ci.appveyor.com/project/fairlight1337/beast/branch/main)
[![Documentation Status](https://readthedocs.org/projects/beast-project/badge/?version=latest)](https://beast-project.readthedocs.io/en/latest/?badge=latest)
[![Coverage Status](https://coveralls.io/repos/github/dedicate-project/beast/badge.svg?branch=main)](https://coveralls.io/github/dedicate-project/beast?branch=main)
[![Maintainability Rating](https://sonarcloud.io/api/project_badges/measure?project=dedicate-project_beast&metric=sqale_rating&branch=main)](https://sonarcloud.io/summary/new_code?id=dedicate-project_beast&branch=main)
[![Code Smells](https://sonarcloud.io/api/project_badges/measure?project=dedicate-project_beast&metric=code_smells&branch=main)](https://sonarcloud.io/summary/new_code?id=dedicate-project_beast&branch=main)
[![Lines of Code](https://sonarcloud.io/api/project_badges/measure?project=dedicate-project_beast&metric=ncloc&branch=main)](https://sonarcloud.io/summary/new_code?id=dedicate-project_beast&branch=main)
[![Technical Debt](https://sonarcloud.io/api/project_badges/measure?project=dedicate-project_beast&metric=sqale_index&branch=main)](https://sonarcloud.io/summary/new_code?id=dedicate-project_beast&branch=main)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
![Maintenance](https://img.shields.io/maintenance/yes/2023)
![Platforms](https://img.shields.io/badge/platforms-linux%20%7C%20windows-lightgrey)
![Version](https://img.shields.io/github/v/release/dedicate-project/beast?sort=semver)

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
  while (vm.step(session, false)) {
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
sudo apt install clang-tidy ccache libasio-dev
```

If you are on Ubuntu 22.04 or newer and encounter an issue where `clang-tidy` cannot find standard headers (like `<array>`), install this package:
```bash
sudo apt install libstdc++-12-dev
```
ane try compiling again.

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

To speed up the build process, increase the number of processes used by the `make` command via `make
-j$(nproc)`.

To run all tests, afterwards run:
```bash
make test
```

To not build the tests (and save some time while developing or you just don't need them) you can disable them in CMake using this when configuring the build:
```bash
cmake -DBEAST_BUILD_TESTS=NO ..
```

### Optional GPU backend (CUDA)

BEAST ships an optional CUDA backend that runs the SHA-256 round evaluator on the GPU
via `cuda::CudaSha256RoundEvaluator` -- a `BatchEvaluator` that drops into any
`EvolutionPipe::setBatchEvaluator`. Programs are compiled once on the host (BEAST
bytecode → a tight 16-byte instruction stream), uploaded to device memory, and run by
a `__device__` VM covering the arithmetic / bit / comparison / constant-target-jump
opcode subset typical of evolved SHA-256 candidates. Opcodes the device VM doesn't
implement (system calls, string tables, stacks, subroutines, variable-target jumps)
fold to no-ops at compile time.

CUDA support is **on by default**. The configure step probes for a CUDA compiler and
the CUDA Toolkit: if either is missing, the option auto-flips to OFF with a one-line
status message and the build proceeds CPU-only. There's nothing to install for the
common case beyond the regular dependencies. To force the GPU build off (e.g. to
skip the ~20 s extra compile time on a CPU-only laptop):
```bash
cmake -DBEAST_ENABLE_CUDA=OFF ..
```

For maximum runtime speed, **always configure a release build** -- debug builds
default to `-O0` plus coverage instrumentation, which makes the CPU-side host
code 10-100x slower (and dwarfs any GPU speedup):
```bash
cmake -DCMAKE_BUILD_TYPE=Release -DBEAST_BUILD_TESTS=OFF ..
cmake --build . -j$(nproc)
```

Without a CUDA-capable GPU the CUDA-enabled binary still runs:
`cuda::isCudaAvailable()` returns false on hosts without a driver, and any pipe that
would have injected the GPU evaluator falls back to the CPU `ThreadPoolBatchEvaluator`.

#### Sizing pipelines for the GPU

The GPU kernel runs **one thread per genome per generation**, so GPU utilisation
is driven by the GA population size, not the individual genome length:

| Pipe knob                                  | What it controls                                                       | GPU implication                                                                                          |
|--------------------------------------------|------------------------------------------------------------------------|----------------------------------------------------------------------------------------------------------|
| `EvaluatorPipe.max_candidates`             | GA population per generation (== GPU batch size per launch)            | The big lever. Must be ≥ 32 to fill one warp; ≥ 256 to start hiding launch overhead; **≥ 1024 to actually saturate a modern GPU** (e.g. RTX 5880 has 110 SMs and wants thousands of threads in flight). |
| `evolution_parameters.starting_program_size` / `max_genome_bytes` | Per-thread instruction stream length         | Longer programs mean more work per thread, which amortises CUDA launch + H2D copy overhead. 256-512 bytes is a good floor; 4 KB is fine.                                  |
| `evaluators[0].parameters.max_steps_per_trial` × `trial_count` × `rounds_per_trial` | Total VM steps per thread                  | Same story as genome length: more steps = more compute per launch. The SHA-256 example uses 4000 × 8 × 1 = 32k steps per thread, plenty.                                  |
| `EvaluatorPipe.memory_variables`           | Variable count per VM                                                  | The GPU register file is fixed at 32; values above 32 are clamped on the GPU (CPU is unbounded). Stay at ≤ 32 for bit-exact CPU/GPU parity.                                |

Rule of thumb for the SHA-256 round on an RTX 5880: bump `max_candidates` from
the CPU-friendly default of 24 to **1024 or more** before you'll see the GPU pull
ahead. The bundled
[`examples/compose-pipelines/sha256-round-gpu.json`](examples/compose-pipelines/sha256-round-gpu.json)
uses `max_candidates: 1024` and `"backend": "auto"` as a ready-to-load starting
point. Don't raise the GA `generations` per cycle along with the population --
keeping it at 8-16 keeps the UI's progress counters responsive while still
issuing big batches to the GPU.

#### Selecting the backend from pipeline JSON

Activate the GPU path per-pipe by setting `parameters.backend` on an `EvaluatorPipe`:

```json
"sha256_round": {
  "type": "EvaluatorPipe",
  "parameters": {
    "backend": "auto",                // "cpu" (default) | "gpu" | "auto"
    "evaluators": [{ "type": "Sha256RoundEvaluator", ... }]
    // ... rest of the pipe config ...
  }
}
```

The Compose UI exposes the same knob in the per-pipe edit dialog under
"Execution backend". The selection rules are:

| Value   | Behaviour                                                                                                                         |
|---------|-----------------------------------------------------------------------------------------------------------------------------------|
| `cpu`   | Use the CPU thread-pool evaluator (the default; identical to all prior versions).                                                 |
| `gpu`   | Use the CUDA evaluator when (a) built with `BEAST_ENABLE_CUDA`, (b) a CUDA device is visible, and (c) the evaluator has a GPU port (today only `Sha256RoundEvaluator`). Otherwise silently falls back to CPU. |
| `auto`  | Same as `gpu`, intended for JSON shared across GPU and CPU-only hosts.                                                            |

The field is optional; pipelines without a `backend` key keep their prior CPU
behaviour exactly. A worked example lives at
[`examples/compose-pipelines/sha256-round-gpu.json`](examples/compose-pipelines/sha256-round-gpu.json),
which mirrors the stock `sha256-round.json` but adds `"backend": "auto"` so it
opportunistically uses the GPU when one is present.

CPU/GPU parity is enforced by `tests/cuda_sha256_parity.cpp`: identical genome bytes
produce identical scores on both backends (bit-exact for the supported-opcode subset).
The performance win is workload-dependent -- BEAST's typical populations of 32-128
genomes leave most of the GPU's SMs idle, and the device VM is divergence-bound by
its 40-way switch dispatch. The Tier 2 architecture sets up future improvements
(warp-cooperative scheduling, multi-pipeline GPU sharing, larger batched workloads)
without changing the user-facing API.

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

## Running BEAST Compose

Compose is a web frontend/backend combination that allows intuitive interaction with evolutionary
pipelines using BEAST. To run it, from the root directory of the project, run:
```bash
./build/bin/beast-compose --html_root html_static/ --storage_folder models
```

This will start a web server that exposes the BEAST interface at the address http://0.0.0.0:9192
. Any pipeline models will be read from and stored to the folder given as `storage_folder`, the
served HTML content will be served from the folder given as `html_root`.

## Defined Operators

Information about which operators are available (including a detailed description and a programming
interface reference) can be found in the [RTD Operators
section](https://beast-project.readthedocs.io/en/latest/operators.html).
