#include "dsp.h"
#include "jitconfig.h"
#include "jitops.h"

namespace dsp56k
{
	namespace
	{
#ifdef HAVE_X86_64
		bool dumpMemory(const DSP* _dsp, TWord _pc)
		{
			std::stringstream ss;
			ss << "dsp_dynamicPeripheralAccess_" << HEX(_pc) << ".asm";
			return _dsp->memory().saveAssembly(ss.str().c_str(), 0, _dsp->memory().sizeP(), true, false, _dsp->getPeriph(0), _dsp->getPeriph(1));
		}
#endif
	}
	void JitOps::debugDynamicPeripheralAddressing(const JitRegGP& _offset) const
	{
#ifdef HAVE_X86_64
		if(!m_block.getConfig().debugDynamicPeripheralAddressing)
			return;

		const SkipLabel skip(m_block.asm_());
		m_asm.cmp(r32(_offset), asmjit::Imm(getPeriphStartAddr()));
		m_asm.jl(skip);
		m_asm.mov(g_funcArgGPs[0], &m_block.dsp());
		m_asm.mov(g_funcArgGPs[1], m_pcCurrentOp);
		m_block.stack().call(reinterpret_cast<const void*>(dumpMemory));
		m_asm.int3();
		m_asm.nop(ptr(JitReg64(0), static_cast<int>(m_pcCurrentOp)));
#endif
	}

	void JitOps::readMemOrPeriph(DspValue& _dst, EMemArea _area, const DspValue& _offset, const Instruction _inst) const
	{
		if (_offset.isImm24())
		{
			const auto o = _offset.imm24();
			if (isPeriphAddress(o))
				m_block.mem().readPeriph(_dst, _area, o, _inst);
			else
				m_block.mem().readDspMemory(_dst, _area, o);
		}
		else
		{
			if(!m_block.getConfig().dynamicPeripheralAddressing)
			{
				debugDynamicPeripheralAddressing(r32(_offset));
				// Disable reading peripherals with dynamic addressing (such as (r0)+) as it is costly but most likely unused
				m_block.mem().readDspMemory(_dst, _area, _offset);
			}
			else
			{
				If(m_block, m_blockRuntimeData, [&](const asmjit::Label& _toFalse)
				{
#ifdef HAVE_ARM64
					const RegScratch scratch(m_block);
					m_asm.mov(r32(scratch), asmjit::Imm(getPeriphStartAddr()));
					m_asm.cmp(_offset.get(), r32(scratch));
#else
					m_asm.cmp(_offset.get(), asmjit::Imm(getPeriphStartAddr()));
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

	void JitOps::writeMemOrPeriph(const EMemArea _area, const DspValue& _offset, const DspValue& _value) const
	{
		if (_offset.isImm24())
		{
			const auto o = _offset.imm24();
			if (isPeriphAddress(o))
				m_block.mem().writePeriph(_area, o, _value);
			else
				m_block.mem().writeDspMemory(_area, o, _value);
		}
		else
		{
			if(!m_block.getConfig().dynamicPeripheralAddressing)
			{
				debugDynamicPeripheralAddressing(r32(_offset));
				// Disable writing to peripherals with dynamic addressing (such as (r0)+) for now as it is costly but most likely unused
				m_block.mem().writeDspMemory(_area, _offset, _value);
			}
			else
			{
				If(m_block, m_blockRuntimeData, [&](const asmjit::Label& _toFalse)
				{
#ifdef HAVE_ARM64
					const RegScratch scratch(m_block);
					m_asm.mov(r32(scratch), asmjit::Imm(getPeriphStartAddr()));
					m_asm.cmp(_offset.get(), r32(scratch));
#else
					m_asm.cmp(r32(_offset.get()), asmjit::Imm(getPeriphStartAddr()));
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
