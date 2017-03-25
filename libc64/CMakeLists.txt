

add_library(c64 fake6502.c basic.c  chargen.c  fake6502.c  kernal.c c64.c vicii.c cia.c tape.c)

add_library(c64test all_lib.c)

target_include_directories(c64test PUBLIC .)
target_include_directories(c64 PUBLIC .)