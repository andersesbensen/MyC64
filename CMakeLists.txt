
cmake_minimum_required(VERSION 3.2)

project(MyC64 C CXX ASM )

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O3")
add_subdirectory(libc64)
#add_subdirectory(residfp)


if( CMAKE_SYSTEM_NAME STREQUAL "None")
add_subdirectory(arm)
else()
add_subdirectory(qt)
endif()


