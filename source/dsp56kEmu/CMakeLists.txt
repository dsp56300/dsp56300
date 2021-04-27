cmake_minimum_required(VERSION 3.10)
project(dsp56kEmu)

add_library(dsp56kEmu STATIC)

set(SOURCES
aar.h
agu.cpp agu.h
audio.cpp audio.h
bitfield.h
buildconfig.h
assert.cpp assert.h
disasm.cpp disasm.h
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

target_sources(dsp56kEmu PRIVATE ${SOURCES})
source_group("source" FILES ${SOURCES})

if(WIN32)
	target_link_libraries(dsp56kEmu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../dsp56k/CM56300.lib)
endif()
