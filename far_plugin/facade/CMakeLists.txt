# TODO: implement far facade library
add_library(facade INTERFACE)
target_include_directories(facade INTERFACE include)

if (WIN32)
  add_subdirectory(far3)
else (WIN32)
  add_subdirectory(far2l)
endif (WIN32)
