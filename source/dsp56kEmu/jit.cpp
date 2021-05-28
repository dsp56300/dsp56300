#include "jit.h"

#include "dsp.h"
#include "jitblock.h"
#include "jitops.h"

#include "asmjit/core/jitruntime.h"
#include "asmjit/x86/x86assembler.h"

using namespace asmjit;
using namespace x86;

/*
	The idea of this JIT is to keep all relevant DSP registers in X64 registers. Register allocation looks like this:

	RAX	= func ret val                             |               XMM00 = DSP AGU 0 [0,M,N,R]
	RBX							*                  |               XMM01 = DSP AGU 1 [0,M,N,R]
	RCX	= func arg 0 (Microsoft)                   |               XMM02 = DSP AGU 2 [0,M,N,R]
	RDX	= func arg 1 / temp			               |               XMM03 = DSP AGU 3 [0,M,N,R]
	RBP							*                  |               XMM04 = DSP AGU 4 [0,M,N,R]
	RSI	=    						               |               XMM05 = DSP AGU 5 [0,M,N,R]
	RDI	= func arg 0 (linux)    				   |               XMM06 = DSP AGU 6 [0,M,N,R]
	RSP							*	               |               XMM07 = DSP AGU 7 [0,M,N,R]
	R8  = func arg 2 / DSP Status Register		   |               XMM08 = DSP A
	R9  = func arg 3 / DSP Program Counter		   |               XMM09 = DSP B
	R10 = DSP Loop Counter			               |               XMM10 = DSP X
	R11 = DSP Loop Address			               |               XMM11 = DSP Y
	R12	= temp                  *                  |               XMM12 = last modified ALU for lazy SR updates
	R13	= temp                  *                  |               XMM13 = temp
	R14	= temp                  *                  |               XMM14 = temp
	R15	= temp                  *                  |               XMM15 = temp

	* = callee-save = we need to restore the previous register state before returning
*/

namespace dsp56k
{
	Jit::Jit(DSP& _dsp) : m_dsp(_dsp)
	{
		m_jitCache.resize(_dsp.memory().size(), nullptr);
	}

	Jit::~Jit()
	{
		for(size_t i=0; i<m_jitCache.size(); ++i)
		{
			auto* entry = m_jitCache[i];
			if(entry)
				destroy(entry);
		}
	}

	void Jit::exec()
	{
		const auto pc = static_cast<TWord>(m_dsp.getPC().var);

		auto& cacheEntry = m_jitCache[pc];

		if(cacheEntry == nullptr)
		{
			LOG("Generating new JIT block for PC " << HEX(pc));
			emit(pc);
		}
		else
		{
			if(cacheEntry->getPCFirst() < pc)
			{
				LOG("Unable to jump into the middle of a block, destroying existing block & recreating from " << HEX(pc));
				destroy(cacheEntry);
				emit(pc);
			}
		}
		assert(cacheEntry);
		cacheEntry->exec();
	}

	void Jit::emit(const TWord _pc)
	{
		CodeHolder code;

		code.init(m_rt.environment());

		Assembler m_asm(&code);

		m_asm.push(r12);
		m_asm.push(r13);
		m_asm.push(r14);
		m_asm.push(r15);

		auto* b = new JitBlock(m_asm, m_dsp);

		if(!b->emit(_pc))
		{
			LOG("FATAL: code generation failed for PC " << HEX(_pc));
			delete b;
			return;
		}

		m_asm.pop(r15);
		m_asm.pop(r14);
		m_asm.pop(r13);
		m_asm.pop(r12);

		m_asm.ret();

		JitBlock::JitEntry func;

		const auto err = m_rt.add(&func, &code);

		if(err)
		{
			const auto* const errString = DebugUtils::errorAsString(err);
			LOG("JIT failed: " << err << " - " << errString);
			return;
		}

		b->setFunc(func);

		const auto first = b->getPCFirst();
		const auto last = first + b->getPMemSize();

		for(auto i=first; i<last; ++i)
			m_jitCache[i] = b;
	}

	void Jit::destroy(JitBlock* _block)
	{
		const auto first = _block->getPCFirst();
		const auto last = first + _block->getPMemSize();

		for(auto i=first; i<last; ++i)
			m_jitCache[i] = nullptr;

		m_rt.release(_block->getFunc());

		delete _block;
	}
}
