#include "jit.h"

#include "dsp.h"
#include "jitblock.h"
#include "jithelper.h"
#include "jitops.h"

#include "asmjit/core/jitruntime.h"
#include "asmjit/x86/x86assembler.h"

using namespace asmjit;
using namespace x86;

/*
	The idea of this JIT is to keep all relevant DSP registers in x64 registers. Register allocation looks like this:

	RAX	= func ret val                             |               XMM00 = DSP AGU 0 [0,M,N,R]
	RBX							*                  |               XMM01 = DSP AGU 1 [0,M,N,R]
	RCX	= func arg 0 (Microsoft)                   |               XMM02 = DSP AGU 2 [0,M,N,R]
	RDX	= func arg 1 / temp			               |               XMM03 = DSP AGU 3 [0,M,N,R]
	RBP	= temp					*                  |               XMM04 = DSP AGU 4 [0,M,N,R]
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
	constexpr bool g_traceOps = false;

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
		// loop processing
		if(m_dsp.sr_test(SR_LF))
		{
			if(m_dsp.getPC().var == m_dsp.regs().la.var + 1)
			{
				auto& lc = m_dsp.regs().lc.var;
				if(lc <= 1)
				{
					m_dsp.do_end();					
				}
				else
				{
					--lc;
					m_dsp.setPC(hiword(m_dsp.regs().ss[m_dsp.ssIndex()]));
				}
			}
		}

		const auto pc = static_cast<TWord>(m_dsp.getPC().var);

		LOG("Exec @ " << HEX(pc));

		// get JIT code
		auto& cacheEntry = m_jitCache[pc];

		if(cacheEntry == nullptr)
		{
			// No code preset, generate
			LOG("Generating new JIT block for PC " << HEX(pc));
			emit(pc);
		}
		else
		{
			// there is code, but the JIT block does not start at the PC position that we want to run. We need to throw the block away and regenerate
			if(cacheEntry->getPCFirst() < pc)
			{
				LOG("Unable to jump into the middle of a block, destroying existing block & recreating from " << HEX(pc));
				destroy(cacheEntry);
				emit(pc);
			}
		}

		assert(cacheEntry);
		
		// run JIT code
		cacheEntry->exec();

		m_dsp.m_instructions += cacheEntry->getExecutedInstructionCount();

		if(cacheEntry->nextPC() != g_pcInvalid)
		{
			// If the JIt block executed a branch, point PC to the new location
			m_dsp.setPC(cacheEntry->nextPC());
		}
		else
		{
			// Otherwise, move PC forward
			m_dsp.setPC(pc + cacheEntry->getPMemSize());
		}

		if(g_traceOps)
		{
			TWord op, opB;
			m_dsp.mem.getOpcode(pc, op, opB);
			m_dsp.traceOp(pc, op, opB, cacheEntry->getPMemSize());
		}

		// if JIT code has written to P memory, destroy a JIT block if present at the write location
		if(cacheEntry->pMemWriteAddress() != g_pcInvalid)
		{
			notifyProgramMemWrite(cacheEntry->pMemWriteAddress());
			m_dsp.notifyProgramMemWrite(cacheEntry->pMemWriteAddress());
		}
	}

	void Jit::notifyProgramMemWrite(TWord _offset)
	{
		destroy(_offset);
	}

	void Jit::emit(const TWord _pc)
	{
		AsmJitLogger logger;
		AsmJitErrorHandler errorHandler;
		CodeHolder code;

//		code.setLogger(&logger);
		code.setErrorHandler(&errorHandler);
		code.init(m_rt.environment());

		Assembler m_asm(&code);

		// TODO: we need to stop emit inside block if the PC already has a block, atm we may have overlapping blocks
		JitBlock* b;

		{
			PushTemps temps(m_asm);

			b = new JitBlock(m_asm, m_dsp);

			if(!b->emit(_pc))
			{
				LOG("FATAL: code generation failed for PC " << HEX(_pc));
				delete b;
				return;
			}
		}

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

		LOG("New block generated @ " << HEX(_pc) << " up to " << HEX(_pc + b->getPMemSize() - 1) << ", instruction count " << b->getExecutedInstructionCount() << ", disasm " << b->getDisasm());
	}

	void Jit::destroy(JitBlock* _block)
	{
		const auto first = _block->getPCFirst();
		const auto last = first + _block->getPMemSize();

		LOG("Destroying JIT block at PC " << HEX(first) << ", length " << _block->getPMemSize());

		for(auto i=first; i<last; ++i)
			m_jitCache[i] = nullptr;

		m_rt.release(_block->getFunc());

		delete _block;
	}

	void Jit::destroy(TWord _pc)
	{
		const auto cacheEntry = m_jitCache[_pc];
		if(!cacheEntry)
			return;
		destroy(cacheEntry);
	}
}
