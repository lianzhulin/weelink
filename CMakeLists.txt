cmake_minimum_required (VERSION 2.6)
#set(CMAKE_VERBOSE_MAKEFILE on)
set(CMAKE_BUILD_TYPE Release)

project (weelink)
add_executable(hello hello.c)

if(NOT MSVC)
add_definitions(-D_DEBUG)
add_executable(vrelay vrelay.c)
target_link_libraries(vrelay pthread)
endif(NOT MSVC)

add_executable(sha1sum sha1sum.c)
set_target_properties(sha1sum PROPERTIES COMPILE_FLAGS "-D_TEST")

add_executable(p2pget p2pget.c dump.c)
