cmake_minimum_required(VERSION 3.10)

project(dsp56kDisassemble)

add_executable(dsp56kDisassemble)

target_sources(dsp56kDisassemble PRIVATE disassemble.cpp commandline.cpp commandline.h)

target_link_libraries(dsp56kDisassemble PRIVATE dsp56kEmu)
