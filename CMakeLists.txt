cmake_minimum_required(VERSION 3.0)
include_directories(Far)
file(GLOB SOURCES
     "src/*.hpp"
     "src/*.h"
     "src/*.cpp"
     "src/resource.rc"
     "src/exported_calls.def"
)

add_library(ctags SHARED ${SOURCES})
set(PLUGIN_DIR ${CMAKE_BINARY_DIR}/$<CONFIG>/ctags)
add_custom_command(TARGET ctags POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PLUGIN_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/src/tagseng.lng ${PLUGIN_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:ctags> ${PLUGIN_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/COPYING ${PLUGIN_DIR}
)
