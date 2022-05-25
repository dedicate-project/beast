cmake_minimum_required(VERSION 3.8)
project(beast)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include_directories(
  include)

add_library(${PROJECT_NAME}
  src/cpu_virtual_machine.cpp
  src/program.cpp
  src/vm_session.cpp)

add_executable(${PROJECT_NAME}-bin
  src/main.cpp)

target_link_libraries(${PROJECT_NAME}-bin
  ${PROJECT_NAME})
