// dspEmuTesterDlg.cpp : implementation file
//

#include "stdafx.h"
#include "dspEmuTester.h"
#include "dspEmuTesterDlg.h"

#include "../dsp56kEmu/omfloader.h"

// CdspEmuTesterDlg dialog

#define LOCKDSP		ptypes::scopelock sl(m_lockDSP)


CdspEmuTesterDlg::CdspEmuTesterDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CdspEmuTesterDlg::IDD, pParent)
	, ptypes::thread(false)
	, m_mem()
	, m_dsp(m_mem)
	, m_alwaysUpdateRegs(TRUE)
	, m_triggerRun(false,false)
	, m_triggerPost(true,false)
	, m_runUntilPC(-1)
	, m_strRunUntilPC(_T("0x4a7ff"))
	, m_r_ictr(0)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

// _____________________________________________________________________________
// ~CdspEmuTesterDlg
//
CdspEmuTesterDlg::~CdspEmuTesterDlg()
{
	endThread();
}

void CdspEmuTesterDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_R_R0, m_r_r0);
	DDX_Text(pDX, IDC_R_R1, m_r_r1);
	DDX_Text(pDX, IDC_R_R2, m_r_r2);
	DDX_Text(pDX, IDC_R_R3, m_r_r3);
	DDX_Text(pDX, IDC_R_R4, m_r_r4);
	DDX_Text(pDX, IDC_R_R5, m_r_r5);
	DDX_Text(pDX, IDC_R_R6, m_r_r6);
	DDX_Text(pDX, IDC_R_R7, m_r_r7);
	DDX_Text(pDX, IDC_R_N0, m_r_n0);
	DDX_Text(pDX, IDC_R_N1, m_r_n1);
	DDX_Text(pDX, IDC_R_N2, m_r_n2);
	DDX_Text(pDX, IDC_R_N3, m_r_n3);
	DDX_Text(pDX, IDC_R_N4, m_r_n4);
	DDX_Text(pDX, IDC_R_N5, m_r_n5);
	DDX_Text(pDX, IDC_R_N6, m_r_n6);
	DDX_Text(pDX, IDC_R_N7, m_r_n7);
	DDX_Text(pDX, IDC_R_M0, m_r_m0);
	DDX_Text(pDX, IDC_R_M1, m_r_m1);
	DDX_Text(pDX, IDC_R_M2, m_r_m2);
	DDX_Text(pDX, IDC_R_M3, m_r_m3);
	DDX_Text(pDX, IDC_R_M4, m_r_m4);
	DDX_Text(pDX, IDC_R_M5, m_r_m5);
	DDX_Text(pDX, IDC_R_M6, m_r_m6);
	DDX_Text(pDX, IDC_R_M7, m_r_m7);

	DDX_Text(pDX, IDC_R_PC, m_r_pc);
	DDX_Text(pDX, IDC_R_SR, m_r_sr);
	DDX_Text(pDX, IDC_R_SC, m_r_sc);

	DDX_Text(pDX, IDC_R_LC, m_r_lc);
	DDX_Text(pDX, IDC_R_LA, m_r_la);

	DDX_Text(pDX, IDC_R_OMR, m_r_omr);

	DDX_Text(pDX, IDC_R_A2, m_r_a2);
	DDX_Text(pDX, IDC_R_A1, m_r_a1);
	DDX_Text(pDX, IDC_R_A0, m_r_a0);
	DDX_Text(pDX, IDC_R_B2, m_r_b2);
	DDX_Text(pDX, IDC_R_B1, m_r_b1);
	DDX_Text(pDX, IDC_R_B0, m_r_b0);

	DDX_Text(pDX, IDC_R_X1, m_r_x1);
	DDX_Text(pDX, IDC_R_X0, m_r_x0);
	DDX_Text(pDX, IDC_R_Y1, m_r_y1);
	DDX_Text(pDX, IDC_R_Y0, m_r_y0);
	DDX_Check(pDX, IDC_ALWAYSUPDATE, m_alwaysUpdateRegs);
	DDX_Text(pDX, IDC_RUNUNTILPC, m_strRunUntilPC);
	DDX_Text(pDX, IDC_R_ICTR, m_r_ictr);
	DDX_Control(pDX, IDC_LISTASM, m_listASM);
}

BEGIN_MESSAGE_MAP(CdspEmuTesterDlg, CDialog)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	//}}AFX_MSG_MAP
	ON_COMMAND(ID_FILE_LOADOMF, &CdspEmuTesterDlg::OnFileLoadomf)
	ON_BN_CLICKED(IDC_STEP, &CdspEmuTesterDlg::OnBnClickedStep)
	ON_BN_CLICKED(IDC_RUN, &CdspEmuTesterDlg::OnBnClickedRun)
	ON_BN_CLICKED(IDC_STOP, &CdspEmuTesterDlg::OnBnClickedStop)
	ON_WM_TIMER()
	ON_BN_CLICKED(IDC_RESET, &CdspEmuTesterDlg::OnBnClickedReset)
	ON_BN_CLICKED(IDC_REFRESHREGS, &CdspEmuTesterDlg::OnBnClickedRefreshregs)
	ON_BN_CLICKED(IDC_ALWAYSUPDATE, &CdspEmuTesterDlg::OnBnClickedAlwaysupdate)
	ON_BN_CLICKED(IDC_RUNUNTIL, &CdspEmuTesterDlg::OnBnClickedRununtil)
END_MESSAGE_MAP()


// CdspEmuTesterDlg message handlers

BOOL CdspEmuTesterDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	UpdateData(FALSE);

	start();
	updateRegs();

//	loadOMF( "...lod" );

	return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CdspEmuTesterDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CdspEmuTesterDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

// _____________________________________________________________________________
// formatReg
//
void CdspEmuTesterDlg::formatReg( CString& _dst, const TReg8& _src )
{
	_dst.Format( "%02x", _src.var );
}

// _____________________________________________________________________________
// formatReg
//
void CdspEmuTesterDlg::formatReg( CString& _dst, const TReg24& _src )
{
	_dst.Format( "%06x", _src.var&0x00ffffff);
}
// _____________________________________________________________________________
// updateRegs
//
void CdspEmuTesterDlg::updateRegs( const SRegs& _regs )
{
	formatReg( m_r_a0 , _regs.a0);
	formatReg( m_r_a1 , _regs.a1);
	formatReg( m_r_a2 , _regs.a2);
				 
	formatReg( m_r_b0 , _regs.b0);
	formatReg( m_r_b1 , _regs.b1);
	formatReg( m_r_b2 , _regs.b2);
				 
	formatReg( m_r_x0 , _regs.x0);
	formatReg( m_r_x1 , _regs.x1);
				 
	formatReg( m_r_y0 , _regs.y0);
	formatReg( m_r_y1 , _regs.y1);
				 
	formatReg( m_r_pc , _regs.pc);
	formatReg( m_r_sr , _regs.sr);
	formatReg( m_r_omr, _regs.omr);
	formatReg( m_r_sc , _regs.sc);

	formatReg( m_r_la , _regs.la);
	formatReg( m_r_lc , _regs.lc);
				 
	formatReg( m_r_r0 , _regs.r0);
	formatReg( m_r_r1 , _regs.r1);
	formatReg( m_r_r2 , _regs.r2);
	formatReg( m_r_r3 , _regs.r3);
	formatReg( m_r_r4 , _regs.r4);
	formatReg( m_r_r5 , _regs.r5);
	formatReg( m_r_r6 , _regs.r6);
	formatReg( m_r_r7 , _regs.r7);
				 
	formatReg( m_r_n0 , _regs.n0);
	formatReg( m_r_n1 , _regs.n1);
	formatReg( m_r_n2 , _regs.n2);
	formatReg( m_r_n3 , _regs.n3);
	formatReg( m_r_n4 , _regs.n4);
	formatReg( m_r_n5 , _regs.n5);
	formatReg( m_r_n6 , _regs.n6);
	formatReg( m_r_n7 , _regs.n7);
				 
	formatReg( m_r_m0 , _regs.m0);
	formatReg( m_r_m1 , _regs.m1);
	formatReg( m_r_m2 , _regs.m2);
	formatReg( m_r_m3 , _regs.m3);
	formatReg( m_r_m4 , _regs.m4);
	formatReg( m_r_m5 , _regs.m5);
	formatReg( m_r_m6 , _regs.m6);
	formatReg( m_r_m7 , _regs.m7);

	m_r_ictr = _regs.ictr.var;
}
// _____________________________________________________________________________
// updateRegs
//
void CdspEmuTesterDlg::updateRegs()
{
	UpdateData(TRUE);
	updateRegs( m_regs );
	UpdateData(FALSE);
}

void CdspEmuTesterDlg::OnFileLoadomf()
{
	CFileDialog dlg(TRUE,"lod",0,OFN_FILEMUSTEXIST|OFN_HIDEREADONLY|OFN_ENABLESIZING,"lod Files (*.lod)|*.lod||", this );

	if( dlg.DoModal() == IDOK )
	{
		loadOMF(dlg.GetPathName());
	}
}

void CdspEmuTesterDlg::OnBnClickedStep()
{
	{
		LOCKDSP;
		m_asm.clear();
		execOne();
		m_dsp.readDebugRegs(m_regs);
	}
	updateRegs();
	updateListBox();
}

void CdspEmuTesterDlg::OnBnClickedRun()
{
	m_runUntilPC.var = -1;

	UpdateData();
	m_triggerPost.signal();
	m_triggerRun.signal();
}

void CdspEmuTesterDlg::OnBnClickedRununtil()
{
	UpdateData(TRUE);

	OnBnClickedRun();

	parseHex( m_runUntilPC, m_strRunUntilPC );
}

void CdspEmuTesterDlg::OnBnClickedStop()
{
	m_triggerRun.reset();
	updateRegs();
	m_listASM.ResetContent();
}

void CdspEmuTesterDlg::OnTimer(UINT_PTR nIDEvent)
{
	CDialog::OnTimer(nIDEvent);
}

void CdspEmuTesterDlg::OnBnClickedReset()
{
	OnBnClickedStop();
	{
		LOCKDSP;
		m_dsp.resetHW();
	}
	m_pcHistory.clear();
	updateRegs();
}

void CdspEmuTesterDlg::OnBnClickedRefreshregs()
{
	updateRegs();
}

void CdspEmuTesterDlg::OnBnClickedAlwaysupdate()
{
	UpdateData(TRUE);
}

// _____________________________________________________________________________
// execute
//
void CdspEmuTesterDlg::execute()
{
	::SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);

	while( !get_signaled() )
	{
		m_triggerRun.wait();
		m_triggerPost.wait();
		{
			LOCKDSP;
			if( m_runUntilPC == m_dsp.getPC() )
			{
				m_triggerRun.reset();
				m_dsp.readDebugRegs(m_regs);
			}
			else
			{
				execOne();

				if( m_alwaysUpdateRegs )
					m_dsp.readDebugRegs(m_regs);
			}
		}
		PostMessage( WM_USER, 0, 0 );
//		ptypes::psleep((m_alwaysUpdateRegs || m_runUntilPC == -1) ? 1 : 0);
	}
}

// _____________________________________________________________________________
// PreTranslateMessage
//
BOOL CdspEmuTesterDlg::PreTranslateMessage( MSG* pMsg )
{
	switch( pMsg->message )
	{
	case WM_USER:
		onPostExecByTimer();
		m_triggerPost.signal();
		break;
	}
	return __super::PreTranslateMessage(pMsg);
}
// _____________________________________________________________________________
// execByTimer
//
void CdspEmuTesterDlg::onPostExecByTimer()
{
	{
		LOCKDSP;

		if( m_runUntilPC == m_dsp.getPC() )
		{
			OnBnClickedStop();
			return;
		}
	}

	if( m_alwaysUpdateRegs )
		updateRegs();
}
// _____________________________________________________________________________
// parseHex
//
void CdspEmuTesterDlg::parseHex( TReg24& _dst, CString _src )
{
	if( _src.IsEmpty() )
	{
		_dst.var = -1;
		return;
	}

	_src.Replace( "0x", "" );

	sscanf_s( _src, "%x", &_dst );
}
// _____________________________________________________________________________
// OnCancel
//
void CdspEmuTesterDlg::OnCancel()
{
	endThread();
	__super::OnCancel();
}
// _____________________________________________________________________________
// endThread
//
void CdspEmuTesterDlg::endThread()
{
	signal();
	m_triggerPost.signal();
	m_triggerRun.signal();
	waitfor();
}
// _____________________________________________________________________________
// loadOMF
//
void CdspEmuTesterDlg::loadOMF( const CString& _file )
{
	CWaitCursor wc;

	OMFLoader l;
	l.load( _file, m_mem );
	{
		LOCKDSP;
		m_dsp.resetHW();
		m_dsp.setPC( TReg24(0x04C4E7) );
		m_dsp.setPC( TReg24(0x4b16d) );
		m_dsp.readDebugRegs(m_regs);
	}
	m_pcHistory.clear();
	updateRegs();
}

// _____________________________________________________________________________
// execOne
//
void CdspEmuTesterDlg::execOne()
{
	const TWord pc = m_dsp.getPC().toWord();

	m_dsp.exec();

	char ass[256];
	sprintf_s( ass, 255, "%06x: %s", m_dsp.getPC().toWord(), m_dsp.getASM() );

	m_asm.push_back( std::string(ass) );
}
// _____________________________________________________________________________
// updateListBox
//
void CdspEmuTesterDlg::updateListBox()
{
	std::vector<std::string> myAsm;

	{
		LOCKDSP;
		myAsm.swap( m_asm );
	}

	for( size_t i=0; i<myAsm.size(); ++i )
		m_listASM.AddString( CString(myAsm[i].c_str()) );
	m_listASM.SetCurSel( m_listASM.GetCount() - 1 );
}