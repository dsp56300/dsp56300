#include "registers.h"

#include "debugger.h"

namespace dsp56kDebugger
{
	constexpr auto g_globalAlpha = 170;

	const auto posX = wxPoint(0,0);
	const auto posY = wxPoint(2,0);
	const auto posA = wxPoint(0,1);
	const auto posB = wxPoint(2,1);

	const auto posX1 = wxPoint(2,2);
	const auto posX0 = wxPoint(4,2);
	const auto posY1 = wxPoint(2,3);
	const auto posY0 = wxPoint(4,3);
	const auto posA2 = wxPoint(0,4);
	const auto posA1 = wxPoint(2,4);
	const auto posA0 = wxPoint(4,4);
	const auto posB2 = wxPoint(0,5);
	const auto posB1 = wxPoint(2,5);
	const auto posB0 = wxPoint(4,5);

	const auto posR0 = wxPoint( 6,2);
	const auto posN0 = wxPoint( 8,2);
	const auto posM0 = wxPoint(10,2);

	const auto posPC = wxPoint(0,7);
	const auto posSR = wxPoint(2,7);
	const auto posOMR = wxPoint(4,7);

	const auto posLA = wxPoint(0,8);
	const auto posLC = wxPoint(2,8);

	const auto posSSH = wxPoint(0,9);
	const auto posSSL = wxPoint(2,9);
	const auto posSP = wxPoint(4,9);

	const auto posEP = wxPoint(0,10);
	const auto posSZ = wxPoint(2,10);
	const auto posSC = wxPoint(4,10);

	Registers::Registers(Debugger& _debugger, wxWindow* _parent)
		: Grid(_parent, wxSize(500,300))
		, DebuggerListener(_debugger)
	{
		CreateGrid(11, 12, wxGridSelectNone);
		SetDefaultCellAlignment(wxALIGN_RIGHT, wxALIGN_CENTER);
		SetRowLabelSize(0);
		SetColLabelSize(0);

		const auto wLabel = 40;
		const auto wWide = 150;
		const auto wDefault = 70;

		SetColSize(0, wLabel);
		SetColSize(1, wWide);
		SetColSize(2, wLabel);
		SetColSize(3, wWide);

		for(auto i=4; i<12; ++i)
			SetColSize(i, i & 1 ? wDefault : wLabel);

		m_timer.Bind(wxEVT_TIMER, &Registers::onTimer, this);
		m_timer.Start(500, false);

		initCell("x", posX);	initCell("y", posY);
		initCell("a", posA);	initCell("b", posB);
		initCell("x1", posX1);  initCell("x0", posX0);
		initCell("y1", posY1);  initCell("y0", posY0);
		initCell("a2", posA2);  initCell("a1", posA1); initCell("a0", posA0);
		initCell("b2", posB2);  initCell("b1", posB1); initCell("b0", posB0);

		for(size_t i=0; i<8; ++i)
		{
			initCell("r" + std::to_string(i), posR0 + wxPoint(0,i));
			initCell("n" + std::to_string(i), posN0 + wxPoint(0,i));
			initCell("m" + std::to_string(i), posM0 + wxPoint(0,i));
		}

		initCell("pc", posPC);		initCell("sr", posSR);		initCell("omr", posOMR);
		initCell("la", posLA);		initCell("lc", posLC);
		initCell("ssh", posSSH);	initCell("ssl", posSSL);	initCell("sp", posSP);
		initCell("ep", posEP);		initCell("sz", posSZ);		initCell("sc", posSC);

		const auto cA = wxColour(255,0,0,30);
		const auto cB = wxColour(255,100,0,30);
		const auto cX = wxColour(0,200,0,20);
		const auto cY = wxColour(0,200,0,30);
		const auto cR = wxColour(0,0,255,10);
		const auto cN = wxColour(0,0,255,20);
		const auto cM = wxColour(0,0,255,30);
		const auto cS = wxColour(0,255,255,50);
		const auto cL = wxColour(255,0,255,20);

		initColor(posA, cA);		initColor(posA0, cA);		initColor(posA1, cA);		initColor(posA2, cA);
		initColor(posB, cB);		initColor(posB0, cB);		initColor(posB1, cB);		initColor(posB2, cB);
		initColor(posX, cX);		initColor(posX0, cX);		initColor(posX1, cX);
		initColor(posY, cY);		initColor(posY0, cY);		initColor(posY1, cY);

		initColor(posLC, cL);		initColor(posLA, cL);
		initColor(posEP, cS);		initColor(posSC, cS);		initColor(posSP, cS);		initColor(posSZ, cS);		

		for(size_t i=0; i<8; ++i)
		{
			initColor(posR0 + wxPoint(0,i), cR);
			initColor(posN0 + wxPoint(0,i), cN);
			initColor(posM0 + wxPoint(0,i), cM);
		}
	}

	void Registers::updateCell(uint64_t _value, uint32_t _bits, const wxPoint& _pos)
	{
		std::stringstream ss;
		ss << std::hex << std::setfill('0') << std::setw(_bits>>2) << _value;
		SetCellValue(_pos.y, _pos.x + 1, ss.str());
	}

	void Registers::onTimer(wxTimerEvent& e)
	{
		refresh();
	}

	void Registers::initColor(const wxPoint& _pos, const wxColour& _col)
	{
		auto convert = [&_col](const uint8_t _value)
		{
			uint32_t diff = 255 - _value;
			diff *= _col.Alpha() * g_globalAlpha / 255;
			diff /= 255;
			diff = std::max(static_cast<uint32_t>(0), std::min(diff,static_cast<uint32_t>(255)));
			return static_cast<uint8_t>(255 - diff);
		};

		const auto c = wxColour(convert(_col.Red()), convert(_col.Green()), convert(_col.Blue()));

		SetCellBackgroundColour(_pos.y, _pos.x, c);
		SetCellBackgroundColour(_pos.y, _pos.x+1, c);
	}

	void Registers::initCell(const std::string& _label, const wxPoint& _pos)
	{
		SetCellValue(_pos.y, _pos.x, _label);
		SetCellTextColour(_pos.y, _pos.x, *wxBLUE);
	}

	void Registers::refresh()
	{
		const auto& regs = debugger().dsp().regs();

		updateCellT(regs.x, posX);	updateCellT(regs.y, posY);
		updateCellT(regs.a, posA);	updateCellT(regs.b, posB);
		updateCellT(hiword(regs.x), posX1); updateCellT(loword(regs.x), posX0);
		updateCellT(hiword(regs.y), posY1); updateCellT(loword(regs.y), posY0);
		updateCellT(extword(regs.a), posA2); updateCellT(hiword(regs.a), posA1); updateCellT(loword(regs.a), posA0);
		updateCellT(extword(regs.b), posB2); updateCellT(hiword(regs.b), posB1); updateCellT(loword(regs.b), posB0);

		for(size_t i=0; i<8; ++i)
		{
			updateCellT(regs.r[i], posR0 + wxPoint(0,i));
			updateCellT(regs.n[i], posN0 + wxPoint(0,i));
			updateCellT(regs.m[i], posM0 + wxPoint(0,i));
		}

		const auto ss = regs.ss[regs.sp.var];

		updateCellT(regs.pc, posPC);		updateCellT(regs.sr, posSR);		updateCellT(regs.omr, posOMR);
		updateCellT(regs.la, posLA);		updateCellT(regs.lc, posLC);
		updateCellT(hiword(ss), posSSH);	updateCellT(loword(ss), posSSL);	updateCellT(regs.sp, posSP);
		updateCellT(regs.ep, posEP);		updateCellT(regs.sz, posSZ);		updateCellT(regs.sc, posSC);
	}
}
