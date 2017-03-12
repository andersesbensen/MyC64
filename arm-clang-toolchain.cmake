set(CMAKE_SYSTEM_NAME None)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(triple arm-none-eabi)

set(CMAKE_C_COMPILER clang)
set(CMAKE_C_COMPILER_TARGET ${triple})
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_CXX_COMPILER_TARGET ${triple})


set(CMAKE_C_FLAGS "-O3 -march=armv7-m -g")

set(CMAKE_EXE_LINKER_FLAGS "-nostdlib ")
set(CMAKE_SYSROOT /opt/local/arm-none-eabi)
include_directories(SYSTEM ${CMAKE_SYSROOT}/include )