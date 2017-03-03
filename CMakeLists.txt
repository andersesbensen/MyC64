
project(MyC64)

set (CMAKE_PREFIX_PATH /opt/local/libexec/qt5)
# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)


find_package(Qt5Widgets)
find_package(Qt5OpenGL)

add_executable(myc64 fake6502.c basic.c  chargen.c  fake6502.c  kernal.c c64.c vicii.c cia.c main.cpp)

target_link_libraries(myc64 Qt5::Core Qt5::OpenGL Qt5::Widgets)
