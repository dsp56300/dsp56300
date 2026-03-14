cmake_minimum_required(VERSION 3.10)
project(dsp56kBase)

add_library(dsp56kBase)

set(SOURCES
bitfield.h
buildconfig.h
conditionvariable.cpp conditionvariable.h
dspassert.cpp dspassert.h
fastmath.h
logging.cpp logging.h
ringbuffer.h
semaphore.h
threadtools.cpp threadtools.h
trigger.cpp trigger.h
win8sync.cpp win8sync.h
)

target_sources(dsp56kBase PRIVATE ${SOURCES})
source_group("source" FILES ${SOURCES})

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
	target_link_libraries(dsp56kBase PUBLIC Threads::Threads)
endif()

target_include_directories(dsp56kBase PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
