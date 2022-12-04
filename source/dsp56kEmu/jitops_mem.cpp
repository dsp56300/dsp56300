#include "jitconfig.h"
#include "jitops.h"

namespace dsp56k
{
	void JitOps::readMemOrPeriph(DspValue& _dst, EMemArea _area, const DspValue& _offset, Instruction _inst)
	{
		if (_offset.isImm24())
		{
			const auto o = _offset.imm24();
			if (o >= XIO_Reserved_High_First)
				m_block.mem().readPeriph(_dst, _area, o, _inst);
			else
				m_block.mem().readDspMemory(_dst, _area, o);
		}
		else
		{
			if(!m_block.getConfig().dynamicPeripheralAddressing)
			{
				// Disable reading peripherals with dynamic addressing (such as (r0)+) as it is costly but most likely unused
				m_block.mem().readDspMemory(_dst, _area, _offset);
			}
			else
			{
				If(m_block, [&](const asmjit::Label& _toFalse)
				{
#ifdef HAVE_ARM64
					const RegScratch scratch(m_block);
					m_asm.mov(r32(scratch), asmjit::Imm(XIO_Reserved_High_First));
					m_asm.cmp(_offset.get(), r32(scratch));
#else
					m_asm.cmp(_offset.get(), asmjit::Imm(XIO_Reserved_High_First));
#endif
					m_asm.jge(_toFalse);
				}, [&]()
				{
					m_block.mem().readDspMemory(_dst, _area, _offset);
				}, [&]()
				{
					m_block.mem().readPeriph(_dst, _area, _offset, _inst);
				}, true, false, false
				);
			}
		}
	}

	void JitOps::writeMemOrPeriph(EMemArea _area, const DspValue& _offset, const DspValue& _value)
	{
		if (_offset.isImm24())
		{
			const auto o = _offset.imm24();
			if (o >= XIO_Reserved_High_First)
				m_block.mem().writePeriph(_area, o, _value);
			else
				m_block.mem().writeDspMemory(_area, o, _value);
		}
		else
		{
			if(!m_block.getConfig().dynamicPeripheralAddressing)
			{
				// Disable writing to peripherals with dynamic addressing (such as (r0)+) for now as it is costly but most likely unused
				m_block.mem().writeDspMemory(_area, _offset, _value);
			}
			else
			{
				If(m_block, [&](const asmjit::Label& _toFalse)
				{
#ifdef HAVE_ARM64
					const RegScratch scratch(m_block);
					m_asm.mov(r32(scratch), asmjit::Imm(XIO_Reserved_High_First));
					m_asm.cmp(_offset.get(), r32(scratch));
#else
					m_asm.cmp(r32(_offset.get()), asmjit::Imm(XIO_Reserved_High_First));
#endif
					m_asm.jge(_toFalse);
				}, [&]()
				{
					m_block.mem().writeDspMemory(_area, _offset, _value);
				}, [&]()
				{
					m_block.mem().writePeriph(_area, _offset, _value);
				}, true, false, false
				);
			}
		}
	}
}
