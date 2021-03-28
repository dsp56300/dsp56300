cmake_minimum_required(VERSION 3.10)
project(dsp56kEmu)

set(SOURCES
agu.cpp agu.h
assert.cpp assert.h
device.cpp device.h
disasm.cpp disasm.h
dsp.cpp dsp.h
error.cpp error.h
essi.cpp essi.h
instructioncache.cpp instructioncache.h
logging.cpp logging.h
memory.cpp memory.h
omfloader.cpp omfloader.h
opcodes.cpp opcodes.h
ops.cpp ops.h
peripherals.cpp peripherals.h
registers.cpp registers.h
staticArray.h 
types.cpp types.h
unittests.cpp unittests.h
utils.cpp utils.h
)

add_library(dsp56kEmu STATIC ${SOURCES})
