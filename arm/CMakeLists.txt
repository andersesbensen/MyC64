
add_subdirectory(TivaWare)
add_executable(c64arm arm_main.c startup_gcc.c )

target_link_libraries(c64arm tivadrv c64  )
set_target_properties(c64arm PROPERTIES LINK_FLAGS "-T${CMAKE_CURRENT_SOURCE_DIR}/project.ld")