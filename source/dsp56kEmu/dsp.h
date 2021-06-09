#pragma once

#include "disasm.h"
#include "registers.h"
#include "memory.h"
#include "utils.h"
#include "instructioncache.h"
#include "opcodes.h"
#include "logging.h"

namespace dsp56k
{
	class Memory;
	class UnitTests;
	class JitUnittests;
	class JitDspRegs;
	class JitOps;
	class Jit;
	
	using TInstructionFunc = void (DSP::*)(TWord op);

	class DSP final
	{
		friend class UnitTests;
		friend class JitUnittests;
		friend class JitDspRegs;
		friend class JitOps;
		friend class Jit;

		// _____________________________________________________________________________
		// types
		//
	public:
		struct SRegs
		{
			// accumulator
			TReg48 x,y;						// 48 bit
			TReg56 a,b;						// 56 bit

			// ---- AGU ----
			StaticArray< TReg24, 8> r;
			StaticArray< TReg24, 8> n;
			StaticArray< TReg24, 8> m;

			// ---- PCU ----
			TReg24 sr;						// status register (SR_..)
			TReg24 omr;						// operation mode register

			TReg24 pc;						// program counter
			TReg24 la, lc;					// loop address, loop counter

			//TReg24	ssh, ssl;			// system stack high, system stack low
			TReg24	sp;						// stack pointer
			TReg5	sc;						// stack counter

			StaticArray< TReg48, 16> ss;	// system stack

			TReg24	sz;						// stack size (used for stack extension)

			TReg24 vba;						// vector base address

//			TReg24 bcr, dcr;
//			TReg24 aar0, aar1, aar2, aar3;

			TReg24 ep;						// stack extension pointer register

			// ---- ----
//			TReg24 hit;		
//			TReg24 miss;		
//			TReg24 replace;	
//			TReg24 cyc;
//			TReg24 cnt1;
//			TReg24 cnt2;
//			TReg24 cnt3;		
//			TReg24 cnt4;
		};

		struct CCRCache
		{
			bool ab;
			TReg56 alu;
			uint32_t dirty;
		};

		enum ProcessingMode
		{
			Default,
			DefaultPreventInterrupt,	// The only purpose of this state is to give time to regular processing if the emulation is so slow that interrupts are constantly processed, leaving no room for regular processing
			FastInterrupt,
			LongInterrupt,
		};

		enum TraceMode
		{
			Disabled	= 0,

			Ops			= 0x01,
			Regs		= 0x02,
			StackIndent	= 0x04,
		};

		using TInterruptFunc = void (DSP::*)();

	private:
		// _____________________________________________________________________________
		// members
		//
		Memory&							mem;
		std::array<IPeripherals*, 2>	perif;
		uint32_t						peripheralCounter = 0;
		
		TWord							pcCurrentInstruction = 0;
		TWord							m_opWordB = 0;
		uint32_t						m_currentOpLen = 0;
		uint32_t						m_instructions = 0;

		SRegs							reg;
		CCRCache						ccrCache;

		ProcessingMode					m_processingMode = Default;

		TInterruptFunc					m_interruptFunc = &DSP::execNoPendingInterrupts;

		RingBuffer<TWord, 1024, false>	m_pendingInterrupts;	// TODO: array is way too large

		Opcodes							m_opcodes;

		struct OpcodeCacheEntry
		{
			TInstructionFunc op;
			TInstructionFunc opMove;
			TInstructionFunc opAlu;
		};

		std::vector<OpcodeCacheEntry>	m_opcodeCache;
		
		InstructionCache				cache;

		// used to monitor ALL register changes during exec
		struct SRegChange
		{
			EReg			reg;
			TReg24			valOld;
			TReg24			valNew;
			unsigned int	pc;
			unsigned int	ictr;
		};

		std::vector<SRegChange>		m_regChanges;

		// used to compare registers
		struct SRegState
		{
			int64_t	val;
		};

		StaticArray<SRegState,Reg_COUNT>	m_prevRegStates;

		TraceMode m_trace = Disabled;

		std::string		m_asm;
		Disassembler	m_disasm;

		Jit* m_jit = nullptr;

		// _____________________________________________________________________________
		// implementation
		//
	public:
				DSP								( Memory& _memory, IPeripherals* _pX, IPeripherals* _pY );

		void 	resetHW							();
		void 	resetSW							();

		void	jsr								(const TReg24& _val);
		void	jsr								( const TWord _val )						{ jsr(TReg24(_val)); }

		void 	setPC							( const TWord _val )						{ setPC(TReg24(_val)); }
		void 	setPC							( const TReg24& _val )						{ reg.pc = _val; }

		TReg24	getPC							() const									{ return reg.pc; }

		void 	exec							();
		void	execPeriph						();
		void	execInterrupts					();
		void	execDefaultPreventInterrupt		();
		void	execNoPendingInterrupts			();
		void	nop								() {}

		bool	readReg							( EReg _reg, TReg8& _res ) const;
		bool	readReg							( EReg _reg, TReg48& _res ) const;
		bool	readReg							( EReg _reg, TReg5& _res ) const;

		bool	readReg							( EReg _reg, TReg24& _res ) const;
		bool	writeReg						( EReg _reg, const TReg24& _val );

		bool	readReg							( EReg _reg, TReg56& _res ) const;
		bool	writeReg						( EReg _reg, const TReg56& _val );

		bool	readRegToInt					( EReg _reg, int64_t& _dst ) const;

		uint32_t	getInstructionCounter		() const									{ return m_instructions; }

		const char*			getASM						(TWord wordA, TWord wordB);
		const std::string&	getASM						() const							{ return m_asm; }

		const SRegs&	readRegs						() const		{ return reg; }

		void			readDebugRegs					( dsp56k::SRegs& _dst ) const;

		void			dumpCCCC						() const;

		void			logSC							( const char* _func ) const;

		bool			save							( FILE* _file ) const;
		bool			load							( FILE* _file );

		void			injectInterrupt					(uint32_t _interruptVectorAddress);

		void			clearOpcodeCache				();
		void			clearOpcodeCache				(TWord _address);

		void			dumpRegisters					() const;
		void			dumpRegisters					(std::stringstream& _ss) const;
		void			enableTrace						(TraceMode _trace) { m_trace = _trace; }

		Memory&			memory							()											{ return mem; }
		SRegs&			regs							()											{ return reg; }
		const Opcodes&	opcodes							() const									{ return m_opcodes; }
		Disassembler&	disassembler					()											{ return m_disasm; }

		void			setPeriph						(size_t _index, IPeripherals* _periph)		{ perif[_index] = _periph; _periph->setDSP(this); }
		IPeripherals*	getPeriph						(size_t _index)								{ return perif[_index]; }
		
		ProcessingMode getProcessingMode() const		{return m_processingMode;}

		void			setJit							(Jit* _jit) { m_jit = _jit; }
		Jit*			getJit							() { return m_jit; }
	private:

		std::string getSSindent() const;

		TWord	fetchOpWordB()
		{
			++m_currentOpLen;
			if(m_processingMode != FastInterrupt)
				++reg.pc.var;
			return m_opWordB;
		}

		// -- execution 
		TWord fetchPC()
		{
			TWord ret;
			memReadOpcode(reg.pc.toWord(), ret, m_opWordB );
			++reg.pc.var;
			return ret;
		}

		void 	execOp							(TWord op);

		void	exec_jump						(const TInstructionFunc& _func, TWord op);
		
		bool	exec_parallel					(const TInstructionFunc& instMove, const TInstructionFunc& instAlu, TWord op);

		bool	alu_multiply					(TWord op);

		bool	do_exec							( TWord _loopcount, TWord _addr );
		bool	do_end							();

		bool	rep_exec						(TWord loopCount);

		void	traceOp							();
		void	traceOp							(TWord pc, TWord opA, TWord opB, TWord opLen);

		// -- decoding helper functions

		int		decode_cccc				( TWord cccc ) const;

		template<TWord mmm>
		TWord	decode_MMMRRR_read		( TWord _rrr );
		TWord	decode_MMMRRR_read		( TWord _mmm, TWord _rrr );
		TWord	decode_XMove_MMRRR		( TWord _mm, TWord _rrr );

		TWord	decode_RRR_read			( TWord _mmmrrr ) const;
		TWord	decode_RRR_read			( TWord _mmmrrr, int _shortDisplacement ) const;

		template<typename T> T decode_ddddd_read( TWord _ddddd );

		template<typename T> bool decode_ddddd_write(TWord _ddddd, const T& _val );

		// Six-Bit Encoding for all on-chip registers
		TReg24	decode_dddddd_read		(TWord _dddddd);
		void	decode_dddddd_write		(TWord _dddddd, TReg24 _val);

		TReg24	decode_ddddd_pcr_read 	( TWord _ddddd );
		void	decode_ddddd_pcr_write	( TWord _ddddd, TReg24 _val );

		TReg8 decode_EE_read(TWord _ee) const;
		void decode_EE_write(TWord _ee, TReg8 _val);

		template<TWord ee> TReg24 decode_ee_read();
		template<TWord ee> void decode_ee_write(const TReg24& _value);
		void decode_ee_write(TWord ee, const TReg24& _value);

		template<TWord ff>
		TReg24 decode_ff_read();
		template<TWord ff>
		void decode_ff_write(const TReg24& _value);

		TReg24 decode_ff_read(TWord _ff);
		void decode_ff_write(TWord _ff, const TReg24& _value);

		TReg24 decode_JJ_read(TWord jj) const;
		TReg56 decode_JJJ_read_56(TWord jjj, bool _b) const;
		void decode_JJJ_readwrite(TReg56& alu, TWord jjj, bool _b);

		void decode_LLL_read(TWord _lll, TWord& x, TWord& y);
		void decode_LLL_write(TWord _lll, TReg24 x, TReg24 y);

		void decode_QQQQ_read(TReg24& _s1, TReg24& _s2, TWord _qqqq) const;
		void decode_QQQ_read(TReg24& _s1, TReg24& _s2, TWord _qqq) const;
		TReg24 decode_QQ_read(TWord _qq) const;
		TReg24 decode_qq_read(TWord _qq) const;

		template<typename T> T decode_sss_read( TWord _sss ) const;
		static TWord decode_sssss(TWord _sssss);

		// -- status register management

		void 	sr_set					( CCRMask _bits )					{ reg.sr.var |= _bits;	}
		void 	sr_clear				( CCRMask _bits )					{ reg.sr.var &= ~_bits; }
		int 	sr_test					( CCRMask _bits ) const				{ updateDirtyCCR(); return sr_test_noCache(_bits); }
		int 	sr_test_noCache			( CCRMask _bits ) const				{ return (reg.sr.var & _bits); }
		int 	sr_val					( CCRBit _bitNum ) const			{ updateDirtyCCR(); return sr_val_noCache(_bitNum); }
		int 	sr_val_noCache			( CCRBit _bitNum ) const			{ return (reg.sr.var >> _bitNum) & 1; }
		void 	sr_toggle				( CCRMask _bits, bool _set )		{ if( _set ) { sr_set(_bits); } else { sr_clear(_bits); } }
		void 	sr_toggle				( CCRBit _bit, Bit _value )			{ bitset<int32_t>(reg.sr.var, int32_t(_bit), _value); }

		void	sr_s_update				()
		{
			if( sr_test_noCache(SR_S) )
				return;

			const TWord offset = sr_val_noCache(SRB_S1) - sr_val_noCache(SRB_S0);
			const TWord bitA = 46 + offset;
			const TWord bitB = 45 + offset;

			if	(	(bitvalue(reg.a,bitA) != bitvalue(reg.a,bitB))
				|	(bitvalue(reg.b,bitA) != bitvalue(reg.b,bitB)) )
				sr_set( SR_S );
		}

		void	sr_e_update				( const TReg56& _ab )
		{
			/*
			Extension
			Indicates when the accumulator extension register is in use. This bit is
			cleared if all the bits of the integer portion of the 56-bit result are all
			ones or all zeros; otherwise, this bit is set. As shown below, the
			Scaling mode defines the integer portion. If the E bit is cleared, then
			the low-order fraction portion contains all the significant bits; the
			high-order integer portion is sign extension. In this case, the
			accumulator extension register can be ignored.
			S1 / S0 / Scaling Mode / Integer Portion
			0	0	No Scaling	Bits 55,54..............48,47
			0	1	Scale Down	Bits 55,54..............49,48
			1	0	Scale Up	Bits 55,54..............47,46
			*/

			const uint32_t mask = (0x3fe << sr_val_noCache(SRB_S0) >> sr_val_noCache(SRB_S1)) & 0x3ff;

			const uint32_t d2 = _ab.var >> 46;

			const uint32_t m2 = d2 & mask;

			const auto res = static_cast<int>(m2 != mask) & static_cast<int>(m2 != 0);

			sr_toggle( SRB_E, Bit(res));
		}

		void sr_u_update( const TReg56& _ab )
		{
			const auto sOffset = sr_val_noCache(SRB_S0) - sr_val_noCache(SRB_S1);

			const auto msb = 47 + sOffset;
			const auto lsb = 46 + sOffset;

			sr_toggle( SRB_U, bitvalue(_ab,msb) == bitvalue(_ab,lsb) );
		}

		void sr_n_update( const TReg56& _ab )
		{
			// Negative
			// Set if the MSB of the result is set; otherwise, this bit is cleared.	
			sr_toggle( SRB_N, bitvalue<55>(_ab) );
		}

		void sr_z_update( const TReg56& _ab )
		{
			const TReg56 zero(static_cast<TReg56::MyType>(0));
			sr_toggle( SR_Z, zero == _ab );
		}

		template<typename T>
		void	sr_c_update_arithmetic( const T& _old, const T& _new )
		{
			sr_toggle( SRB_C, bitvalue<55>(_old) != bitvalue<55>(_new) );
		}

		void sr_l_update_by_v()
		{
			// L is never cleared automatically, so only test to set
			if( sr_test_noCache(SR_V) )
				sr_set(SR_L);
		}

		// value needs to fit into 48 (arithmetic saturation mode) or 56 bits
		void sr_v_update( const int64_t& _notLimitedResult, TReg56& _result )
		{
			if( sr_test_noCache(SR_SM) )
			{
				const unsigned int test=(_result.var>>47)&0x13;
				if (!(test ^ 0x13) || !(test)) sr_set(SR_V);
			}
			else
			{
				sr_toggle( SR_V, ((_notLimitedResult>>48)^(_result.var>>48))&255);
			}
		}

		void setSR(const TReg24& _sr)
		{
			reg.sr = _sr;
		}

		void setSR(const TWord _sr)
		{
			setSR(TReg24(_sr));
		}

		const TReg24& getSR() const
		{
			updateDirtyCCR();
			return reg.sr;
		}

		void setCCRDirty(bool ab, const TReg56& _alu, uint32_t _dirtyBitsMask);
		void updateDirtyCCR() const;
		void resetCCRCache() { ccrCache.dirty = 0; }

		void sr_debug(char* _dst) const;

		// register access helpers

		TReg24	x0				() const							{ return loword(reg.x); }
		TReg24	x1				() const							{ return hiword(reg.x); }
		void	x0				(const TReg24& _val)				{ loword(reg.x,_val); }
		void	x0				(const TWord _val)					{ loword(reg.x,TReg24(_val)); }
		void	x1				(const TReg24& _val)				{ hiword(reg.x,_val); }
		void	x1				(const TWord _val)					{ hiword(reg.x,TReg24(_val)); }

		// set signed fraction (store 8 bit data in the MSB of the register)
		void	x0				(TReg8 _val)						{ x0(TReg24(_val.toWord()<<16)); }
		void	x1				(TReg8 _val)						{ x1(TReg24(_val.toWord()<<16)); }

		TReg24	y0				() const							{ return loword(reg.y); }
		TReg24	y1				() const							{ return hiword(reg.y); }
		void	y0				(const TReg24& _val)				{ loword(reg.y,_val); }
		void	y0				(const TWord& _val)					{ loword(reg.y,TReg24(_val)); }
		void	y1				(const TReg24& _val)				{ hiword(reg.y,_val); }
		void	y1				(const TWord& _val)					{ hiword(reg.y,TReg24(_val)); }

		// set signed fraction (store 8 bit data in the MSB of the register)
		void	y0				(const TReg8& _val)					{ y0(TReg24(_val.toWord()<<16)); }
		void	y1				(const TReg8& _val)					{ y1(TReg24(_val.toWord()<<16)); }

		TReg24	a0				() const							{ return loword(reg.a); }
		TReg24	a1				() const							{ return hiword(reg.a); }
		TReg8	a2				() const							{ return extword(reg.a); }

		void	a0				(const TReg24& _val)				{ loword(reg.a,_val); }
		void	a1				(const TReg24& _val)				{ hiword(reg.a,_val); }
		void	a2				(const TReg8& _val)					{ extword(reg.a,_val); }

		TReg24	b0				() const							{ return loword(reg.b); }
		TReg24	b1				() const							{ return hiword(reg.b); }
		TReg8	b2				() const							{ return extword(reg.b); }

		void	b0				(const TReg24& _val)				{ loword(reg.b,_val); }
		void	b1				(const TReg24& _val)				{ hiword(reg.b,_val); }
		void	b2				(const TReg8& _val)					{ extword(reg.b,_val); }

		void	iprc			(const TWord _value)				{ memWritePeriph(MemArea_X, XIO_IPRC, _value); }
		void	iprp			(const TWord _value)				{ memWritePeriph(MemArea_X, XIO_IPRP, _value); }

		TWord	iprc			() const							{ return memReadPeriph(MemArea_X, XIO_IPRC); }
		TWord	iprp			() const							{ return memReadPeriph(MemArea_X, XIO_IPRP); }

		template<typename T> T getA()
		{
			TReg56 temp = reg.a;
			scale( temp );
			T res;
			limit_transfer( res, temp );
			return res;
		}

		template<typename T> T getB()
		{
			TReg56 temp(reg.b);
			scale(temp);
			T res;
			limit_transfer( res, temp );
			return res;
		}

		void scale( TReg56& _scale ) const
		{
			if( sr_test_noCache(SR_S1) )
				_scale.var <<= 1;
			else if( sr_test_noCache(SR_S0) )
				_scale.var >>= 1;
		}

		void limit_transfer( int& _dst, const TReg56& _src )
		{
			const int64_t& test = _src.signextend<int64_t>();

			if( test < -140737488355328 )			// ff 800000 000000
			{
				sr_set( SR_L );
				_dst = 0x800000;
			}
			else if( test > 140737471578112 )		// 00 7fffff 000000
			{
				sr_set( SR_L );
				_dst = 0x7FFFFF;
			}
			else
				_dst = static_cast<int>(_src.var >> 24) & 0xffffff;
			assert( (_dst & 0xff000000) == 0 );
		}

		void limit_transfer( TReg24& _dst, const TReg56& _src )
		{
			limit_transfer( _dst.var, _src );
		}

		void limit_transfer( TWord& _dst, const TReg56& _src )
		{
			limit_transfer( reinterpret_cast<int&>(_dst), _src );
		}

		void limit_arithmeticSaturation( TReg56& _dst )
		{
			if( !sr_test_noCache(SR_SM) )
				return;

			const auto v = (bitvalue( _dst, 55 ).bit << 2) | (bitvalue( _dst, 48 ).bit << 1) | bitvalue(_dst,47).bit;

			switch( v )
			{
			case 0:
			case 7:	/* do nothing */								break;
			case 1:
			case 2:
			case 3:	_dst.var = 0x007fffffffffff;	sr_set(SR_V);	break;
			case 4:
			case 5:
			case 6: _dst.var = 0xff800000000000;	sr_set(SR_V);	break;
			default: assert( 0 && "impossible" );
			}
		}

		TReg8	ccr				() const							{ return byte0(getSR()); }
		TReg8	mr				() const							{ return byte1(reg.sr); }
		void	ccr				( TReg8 _val )						{ byte0(reg.sr,_val); resetCCRCache(); }
		void	mr				( TReg8 _val )						{ byte1(reg.sr,_val); }

		TReg8	com				() const							{ return byte0(reg.omr); }
		TReg8	eom				() const							{ return byte1(reg.omr); }
		void	com				( TReg8 _val )						{ byte0(reg.omr,_val); }
		void	eom				( TReg8 _val )						{ byte1(reg.omr,_val); }

		void	setA			( const TReg24& _src )				{ convert( reg.a, _src ); }
		void	setB			( const TReg24& _src )				{ convert( reg.b, _src ); }

		void	setA			( const TReg56& _src )				{ reg.a = _src; }
		void	setB			( const TReg56& _src )				{ reg.b = _src; }

		TWord 	moduloMask[8], modulo[8];
		void 	set_m			( const int which, const TWord val);

		
		// STACK
		void	decSP			();
		void	incSP			();
		TWord	ssIndex			() const							{ return reg.sp.var & 0xf; }
		void	ssIndex			(const TWord _index)				{ reg.sp.var = (reg.sp.var & ~0xf) | (_index & 0xf); }

		TReg24	ssh()				{ TReg24 res = hiword(reg.ss[ssIndex()]); decSP(); return res; }
		TReg24	ssl() const			{ return loword(reg.ss[ssIndex()]); }

		void	ssl(const TReg24& _val)	{ loword(reg.ss[ssIndex()],_val); }
		void	ssh(const TReg24& _val)	{ incSP(); hiword(reg.ss[ssIndex()],_val); }

		void	pushPCSR()			{ ssh(reg.pc); ssl(getSR()); }
		void	popPCSR()			{ setSR(ssl()); setPC(ssh()); }
		void	popPC()				{ setPC(ssh()); }

		// - ALU
		void	alu_and				( bool ab, TWord   _val );
		void	alu_or				( bool ab, TWord   _val );
		void	alu_add				( bool ab, const TReg56& _val );
		void	alu_cmp				( bool ab, const TReg56& _val, bool _magnitude );
		void	alu_sub				( bool ab, const TReg56& _val );
		void	alu_asr				( bool abDst, bool abSrc, int _shiftAmount );
		void	alu_asl				( bool abDst, bool abSrc, int _shiftAmount );

		void	alu_lsl				( bool ab, int _shiftAmount );
		void	alu_lsr				( bool ab, int _shiftAmount );

		void	alu_addl			(bool ab);
		void	alu_addr			(bool ab);

		void	alu_rol				(bool ab);

		void	alu_clr				(bool ab);
		
		TWord	alu_bclr			( TWord _bit, TWord _val );
		void	alu_mpy				( bool ab, const TReg24& _s1, const TReg24& _s2, bool _negate, bool _accumulate );
		void	alu_mpysuuu			( bool ab, TReg24 _s1, TReg24 _s2, bool _negate, bool _accumulate, bool _suuu );
		void	alu_dmac			( bool ab, TReg24 _s1, TReg24 _s2, bool _negate, bool srcUnsigned, bool dstUnsigned );
		void	alu_mac				( bool ab, TReg24 _s1, TReg24 _s2, bool _negate, bool _uu );

		void	alu_rnd				( bool _ab );

		void	alu_abs				( bool ab );

		void	alu_tfr				( bool ab, const TReg56& src);

		void	alu_tst				( bool ab );

		void	alu_neg				(bool ab);

		void	alu_not				(bool ab);

		// -- memory

		bool	memWrite			( EMemArea _area, TWord _offset, TWord _value );
		bool	memWritePeriph		( EMemArea _area, TWord _offset, TWord _value  );
		bool	memWritePeriphFFFF80( EMemArea _area, TWord _offset, TWord _value  );
		bool	memWritePeriphFFFFC0( EMemArea _area, TWord _offset, TWord _value  );

		TWord	memRead				( EMemArea _area, TWord _offset ) const;
		void	memReadOpcode		( TWord _offset, TWord& _wordA, TWord& _wordB ) const;
		TWord	memReadPeriph		( EMemArea _area, TWord _offset ) const;
		TWord	memReadPeriphFFFF80	( EMemArea _area, TWord _offset ) const;
		TWord	memReadPeriphFFFFC0	( EMemArea _area, TWord _offset ) const;

		void	aarTranslate		( EMemArea _area, TWord& _offset ) const;

		// --- operations
	public:
		void op_Abs(TWord op);
		template<TWord ab> void opCE_Abs(TWord op);
		void op_ADC(TWord op);
		void op_Add_SD(TWord op);
		void op_Add_xx(TWord op);
		void op_Add_xxxx(TWord op);
		void op_Addl(TWord op);
		void op_Addr(TWord op);
		template<TWord D, TWord JJ> void opCE_And_SD(TWord op);
		void op_And_SD(TWord op);
		void op_And_xx(TWord op);
		void op_And_xxxx(TWord op);
		void op_Andi(TWord op);
		void op_Asl_D(TWord op);
		template<TWord ab> void opCE_Asl_D(TWord op);
		void op_Asl_ii(TWord op);
		void op_Asl_S1S2D(TWord op);
		void op_Asr_D(TWord op);
		void op_Asr_ii(TWord op);
		void op_Asr_S1S2D(TWord op);
		void op_Bcc_xxxx(TWord op);
		void op_Bcc_xxx(TWord op);
		void op_Bcc_Rn(TWord op);
		void op_Bchg_ea(TWord op);
		void op_Bchg_aa(TWord op);
		void op_Bchg_pp(TWord op);
		void op_Bchg_qq(TWord op);
		void op_Bchg_D(TWord op);
		void op_Bclr_ea(TWord op);
		void op_Bclr_aa(TWord op);
		void op_Bclr_pp(TWord op);
		void op_Bclr_qq(TWord op);
		void op_Bclr_D(TWord op);
		void op_Bra_xxxx(TWord op);
		void op_Bra_xxx(TWord op);
		void op_Bra_Rn(TWord op);
		void op_Brclr_ea(TWord op);
		void op_Brclr_aa(TWord op);
		void op_Brclr_pp(TWord op);
		void op_Brclr_qq(TWord op);
		void op_Brclr_S(TWord op);
		void op_BRKcc(TWord op);
		void op_Brset_ea(TWord op);
		void op_Brset_aa(TWord op);
		void op_Brset_pp(TWord op);
		void op_Brset_qq(TWord op);
		void op_Brset_S(TWord op);
		void op_BScc_xxxx(TWord op);
		void op_BScc_xxx(TWord op);
		void op_BScc_Rn(TWord op);
		void op_Bsclr_ea(TWord op);
		void op_Bsclr_aa(TWord op);
		void op_Bsclr_pp(TWord op);
		void op_Bsclr_qq(TWord op);
		void op_Bsclr_S(TWord op);
		void op_Bset_ea(TWord op);
		void op_Bset_aa(TWord op);
		void op_Bset_pp(TWord op);
		void op_Bset_qq(TWord op);
		void op_Bset_D(TWord op);
		void op_Bsr_xxxx(TWord op);
		void op_Bsr_xxx(TWord op);
		void op_Bsr_Rn(TWord op);
		void op_Bsset_ea(TWord op);
		void op_Bsset_aa(TWord op);
		void op_Bsset_pp(TWord op);
		void op_Bsset_qq(TWord op);
		void op_Bsset_S(TWord op);
		void op_Btst_ea(TWord op);
		void op_Btst_aa(TWord op);
		void op_Btst_pp(TWord op);
		void op_Btst_qq(TWord op);
		void op_Btst_D(TWord op);
		void op_Clb(TWord op);
		void op_Clr(TWord op);
		void op_Cmp_S1S2(TWord op);
		void op_Cmp_xxS2(TWord op);
		void op_Cmp_xxxxS2(TWord op);
		void op_Cmpm_S1S2(TWord op);
		void op_Cmpu_S1S2(TWord op);
		void op_Debug(TWord op);
		void op_Debugcc(TWord op);
		void op_Dec(TWord op);
		void op_Div(TWord op);
		void op_Dmac(TWord op);
		void op_Do_ea(TWord op);
		void op_Do_aa(TWord op);
		void op_Do_xxx(TWord op);
		void op_Do_S(TWord op);
		void op_DoForever(TWord op);
		void op_Dor_ea(TWord op);
		void op_Dor_aa(TWord op);
		void op_Dor_xxx(TWord op);
		void op_Dor_S(TWord op);
		void op_DorForever(TWord op);
		void op_Enddo(TWord op);
		void op_Eor_SD(TWord op);
		void op_Eor_xx(TWord op);
		void op_Eor_xxxx(TWord op);
		void op_Extract_S1S2(TWord op);
		void op_Extract_CoS2(TWord op);
		void op_Extractu_S1S2(TWord op);
		void op_Extractu_CoS2(TWord op);
		void op_Ifcc(TWord op);
		void op_Ifcc_U(TWord op);
		void op_Illegal(TWord op);
		void op_Inc(TWord op);
		void op_Insert_S1S2(TWord op);
		void op_Insert_CoS2(TWord op);
		void op_Jcc_xxx(TWord op);
		void op_Jcc_ea(TWord op);
		void op_Jclr_ea(TWord op);
		void op_Jclr_aa(TWord op);
		void op_Jclr_pp(TWord op);
		void op_Jclr_qq(TWord op);
		void op_Jclr_S(TWord op);
		void op_Jmp_ea(TWord op);
		void op_Jmp_xxx(TWord op);
		void op_Jscc_xxx(TWord op);
		void op_Jscc_ea(TWord op);
		void op_Jsclr_ea(TWord op);
		void op_Jsclr_aa(TWord op);
		void op_Jsclr_pp(TWord op);
		void op_Jsclr_qq(TWord op);
		void op_Jsclr_S(TWord op);
		void op_Jset_ea(TWord op);
		void op_Jset_aa(TWord op);
		void op_Jset_pp(TWord op);
		void op_Jset_qq(TWord op);
		void op_Jset_S(TWord op);
		void op_Jsr_ea(TWord op);
		void op_Jsr_xxx(TWord op);
		void op_Jsset_ea(TWord op);
		void op_Jsset_aa(TWord op);
		void op_Jsset_pp(TWord op);
		void op_Jsset_qq(TWord op);
		void op_Jsset_S(TWord op);
		void op_Lra_Rn(TWord op);
		void op_Lra_xxxx(TWord op);
		void op_Lsl_D(TWord op);
		void op_Lsl_ii(TWord op);
		void op_Lsl_SD(TWord op);
		void op_Lsr_D(TWord op);
		void op_Lsr_ii(TWord op);
		void op_Lsr_SD(TWord op);
		void op_Lua_ea(TWord op);
		void op_Lua_Rn(TWord op);
		void op_Mac_S1S2(TWord op);
		void op_Mac_S(TWord op);
		void op_Maci_xxxx(TWord op);
		void op_Macsu(TWord op);
		void op_Macr_S1S2(TWord op);
		void op_Macr_S(TWord op);
		void op_Macri_xxxx(TWord op);
		void op_Max(TWord op);
		void op_Maxm(TWord op);
		void op_Merge(TWord op);
		void op_Move_Nop(TWord op);
		void op_Move_xx(TWord op);
		void op_Mover(TWord op);
		void op_Move_ea(TWord op);
		template<TWord W, TWord MMM> void opCE_Movex_ea(TWord op);
		template<TWord W> void opCE_Movex_aa(TWord op);
		void op_Movex_Rnxxxx(TWord op);
		void op_Movex_Rnxxx(TWord op);
		void op_Movexr_ea(TWord op);
		void op_Movexr_A(TWord op);
		template<TWord W, TWord MMM> void opCE_Movey_ea(TWord op);
		template<TWord W> void opCE_Movey_aa(TWord op);
		void op_Movey_Rnxxxx(TWord op);
		void op_Movey_Rnxxx(TWord op);
		void op_Moveyr_ea(TWord op);
		void op_Moveyr_A(TWord op);
		void op_Movel_ea(TWord op);
		void op_Movel_aa(TWord op);
		void op_Movexy(TWord op);
		template<TWord W, TWord w, TWord ee, TWord ff>
		void opCE_Movexy(TWord op);
		void op_Movec_ea(TWord op);
		void op_Movec_aa(TWord op);
		void op_Movec_S1D2(TWord op);
		void op_Movec_xx(TWord op);
		void op_Movem_ea(TWord op);
		void op_Movem_aa(TWord op);
		void op_Movep_ppea(TWord op);
		void op_Movep_Xqqea(TWord op);
		void op_Movep_Yqqea(TWord op);
		void op_Movep_eapp(TWord op);
		void op_Movep_eaqq(TWord op);
		void op_Movep_Spp(TWord op);
		void op_Movep_SXqq(TWord op);
		void op_Movep_SYqq(TWord op);
		void op_Mpy_S1S2D(TWord op);
		void op_Mpy_SD(TWord op);
		void op_Mpy_su(TWord op);
		void op_Mpyi(TWord op);
		void op_Mpyr_S1S2D(TWord op);
		void op_Mpyr_SD(TWord op);
		void op_Mpyri(TWord op);
		void op_Neg(TWord op);
		void op_Nop(TWord op);
		void op_Norm(TWord op);
		void op_Normf(TWord op);
		void op_Not(TWord op);
		void op_Or_SD(TWord op);
		void op_Or_xx(TWord op);
		void op_Or_xxxx(TWord op);
		void op_Ori(TWord op);
		void op_Pflush(TWord op);
		void op_Pflushun(TWord op);
		void op_Pfree(TWord op);
		void cachePlock(TWord _effectiveAddress);
		void op_Plock(TWord op);
		void op_Plockr(TWord op);
		void op_Punlock(TWord op);
		void op_Punlockr(TWord op);
		void op_Rep_ea(TWord op);
		void op_Rep_aa(TWord op);
		void op_Rep_xxx(TWord op);
		void op_Rep_S(TWord op);
		void op_Reset(TWord op);
		void op_Rnd(TWord op);
		void op_Rol(TWord op);
		void op_Ror(TWord op);
		void op_Rti(TWord op);
		void op_Rts(TWord op);
		void op_Sbc(TWord op);
		void op_Stop(TWord op);
		void op_Sub_SD(TWord op);
		void op_Sub_xx(TWord op);
		void op_Sub_xxxx(TWord op);
		void op_Subl(TWord op);
		void op_subr(TWord op);
		void op_Tcc_S1D1(TWord op);
		void op_Tcc_S1D1S2D2(TWord op);
		void op_Tcc_S2D2(TWord op);
		void op_Tfr(TWord op);
		void op_Trap(TWord op);
		void op_Trapcc(TWord op);
		void op_Tst(TWord op);
		void op_Vsl(TWord op);
		void op_Wait(TWord op);
		void op_ResolveCache(TWord op);
		void op_Parallel(TWord op);

		// ------------- function permutations -------------
		static TInstructionFunc resolvePermutation(Instruction _inst, TWord _op);

		// ------------- operation helper methods -------------

		// Check Condition
		template <Instruction I> int checkCondition(TWord op) const;

		// Effective Address
		template<Instruction Inst, typename std::enable_if<hasFields<Inst,Field_MMM, Field_RRR>()>::type* = nullptr>
		TWord effectiveAddress(TWord op);

		template<Instruction Inst, typename std::enable_if<hasField<Inst,Field_aaaaaaaaaaaa>()>::type* = nullptr>
		TWord effectiveAddress(TWord op) const;

		template<Instruction Inst, typename std::enable_if<!hasAnyField<Inst, Field_a, Field_RRR>() && hasField<Inst,Field_aaaaaa>()>::type* = nullptr>
		TWord effectiveAddress(TWord op) const;

		template<Instruction Inst, typename std::enable_if<hasFields<Inst,Field_aaaaaa, Field_a, Field_RRR>()>::type* = nullptr>
		TWord effectiveAddress(TWord op) const;

		// Relative Address Offset
		template <Instruction Inst, typename std::enable_if<hasFields<Inst, Field_aaaa, Field_aaaaa>()>::type* = nullptr>
		int relativeAddressOffset(TWord op) const;
		template <Instruction Inst, typename std::enable_if<hasField<Inst, Field_RRR>()>::type* = nullptr> 
		int relativeAddressOffset(TWord op) const;

		// Memory Read
		template <Instruction Inst, typename std::enable_if<!hasField<Inst,Field_s>() && hasFields<Inst, Field_MMM, Field_RRR, Field_S>()>::type* = nullptr>
		TWord readMem(TWord op);

		template <Instruction Inst, typename std::enable_if<!hasFields<Inst,Field_s, Field_S>() && hasFields<Inst, Field_MMM, Field_RRR>()>::type* = nullptr>
		TWord readMem(TWord op, EMemArea area);

		template <Instruction Inst, TWord MMM, typename std::enable_if<!hasFields<Inst,Field_s, Field_S>() && hasFields<Inst, Field_MMM, Field_RRR>()>::type* = nullptr>
		TWord readMem(TWord op, EMemArea area);

		template <Instruction Inst, typename std::enable_if<hasField<Inst, Field_aaaaaaaaaaaa>()>::type* = nullptr>
		TWord readMem(TWord op, EMemArea area) const;

		template <Instruction Inst, typename std::enable_if<hasField<Inst, Field_aaaaaa>()>::type* = nullptr>
		TWord readMem(TWord op, EMemArea area) const;

		template <Instruction Inst, typename std::enable_if<hasFields<Inst, Field_aaaaaa, Field_S>()>::type* = nullptr>
		TWord readMem(TWord op) const;

		template <Instruction Inst, typename std::enable_if<!hasAnyField<Inst, Field_MMM, Field_RRR>() && hasFields<Inst, Field_qqqqqq, Field_S>()>::type* = nullptr> TWord readMem(TWord op) const;
		template <Instruction Inst, typename std::enable_if<!hasAnyField<Inst, Field_MMM, Field_RRR>() && hasFields<Inst, Field_pppppp, Field_S>()>::type* = nullptr> TWord readMem(TWord op) const;

		// Memory Write
		template <Instruction Inst, typename std::enable_if<hasFields<Inst, Field_MMM, Field_RRR, Field_S>()>::type* = nullptr>
		void writeMem(TWord op, TWord value);

		template <Instruction Inst, typename std::enable_if<hasFields<Inst, Field_MMM, Field_RRR>()>::type* = nullptr>
		void writeMem(TWord op, EMemArea area, TWord value);

		template <Instruction Inst, TWord MMM, typename std::enable_if<hasFields<Inst, Field_MMM, Field_RRR>()>::type* = nullptr>
		void writeMem(TWord op, EMemArea area, TWord value);

		template <Instruction Inst, typename std::enable_if<hasField<Inst, Field_aaaaaaaaaaaa>()>::type* = nullptr>
		void writeMem(TWord op, EMemArea area, TWord value);

		template <Instruction Inst, typename std::enable_if<hasField<Inst, Field_aaaaaa>()>::type* = nullptr>
		void writeMem(TWord op, EMemArea area, TWord value);

		template <Instruction Inst, typename std::enable_if<hasFields<Inst, Field_aaaaaa, Field_S>()>::type* = nullptr>
		void writeMem(TWord op, TWord value);

		template <Instruction Inst, typename std::enable_if<!hasAnyField<Inst, Field_MMM, Field_RRR>() && hasFields<Inst, Field_qqqqqq, Field_S>()>::type* = nullptr> void writeMem(TWord op, TWord value);
		template <Instruction Inst, typename std::enable_if<!hasAnyField<Inst, Field_MMM, Field_RRR>() && hasFields<Inst, Field_pppppp, Field_S>()>::type* = nullptr> void writeMem(TWord op, TWord value);

		// bit manipulation
		template <Instruction Inst> bool bitTest(TWord op, TWord toBeTested)
		{
			const auto bit = getBit<Inst>(op);
			return dsp56k::bittest<TWord>(toBeTested, bit);
		}
		template <Instruction Inst, typename std::enable_if<hasFields<Inst, Field_bbbbb, Field_S>()>::type* = nullptr> bool bitTestMemory(TWord op);

		// extension word access
		template<Instruction Inst> TWord absAddressExt()
		{
			static_assert(g_opcodes[Inst].m_extensionWordType & AbsoluteAddressExt, "opcode does not have an absolute address extension word");
			return fetchOpWordB();
		}

		template<Instruction Inst> TWord immediateDataExt()
		{
			// TODO: it would be better to check for extension word type & ImmediateDataExt but as there is code that is not compiled at compile time, we can't do that yet
			static_assert(g_opcodes[Inst].m_extensionWordType, "opcode does not have an immediate data extension word");
			return fetchOpWordB();
		}

		template<Instruction Inst> int pcRelativeAddressExt()
		{
			static_assert(g_opcodes[Inst].m_extensionWordType & PCRelativeAddressExt, "opcode does not have a PC-relative address extension word");
			return signextend<int,24>(fetchOpWordB());
		}

		// -------------- bra variants
		template<BraMode Bmode> void braOrBsr(int offset);

		template<Instruction Inst, BraMode Bmode> void braIfCC(TWord op);

		template<Instruction Inst, BraMode Bmode> void braIfCC(TWord op, int offset);

		template<Instruction Inst, BraMode Bmode, ExpectedBitValue BitValue> void braIfBitTestMem(TWord op);
		template<Instruction Inst, BraMode Bmode, ExpectedBitValue BitValue> void braIfBitTestDDDDDD(TWord op);

		// -------------- jmp variants
		template<JumpMode Jsr> void jumpOrJSR(TWord ea);

		template<Instruction Inst, JumpMode Jsr>
		void jumpIfCC(TWord op);

		template<Instruction Inst, JumpMode Jsr>
		void jumpIfCC(TWord op, TWord ea);
		
		template<Instruction Inst, JumpMode Jsr, ExpectedBitValue BitValue> void jumpIfBitTestMem(TWord op);
		template<Instruction Inst, JumpMode Jsr, ExpectedBitValue BitValue> void jumpIfBitTestDDDDDD(TWord op);

		// -------------- move helper
		template<Instruction Inst, EMemArea Area, TWord W> void move_ddddd_absAddr(TWord _op);
		template<Instruction Inst, EMemArea Area, TWord W, TWord MMM> void move_ddddd_MMMRRR(TWord op);
		template<Instruction Inst> void move_L(TWord op);
		template<Instruction Inst, EMemArea Area> void move_Rnxxxx(TWord op);
		
		// --- debugging tools
	private:
		void errNotImplemented(const char* _opName);
		void updatePreviousRegisterStates();
	public:
		void coreDump(std::stringstream& _dst);
		void coreDump();
	};
}
