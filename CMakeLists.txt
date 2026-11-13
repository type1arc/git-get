cmake_minimum_required(VERSION 3.16)
project(gg LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(CURL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(NCURSES REQUIRED IMPORTED_TARGET ncursesw)

add_executable(gg
    src/main.cpp
    src/github.cpp
    src/ui.cpp
)
target_include_directories(gg PRIVATE third_party)
target_link_libraries(gg PRIVATE CURL::libcurl PkgConfig::NCURSES)
