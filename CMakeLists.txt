cmake_minimum_required(VERSION 2.8)
project(libchars)

message(STATUS "CMAKE_GENERATOR : ${CMAKE_GENERATOR}")
message(STATUS "SYSTEM_NAME     : ${CMAKE_SYSTEM_NAME}")

#set(CMAKE_BUILD_TYPE Release)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS "-Wall")

# libchars library

set(LIBCHARS_SOURCE commands.cpp debug.cpp editor.cpp history.cpp parameter.cpp terminal.cpp validation.cpp)

add_library(chars SHARED ${LIBCHARS_SOURCE})


# libchars tests and samples

set(PROGRAMS test_editor test_commands)

foreach(program ${PROGRAMS})
  add_executable(${program} ${program}.cpp)
  target_link_libraries(${program} chars)
endforeach(program)


#install(TARGETS test_commands DESTINATION bin)
