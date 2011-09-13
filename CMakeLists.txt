cmake_minimum_required (VERSION 2.6)
#set(CMAKE_VERBOSE_MAKEFILE on)
set(CMAKE_BUILD_TYPE Release)

project (weelink)
add_executable(hello hello.c)

add_definitions(-D_DEBUG)
add_executable(vrelay vrelay.c)
target_link_libraries(vrelay pthread)
