

set (CMAKE_PREFIX_PATH /opt/local/libexec/qt5)
# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_C_FLAGS "-O3 std=c++11")

find_package(Qt5Widgets)
find_package(Qt5OpenGL)
find_package(Qt5Multimedia)

add_executable(myc64 main.cpp keyboard.cpp )

target_link_libraries(myc64 c64 Qt5::Multimedia Qt5::Core Qt5::OpenGL Qt5::Widgets)

