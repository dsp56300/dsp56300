// dspEmuTesterDlg.h : header file
//

#pragma once
#include <atomic>
#include <mutex>

#include "afxwin.h"

// CdspEmuTesterDlg dialog
class CdspEmuTesterDlg : public CDialog
{
// Construction
public:
	CdspEmuTesterDlg(CWnd* pParent = NULL);	// standard constructor
	virtual ~CdspEmuTesterDlg();

// Dialog Data
	enum { IDD = IDD_DSPEMUTESTER_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	CString m_r_r0;
	CString m_r_r1;
	CString m_r_r2;
	CString m_r_r3;
	CString m_r_r4;
	CString m_r_r5;
	CString m_r_r6;
	CString m_r_r7;

	CString m_r_n0;
	CString m_r_n1;
	CString m_r_n2;
	CString m_r_n3;
	CString m_r_n4;
	CString m_r_n5;
	CString m_r_n6;
	CString m_r_n7;

	CString m_r_m0;
	CString m_r_m1;
	CString m_r_m2;
	CString m_r_m3;
	CString m_r_m4;
	CString m_r_m5;
	CString m_r_m6;
	CString m_r_m7;

	CString m_r_pc;
	CString m_r_sr;
	CString m_r_omr;
	CString m_r_sc;
	CString m_r_lc;
	CString m_r_la;
	CString m_r_a2, m_r_a1, m_r_a0;
	CString m_r_b2, m_r_b1, m_r_b0;
	CString m_r_x1, m_r_x0;
	CString m_r_y1, m_r_y0;

	void formatReg( CString& _dst, const TReg8& _src );
	void formatReg( CString& _dst, const TReg24& _src );

	void updateRegs( const SRegs& _regs );
	void updateRegs();

	Peripherals56303 m_periphA, m_periphB;
	Memory	m_mem;
	DSP		m_dsp;

	TReg24	m_runUntilPC;

	afx_msg void OnFileLoadomf();
	afx_msg void OnBnClickedStep();
	afx_msg void OnBnClickedRun();
	afx_msg void OnBnClickedStop();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnBnClickedReset();
	BOOL m_alwaysUpdateRegs;
	afx_msg void OnBnClickedRefreshregs();
	afx_msg void OnBnClickedAlwaysupdate();

	virtual void execute();

	virtual BOOL PreTranslateMessage(MSG* pMsg);

	void onPostExecByTimer();

	std::atomic<bool>	m_triggerRun;
	std::mutex m_triggerMutex;
	std::condition_variable m_triggerPost;

	std::atomic<bool> m_runThread;
	std::unique_ptr<std::thread> m_thread;

	afx_msg void OnBnClickedRununtil();
	CString m_strRunUntilPC;

	void parseHex( TReg24& _dst, CString _src );

	void OnCancel();

	void endThread();

	void loadOMF( const CString& _file );

	std::vector<TReg24>	m_pcHistory;

	void execOne();

	std::mutex m_lockDSP;
	DWORD m_r_ictr;

	SRegs m_regs;
	CListBox m_listASM;

	std::vector<std::string>	m_asm;

	void updateListBox();
	afx_msg void OnChangeRegPC();
	afx_msg void OnFileLoadstate();
	afx_msg void OnFileSavestate();
};
