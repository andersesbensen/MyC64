set(CMAKE_SYSTEM_NAME None)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(triple arm-none-eabi)
#set(BINUTILS "/Users/aes/git/MyC64/install/arm-none-eabi/bin/" )
#set(BINUTILS "/Users/aes/git/MyC64/install-2/" )
set(CMAKE_C_COMPILER clang)
set(CMAKE_C_COMPILER_TARGET ${triple})
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_CXX_COMPILER_TARGET ${triple})

#-march=armv7-m
set(CMAKE_C_FLAGS "-O3  -g -target ${triple} -march=armv7-m")
set(CMAKE_ASM_FLAGS "-O3  -g -target ${triple} -march=armv7-m")

#set(CMAKE_EXE_LINKER_FLAGS "-nostdlib  -B ${BINUTILS}")

set(CMAKE_EXE_LINKER_FLAGS "-nostdlib ")
set(CMAKE_SYSROOT /opt/local/arm-none-eabi)


include_directories(SYSTEM ${CMAKE_SYSROOT}/include )