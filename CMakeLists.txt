cmake_minimum_required(VERSION 3.8)
project(beast
  LANGUAGES CXX
  VERSION 0.0.9
  DESCRIPTION "Byte code generator, parser, and virtual machine for evaluating evolutionary code sequences."
  HOMEPAGE_URL "https://github.com/dedicate-project/beast/")

option(BEAST_BUILD_TESTS "Whether to build tests or not" YES)
option(BEAST_BUILD_DOCS "Whether to build documentation or not" YES)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

set(CMAKE_CXX_CLANG_TIDY "clang-tidy;-warnings-as-errors=*;-checks=*,-modernize-use-trailing-return-type,-cppcoreguidelines-avoid-magic-numbers,-readability-magic-numbers,-fuchsia-default-arguments-calls,-llvmlibc-callee-namespace,-llvmlibc-implementation-in-namespace,-llvmlibc-restrict-system-libc-headers,-altera-id-dependent-backward-branch,-readability-function-cognitive-complexity,-altera-unroll-loops,-bugprone-easily-swappable-parameters,-misc-no-recursion")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_COMPILER_LAUNCHER "ccache")

if(NOT CMAKE_GENERATOR MATCHES "Visual Studio")
  add_compile_options(-Werror -pedantic)
endif()

if(BEAST_BUILD_DOCS)
  add_subdirectory("docs")
endif()

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
  include
  ${CMAKE_BINARY_DIR}/include)

configure_file(include/beast/version.h.in include/beast/version.h)

# Main BEAST library
add_library(${PROJECT_NAME}
  src/beast.cpp
  src/cpu_virtual_machine.cpp
  src/program.cpp
  src/time_functions.cpp
  src/vm_session.cpp
  src/virtual_machine.cpp)

# Examples
add_executable(feedloop
  examples/feedloop.cpp)

target_link_libraries(feedloop
  ${PROJECT_NAME})

if(CMAKE_BUILD_TYPE MATCHES "Debug")
  target_link_libraries(feedloop gcov)
endif(CMAKE_BUILD_TYPE MATCHES "Debug")

# Testing
if(BEAST_BUILD_TESTS)
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

  declare_test(beast)
  declare_test(bit_manipulation)
  declare_test(cpu_vm)
  declare_test(io)
  declare_test(jumps)
  declare_test(math)
  declare_test(misc)
  declare_test(printing_and_string_table)
  declare_test(program)
  declare_test(stacks)
  declare_test(system_calls)
  declare_test(variables)
endif(BEAST_BUILD_TESTS)

# Install targets
install(TARGETS ${PROJECT_NAME}
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
