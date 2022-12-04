cmake_minimum_required(VERSION 3.10)

project(dsp56kHxD)

add_library(dsp56kHxD SHARED)

set(SOURCES_API
	HxDapi/Interface/C++/DataInspectorPluginServer.cpp
	HxDapi/Interface/C++/DataInspectorPluginServer.h

	HxDapi/Interface/C/DataInspectorPluginInterface.h
	HxDapi/Interface/C/DataInspectorShared.h
	
	HxDapi/Examples/C++/DataInspectorPluginExample/DataInspectorPluginExample.def
)

set(SOURCES
	dllmain.cpp
	HxDdisassemble.cpp
	HxDdisassemble.h
)

target_sources(dsp56kHxD PRIVATE ${SOURCES_API} ${SOURCES})

source_group("source" FILES ${SOURCES})
source_group("source\\API" FILES ${SOURCES_API})

target_link_libraries(dsp56kHxD PUBLIC dsp56kEmu)

target_include_directories(dsp56kHxD PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/HxDapi/Interface/C/)
target_include_directories(dsp56kHxD PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/HxDapi/Interface/C++/)

target_compile_definitions(dsp56kHxD PRIVATE NOMINMAX)
target_compile_definitions(dsp56kHxD PRIVATE WIN32_LEAN_AND_MEAN)
