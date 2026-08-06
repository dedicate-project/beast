cmake_minimum_required(VERSION 3.18) # CUDA support requires CMake >= 3.18 for the
                                     # modern `enable_language(CUDA)` + CUDAToolkit
                                     # find_package combo we use under BEAST_ENABLE_CUDA.
project(beast
  LANGUAGES CXX
  VERSION 0.1.2
  DESCRIPTION "Byte code generator, parser, and virtual machine for evaluating evolutionary code sequences."
  HOMEPAGE_URL "https://github.com/dedicate-project/beast/")

option(BEAST_BUILD_TESTS "Whether to build tests or not" YES)
option(BEAST_BUILD_PIPELINES "Whether to build example pipelines or not" YES)
option(BEAST_BUILD_PROGRAMS "Whether to build example programs or not" YES)
option(BEAST_BUILD_DOCS "Whether to build documentation or not" YES)
# Tier 2: GPU acceleration. Enabled by default so the `backend: "gpu" | "auto"` field
# on EvaluatorPipe Just Works out of the box. Builds the `beast-cuda` library
# containing `cuda::CudaSha256RoundEvaluator` (a `BatchEvaluator` implementation that
# drops into any `EvolutionPipe::setBatchEvaluator`). Requires CUDA Toolkit
# (`find_package(CUDAToolkit)`); on hosts without a CUDA driver the library still
# builds and links, but `cuda::isCudaAvailable()` returns false at runtime and pipes
# silently fall back to the CPU `ThreadPoolBatchEvaluator`. Configure with
# `-DBEAST_ENABLE_CUDA=OFF` to skip the CUDA build entirely (saves ~20s of compile
# time and removes the CUDA Toolkit requirement).
option(BEAST_ENABLE_CUDA "Build the optional CUDA-backed GPU evaluators" YES)
# Running clang-tidy with `-checks=*,-warnings-as-errors=*` is extremely sensitive to the local
# clang-tidy version: a fresh checks list ships with every LLVM release and almost any newer
# revision will refuse to compile against this codebase. We therefore make the check opt-in so
# that day-to-day builds are reproducible. CI should enable it explicitly with a pinned
# clang-tidy version.
#
# Note for Ubuntu 22.04 hosts: clang-tidy uses the compiler's resource directory to find the
# C++ standard library headers. Stock 22.04 ships only libstdc++-11-dev which has a known
# layout incompatibility with clang-tidy 14+. Install the matching newer headers with:
#     sudo apt install libstdc++-12-dev
# Otherwise you will see "fatal error: 'string' file not found" while parsing system headers.
option(BEAST_ENABLE_CLANG_TIDY "Run clang-tidy as part of the build (CI only)" NO)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

# Add utilities used for evaluations. This needs to be placed before the clang-tidy configuration to
# not impose clang rules into 3rdparty components.
set(MAZE_BUILD_TESTS NO)
add_subdirectory(${CMAKE_SOURCE_DIR}/3rdparty/maze)

if(BEAST_ENABLE_CLANG_TIDY)
  # The disable list below is split per-line so the rationale is reviewable. Categories:
  #   * Style / formatting churn (use auto, trailing return types, braced init lists,
  #     identifier length, magic numbers): leave to clang-format + reviewer judgement.
  #   * False positives in switch statements over related opcodes (branch-clone, identical
  #     RHS), in POD struct init (member-init), and in correct bitwise math on small ints
  #     promoted to int (signed-bitwise, narrowing-conversions).
  #   * Aspirational rules that conflict with the project's existing API choices (default
  #     argument decls, public POD members, type-const-cast around 3rdparty types).
  #   * Altera / llvmlibc / fuchsia / cppcoreguidelines fluff that doesn't apply to a host
  #     C++17 evolution toolkit.
  set(BEAST_CLANG_TIDY_DISABLED_CHECKS
      -modernize-use-trailing-return-type
      -modernize-use-auto
      -modernize-return-braced-init-list
      -cppcoreguidelines-avoid-magic-numbers
      -cppcoreguidelines-narrowing-conversions
      -cppcoreguidelines-pro-type-member-init
      -cppcoreguidelines-pro-type-const-cast
      -readability-magic-numbers
      -readability-identifier-length
      -readability-function-cognitive-complexity
      -bugprone-branch-clone
      -bugprone-easily-swappable-parameters
      -bugprone-narrowing-conversions
      -hicpp-use-auto
      -hicpp-member-init
      -hicpp-signed-bitwise
      -fuchsia-default-arguments-calls
      -fuchsia-default-arguments-declarations
      -misc-no-recursion
      -misc-non-private-member-variables-in-classes
      -llvmlibc-callee-namespace
      -llvmlibc-implementation-in-namespace
      -llvmlibc-restrict-system-libc-headers
      -altera-id-dependent-backward-branch
      -altera-unroll-loops
      -altera-struct-pack-align
      # We don't depend on absl, so reroute the suggestion-to-use-absl checks. Replacing
      # std::string::find != npos with absl::StrContains would force us to vendor abseil
      # purely to satisfy this lint.
      -abseil-string-find-str-contains)
  string(REPLACE ";" "," BEAST_CLANG_TIDY_CHECKS "${BEAST_CLANG_TIDY_DISABLED_CHECKS}")
  set(CMAKE_CXX_CLANG_TIDY
      "clang-tidy;-warnings-as-errors=*;-checks=*,${BEAST_CLANG_TIDY_CHECKS}")
endif()

find_program(CLANG_FORMAT_EXECUTABLE NAMES clang-format clang-format-12 clang-format-11 clang-format-10 clang-format-9)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_COMPILER_LAUNCHER "ccache")

if(NOT CMAKE_GENERATOR MATCHES "Visual Studio")
  # Scope the strict-by-default flags to C++ only -- nvcc rejects `-pedantic` because
  # it doesn't recognise the value for its own `-Werror` option, and a global add
  # would otherwise leak into every CUDA translation unit.
  add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Werror>
                     $<$<COMPILE_LANGUAGE:CXX>:-pedantic>)
endif()

if(BEAST_BUILD_DOCS)
  add_subdirectory(docs)
endif()

# Create a list of all .h files in the "ga" subdirectory of the galib source directory
file(GLOB GALIB_HEADERS ${CMAKE_SOURCE_DIR}/3rdparty/galib/ga/*.h)

# Copy the .h files to the galib_src/ga subdirectory of the build directory
file(COPY ${GALIB_HEADERS} DESTINATION ${CMAKE_BINARY_DIR}/galib_src/ga/)

set(GALIB_SRCS
  garandom.C gaerror.C GAParameter.C GAStatistics.C GABaseGA.C GASStateGA.C
  GASimpleGA.C GAIncGA.C GADemeGA.C GADCrowdingGA.C GASelector.C GAScaling.C
  GAPopulation.C GAGenome.C GABinStr.C gabincvt.C GAAllele.C GA1DBinStrGenome.C
  GA2DBinStrGenome.C GA3DBinStrGenome.C GABin2DecGenome.C GATreeBASE.C GATree.C
  GATreeGenome.C GAListBASE.C GAList.C GAListGenome.C
  # The following source files are specifically ignored because they are not needed and cause
  # compilation issues.
  # GAStringGenome.C GARealGenome.C GA1DArrayGenome.C GA2DArrayGenome.C GA3DArrayGenome.C
  )

set(GALIB_SRCS_FULL)
foreach(file ${GALIB_SRCS})
  list(APPEND GALIB_SRCS_FULL ${CMAKE_SOURCE_DIR}/3rdparty/galib/ga/${file})
endforeach()
# Copy all files in GALIB_SRCS_FULL to the build directory's galib_src subdirectory
file(COPY ${GALIB_SRCS_FULL} DESTINATION ${CMAKE_BINARY_DIR}/galib_src/ga)
if(CMAKE_GENERATOR MATCHES "Visual Studio")
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()
add_library(galib ${GALIB_SRCS_FULL})
set_target_properties(galib PROPERTIES CXX_CLANG_TIDY "")
if(NOT CMAKE_GENERATOR MATCHES "Visual Studio")
  target_compile_options(galib PRIVATE -Wno-register -Wno-write-strings -Wno-permissive)
else()
  target_compile_options(galib PRIVATE /wd4996 /wd5033 /wd4244 /wd4267)
endif()

if(CMAKE_BUILD_TYPE MATCHES "Debug")
  set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/3rdparty/cmake-modules ${CMAKE_MODULE_PATH})
  include(CodeCoverage)

  add_compile_options(-g -O0 -lprofiler -fprofile-arcs -ftest-coverage -coverage)

  setup_target_for_coverage_lcov(NAME coverage
    EXECUTABLE make test
    BASE_DIRECTORY ${CMAKE_SOURCE_DIR}
    EXCLUDE ${CMAKE_SOURCE_DIR}/3rdparty/* ${CMAKE_SOURCE_DIR}/tests/* /usr/include/c++/* /usr/include/x86_64-linux-gnu/c++/* ${CMAKE_SOURCE_DIR}/examples/* ${CMAKE_SOURCE_DIR}/pipelines/* asio/* ${CMAKE_SOURCE_DIR}/src/compose.cpp ${CMAKE_BINARY_DIR}/* ${CMAKE_BINARY_DIR}/galib_src/* ${CMAKE_SOURCE_DIR}/compose.cpp
    SONARQUBE)
else()
  if(NOT CMAKE_GENERATOR MATCHES "Visual Studio")
    add_compile_options(-O3)
  endif()
endif(CMAKE_BUILD_TYPE MATCHES "Debug")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Utilities library
add_library(${PROJECT_NAME}-utils
  src/filesystem_helper.cpp
  src/time_functions.cpp
  src/version.cpp)

# VM library
add_library(${PROJECT_NAME}-vm
  src/cpu_virtual_machine.cpp
  src/program.cpp
  src/program_c_codegen.cpp
  src/program_disassembler.cpp
  src/program_parser.cpp
  src/random_program_factory.cpp
  src/subroutine_library.cpp
  src/virtual_machine.cpp
  src/vm_session.cpp)

# Evaluators library
add_library(${PROJECT_NAME}-evaluators
  src/evaluators/adder_evaluator.cpp
  src/evaluators/aggregation_evaluator.cpp
  src/evaluators/bit_distance_evaluator.cpp
  src/evaluators/bit_reverse_evaluator.cpp
  src/evaluators/bitwise_evaluator.cpp
  src/evaluators/identity_evaluator.cpp
  src/evaluators/maximum_evaluator.cpp
  src/evaluators/maze_evaluator.cpp
  src/evaluators/minimum_evaluator.cpp
  src/evaluators/parity_evaluator.cpp
  src/evaluators/popcount_evaluator.cpp
  src/evaluators/random_serial_data_passthrough_evaluator.cpp
  src/evaluators/rotate_evaluator.cpp
  src/evaluators/sha256_ch_evaluator.cpp
  src/evaluators/sha256_maj_evaluator.cpp
  src/evaluators/sha256_round_evaluator.cpp
  src/evaluators/sha256_sigma_evaluator.cpp
  src/evaluators/runtime_statistics_evaluator.cpp
  src/evaluators/operator_usage_evaluator.cpp)

# Pipelines library
add_library(${PROJECT_NAME}-pipelines
  src/internal/beast_ga.cpp
  src/pipe.cpp
  src/pipeline.cpp
  src/pipeline_manager.cpp
  src/pipeline_server.cpp
  src/pipes/demultiplexer_pipe.cpp
  src/pipes/evaluator_pipe.cpp
  src/pipes/evolution_pipe.cpp
  src/pipes/fan_pipe.cpp
  src/pipes/filter_pipe.cpp
  src/pipes/multiplexer_pipe.cpp
  src/pipes/null_sink_pipe.cpp
  src/pipes/program_factory_pipe.cpp
  src/pipes/program_storage_sink_pipe.cpp
  src/pipes/program_storage_source_pipe.cpp
  src/pipes/results_summary_pipe.cpp
  src/pipes/score_graph_pipe.cpp)

set(PROJECT_LIBS ${PROJECT_NAME}-pipelines ${PROJECT_NAME}-evaluators ${PROJECT_NAME}-utils ${PROJECT_NAME}-vm)

# ---- Optional CUDA backend ----------------------------------------------------------
#
# When `BEAST_ENABLE_CUDA=ON` we enable the CUDA language for the project and build a
# small `beast-cuda` library containing the device kernels + host-side glue. The
# library is linked into PROJECT_LIBS unconditionally (CMake propagates the link to
# every consumer) so any pipeline binary can call into `beast::cuda::` symbols.
#
# A runtime check (`isCudaAvailable()`) gates actual GPU use, so a binary built with
# CUDA support still runs on machines without a CUDA driver -- the GPU evaluator
# constructor will fail, callers catch and fall back to CPU.
if(BEAST_ENABLE_CUDA)
  # Because BEAST_ENABLE_CUDA defaults to YES, we can't just hard-require the CUDA
  # Toolkit -- contributors and CI hosts without a CUDA install would all see a
  # cryptic configure failure on what is otherwise a CPU-only build. Probe for the
  # toolkit first; if it isn't there, downgrade the flag, emit a one-line status,
  # and continue with the CPU-only build. Users on CUDA-less hosts who want the
  # warning gone can pass `-DBEAST_ENABLE_CUDA=OFF` explicitly.
  include(CheckLanguage)
  check_language(CUDA)
  if(CMAKE_CUDA_COMPILER)
    enable_language(CUDA)
    find_package(CUDAToolkit QUIET)
    if(NOT CUDAToolkit_FOUND)
      message(STATUS
        "BEAST_ENABLE_CUDA=ON but CUDAToolkit was not found; "
        "disabling CUDA backend (the `backend: \"gpu\"` JSON knob will silently "
        "fall back to CPU). Pass -DBEAST_ENABLE_CUDA=OFF to silence this notice.")
      set(BEAST_ENABLE_CUDA OFF CACHE BOOL "" FORCE)
    endif()
  else()
    message(STATUS
      "BEAST_ENABLE_CUDA=ON but no CUDA compiler was detected on PATH; "
      "disabling CUDA backend (the `backend: \"gpu\"` JSON knob will silently "
      "fall back to CPU). Pass -DBEAST_ENABLE_CUDA=OFF to silence this notice.")
    set(BEAST_ENABLE_CUDA OFF CACHE BOOL "" FORCE)
  endif()
endif()

# Second pass: the actual CUDA build only runs when the probe above succeeded. We
# split it out so the option flip-to-OFF is visible to the rest of the file (target
# install lists, beast-pipelines compile definitions, the parity test, ...).
if(BEAST_ENABLE_CUDA)
  # CMake 3.18+ requires every target to declare CUDA_ARCHITECTURES once the CUDA
  # language is enabled, even for targets that contain no CUDA source. Set a sane
  # default at the project level; the GPU library overrides it below with its own list.
  if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES)
    set(CMAKE_CUDA_ARCHITECTURES "70;75;80;86;89")
  endif()
  add_library(${PROJECT_NAME}-cuda
    src/cuda/compiler.cpp
    src/cuda/opcode_coverage.cpp
    src/cuda/cuda_sha256_round_evaluator.cu)
  # Compute capability 7.0+ covers everything from Volta (V100) forward; Ada (the
  # development host) is 8.9. We compile for a broad range to keep binaries portable;
  # increase the floor (CUDA_ARCHITECTURES list) if a target site doesn't need pre-Ampere.
  set_target_properties(${PROJECT_NAME}-cuda PROPERTIES
    CUDA_ARCHITECTURES "70;75;80;86;89"
    # No separable compilation -- the kernel lives in a single TU and we want it
    # whole-program linkable into the consumer test/binary without an explicit nvcc
    # device-link step. Saves the `__cudaRegisterLinkedBinary_*` undefined-reference
    # mess at executable link time.
    CUDA_SEPARABLE_COMPILATION OFF
    CXX_CLANG_TIDY ""  # clang-tidy doesn't understand __global__ / __device__ markers.
    CUDA_CLANG_TIDY ""
  )
  target_compile_options(${PROJECT_NAME}-cuda PRIVATE
    $<$<COMPILE_LANGUAGE:CUDA>:--expt-relaxed-constexpr>)
  # Link beast-vm so the compiler can call into ProgramParser. beast-utils stays in
  # PUBLIC so the static lib pulls in the right transitive symbols when consumers
  # depend on us via beast-pipelines.
  target_link_libraries(${PROJECT_NAME}-cuda
    PUBLIC ${PROJECT_NAME}-utils ${PROJECT_NAME}-vm CUDA::cudart)
  # Make the library available to PROJECT_LIBS consumers so tests / pipelines see it.
  list(APPEND PROJECT_LIBS ${PROJECT_NAME}-cuda)
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

target_link_libraries(${PROJECT_NAME}-evaluators galib maze)
target_link_libraries(${PROJECT_NAME}-pipelines Threads::Threads)
# `beast-pipelines` itself reaches into the CUDA evaluator when configuring a pipe
# with `backend: "gpu"` from JSON (see `EvaluatorPipe::applyBackendSelection`).
# When CUDA support isn't built, the same code paths see `BEAST_HAS_CUDA` undefined
# and silently fall back to the CPU thread-pool evaluator -- pipeline JSON stays
# portable across both build configurations.
if(BEAST_ENABLE_CUDA)
  target_link_libraries(${PROJECT_NAME}-pipelines ${PROJECT_NAME}-cuda)
  target_compile_definitions(${PROJECT_NAME}-pipelines PUBLIC BEAST_HAS_CUDA)
endif()

add_executable(${PROJECT_NAME}-compose
  src/compose.cpp)

if(CMAKE_BUILD_TYPE MATCHES "Debug")
  target_link_libraries(${PROJECT_NAME}-compose PUBLIC gcov)
endif(CMAKE_BUILD_TYPE MATCHES "Debug")

target_link_libraries(${PROJECT_NAME}-compose PUBLIC ${PROJECT_LIBS})

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(
  include
  ${CMAKE_BINARY_DIR}/include
  ${CMAKE_BINARY_DIR}/galib_src
  ${CMAKE_SOURCE_DIR}/3rdparty/Crow/include
  ${CMAKE_SOURCE_DIR}/3rdparty/CLI11/include
  ${CMAKE_SOURCE_DIR}/3rdparty/asio/asio/include
  ${CMAKE_SOURCE_DIR}/3rdparty/json/include
  ${CMAKE_SOURCE_DIR}/3rdparty/maze/include)

link_directories(
  ${CMAKE_BINARY_DIR}/lib)

configure_file(include/beast/version.h.in include/beast/version.h)

# Check if clang-format is available
if (CLANG_FORMAT_EXECUTABLE)
  # Format C++ files target
  file(GLOB_RECURSE ALL_CPP_FILES
    ${CMAKE_SOURCE_DIR}/src/*.cpp
    ${CMAKE_SOURCE_DIR}/src/pipes/*.cpp
    ${CMAKE_SOURCE_DIR}/src/evaluators/*.cpp
    ${CMAKE_SOURCE_DIR}/examples/pipelines/*.cpp
    ${CMAKE_SOURCE_DIR}/examples/programs/*.cpp
    ${CMAKE_SOURCE_DIR}/tests/*.cpp
    ${CMAKE_SOURCE_DIR}/include/*.hpp
    ${CMAKE_SOURCE_DIR}/include/beast/*.hpp
    ${CMAKE_SOURCE_DIR}/include/beast/pipes/*.hpp
    ${CMAKE_SOURCE_DIR}/include/beast/evaluators/*.hpp)

  # Format JavaScript files target
  file(GLOB_RECURSE ALL_JS_FILES
    ${CMAKE_SOURCE_DIR}/html_static/js/*.js)

  # Exclude 3rdparty JS libs
  list(FILTER ALL_JS_FILES EXCLUDE REGEX "${CMAKE_SOURCE_DIR}/html_static/js/konva*")
  list(FILTER ALL_JS_FILES EXCLUDE REGEX "${CMAKE_SOURCE_DIR}/html_static/js/react*")
  list(FILTER ALL_JS_FILES EXCLUDE REGEX "${CMAKE_SOURCE_DIR}/html_static/js/react-dom*")
  list(FILTER ALL_JS_FILES EXCLUDE REGEX "${CMAKE_SOURCE_DIR}/html_static/js/material-ui*")

  set(ALL_SOURCE_FILES ${ALL_CPP_FILES})
  list(APPEND ALL_SOURCE_FILES ${ALL_JS_FILES})

  # Format all files
  add_custom_target(format
    COMMAND ${CLANG_FORMAT_EXECUTABLE} -i -style=file
            -fallback-style=none ${ALL_SOURCE_FILES}
    COMMENT "Running clang-format on all source files"
    VERBATIM)
else()
  message(WARNING "clang-format not found, format targets are not available.")
endif()

# Pipelines
if(BEAST_BUILD_PIPELINES)
  macro(declare_pipeline name)
    add_executable(pipeline_${name} examples/pipelines/${name}.cpp)
    target_link_libraries(pipeline_${name} PUBLIC ${PROJECT_LIBS})

    set_target_properties(pipeline_${name}
      PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/examples/pipelines)

    if(CMAKE_BUILD_TYPE MATCHES "Debug")
      target_link_libraries(pipeline_${name} PUBLIC gcov)
    endif(CMAKE_BUILD_TYPE MATCHES "Debug")
  endmacro()

  declare_pipeline(factory)
  declare_pipeline(forwarder)
  declare_pipeline(maze)
endif()

# Programs
if(BEAST_BUILD_PROGRAMS)
  macro(declare_program name)
    add_executable(program_${name} examples/programs/${name}.cpp)
    target_link_libraries(program_${name} PUBLIC ${PROJECT_LIBS})

    set_target_properties(program_${name}
      PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/examples/programs)

    if(CMAKE_BUILD_TYPE MATCHES "Debug")
      target_link_libraries(program_${name} PUBLIC gcov)
    endif(CMAKE_BUILD_TYPE MATCHES "Debug")
  endmacro()

  declare_program(adder)
  declare_program(bubblesort)
  declare_program(evaluation)
  declare_program(feedloop)
  declare_program(hello_world)
  declare_program(pipe)
endif()

# Testing
if(BEAST_BUILD_TESTS)
  enable_testing()

  add_library(tests_main_beast tests/tests_main.cpp)
  target_include_directories(tests_main_beast PUBLIC 3rdparty/Catch2/single_include)
  target_link_libraries(tests_main_beast PUBLIC ${PROJECT_LIBS})

  macro(declare_test name)
    message(STATUS "Add test: ${name}")
    add_executable(test_${name} tests/${name}.cpp)
    target_include_directories(test_${name} PUBLIC 3rdparty/Catch2/single_include)
    target_link_libraries(test_${name} PUBLIC tests_main_beast)
    if(CMAKE_BUILD_TYPE MATCHES "Debug")
      target_link_libraries(test_${name} PUBLIC gcov)
    endif(CMAKE_BUILD_TYPE MATCHES "Debug")

    set_target_properties(test_${name}
      PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests)
    add_test(NAME test_${name} COMMAND test_${name})
  endmacro()

  declare_test(beast)
  declare_test(bit_manipulation)
  declare_test(cpu_vm)
  declare_test(evaluators)
  declare_test(evolution_pipe)
  declare_test(filesystem_helper)
  declare_test(io)
  declare_test(jumps)
  declare_test(math)
  declare_test(maze_evaluator)
  declare_test(misc)
  declare_test(adder_evaluator)
  declare_test(call_subroutine)
  declare_test(demultiplexer_pipe)
  declare_test(fan_pipe)
  declare_test(filter_pipe)
  declare_test(adder_with_filter_smoke)
  declare_test(curriculum_evaluators)
  declare_test(sha256_round_evaluator)
  if(BEAST_ENABLE_CUDA)
    # Tier 2 parity test: only built when the CUDA library is built. The test itself
    # is safe to run on hosts without a CUDA device -- it skips with a warning.
    declare_test(cuda_sha256_parity)
    # Tier 3 contracts. cuda_opcode_coverage exercises the host-side annotation
    # table only (no device required) but lives in beast-cuda's translation units
    # for now, hence it's gated on BEAST_ENABLE_CUDA. cuda_opcode_parity does need
    # a device and skips with a warning when none is present.
    declare_test(cuda_opcode_coverage)
    declare_test(cuda_opcode_parity)
  endif()
  declare_test(multiplexer_pipe)
  declare_test(null_sink_pipe)
  declare_test(pipe)
  declare_test(pipeline)
  declare_test(program_storage_pipes)
  declare_test(results_summary_pipe)
  declare_test(score_graph_pipe)
  declare_test(pipeline_manager)
  declare_test(pipeline_server)
  declare_test(pipelines)
  declare_test(printing_and_string_table)
  declare_test(program)
  declare_test(program_factory_pipe)
  declare_test(program_c_codegen)
  declare_test(program_disassembler)
  declare_test(program_parser)
  declare_test(programs)
  declare_test(random_program_factory)
  declare_test(stacks)
  declare_test(system_calls)
  declare_test(variables)
  declare_test(virtual_machine)
  declare_test(vm_session)
endif(BEAST_BUILD_TESTS)

# Install targets
set(BEAST_INSTALL_TARGETS
  ${PROJECT_NAME}-utils ${PROJECT_NAME}-vm ${PROJECT_NAME}-evaluators
  ${PROJECT_NAME}-pipelines galib)
if(BEAST_ENABLE_CUDA)
  list(APPEND BEAST_INSTALL_TARGETS ${PROJECT_NAME}-cuda)
endif()
install(
  TARGETS ${BEAST_INSTALL_TARGETS}
  EXPORT ${PROJECT_NAME}-targets
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

install(DIRECTORY include/
  DESTINATION include
  PATTERN "*.in" EXCLUDE)
install(DIRECTORY ${CMAKE_BINARY_DIR}/include/
  DESTINATION include/)

install(EXPORT ${PROJECT_NAME}-targets
  FILE ${PROJECT_NAME}-targets.cmake
  NAMESPACE ${PROJECT_NAME}::
  DESTINATION lib/cmake/${PROJECT_NAME}
)

# CPack
include(CPack)
set(CPACK_GENERATOR "TGZ")
set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
