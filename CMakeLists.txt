cmake_minimum_required(VERSION 3.10)
project(CarOS)

set(CMAKE_CXX_STANDARD 17)

# GTK4 finden
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED gtk4)

# Header und Quellen
include_directories(include ${GTK4_INCLUDE_DIRS})
link_directories(${GTK4_LIBRARY_DIRS})

add_executable(CarOS 
    src/main.cpp 
    src/ui_manager.cpp 
    src/gpio_handler.cpp
)

target_link_libraries(CarOS ${GTK4_LIBRARIES} pthread gpiod)