#include "mainWindow.h"

#include "addressInfo.h"
#include "debugger.h"

#include "assembly.h"
#include "memory.h"
#include "types.h"
#include "registers.h"
#include "addressInfo.h"
#include "callstack.h"
#include "gotoAddress.h"
#include "jitconfigeditor.h"
#include "statusBar.h"

namespace dsp56kDebugger
{
	MainWindow::MainWindow(Debugger& _debugger) : wxFrame(nullptr, wxID_ANY, "DSP 56300 Debugger", wxDefaultPosition, wxSize(1400,1000)), m_debugger(_debugger)
	{
		m_aui.SetManagedWindow(this);
		m_aui.SetFlags(m_aui.GetFlags() | wxAUI_MGR_LIVE_RESIZE);

		auto* text = new Assembly(_debugger, this);

		auto* pg = new AddressInfo(_debugger, this);

		auto* regs = new Registers(_debugger, this);

		auto* memX = new Memory(_debugger, this, dsp56k::MemArea_X);
		auto* memY = new Memory(_debugger, this, dsp56k::MemArea_Y);
		auto* memP = new Memory(_debugger, this, dsp56k::MemArea_P);

		auto* callstack = new Callstack(_debugger, this);

		m_aui.AddPane(text, wxCENTER, "Assembly");

		m_aui.AddPane(pg, wxTOP, "Address Info");	m_aui.GetPane(pg).dock_proportion /= 2;
		m_aui.AddPane(regs, wxTOP, "Registers");

		m_aui.AddPane(memX, wxRIGHT, "X Memory");
		m_aui.AddPane(memY, wxRIGHT, "Y Memory");
		m_aui.AddPane(memP, wxRIGHT, "P Memory");
		m_aui.AddPane(callstack, wxRIGHT, "Callstack");

		m_aui.SetDockSizeConstraint(1.0f, 1.0f);

		m_aui.Update();

		LOG("Perspective: " << m_aui.SavePerspective());

		m_timer.Bind(wxEVT_TIMER, &MainWindow::onTimer, this);
		m_timer.Start(500, false);

		auto* statusBar = new StatusBar(m_debugger, this);
		SetStatusBar(statusBar);

		auto* menu = new wxMenuBar();

		auto* mFile = new wxMenu();
		mFile->Append(wxID_EXIT, "Close\tCtrl+Q");
		menu->Append(mFile, "File");

		auto* mEdit = new wxMenu();
		mEdit->Append(IdEditJitConfig, "JIT Config\tCtrl+J");
		menu->Append(mEdit, "Edit");

		auto* mDebug = new wxMenu();
		mDebug->Append(IdToggleBreakpoint, "Toggle Breakpoint\tF9");
		mDebug->Append(IdDebugBreak, "Break\tPause");
		mDebug->Append(IdDebugContinue, "Continue\tF5");
		mDebug->Append(IdDebugStepOver, "Step over\tF10");
		mDebug->Append(IdDebugStepInto, "Step into\tF11");
		mDebug->Append(IdDebugStepOut, "Step out\tShift+F11");

		menu->Append(mDebug, "Debug");

		auto* mGoto = new wxMenu();
		mGoto->Append(IdGotoPC, "Current PC\tHome");
		mGoto->Append(IdGotoAddress, "Address...\tCtrl+G");
		menu->Append(mGoto, "Go to");

		auto* mBookmarks = new wxMenu();
		mBookmarks->Append(IdBookmarkAdd, "Toggle Bookmark\tCtrl+F2");
		mBookmarks->Append(IdBookmarkGoto, "Go to next bookmark\tF2");
		menu->Append(mBookmarks, "Bookmarks");
/*
		wxAcceleratorEntry entries[] =
		{
			{wxACCEL_NORMAL, WXK_F9, IdToggleBreakpoint},
			{wxACCEL_CTRL, 'Q', wxID_EXIT}
		};

		const wxAcceleratorTable accel(std::size(entries), entries);

		SetAcceleratorTable(accel);
*/		SetMenuBar(menu);
	}

	MainWindow::~MainWindow()
	{
	}

	void MainWindow::onTimer(wxTimerEvent&)
	{
		m_debugger.jitState().processChanges();
	}

	void MainWindow::onEditJitConfig(wxCommandEvent& e)
	{
		JitConfigEditor d(m_debugger, this);

		if(d.ShowModal() == wxID_OK)
		{
			const auto config = d.getResult();

			m_debugger.setJitConfig(config);
		}
	}

	void MainWindow::onToggleBreakpoint(wxCommandEvent&)
	{
		m_debugger.sendEvent(&DebuggerListener::evToggleBreakpoint);
	}

	void MainWindow::onExecBreak(wxCommandEvent&)
	{
		m_debugger.dspExec().execBreak();
	}

	void MainWindow::onExecStepInfo(wxCommandEvent&)
	{
		m_debugger.dspExec().execStep();
	}

	void MainWindow::onExecStepOver(wxCommandEvent&)
	{
		m_debugger.dspExec().execStepOver();
	}

	void MainWindow::onExecStepOut(wxCommandEvent&)
	{
		m_debugger.dspExec().execStepOut();
	}

	void MainWindow::onExecContinue(wxCommandEvent&)
	{
		m_debugger.dspExec().execContinue();
	}

	void MainWindow::onGotoPC(wxCommandEvent&)
	{
		m_debugger.sendEvent(&DebuggerListener::evGotoPC);
	}

	void MainWindow::onGotoAddress(wxCommandEvent&)
	{
		GotoAddress dialog(this);

		if(dialog.ShowModal() == wxID_OK)
			m_debugger.sendEvent(&DebuggerListener::evGotoAddress, dialog.getAddress());
	}

	void MainWindow::onAddBookmark(wxCommandEvent& e)
	{
		m_debugger.sendEvent(&DebuggerListener::evToggleBookmark);
	}

	void MainWindow::onGotoBookmark(wxCommandEvent& e)
	{
		m_debugger.sendEvent(&DebuggerListener::evGotoBookmark);
	}

	wxBEGIN_EVENT_TABLE(MainWindow, wxFrame)
		EVT_MENU(IdEditJitConfig, MainWindow::onEditJitConfig)
		EVT_MENU(IdToggleBreakpoint, MainWindow::onToggleBreakpoint)
		EVT_MENU(IdDebugBreak, MainWindow::onExecBreak)
		EVT_MENU(IdDebugContinue, MainWindow::onExecContinue)
		EVT_MENU(IdDebugStepInto, MainWindow::onExecStepInfo)
		EVT_MENU(IdDebugStepOver, MainWindow::onExecStepOver)
		EVT_MENU(IdDebugStepOut, MainWindow::onExecStepOut)
		EVT_MENU(IdGotoPC, MainWindow::onGotoPC)
		EVT_MENU(IdGotoAddress, MainWindow::onGotoAddress)
		EVT_MENU(IdBookmarkAdd, MainWindow::onAddBookmark)
		EVT_MENU(IdBookmarkGoto, MainWindow::onGotoBookmark)
	wxEND_EVENT_TABLE()
}
