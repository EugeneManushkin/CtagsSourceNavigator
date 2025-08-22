add_executable(safe_call_tests safe_call/main.cpp)
target_link_libraries(safe_call_tests facade gtest)

if (WIN32)
  add_subdirectory(far3)
endif (WIN32)
