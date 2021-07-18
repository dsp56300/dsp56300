cmake_minimum_required(VERSION 3.15)

project(dsp56kemu VERSION 0.0.1)

include(FetchContent)
FetchContent_Declare(
        Catch2
        URL https://github.com/catchorg/Catch2/archive/refs/tags/v2.13.6.tar.gz
        URL_HASH SHA256=48dfbb77b9193653e4e72df9633d2e0383b9b625a47060759668480fdf24fbd4
)

include("source/base.cmake")

add_subdirectory(source)

enable_testing()
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()