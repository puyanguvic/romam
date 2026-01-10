cmake_minimum_required(VERSION 3.16)

project(romam_standalone LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ROMAM_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

add_library(romam_warnings INTERFACE)
target_compile_options(romam_warnings INTERFACE
  -Wall -Wextra -Wpedantic
)
if(ROMAM_WARNINGS_AS_ERRORS)
  target_compile_options(romam_warnings INTERFACE -Werror)
endif()

add_subdirectory(protocol/core core)
add_subdirectory(protocol/daemon daemon)
add_subdirectory(traffic traffic)
