cmake_minimum_required(VERSION 3.10)

project(dsp56kDisassemble)

add_executable(dsp56kDisassemble)

target_sources(dsp56kDisassemble PRIVATE disassemble.cpp commandline.cpp commandline.h)

target_link_libraries(dsp56kDisassemble PRIVATE dsp56kEmu)

if(BUILD_TESTING)
	add_test(
		NAME dsp56kDisassembleZeroWords
		COMMAND ${CMAKE_COMMAND}
			-DDISASSEMBLER=$<TARGET_FILE:dsp56kDisassemble>
			-DTEST_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/test
			-P ${CMAKE_CURRENT_SOURCE_DIR}/disassemble_test.cmake
	)
endif()
