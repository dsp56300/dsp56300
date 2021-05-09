#pragma once

#include "jitops.h"

#include "asmjit/core/operand.h"
#include "asmjit/x86/x86operand.h"

namespace dsp56k
{
	inline void JitOps::signextend56to64(const asmjit::x86::Gpq& _reg) const
	{
		m_asm.sal(_reg, asmjit::Imm(8));
		m_asm.sar(_reg, asmjit::Imm(8));
	}

	inline void JitOps::signextend48to56(const asmjit::x86::Gpq& _reg) const
	{
		m_asm.sal(_reg, asmjit::Imm(16));
		m_asm.sar(_reg, asmjit::Imm(8));	// we need to work around the fact that there is no AND with 64 bit immediate operand
		m_asm.shr(_reg, asmjit::Imm(8));
	}

	void JitOps::signextend24to56(const asmjit::x86::Gpq& _reg) const
	{
		m_asm.sal(_reg, asmjit::Imm(40));
		m_asm.sar(_reg, asmjit::Imm(32));	// we need to work around the fact that there is no AND with 64 bit immediate operand
		m_asm.shr(_reg, asmjit::Imm(8));
	}

	inline void JitOps::mask56(const RegGP& _alu) const
	{
		m_asm.shl(_alu, asmjit::Imm(8));	// we need to work around the fact that there is no AND with 64 bit immediate operand
		m_asm.shr(_alu, asmjit::Imm(8));
	}
	
	inline void JitOps::signed24To56(const asmjit::x86::Gpq& _r) const
	{
		m_asm.shl(_r, asmjit::Imm(40));
		m_asm.sar(_r, asmjit::Imm(8));		// we need to work around the fact that there is no AND with 64 bit immediate operand
		m_asm.shr(_r, asmjit::Imm(8));
	}

	void JitOps::getMR(const asmjit::x86::Gpq& _dst) const
	{
		m_asm.mov(_dst, m_dspRegs.getSR());
		m_asm.shr(_dst, asmjit::Imm(8));
		m_asm.and_(_dst, asmjit::Imm(0xff));
	}

	void JitOps::getCCR(RegGP& _dst)
	{
		_dst.release();
		updateDirtyCCR();
		_dst.acquire();
		m_asm.mov(_dst, m_dspRegs.getSR());
		m_asm.and_(_dst, asmjit::Imm(0xff));
	}

	void JitOps::getCOM(const asmjit::x86::Gpq& _dst) const
	{
		m_block.mem().mov(_dst, m_block.dsp().regs().omr);
		m_asm.and_(_dst, asmjit::Imm(0xff));
	}

	void JitOps::getEOM(const asmjit::x86::Gpq& _dst) const
	{
		m_block.mem().mov(_dst, m_block.dsp().regs().omr);
		m_asm.shr(_dst, asmjit::Imm(8));
		m_asm.and_(_dst, asmjit::Imm(0xff));
	}

	void JitOps::setMR(const asmjit::x86::Gpq& _src) const
	{
		const RegGP r(m_block);
		m_asm.mov(r, m_dspRegs.getSR());
		m_asm.and_(r, asmjit::Imm(0xff00ff));
		m_asm.shl(_src, asmjit::Imm(8));
		m_asm.or_(r, _src);
		m_asm.shr(_src, asmjit::Imm(8));	// TODO: we don't wanna be destructive to the input for now
		m_asm.mov(m_dspRegs.getSR(), r.get());
	}

	void JitOps::setCCR(const asmjit::x86::Gpq& _src)
	{
		m_ccrDirty = false;
		m_asm.and_(m_dspRegs.getSR(), asmjit::Imm(0xffff00));
		m_asm.or_(m_dspRegs.getSR(), _src);
	}

	void JitOps::setCOM(const asmjit::x86::Gpq& _src) const
	{
		const RegGP r(m_block);
		m_block.mem().mov(r, m_block.dsp().regs().omr);
		m_asm.and_(r, asmjit::Imm(0xffff00));
		m_asm.or_(r, _src);
		m_block.mem().mov(m_block.dsp().regs().omr, r);
	}

	void JitOps::setEOM(const asmjit::x86::Gpq& _src) const
	{
		const RegGP r(m_block);
		m_block.mem().mov(r, m_block.dsp().regs().omr);
		m_asm.and_(r, asmjit::Imm(0xff00ff));
		m_asm.shl(_src, asmjit::Imm(8));
		m_asm.or_(r, _src);
		m_block.mem().mov(m_block.dsp().regs().omr, r);
		m_asm.shr(_src, asmjit::Imm(8));	// TODO: we don't wanna be destructive to the input for now
	}
}
