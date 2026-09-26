cmake_minimum_required(VERSION 3.10)
project(dsp56kBase)

add_library(dsp56kBase)

set(SOURCES
audioworkgroup.cpp audioworkgroup.h
bitfield.h
buildconfig.h
conditionvariable.cpp conditionvariable.h
dspassert.cpp dspassert.h
fastmath.h
logging.cpp logging.h
mmuarray.h
mmuhelper.cpp mmuhelper.h
mutex.cpp mutex.h
profiler.cpp profiler.h
ringbuffer.h
semaphore.h
sharedaudiobuffer.h
sharedaudioreducer.h
threadtools.cpp threadtools.h
trigger.cpp trigger.h
win8sync.cpp win8sync.h
)

target_sources(dsp56kBase PRIVATE ${SOURCES})
source_group("source" FILES ${SOURCES})

# Profiler integrations, see profiler.cpp. vtuneSdk is only added for these platforms, Superluminal is Windows only.
if(WIN32 OR (UNIX AND NOT APPLE))
	target_link_libraries(dsp56kBase PRIVATE vtuneSdk)
	target_compile_definitions(dsp56kBase PRIVATE DSP56K_USE_VTUNE_JIT_PROFILING_API)
endif()

if(WIN32)
	target_compile_definitions(dsp56kBase PRIVATE DSP56K_USE_SUPERLUMINAL_PROFILING_API)
endif()

if(MSVC)
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /constexpr:steps10000000")
elseif(UNIX AND NOT APPLE)
	if(NOT ANDROID)
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fconstexpr-loop-limit=10000000")
	endif()
endif()

if(UNIX AND NOT APPLE)
	set(THREADS_PREFER_PTHREAD_FLAG ON)
	find_package(Threads REQUIRED)
	target_link_libraries(dsp56kBase PUBLIC Threads::Threads rt)
endif()

target_include_directories(dsp56kBase PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

add_executable(sharedAudioBufferTest sharedaudiobuffer_test.cpp)
target_link_libraries(sharedAudioBufferTest PRIVATE dsp56kBase)
add_test(NAME sharedAudioBufferTest COMMAND sharedAudioBufferTest)

add_executable(sharedAudioReducerTest sharedaudioreducer_test.cpp)
target_link_libraries(sharedAudioReducerTest PRIVATE dsp56kBase)
add_test(NAME sharedAudioReducerTest COMMAND sharedAudioReducerTest)

add_executable(ringBufferTest ringbuffer_test.cpp)
target_link_libraries(ringBufferTest PRIVATE dsp56kBase)
add_test(NAME ringBufferTest COMMAND ringBufferTest)

add_executable(semaphoreTest semaphore_test.cpp)
target_link_libraries(semaphoreTest PRIVATE dsp56kBase)
add_test(NAME semaphoreTest COMMAND semaphoreTest)

add_executable(profilerTest profiler_test.cpp)
target_link_libraries(profilerTest PRIVATE dsp56kBase)
add_test(NAME profilerTest COMMAND profilerTest)
