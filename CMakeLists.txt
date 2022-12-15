cmake_minimum_required(VERSION 3.8)
project(beast)

set(CMAKE_CXX_CLANG_TIDY "clang-tidy;-checks=*,-modernize-use-trailing-return-type,-cppcoreguidelines-avoid-magic-numbers,-readability-magic-numbers,-fuchsia-default-arguments-calls")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_COMPILER_LAUNCHER "ccache")

if(CMAKE_BUILD_TYPE MATCHES "Debug")
  set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/3rdparty/cmake-modules ${CMAKE_MODULE_PATH})
  include(CodeCoverage)

  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
  add_compile_options(-g -O0 -lprofiler -fprofile-arcs -ftest-coverage --coverage)

  setup_target_for_coverage_lcov(NAME coverage
    EXECUTABLE make test
    EXCLUDE ${CMAKE_SOURCE_DIR}/3rdparty/* ${CMAKE_SOURCE_DIR}/tests/* /usr/include/c++/* /usr/include/x86_64-linux-gnu/c++/* ${CMAKE_SOURCE_DIR}/examples/*)
else()
  add_compile_options(-O3)
endif(CMAKE_BUILD_TYPE MATCHES "Debug")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include_directories(
  include)

add_library(${PROJECT_NAME}
  src/cpu_virtual_machine.cpp
  src/program.cpp
  src/vm_session.cpp
  src/virtual_machine.cpp)

add_executable(feedloop
  examples/feedloop.cpp)

target_link_libraries(feedloop
  ${PROJECT_NAME})

if(CMAKE_BUILD_TYPE MATCHES "Debug")
  target_link_libraries(${PROJECT_NAME}-bin gcov)
endif(CMAKE_BUILD_TYPE MATCHES "Debug")

enable_testing()

macro(declare_test name)
  message(STATUS "Add test: ${name}")
  add_executable(test_${name} tests/${name}.cpp)
  target_include_directories(test_${name} PUBLIC 3rdparty/Catch2/single_include)
  target_link_libraries(test_${name} PUBLIC ${PROJECT_NAME})
  if(CMAKE_BUILD_TYPE MATCHES "Debug")
    target_link_libraries(test_${name} PUBLIC gcov)
  endif(CMAKE_BUILD_TYPE MATCHES "Debug")

  set_target_properties(test_${name}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests)
  add_test(NAME test_${name} COMMAND test_${name})
endmacro()

declare_test(cpu_vm)
declare_test(noop)
declare_test(program)
declare_test(programs)
declare_test(variables)
