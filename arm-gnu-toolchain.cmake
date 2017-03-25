set(CMAKE_SYSTEM_NAME None)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(triple arm-none-eabi)
#set(BINUTILS "/Users/aes/git/MyC64/install/arm-none-eabi/bin/" )
#set(BINUTILS "/Users/aes/git/MyC64/install-2/" )

set(PREFIX "arm-none-eabi-")
set(CMAKE_C_COMPILER ${PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${PREFIX}g++)
set(CMAKE_ASM_COMPILER ${PREFIX}gcc)


#-march=armv7-m
set(CMAKE_C_FLAGS "-O3  -g  -flto -ffunction-sections -mcpu=cortex-m4 -mthumb ")
set(CMAKE_ASM_FLAGS "-O3  -g  -ffunction-sections   -mcpu=cortex-m4 -mthumb")

#set(CMAKE_EXE_LINKER_FLAGS "-nostdlib  -B ${BINUTILS}")

set(CMAKE_EXE_LINKER_FLAGS "-nostdlib ")
set(CMAKE_SYSROOT /opt/local/arm-none-eabi)


include_directories(SYSTEM ${CMAKE_SYSROOT}/include )