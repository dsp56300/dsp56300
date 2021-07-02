cmake_minimum_required(VERSION 3.10)
project(dsp56kEmu)

add_library(dsp56kEmu STATIC)

set(SOURCES
aar.h
agu.cpp agu.h
audio.cpp audio.h
bitfield.h
buildconfig.h
disasm.cpp disasm.h
dspassert.cpp dspassert.h
dspconfig.h
dsp.cpp dsp.h 
dsp_decode.inl
dsp_jumptable.inl
dsp_ops.inl dsp_ops_helper.inl 
dsp_ops_alu.inl dsp_ops_bra.inl dsp_ops_jmp.inl dsp_ops_move.inl
dspthread.cpp dspthread.h
error.cpp error.h
essi.cpp essi.h
esai.cpp esai.h
fastmath.h
hdi08.cpp hdi08.h
hi08.h
instructioncache.cpp instructioncache.h
interrupts.h
logging.cpp logging.h
memory.cpp memory.h
omfloader.cpp omfloader.h
opcodes.cpp opcodes.h
opcodefields.h
opcodeinfo.cpp opcodeinfo.h
opcodetypes.h
ops.cpp ops.h
peripherals.cpp peripherals.h
registers.cpp registers.h
ringbuffer.h
semaphore.h
staticArray.h
timers.cpp timers.h
types.cpp types.h
unittests.cpp unittests.h
utils.cpp utils.h
)

set(SOURCES_JIT
jit.cpp jit.h
jitblock.cpp jitblock.h
jitcacheentry.h
jithelper.cpp jithelper.h
jitdspregs.cpp jitdspregs.h
jitdspregpool.cpp jitdspregpool.h
jitmem.cpp jitmem.h
jitops.cpp jitops.h
jitops_alu.inl jitops_ccr.inl jitops_decode.inl jitops_helper.inl jitops_jmp.inl jitops_mem.inl jitops_move.inl
jitregtracker.cpp jitregtracker.h
jitregtypes.h
jitruntimedata.cpp jitruntimedata.h
jitstackhelper.cpp jitstackhelper.h
jitunittests.cpp jitunittests.h
)

set(SOURCES_VTUNE
../vtuneSdk/include/ittnotify.h
../vtuneSdk/include/jitprofiling.h
../vtuneSdk/include/libittnotify.h
../vtuneSdk/include/llvm_jit_event_listener.hpp
)

target_sources(dsp56kEmu PRIVATE ${SOURCES} ${SOURCES_JIT} ${SOURCES_VTUNE})
source_group("source" FILES ${SOURCES})
source_group("source\\jit" FILES ${SOURCES_JIT})
source_group("source\\vtune" FILES ${SOURCES_VTUNE})

if(WIN32 AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../dsp56k/CM56300.lib AND NOT ${CMAKE_VS_PLATFORM_NAME} STREQUAL "x64")
	target_link_libraries(dsp56kEmu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../dsp56k/CM56300.lib)
	target_compile_definitions(dsp56kEmu PRIVATE DSP56K_USE_MOTOROLA_UNASM)
endif()

if(WIN32)
	if(${CMAKE_VS_PLATFORM_NAME} STREQUAL "x64")
		target_link_libraries(dsp56kEmu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../vtuneSdk/lib64/jitprofiling.lib)
	endif()
endif()

if(UNIX AND NOT APPLE)
	set(THREADS_PREFER_PTHREAD_FLAG ON)
	find_package(Threads REQUIRED)
	target_link_libraries(dsp56kEmu PUBLIC Threads::Threads)
endif()

target_link_libraries(dsp56kEmu PUBLIC asmjit)
