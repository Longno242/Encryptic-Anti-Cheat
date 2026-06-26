cmake_minimum_required(VERSION 3.16)

add_executable(encryptic_test_game WIN32 main.cpp kick_dialog.cpp)
target_link_libraries(encryptic_test_game PRIVATE encryptic_core_static user32 gdi32 gdiplus)

add_library(evil_test_dll SHARED evil_dll/evil.cpp)
set_target_properties(evil_test_dll PROPERTIES OUTPUT_NAME evil PREFIX "")

add_dependencies(encryptic_test_game evil_test_dll)

add_custom_command(
    TARGET encryptic_test_game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:evil_test_dll>"
        "$<TARGET_FILE_DIR:encryptic_test_game>/evil.dll"
    COMMENT "Copying evil.dll next to test game"
)
