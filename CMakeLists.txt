cmake_minimum_required(VERSION 3.16)
project(EncrypticAntiCheat VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENCRYPTIC_BUILD_SHARED "Build encryptic_core as shared library" ON)
option(ENCRYPTIC_BUILD_EXAMPLES "Build examples" ON)

add_subdirectory(core)

if(ENCRYPTIC_BUILD_EXAMPLES)
    add_subdirectory(examples/native)
    add_subdirectory(examples/watermark_preview)
    add_subdirectory(examples/test_game)
endif()
