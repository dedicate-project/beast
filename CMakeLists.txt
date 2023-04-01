cmake_minimum_required(VERSION 3.8)
project(beast
  LANGUAGES CXX
  VERSION 0.1.2
  DESCRIPTION "Byte code generator, parser, and virtual machine for evaluating evolutionary code sequences."
  HOMEPAGE_URL "https://github.com/dedicate-project/beast/")

option(BEAST_BUILD_TESTS "Whether to build tests or not" YES)
option(BEAST_BUILD_PIPELINES "Whether to build pipelines or not" YES)
option(BEAST_BUILD_EXAMPLES "Whether to build examples or not" YES)
option(BEAST_BUILD_DOCS "Whether to build documentation or not" YES)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

# Add utilities used for evaluations. This needs to be placed before the clang-tidy configuration to
# not impose clang rules into 3rdparty components.
add_subdirectory(${CMAKE_SOURCE_DIR}/3rdparty/maze)

set(CMAKE_CXX_CLANG_TIDY "clang-tidy;-warnings-as-errors=*;-checks=*,-modernize-use-trailing-return-type,-cppcoreguidelines-avoid-magic-numbers,-readability-magic-numbers,-fuchsia-default-arguments-calls,-llvmlibc-callee-namespace,-llvmlibc-implementation-in-namespace,-llvmlibc-restrict-system-libc-headers,-altera-id-dependent-backward-branch,-readability-function-cognitive-complexity,-altera-unroll-loops,-bugprone-easily-swappable-parameters,-misc-no-recursion,-altera-struct-pack-align")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_COMPILER_LAUNCHER "ccache")

if(NOT CMAKE_GENERATOR MATCHES "Visual Studio")
  add_compile_options(-Werror -pedantic)
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
    EXCLUDE ${CMAKE_SOURCE_DIR}/3rdparty/* ${CMAKE_SOURCE_DIR}/tests/* /usr/include/c++/* /usr/include/x86_64-linux-gnu/c++/* ${CMAKE_SOURCE_DIR}/examples/* ${CMAKE_SOURCE_DIR}/pipelines/* asio/* ${CMAKE_SOURCE_DIR}/src/compose.cpp ${CMAKE_BINARY_DIR}/* ${CMAKE_BINARY_DIR}/galib_src/*
    SONARQUBE)
else()
  if(NOT CMAKE_GENERATOR MATCHES "Visual Studio")
    add_compile_options(-O3)
  endif()
endif(CMAKE_BUILD_TYPE MATCHES "Debug")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Main BEAST library
add_library(${PROJECT_NAME}
  src/beast.cpp
  src/cpu_virtual_machine.cpp
  src/filesystem_helper.cpp
  src/pipe.cpp
  src/pipeline.cpp
  src/pipeline_manager.cpp
  src/program.cpp
  src/random_program_factory.cpp
  src/time_functions.cpp
  src/vm_session.cpp
  src/virtual_machine.cpp
  src/evaluators/aggregation_evaluator.cpp
  src/evaluators/maze_evaluator.cpp
  src/evaluators/random_serial_data_passthrough_evaluator.cpp
  src/evaluators/runtime_statistics_evaluator.cpp
  src/evaluators/operator_usage_evaluator.cpp
  src/pipes/evaluator_pipe.cpp)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} galib Threads::Threads maze)

add_executable(${PROJECT_NAME}-compose
  src/compose.cpp)

if(CMAKE_BUILD_TYPE MATCHES "Debug")
  target_link_libraries(${PROJECT_NAME}-compose PUBLIC gcov)
endif(CMAKE_BUILD_TYPE MATCHES "Debug")

target_link_libraries(${PROJECT_NAME}-compose PUBLIC ${PROJECT_NAME})

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

# Pipelines
if(BEAST_BUILD_PIPELINES)
  macro(declare_pipeline name)
    add_executable(pipeline_${name} pipelines/${name}.cpp)
    target_link_libraries(pipeline_${name} PUBLIC ${PROJECT_NAME})

    set_target_properties(pipeline_${name}
      PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/pipelines)

    if(CMAKE_BUILD_TYPE MATCHES "Debug")
      target_link_libraries(pipeline_${name} PUBLIC gcov)
    endif(CMAKE_BUILD_TYPE MATCHES "Debug")
  endmacro()

  declare_pipeline(sorter)
endif()

# Examples
if(BEAST_BUILD_EXAMPLES)
  macro(declare_example name)
    add_executable(example_${name} examples/${name}.cpp)
    target_link_libraries(example_${name} PUBLIC ${PROJECT_NAME})

    set_target_properties(example_${name}
      PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/examples)

    if(CMAKE_BUILD_TYPE MATCHES "Debug")
      target_link_libraries(example_${name} PUBLIC gcov)
    endif(CMAKE_BUILD_TYPE MATCHES "Debug")
  endmacro()

  declare_example(adder)
  declare_example(bubblesort)
  declare_example(evaluation)
  declare_example(feedloop)
  declare_example(hello_world)
  declare_example(pipe)
endif()

# Testing
if(BEAST_BUILD_TESTS)
  enable_testing()

  add_library(tests_main tests/tests_main.cpp)
  target_include_directories(tests_main PUBLIC 3rdparty/Catch2/single_include)
  target_link_libraries(tests_main PUBLIC ${PROJECT_NAME})

  macro(declare_test name)
    message(STATUS "Add test: ${name}")
    add_executable(test_${name} tests/${name}.cpp)
    target_include_directories(test_${name} PUBLIC 3rdparty/Catch2/single_include)
    target_link_libraries(test_${name} PUBLIC tests_main)
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
  declare_test(filesystem_helper)
  declare_test(io)
  declare_test(jumps)
  declare_test(math)
  declare_test(misc)
  declare_test(pipe)
  declare_test(pipeline)
  declare_test(pipeline_manager)
  declare_test(pipelines)
  declare_test(printing_and_string_table)
  declare_test(program)
  declare_test(programs)
  declare_test(random_program_factory)
  declare_test(stacks)
  declare_test(system_calls)
  declare_test(variables)
  declare_test(virtual_machine)
  declare_test(vm_session)
endif(BEAST_BUILD_TESTS)

# Install targets
install(TARGETS ${PROJECT_NAME} galib
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
