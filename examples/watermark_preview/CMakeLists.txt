cmake_minimum_required(VERSION 3.16)

add_executable(watermark_preview main.cpp)
target_link_libraries(watermark_preview PRIVATE encryptic_core_static)
