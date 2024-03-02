cmake_minimum_required(VERSION 3.10)
project(vtuneSdk)

add_library(vtuneSdk)

set(SOURCES
	include/advisor-annotate.h
	include/ittnotify.h
	include/ittnotify-zca.h
	include/jitprofiling.h
	include/libittnotify.h
	
	src/ittnotify/disable_warnings.h
	src/ittnotify/ittnotify_config.h
	src/ittnotify/ittnotify_static.c
	src/ittnotify/ittnotify_static.h
	src/ittnotify/ittnotify_types.h
	src/ittnotify/jitprofiling.c
)

target_sources(vtuneSdk PRIVATE ${SOURCES})
source_group("source" FILES ${SOURCES})

target_include_directories(vtuneSdk PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(UNIX)
	target_link_libraries(vtuneSdk PUBLIC "dl")
endif()
