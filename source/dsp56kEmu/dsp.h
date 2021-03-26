#pragma once

#include "registers.h"
#include "memory.h"
#include "utils.h"
#include "error.h"
#include "essi.h"
#include "instructioncache.h"

namespace dsp56k
{
	class Memory;

	class DSP
	{
		// _____________________________________________________________________________
		// types
		//
	public:
		struct SRegs
		{
			// accumulator
			TReg48 x,y;		// 48 bit
			TReg56 a,b;		// 56 bit

			// ---- PCU ----
			TReg24 omr;						// operation mode register
			TReg24 sr;						// status register (SR_..)

			StaticArray< TReg48, 16> ss;	// system stack

			//TReg24	ssh, ssl;			// system stack high, system stack low
			TReg24	sp;						// stack pointer
			TReg5	sc;						// stack counter

			TReg24	sz;						// stack size (used for stack extension)

			TReg24 pc;						// program counter
			TReg24 la, lc;					// loop address, loop counter

			TReg24 vba;						// vector base address

			TReg24 iprc, iprp, bcr, dcr;

			TReg24 aar0, aar1, aar2, aar3;

			// ---- AGU ----
			StaticArray< TReg24, 8> r;
			StaticArray< TReg24, 8> n;
			StaticArray< TReg24, 8> m;

			TReg24 ep;						// stack extension pointer register

			// ---- ----
			TReg24 hit;		
			TReg24 miss;		
			TReg24 replace;	
			TReg24 cyc;		
			TReg24 ictr;		
			TReg24 cnt1;		
			TReg24 cnt2;		
			TReg24 cnt3;		
			TReg24 cnt4;
		};

		// _____________________________________________________________________________
		// registers
		//
	private:
		SRegs reg;

		bool		repRunning;

		TReg24		tempLCforRep;	// used as backup storage if a rep instruction happens inside a loop

		// _____________________________________________________________________________
		// members
		//
		Memory& mem;
		TWord	pcLastExec;
		
		Essi	essi;

		char	m_asm[128];

		InstructionCache	cache;

		// _____________________________________________________________________________
		// implementation
		//
	public:
				DSP								( Memory& _memory );

		void 	resetHW							();
		void 	resetSW							();

		void	jsr								( TReg24 _val )								{ pushPCSR(); setPC(_val); }
		void	jsr								( TWord _val )								{ jsr(TReg24(_val)); }

		void 	setPC							( TReg24 _val )								{ reg.pc = _val; }

		TReg24	getPC							() const									{ return reg.pc; }

		void 	exec							();

		bool	readReg							( EReg _reg, TReg8& _res ) const;
		bool	readReg							( EReg _reg, TReg48& _res ) const;
		bool	readReg							( EReg _reg, TReg5& _res ) const;

		bool	readReg							( EReg _reg, TReg24& _res ) const;
		bool	writeReg						( EReg _reg, const TReg24& _val );

		bool	readReg							( EReg _reg, TReg56& _res ) const;
		bool	writeReg						( EReg _reg, const TReg56& _val );

		const char*	getASM						();

		void	execUntilRTS					()
		{
			TReg5 oldSC = reg.sc;
			while( reg.sc.var >= oldSC.var )
				exec();
		}

		const SRegs&	readRegs						() const		{ return reg; }

		void			readDebugRegs					( dsp56k::SRegs& _dst ) const;

		void			dumpCCCC						() const;

		void			logSC							( const char* _func );

		bool			save							( FILE* _file ) const;
		bool			load							( FILE* _file );

	private:

		// -- execution 
		TWord	fetchPC							()
		{
			TWord ret;

			if( sr_test(SR_CE) )
				ret = cache.fetch( mem, reg.pc.toWord(), (reg.omr.var & OMR_BE) != 0 );
			else
				ret = memRead( MemArea_P, reg.pc.toWord() );

			// REP
			if( repRunning )
			{
				if( reg.lc.var > 1 )
				{
					--reg.lc.var;
					return ret;
				}
				else
				{
					reg.lc = tempLCforRep;
					repRunning = false;
				}
			}
			
			// DO
			if( sr_test(SR_LF) && reg.pc == reg.la )
			{
				if( reg.lc.var > 1 )
				{
					--reg.lc.var;
					reg.pc = hiword(reg.ss[reg.sc.toWord()]);
				}
				else
					exec_do_end();
			}
			else
				++reg.pc.var;

			return ret;
		}

		bool 	exec_pcu						( TWord dbmfop, TWord dbmf, TWord op );

		bool 	exec_parallel_move				( TWord dbmfop, TWord dbmf, TWord op );
		bool 	exec_move						( TWord dbmfop, TWord dbmf, TWord op );

		bool 	exec_arithmetic_parallel		( TWord dbmfop, TWord dbmf, TWord op );

		bool 	exec_logical_parallel			( TWord dbmfop, TWord dbmf, TWord op );

		bool 	exec_logical_nonparallel		( TWord dbmfop, TWord dbmf, TWord op );

		bool 	exec_operand_8bits				( TWord dbmfop, TWord dbmf, TWord op );

		bool 	exec_bitmanip					( TWord dbmfop, TWord dbmf, TWord op );

		bool	exec_do							( TReg24 _loopcount, TWord _addr );
		bool	exec_do_end						();

		// -- execution helpers

		void exec_move_ddddd_MMMRRR			( TWord ddddd, TWord mmmrrr, bool write, EMemArea memArea );

		// -- decoding helper functions

		TWord	decode_MMMRRR_read		( TWord _mmmrrr );
		TWord	decode_XMove_MMRRR		( TWord _mmrrr );
		TWord	decode_YMove_mmrr		( TWord _mmrr, TWord _regIdxOffset );

		TWord	decode_RRR_read			( TWord _mmmrrr, int _shortDisplacement );

		template<typename T> T decode_ddddd_read( TWord _ddddd )
		{
			T res;

			switch( _ddddd )
			{
			case 0x04:	convert( res, x0() ); 	return res;	// x0
			case 0x05:	convert( res, x1() ); 	return res;	// x1	
			case 0x06:	convert( res, y0() ); 	return res;	// y0
			case 0x07:	convert( res, y1() ); 	return res;	// y1
			case 0x08:	convert( res, a0() ); 	return res;	// a0
			case 0x09:	convert( res, b0() ); 	return res;	// b0
			case 0x0a:	convert( res, a2() ); 	return res;	// a2
			case 0x0b:	convert( res, b2() ); 	return res;	// b2
			case 0x0c:	convert( res, a1() ); 	return res;	// a1
			case 0x0d:	convert( res, b1() ); 	return res;	// b1
			case 0x0e:	return getA<T>();		return res;	// a
			case 0x0f:	return getB<T>();		return res;	// b
			}
			if( (_ddddd & 0x18) == 0x10 )				// r0-r7
				convert( res, reg.r[_ddddd&0x07] );
			else if( (_ddddd & 0x18) == 0x18 )				// n0-n7
				convert( res, reg.n[_ddddd&0x07] );
			else
			{
				assert( 0 && "invalid ddddd value" );
				return T(0xbadbad);
			}
			return res;
		}
		template<typename T> T decode_sss_read( TWord _sss ) const
		{
			T res;

			switch( _sss )
			{
			case 2:		convert( res, a1() );				break;
			case 3:		convert( res, b1() );				break;
			case 4:		convert( res, x0() );				break;
			case 5:		convert( res, y0() );				break;
			case 6:		convert( res, x1() );				break;
			case 7:		convert( res, y1() );				break;
			case 0:
			case 1:
			default:	assert( 0 && "invalid sss value" ); return 0xbadbad;
			}
			return res;
		}

		template<typename T> void	decode_ddddd_write		( TWord _ddddd, const T& _val )
		{
			switch( _ddddd )
			{
				case 0x04:	x0(_val);			return;	// x0
				case 0x05:	x1(_val);			return;	// x1	
				case 0x06:	y0(_val);			return;	// y0
				case 0x07:	y1(_val);			return;	// y1
				case 0x08:	{ TReg24 temp; convert(temp,_val); a0(temp); }	return;	// a0
				case 0x09:	{ TReg24 temp; convert(temp,_val); b0(temp); }	return;	// b0
				case 0x0a:	{ TReg8  temp; convert(temp,_val); a2(temp); }	return;	// a2
				case 0x0b:	{ TReg8  temp; convert(temp,_val); b2(temp); }	return;	// b2
				case 0x0c:	{ TReg24 temp; convert(temp,_val); a1(temp); }	return;	// a1
				case 0x0d:	{ TReg24 temp; convert(temp,_val); b1(temp); }	return;	// b1
				case 0x0e:	convert(reg.a,_val);							return;	// a
				case 0x0f:	convert(reg.b,_val);							return;	// b
			}

			if( (_ddddd & 0x18) == 0x10 )									// r0-r7
			{
				convert(reg.r[_ddddd&0x07],_val);
				return;
			}
			else if( (_ddddd & 0x18) == 0x18 )								// n0-n7
			{
				convert(reg.n[_ddddd&0x07],_val);
				return;
			}
			assert( 0 && "unreachable" );
		}

		TReg24	decode_ddddd_pcr_read 	( TWord _ddddd );
		void	decode_ddddd_pcr_write	( TWord _ddddd, TReg24 _val );

		// Six-Bit Encoding for all on-chip registers
		TReg24	decode_dddddd_read		(TWord _dddddd);
		void	decode_dddddd_write		(TWord _dddddd, TReg24 _val);

		TReg24	decode_JJ_read			( TWord jj )
		{
			switch( jj )
			{
			case 0:	return x0();
			case 1: return y0();
			case 2:	return x1();
			case 3: return y1();
			}
			assert( 0 && "unreachable, invalid JJ value" );
			return TReg24(0xbadbad);
		}

		TReg56 decode_JJJ_read_56( TWord jjj, bool _b ) const
		{
			TReg56 res;
			switch( jjj )
			{
			case 0:
			case 1:	res = _b ? reg.b : reg.a;	break;
			case 2:	convert( res, reg.x );		break;
			case 3:	convert( res, reg.y );		break;
			case 4: convert( res, x0() );		break;
			case 5: convert( res, y0() );		break;
			case 6: convert( res, x1() );		break;
			case 7: convert( res, y1() );		break;
			default:
				assert( 0 && "unreachable, invalid JJJ value" );
				return TReg56(0xbadbadbadbadbadb);
			}
			return res;
		}

		TReg24 decode_JJJ_read_24( TWord jjj, bool _b ) const
		{
			switch( jjj )
			{
			case 4: return x0();
			case 5: return y0();
			case 6: return x1();
			case 7: return y1();
			default:
				assert( 0 && "unreachable, invalid JJJ value" );
				return TReg24(0xbadbad);
			}
		}

		void decode_JJJ_readwrite( TReg56& alu, TWord jjj, bool _b )
		{
			switch( jjj )
			{
			case 0:
			case 1:	alu = _b ? reg.b : reg.a;	return;
			case 2:	convert(alu,reg.x);		return;
			case 3:	convert(alu,reg.y);		return;
			case 4: convert(alu,x0());	return;
			case 5: convert(alu,y0());	return;
			case 6: convert(alu,x1());	return;
			case 7: convert(alu,y1());	return;
			default:
				assert( 0 && "unreachable, invalid JJJ value" );
			}
		}

		TReg8	decode_EE_read( TWord _ee ) const
		{
			switch( _ee )
			{
			case 0:	return mr();
			case 1:	return ccr();
			case 2:	return com();
			case 3:	return eom();
			}
			assert( 0 && "invalid EE value" );
			return TReg8((char)0xee);
		}

		void	decode_EE_write( TWord _ee, TReg8 _val )
		{
			switch( _ee )
			{
			case 0:	mr(_val);	return;
			case 1:	ccr(_val);	return;
			case 2:	com(_val);	return;
			case 3:	eom(_val);	return;
			}
			assert( 0 && "invalid EE value" );
		}

		void decode_QQQQ_read( TReg24& _s1, TReg24& _s2, TWord _qqqq ) const
		{
			switch( _qqqq )
			{
			case 0:		_s1 = x0();		_s2 = x0();		return;
			case 1:		_s1 = y0();		_s2 = y0();		return;
			case 2:		_s1 = x1();		_s2 = x0();		return;
			case 3:		_s1 = y1();		_s2 = y0();		return;

			case 4:		_s1 = x0();		_s2 = y1();		return;
			case 5:		_s1 = y0();		_s2 = x0();		return;
			case 6:		_s1 = x1();		_s2 = y0();		return;
			case 7:		_s1 = y1();		_s2 = x1();		return;

			case 8:		_s1 = x1();		_s2 = x1();		return;
			case 9:		_s1 = y1();		_s2 = y1();		return;
			case 10:	_s1 = x0();		_s2 = x1();		return;
			case 11:	_s1 = y0();		_s2 = y1();		return;

			case 12:	_s1 = y1();		_s2 = x0();		return;
			case 13:	_s1 = x0();		_s2 = y0();		return;
			case 14:	_s1 = y0();		_s2 = x1();		return;
			case 15:	_s1 = y1();		_s2 = y1();		return;
			}
			assert( 0 && "invalid QQQQ value" );
		}

		void decode_QQQ_read( TReg24& _s1, TReg24& _s2, TWord _qqq ) const
		{
			switch( _qqq )
			{
			case 0:		_s1 = x0();		_s2 = x0();		return;
			case 1:		_s1 = y0();		_s2 = y0();		return;
			case 2:		_s1 = x1();		_s2 = x0();		return;
			case 3:		_s1 = y1();		_s2 = y0();		return;
			case 4:		_s1 = x0();		_s2 = y1();		return;
			case 5:		_s1 = y0();		_s2 = x0();		return;
			case 6:		_s1 = x1();		_s2 = y0();		return;
			case 7:		_s1 = y1();		_s2 = x1();		return;
			}
			assert( 0 && "invalid QQQ value" );
		}

		TReg24 decode_QQ_read( TWord _qq ) const
		{
			switch( _qq )
			{
			case 0:		return y1();
			case 1:		return x0();
			case 2:		return y0();
			case 3:		return x1();
			}
			assert( 0 && "invalid QQ value" );
			return TReg24(int(0xbadbad));
		}

		TReg24 decode_qq_read( TWord _qq ) const
		{
			switch( _qq )
			{
			case 0:		return x0();
			case 1:		return y0();
			case 2:		return x1();
			case 3:		return y1();
			}
			assert( 0 && "invalid qq value" );
			return TReg24(int(0xbadbad));
		}

		bool	decode_cccc				( TWord cccc ) const;

		TReg24	decode_ff_read( TWord _ff )
		{
			switch( _ff )
			{
			case 0:	return y0();
			case 1:	return y1();
			case 2:	return getA<TReg24>();
			case 3:	return getB<TReg24>();
			}
			assert( 0 && "invalid ff value" );
			return TReg24(int(0xbadbad));
		}

		void decode_ff_write( TWord _ff, TReg24 _value )
		{
			switch( _ff )
			{
			case 0:	y0(_value);		return;
			case 1:	y1(_value);		return;
			case 2:	setA(_value);	return;
			case 3:	setB(_value);	return;
			}
			assert( 0 && "invalid ff value" );
		}

		TReg24	decode_ee_read( TWord _ff )
		{
			switch( _ff )
			{
			case 0:	return x0();
			case 1:	return x1();
			case 2:	return getA<TReg24>();
			case 3:	return getB<TReg24>();
			}
			assert( 0 && "invalid ff value" );
			return TReg24(int(0xbadbad));
		}

		void decode_ee_write( TWord _ff, TReg24 _value )
		{
			switch( _ff )
			{
			case 0:	x0(_value);		return;
			case 1:	x1(_value);		return;
			case 2:	setA(_value);	return;
			case 3:	setB(_value);	return;
			}
			assert( 0 && "invalid ff value" );
		}

		void decode_LLL_read( TWord _lll, TWord& x, TWord& y )
		{
			switch( _lll )
			{
			case 0:		convert(x,a1()); 		convert(y,a0());		return;
			case 1:		convert(x,b1()); 		convert(y,b0());		return;
			case 2:		convert(x,x1()); 		convert(y,x0());		return;
			case 3:		convert(x,y1()); 		convert(y,y0());		return;
			case 4:		convert(x,a1()); 		convert(y,a0());		return;
			case 5:		convert(x,b1()); 		convert(y,b0());		return;
			case 6:		x = getA<TWord>();		y = getB<TWord>();		return;
			case 7:		x = getB<TWord>();		y = getA<TWord>();		return;
			}
			assert( 0 && "invalid LLL value" );
		}

		void decode_LLL_write( TWord _lll, TReg24 x, TReg24 y )
		{
			const TReg24 s1 = x;
			const TReg24 s2 = y;

			switch( _lll )
			{
			case 0:		a1  (s1); a0  (s2);	return;
			case 1:		b1  (s1); b0  (s2);	return;
			case 2:		x1  (s1); x0  (s2);	return;
			case 3:		y1  (s1); y0  (s2);	return;
			case 4:		a1  (s1); a0  (s2);	return;
			case 5:		b1  (s1); b0  (s2);	return;
			case 6:		setA(s1); setB(s2);	return;
			case 7:		setB(s1); setA(s2);	return;
			}
			assert( 0 && "invalid LLL value" );
		}

		TWord decode_sssss( TWord _sssss )
		{
			return (0x800000>>(_sssss));
		}

		// -- status register management

		void 	sr_set					( TWord _bits )						{ reg.sr.var |= _bits;	}
		void 	sr_clear				( TWord _bits )						{ reg.sr.var &= ~_bits; }
		bool 	sr_test					( TWord _bits ) const				{ return (reg.sr.var & _bits) != 0; }
		void 	sr_toggle				( TWord _bits, bool _set )			{ if( _set ) { sr_set(_bits); } else { sr_clear(_bits); } }

		void	sr_s_update				()
		{
			if( sr_test(SR_S) )
				return;

			unsigned int bitA = 46;
			unsigned int bitB = 45;

			if( sr_test(SR_S0) )
			{
				// scale down
				--bitA;
				--bitB;
			}
			else if( sr_test(SR_S1) )
			{
				// scale up
				++bitA;
				++bitB;
			}

			if	(	(bittest(reg.a,bitA) != bittest(reg.a,bitB))
				||	(bittest(reg.b,bitA) != bittest(reg.b,bitB)) )
				sr_set( SR_S );
		}

		void	sr_e_update				( const TReg56& _ab )
		{
			TInt64 mask;

			TInt64 m;

			if( sr_test(SR_S0) )		mask = 0xff000000000000;
			else if( sr_test(SR_S1) )	mask = 0xffc00000000000;
			else						mask = 0xff800000000000;

			m = _ab.var & mask;

			sr_toggle( SR_E, (m != mask) && (m != 0) );
		}

		void	sr_u_update				( const TReg56& _ab )
		{
			if( sr_test( SR_S0 ) )			sr_toggle( SR_U, bittest( _ab, 48 ) != bittest( _ab, 47 ) );
			else if( sr_test( SR_S1 ) )		sr_toggle( SR_U, bittest( _ab, 46 ) != bittest( _ab, 45 ) );
			else							sr_toggle( SR_U, bittest( _ab, 47 ) != bittest( _ab, 46 ) );
		}

		void	sr_n_update_arithmetic( const TReg56& _ab )
		{
			sr_toggle( SR_N, bittest( _ab, 55 ) );
		}

		void	sr_n_update_logical( const TReg56& _ab )
		{
			sr_toggle( SR_N, bittest( _ab, 47 ) );
		}

		void	sr_z_update( const TReg56& _ab )
		{
			sr_toggle( SR_Z, TReg56( TReg56::MyType(0) ) == _ab );
		}

 		void	sr_c_update_logical( const TReg56& _ab )
 		{
 			sr_toggle( SR_C, bittest(_ab,55) );
 		}

		void	sr_c_update_arithmetic( const TReg56& _old, const TReg56& _new )
		{
			sr_toggle( SR_C, bittest(_old,55) != bittest(_new,55) );
		}

		void	sr_l_update_by_v()
		{
			// L is never cleared automatically, so only test to set
			if( sr_test(SR_V) )
				sr_set( SR_L );
		}

		// value needs to fit into 48 or 56 bits
		void	sr_v_update( const TInt64& _notLimitedResult, TReg56& _result )
		{
			if( sr_test(SR_SM) )
			{
				limit_arithmeticSaturation(_result);
			}
			else
			{
				sr_toggle( SR_V, (_notLimitedResult & 0x00ff000000000000) != (_result.var & 0x00ff000000000000) );
			}
		}

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

		void scale( TReg56& _scale )
		{
			if( sr_test(SR_S1) )
				_scale.var <<= 1;
			else if( sr_test(SR_S0) )
				_scale.var >>= 1;
		}

		void limit_transfer( TReg48& _dst, const TReg56& _src )
		{
			__int64 test = _src.signextend<__int64>();
			if( test < -140737488355328 )	// 80000000000
			{
				sr_set( SR_L );
				_dst.var = -140737488355328;
			}
			else if( test > 140737488355327 )
			{
				sr_set( SR_L );
				_dst.var = 140737488355327;
			}
			else
				_dst.var = _src.var;
			assert( (_dst.var & 0xffff000000000000) == 0 );
		}

		void limit_transfer( int& _dst, const TReg56& _src )
		{
			int test = signextend<int,24>(int((_src.var & 0x00ffffff000000) >> 24));

			if( test < -8388608 )	// 800000
			{
				sr_set( SR_L );
				_dst = -8388608;
			}
			else if( test > 8388607 )
			{
				sr_set( SR_L );
				_dst = 8388607;
			}
			else
				_dst = (int)((_src.var & 0x00ffffff000000)>>24);
			assert( (_dst & 0xff000000) == 0 );
		}

		void limit_transfer( TReg24& _dst, const TReg56& _src )
		{
			limit_transfer( _dst.var, _src );
		}

		void limit_transfer( TWord& _dst, const TReg56& _src )
		{
			limit_transfer( (int&)_dst, _src );
		}

		void limit_arithmeticSaturation( TReg56& _dst )
		{
			if( !sr_test(SR_SM) )
				return;

			const unsigned int v = (int(bittest( _dst, 55 )) << 2) | (int(bittest( _dst, 48 )) << 1) | (int(bittest(_dst,47)));

			switch( v )
			{
			case 0:
			case 7:	/* do nothing */	break;
			case 1:
			case 2:
			case 3:	_dst.var = 0x007fffffffffff;	sr_set(SR_V);	break;
			case 4:
			case 5:
			case 6: _dst.var = 0xff800000000000;	sr_set(SR_V);	break;
			default: assert( 0 && "impossible" );
			}
		}

		TReg8	ccr				() const							{ return byte0(reg.sr); }
		TReg8	mr				() const							{ return byte1(reg.sr); }
		void	ccr				( TReg8 _val )						{ byte0(reg.sr,_val); }
		void	mr				( TReg8 _val )						{ byte1(reg.sr,_val); }

		TReg8	com				() const							{ return byte0(reg.omr); }
		TReg8	eom				() const							{ return byte1(reg.omr); }
		void	com				( TReg8 _val )						{ byte0(reg.omr,_val); }
		void	eom				( TReg8 _val )						{ byte1(reg.omr,_val); }

		void	setA			( const TReg24& _src )				{ convert( reg.a, _src ); }
		void	setB			( const TReg24& _src )				{ convert( reg.b, _src ); }

		void	setA			( const TReg56& _src )				{ reg.a = _src; }
		void	setB			( const TReg56& _src )				{ reg.b = _src; }

		// STACK
		void	decSP			()
		{
			assert( reg.sc.var > 0 );
			--reg.sp.var;
			--reg.sc.var;
		}
		void	incSP			()
		{
			assert( reg.sc.var < reg.ss.eSize-1 );
			++reg.sp.var;
			++reg.sc.var;
			assert( reg.sc.var <= 9 );
		}

		TReg24	ssh()				{ TReg24 res = hiword(reg.ss[reg.sc.toWord()]); decSP(); return res; }
		TReg24	ssl() const			{ return loword(reg.ss[reg.sc.toWord()]); }

		void	ssl(TReg24 _val)	{ loword(reg.ss[reg.sc.toWord()],_val); }
		void	ssh(TReg24 _val)	{ incSP(); hiword(reg.ss[reg.sc.toWord()],_val); }

		void	pushPCSR()			{ ssh(reg.pc); ssl(reg.sr); }
		void	popPCSR()			{ reg.sr = ssl(); reg.pc = ssh(); }
		void	popPC()				{ reg.pc = ssh(); }

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

		TWord	alu_bclr			( TWord _bit, TWord _val );
		void	alu_mpy				( bool ab, TReg24 _s1, TReg24 _s2, bool _negate, bool _accumulate );
		void	alu_mpysuuu			( bool ab, TReg24 _s1, TReg24 _s2, bool _negate, bool _accumulate, bool _suuu );
		void	alu_dmac			( bool ab, TReg24 _s1, TReg24 _s2, bool _negate, bool srcUnsigned, bool dstUnsigned );
		void	alu_mac				( bool ab, TReg24 _s1, TReg24 _s2, bool _negate, bool _uu );

		void	alu_rnd				( bool _ab )	{ alu_rnd( _ab ? reg.b : reg.a ); }

		void	alu_rnd				( TReg56& _alu );

		void	alu_abs				( bool ab );

		// -- memory

		bool	memWrite			( EMemArea _area, TWord _offset, TWord _value );
		TWord	memRead				( EMemArea _area, TWord _offset ) const;
		bool	memTranslateAddress	( EMemArea& _area, TWord& _offset ) const;
		bool	memValidateAccess	( EMemArea _area, TWord _addr, bool _write ) const;
	};
}
