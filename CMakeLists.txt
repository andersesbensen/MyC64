
cmake_minimum_required(VERSION 3.7)

project(MyC64)

add_subdirectory(libc64)


if( CMAKE_SYSTEM_NAME STREQUAL "None")
add_subdirectory(arm)
else()
add_subdirectory(qt)
endif()


