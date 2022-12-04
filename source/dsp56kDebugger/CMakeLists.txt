cmake_minimum_required(VERSION 3.10)
project(dsp56kDebugger)

add_library(dsp56kDebugger)

set(SOURCES
	addressInfo.cpp addressInfo.h
	assembly.cpp assembly.h
	callstack.cpp callstack.h
	debugger.cpp debugger.h
	debuggerJitState.cpp debuggerJitState.h
	debuggerListener.cpp debuggerListener.h
	debuggerState.cpp debuggerState.h
	dialog.cpp dialog.h
	dspExec.cpp dspExec.h
	gotoAddress.cpp gotoAddress.h
	grid.cpp grid.h
	jitconfigeditor.cpp jitconfigeditor.h
	mainWindow.cpp mainWindow.h
	memory.cpp memory.h
	registers.cpp registers.h
	statusBar.cpp statusBar.h
	styledTextCtrl.cpp styledTextCtrl.h
	types.h
)

target_sources(dsp56kDebugger PRIVATE ${SOURCES})
source_group("source" FILES ${SOURCES})

target_link_libraries(dsp56kDebugger PUBLIC dsp56kEmu wx::core wx::base wx::aui wx::stc wx::propgrid)
