cmake_minimum_required(VERSION 3.10)
project(dsp56kEmu)

add_library(dsp56kEmu)

set(SOURCES
aar.h
agu.cpp agu.h
audio.cpp audio.h
bitfield.h
buildconfig.h
debuggerinterface.cpp debuggerinterface.h
disasm.cpp disasm.h
dma.cpp dma.h
dspassert.cpp dspassert.h
dspconfig.h
dsplistener.h
dsp.cpp dsp.h 
dsp_decode.inl
dsp_jumptable.inl
dsp_ops.inl dsp_ops_helper.inl 
dsp_ops_alu.inl dsp_ops_bra.inl dsp_ops_jmp.inl dsp_ops_move.inl
dspthread.cpp dspthread.h
error.cpp error.h
esai.cpp esai.h
esaiclock.cpp esaiclock.h
essi.cpp essi.h
fastmath.h
gpio.cpp gpio.h
hdi08.cpp hdi08.h
hdi08queue.cpp hdi08queue.h
hi08.h
instructioncache.cpp instructioncache.h
interpreterunittests.cpp interpreterunittests.h
interrupts.h
logging.cpp logging.h
memory.cpp memory.h
memorybuffer.cpp memorybuffer.h
omfloader.cpp omfloader.h
opcodes.cpp opcodes.h
opcodeanalysis.h
opcodefields.h
opcodeinfo.h
opcodetypes.h
peripherals.cpp peripherals.h
registers.cpp registers.h
ringbuffer.h
semaphore.h
staticArray.h
threadtools.cpp threadtools.h
timers.cpp timers.h
types.cpp types.h
unittests.cpp unittests.h
utils.cpp utils.h
)

set(SOURCES_JIT
jit.cpp jit.h
jitasmjithelpers.cpp jitasmjithelpers.h
jitconfig.h
jitdspmode.cpp jitdspmode.h
jitemitter.cpp jitemitter.h
jitblock.cpp jitblock.h
jitblockchain.cpp jitblockchain.h
jitblockemitter.h
jitblockinfo.cpp jitblockinfo.h
jitblockruntimedata.cpp jitblockruntimedata.h
jitcacheentry.h
jithelper.cpp jithelper.h
jitdspregs.cpp jitdspregs.h
jitdspregpool.cpp jitdspregpool.h
jitdspregpoolregpair.cpp jitdspregpoolregpair.h
jitdspregpooltypes.h
jitdspvalue.cpp jitdspvalue.h
jitmem.cpp jitmem.h
jitops.cpp jitops.h jitops_agu.cpp jitops_alu.cpp jitops_ccr.cpp jitops_decode.cpp jitops_helper.cpp jitops_jmp.cpp jitops_mem.cpp jitops_move.cpp
jitops_alu.inl jitops_helper.inl jitops_jmp.inl jitops_mem.inl jitops_move.inl
jitoptimizer.cpp jitoptimizer.h
jitprofilingsupport.cpp jitprofilingsupport.h
jitregtracker.cpp jitregtracker.h
jitregtypes.h
jitruntimedata.cpp jitruntimedata.h
jitstackhelper.cpp jitstackhelper.h
jittypes.h
jitunittests.cpp jitunittests.h

jitops_agu_aarch64.cpp
jitops_alu_aarch64.cpp
jitops_ccr_aarch64.cpp
jitops_decode_aarch64.cpp
jitops_helper_aarch64.cpp
jitops_jmp_aarch64.inl

jitops_agu_x64.cpp
jitops_alu_x64.cpp
jitops_ccr_x64.cpp
jitops_decode_x64.cpp
jitops_helper_x64.cpp
jitops_jmp_x64.inl
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

if(WIN32)
	# If CMAKE_SIZEOF_VOID_P is equal to 8, it's 64 bit
	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		find_library(JIT_PROFILING
			NAMES
				jitprofiling
				jitprofiling.lib
			PATHS
				${CMAKE_CURRENT_SOURCE_DIR}/../vtuneSdk/lib64/
			REQUIRED
		)
		find_library(VTUNE_INSTRUMENTATION
			NAMES
				libittnotify
				libittnotify.lib
			PATHS
				${CMAKE_CURRENT_SOURCE_DIR}/../vtuneSdk/lib64/
			REQUIRED
		)
		target_link_libraries(dsp56kEmu PUBLIC ${JIT_PROFILING} ${VTUNE_INSTRUMENTATION})
		target_compile_definitions(dsp56kEmu PRIVATE DSP56K_USE_VTUNE_JIT_PROFILING_API)
	else()
		find_library(CM56300
				NAMES
					CM56300
					CM56300.lib
				PATHS
					${CMAKE_CURRENT_SOURCE_DIR}/../dsp56k
				)
		if(CM56300)
			target_link_libraries(dsp56kEmu PUBLIC ${CM56300})
			target_compile_definitions(dsp56kEmu PRIVATE DSP56K_USE_MOTOROLA_UNASM)
		endif()
	endif()
endif()

if(MSVC)
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /constexpr:steps10000000")
endif()

if(UNIX AND NOT APPLE)
	set(THREADS_PREFER_PTHREAD_FLAG ON)
	find_package(Threads REQUIRED)
	target_link_libraries(dsp56kEmu PUBLIC Threads::Threads)
endif()

target_link_libraries(dsp56kEmu PUBLIC asmjit)

if(DSP56300_DEBUGGER)
	target_compile_definitions(dsp56kEmu PUBLIC DSP56300_DEBUGGER=1)
else()
	target_compile_definitions(dsp56kEmu PUBLIC DSP56300_DEBUGGER=0)
endif()

# This should use ${PROJECT_SOURCE_DIR}/source if the top level of the repo
# has the top level cmakelists.txt file.
target_include_directories(dsp56kEmu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
