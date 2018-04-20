cmake_minimum_required(VERSION 3.0)
include_directories(Far)
file(GLOB SOURCES
     "src/*.hpp"
     "src/*.h"
     "src/*.cpp"
     "src/exported_calls.def"
)

add_library(ctags_test SHARED ${SOURCES})
