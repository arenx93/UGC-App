# Portable core of Framecraft for Windows (no Windows APIs): builds and tests on any OS.
cmake_minimum_required(VERSION 3.20)
project(FramecraftCore CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(framecraft_core STATIC
    src/util.cpp src/models.cpp src/presets.cpp src/prompts.cpp src/linter.cpp
    src/skills.cpp src/stories.cpp src/templates.cpp src/library.cpp)
target_include_directories(framecraft_core PUBLIC include third_party)
if(MSVC)
    target_compile_options(framecraft_core PRIVATE /W4 /utf-8 /permissive-)
else()
    target_compile_options(framecraft_core PRIVATE -Wall -Wextra -Wno-missing-field-initializers)
endif()

enable_testing()
add_executable(framecraft_core_tests tests/core_tests.cpp)
target_link_libraries(framecraft_core_tests PRIVATE framecraft_core)
if(MSVC)
    target_compile_options(framecraft_core_tests PRIVATE /utf-8)
endif()
add_test(NAME core COMMAND framecraft_core_tests ${CMAKE_CURRENT_SOURCE_DIR}/../..)
