#pragma once

#include "agu.h"
#include "dsp.h"

namespace dsp56k
{

	// _____________________________________________________________________________
	// decode_cccc
	//
	inline int DSP::decode_cccc( TWord cccc ) const
	{
//	#define			SRT_C			sr_val(CCRB_C)			// carry
	#define 		SRT_V			sr_val(CCRB_V)			// overflow
	#define 		SRT_Z			sr_val(CCRB_Z)			// zero
	#define 		SRT_N			sr_val(CCRB_N)			// negative
	#define 		SRT_U			sr_val(CCRB_U)			// unnormalized
	#define 		SRT_E			sr_val(CCRB_E)			// extension
//	#define 		SRT_L			sr_val(CCRB_L)			// limit
//	#define 		SRT_S			sr_val(CCRB_S)			// scaling

		switch( cccc )
		{
		case CCCC_CarrySet:			return sr_test(CCR_C);							// CC(LO)		Carry Set	(lower)
		case CCCC_CarryClear:		return !sr_test(CCR_C);							// CC(HS)		Carry Clear (higher or same)

		case CCCC_ExtensionSet:		return sr_test(CCR_E);							// ES			Extension set
		case CCCC_ExtensionClear:	return !sr_test(CCR_E);							// EC			Extension clear

		case CCCC_Equal:			return sr_test(CCR_Z);							// EQ			Equal
		case CCCC_NotEqual:			return !sr_test(CCR_Z);							// NE			Not Equal

		case CCCC_LimitSet:			return sr_test(CCR_L);							// LS			Limit set
		case CCCC_LimitClear:		return !sr_test(CCR_L);							// LC			Limit clear

		case CCCC_Minus:			return sr_test(CCR_N);							// MI			Minus
		case CCCC_Plus:				return !sr_test(CCR_N);							// PL			Plus

		case CCCC_GreaterEqual:		return SRT_N == SRT_V;							// GE			Greater than or equal
		case CCCC_LessThan:			return SRT_N != SRT_V;							// LT			Less than

		case CCCC_Normalized:		return (SRT_Z + ((!SRT_U) | (!SRT_E))) == 1;	// NR			Normalized
		case CCCC_NotNormalized:	return (SRT_Z + ((!SRT_U) | !SRT_E)) == 0;		// NN			Not normalized

		case CCCC_GreaterThan:		return (SRT_Z + (SRT_N != SRT_V)) == 0;			// GT			Greater than
		case CCCC_LessEqual:		return (SRT_Z + (SRT_N != SRT_V)) == 1;			// LE			Less than or equal
		}
		assert( 0 && "unreachable" );
		return false;
	}

	template<typename T> T DSP::decode_ddddd_read(const TWord _ddddd )
	{
		T res;
		// TODO: can be replaced with the six bit version, numbers are identical anyway
		switch( _ddddd )
		{
		case 0x04:	convert( res, x0() ); 	return res;	// x0
		case 0x05:	convert( res, x1() ); 	return res;	// x1	
		case 0x06:	convert( res, y0() ); 	return res;	// y0
		case 0x07:	convert( res, y1() ); 	return res;	// y1
		case 0x08:	convert( res, a0() ); 	return res;	// a0
		case 0x09:	convert( res, b0() ); 	return res;	// b0
		case 0x0a:	convertS( res, a2() ); 	return res;	// a2
		case 0x0b:	convertS( res, b2() ); 	return res;	// b2
		case 0x0c:	convert( res, a1() ); 	return res;	// a1
		case 0x0d:	convert( res, b1() ); 	return res;	// b1
		case 0x0e:	return getA<T>();					// a
		case 0x0f:	return getB<T>();					// b
		}
		if( (_ddddd & 0x18) == 0x10 )					// r0-r7
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

	template<typename T> bool DSP::decode_ddddd_write(const TWord _ddddd, const T& _val )
	{
		switch( _ddddd )
		{
			case 0x04:	x0(_val);										return true;	// x0
			case 0x05:	x1(_val);										return true;	// x1	
			case 0x06:	y0(_val);										return true;	// y0
			case 0x07:	y1(_val);										return true;	// y1
			case 0x08:	{ TReg24 temp; convertU(temp,_val); a0(temp); }	return true;	// a0
			case 0x09:	{ TReg24 temp; convertU(temp,_val); b0(temp); }	return true;	// b0
			case 0x0a:	{ TReg8  temp; convertU(temp,_val); a2(temp); }	return true;	// a2
			case 0x0b:	{ TReg8  temp; convertU(temp,_val); b2(temp); }	return true;	// b2
			case 0x0c:	{ TReg24 temp; convertU(temp,_val); a1(temp); }	return true;	// a1
			case 0x0d:	{ TReg24 temp; convertU(temp,_val); b1(temp); }	return true;	// b1
			case 0x0e:	convert(reg.a,_val);							return true;	// a
			case 0x0f:	convert(reg.b,_val);							return true;	// b
			default:
				if( (_ddddd & 0x18) == 0x10 )											// r0-r7
				{
					convertU(reg.r[_ddddd&0x07],_val);
					return true;
				}
				if( (_ddddd & 0x18) == 0x18 )											// n0-n7
				{
					convertU(reg.n[_ddddd&0x07],_val);
					return true;
				}
				assert(0 && "invalid ddddd destination register");
				return false;
		}
	}
	inline TReg24 DSP::decode_dddddd_read( TWord _dddddd )
	{
		switch( _dddddd & 0x3f )
		{
		// 0000DD - 4 registers in data ALU - NOT DOCUMENTED but the motorola disasm claims it works, for example for the lua instruction
		case 0x00:	return x0();
		case 0x01:	return x1();
		case 0x02:	return y0();
		case 0x03:	return y1();
		// 0001DD - 4 registers in data ALU
		case 0x04:	return x0();
		case 0x05:	return x1();
		case 0x06:	return y0();
		case 0x07:	return y1();

		// 001DDD - 8 accumulators in data ALU
		case 0x08:	return a0();
		case 0x09:	return b0();
		case 0x0a:	{ TReg24 res; convertS(res,a2()); return res; }
		case 0x0b:	{ TReg24 res; convertS(res,b2()); return res; }
		case 0x0c:	return a1();
		case 0x0d:	return b1();
		case 0x0e:	return getA<TReg24>();
		case 0x0f:	return getB<TReg24>();

		// 010TTT - 8 address registers in AGU
		case 0x10:	return reg.r[0];
		case 0x11:	return reg.r[1];
		case 0x12:	return reg.r[2];
		case 0x13:	return reg.r[3];
		case 0x14:	return reg.r[4];
		case 0x15:	return reg.r[5];
		case 0x16:	return reg.r[6];
		case 0x17:	return reg.r[7];

		// 011NNN - 8 address offset registers in AGU
		case 0x18:	return reg.n[0];
		case 0x19:	return reg.n[1];
		case 0x1a:	return reg.n[2];
		case 0x1b:	return reg.n[3];
		case 0x1c:	return reg.n[4];
		case 0x1d:	return reg.n[5];
		case 0x1e:	return reg.n[6];
		case 0x1f:	return reg.n[7];

		// 100FFF - 8 address modifier registers in AGU
		case 0x20:	return reg.m[0];
		case 0x21:	return reg.m[1];
		case 0x22:	return reg.m[2];
		case 0x23:	return reg.m[3];
		case 0x24:	return reg.m[4];
		case 0x25:	return reg.m[5];
		case 0x26:	return reg.m[6];
		case 0x27:	return reg.m[7];

		// 101EEE - 1 adress register in AGU
		case 0x2a:	return reg.ep;

		// 110VVV - 2 program controller registers
		case 0x30:	return reg.vba;
		case 0x31:	return TReg24(reg.sc.var);

		// 111GGG - 8 program controller registers
		case 0x38:	return reg.sz;
		case 0x39:	return getSR();
		case 0x3a:	return reg.omr;
		case 0x3b:	return reg.sp;
		case 0x3c:	return ssh();
		case 0x3d:	return ssl();
		case 0x3e:	return reg.la;
		case 0x3f:	return reg.lc;
		}
		assert(0 && "invalid eeeeee value");
		return TReg24(0xbadbad);
	}

	inline void DSP::decode_dddddd_write(const TWord _dddddd, TReg24 _val )
	{
		assert( (_val.var & 0xff000000) == 0 );

		switch( _dddddd & 0x3f )
		{
		// 0001DD - 4 registers in data ALU
		case 0x04:	x0(_val);	return;
		case 0x05:	x1(_val);	return;
		case 0x06:	y0(_val);	return;
		case 0x07:	y1(_val);	return;

		// 001DDD - 8 accumulators in data ALU
		case 0x08:	a0(_val);	return;
		case 0x09:	b0(_val);	return;
		case 0x0a:	{ TReg8 temp; convert(temp,_val); a2(temp);	return; }
		case 0x0b:	{ TReg8 temp; convert(temp,_val); b2(temp);	return; }
		case 0x0c:	a1(_val);	return;
		case 0x0d:	b1(_val);	return;
		case 0x0e:	setA(_val);	return;
		case 0x0f:	setB(_val);	return;

		// 010TTT - 8 address registers in AGU
		case 0x10:	reg.r[0] = _val;	return;
		case 0x11:	reg.r[1] = _val;	return;
		case 0x12:	reg.r[2] = _val;	return;
		case 0x13:	reg.r[3] = _val;	return;
		case 0x14:	reg.r[4] = _val;	return;
		case 0x15:	reg.r[5] = _val;	return;
		case 0x16:	reg.r[6] = _val;	return;
		case 0x17:	reg.r[7] = _val;	return;

		// 011NNN - 8 address offset registers in AGU
		case 0x18:	reg.n[0] = _val;	return;
		case 0x19:	reg.n[1] = _val;	return;
		case 0x1a:	reg.n[2] = _val;	return;
		case 0x1b:	reg.n[3] = _val;	return;
		case 0x1c:	reg.n[4] = _val;	return;
		case 0x1d:	reg.n[5] = _val;	return;
		case 0x1e:	reg.n[6] = _val;	return;
		case 0x1f:	reg.n[7] = _val;	return;

		// 100FFF - 8 address modifier registers in AGU
		case 0x20:	set_m(0, _val.var);	return;
		case 0x21:	set_m(1, _val.var);	return;
		case 0x22:	set_m(2, _val.var);	return;
		case 0x23:	set_m(3, _val.var);	return;
		case 0x24:	set_m(4, _val.var);	return;
		case 0x25:	set_m(5, _val.var);	return;
		case 0x26:	set_m(6, _val.var);	return;
		case 0x27:	set_m(7, _val.var);	return;

		// 101EEE - 1 adress register in AGU
		case 0x2a:	reg.ep = _val;		return;

		// 110VVV - 2 program controller registers
		case 0x30:	reg.vba = _val;			return;
		case 0x31:	reg.sc.var = _val.var & 0x1f;	return;

		// 111GGG - 8 program controller registers
		case 0x38:	reg.sz = _val;	return;
		case 0x39:	setSR(_val);	return;
		case 0x3a:	reg.omr = _val;	return;
		case 0x3b:	reg.sp = _val;	return;
		case 0x3c:	ssh(_val);	return;
		case 0x3d:	ssl(_val);	return;
		case 0x3e:	reg.la = _val;	return;
		case 0x3f:	reg.lc = _val;	return;
		}
		assert(0 && "unknown register");
	}

	inline TReg24 DSP::decode_ddddd_pcr_read( TWord _ddddd )
	{
		if( (_ddddd & 0x18) == 0x00 )
		{
			return reg.m[_ddddd&0x07];
		}

		switch( _ddddd )
		{
		case 0xa:	return reg.ep;
		case 0x10:	return reg.vba;
		case 0x11:	return TReg24(reg.sc.var);
		case 0x18:	return reg.sz;
		case 0x19:	return getSR();
		case 0x1a:	return reg.omr;
		case 0x1b:	return reg.sp;
		case 0x1c:	return ssh();
		case 0x1d:	return ssl();
		case 0x1e:	return reg.la;
		case 0x1f:	return reg.lc;
		}
		assert( 0 && "invalid ddddd value" );
		return TReg24(0xbadbad);
	}

	inline void DSP::decode_ddddd_pcr_write( TWord _ddddd, TReg24 _val )
	{
		if( (_ddddd & 0x18) == 0x00 )
		{
			set_m(_ddddd&0x07, _val.var);
			return;
		}

		switch( _ddddd )
		{
		case 0xa:	reg.ep = _val;				return;
		case 0x10:	reg.vba = _val;				return;
		case 0x11:	reg.sc.var = _val.var&0x1f;	return;
		case 0x18:	reg.sz = _val;				return;
		case 0x19:	setSR(_val);				return;
		case 0x1a:	reg.omr = _val;				return;
		case 0x1b:	reg.sp = _val;				return;
		case 0x1c:	ssh(_val);					return;
		case 0x1d:	ssl(_val);					return;
		case 0x1e:	reg.la = _val;				return;
		case 0x1f:	reg.lc = _val;				return;
		}
		assert( 0 && "unreachable" );
	}

	inline TReg8 DSP::decode_EE_read(TWord _ee) const
	{
		switch (_ee)
		{
		case 0: return mr();
		case 1: return ccr();
		case 2: return com();
		case 3: return eom();
		}
		assert(0 && "invalid EE value");
		return TReg8(static_cast<uint8_t>(0xee));
	}

	inline void DSP::decode_EE_write(TWord _ee, TReg8 _val)
	{
		switch (_ee)
		{
		case 0: mr(_val);			return;
		case 1: ccr(_val);			return;
		case 2: com(_val);			return;
		case 3: eom(_val);			return;
		}
		assert(0 && "invalid EE value");
	}

	template<TWord ee>
	TReg24 DSP::decode_ee_read()
	{
		static_assert(ee <= 3, "invalid ee value");
		
		if constexpr (ee == 0)	return x0();
		if constexpr (ee == 1)	return x1();
		if constexpr (ee == 2)	return getA<TReg24>();
		if constexpr (ee == 3)	return getB<TReg24>();
		return TReg24(0xbadbad);
	}

	template<TWord ee>
	void DSP::decode_ee_write(const TReg24& _value)
	{
		static_assert(ee >= 0 && ee <= 3, "invalid ee value");

		if constexpr (ee == 0)	x0(_value);
		if constexpr (ee == 1)	x1(_value);
		if constexpr (ee == 2)	setA(_value);
		if constexpr (ee == 3)	setB(_value);
	}

	inline void DSP::decode_ee_write(const TWord _ff, const TReg24& _value)
	{
		switch (_ff)
		{
		case 0: x0(_value);		return;
		case 1: x1(_value);		return;
		case 2: setA(_value);	return;
		case 3: setB(_value);	return;
		}
		assert(0 && "invalid ff value");
	}

	template<TWord ff>
	TReg24 DSP::decode_ff_read()
	{
		if constexpr(ff == 0) return y0();
		if constexpr(ff == 1) return y1();
		if constexpr(ff == 2) return getA<TReg24>();
		if constexpr(ff == 3) return getB<TReg24>();
		return TReg24(0xbadbad);
	}

	template<TWord ff>
	inline void DSP::decode_ff_write(const TReg24& _value)
	{
		if constexpr (ff == 0) y0(_value);
		if constexpr (ff == 1) y1(_value);
		if constexpr (ff == 2) setA(_value);
		if constexpr (ff == 3) setB(_value);
	}

	inline TReg24 DSP::decode_ff_read(TWord _ff)
	{
		switch (_ff)
		{
		case 0: return y0();
		case 1: return y1();
		case 2: return getA<TReg24>();
		case 3: return getB<TReg24>();
		}
		assert(0 && "invalid ff value");
		return TReg24(int(0xbadbad));
	}

	inline void DSP::decode_ff_write(TWord _ff, const TReg24& _value)
	{
		switch (_ff)
		{
		case 0: y0(_value);		return;
		case 1: y1(_value);		return;
		case 2: setA(_value);	return;
		case 3: setB(_value);	return;
		}
		assert(0 && "invalid ff value");
	}

	template<TWord _mmm> TWord DSP::decode_MMMRRR_read( TWord _rrr )
	{
		if constexpr(_mmm == 6)																	/* 110         */
			return fetchOpWordB();

		auto& _r = reg.r[_rrr];

		auto r = _r.toWord();

		if constexpr (_mmm == 4)																/* 100 (Rn)    */
			return r;

		const auto&	n  = reg.n[_rrr].var;
		const auto&	m  = reg.m[_rrr].var;

		TWord a;

		if constexpr (_mmm == 0) {	a = r;	AGU::updateAddressRegister(r,-n,m,moduloMask[_rrr],modulo[_rrr]);				}	/* 000 (Rn)-Nn */
		if constexpr (_mmm == 1) {	a = r;	AGU::updateAddressRegister(r,+n,m,moduloMask[_rrr],modulo[_rrr]);				}	/* 001 (Rn)+Nn */
		if constexpr (_mmm == 2) {	a = r;	AGU::updateAddressRegister(r,-1,m,moduloMask[_rrr],modulo[_rrr]);				}	/* 010 (Rn)-   */
		if constexpr (_mmm == 3) {	a = r;	AGU::updateAddressRegister(r,+1,m,moduloMask[_rrr],modulo[_rrr]);				}	/* 011 (Rn)+   */
		if constexpr (_mmm == 5) {	a = r;	AGU::updateAddressRegister(a,+n, m,moduloMask[_rrr],modulo[_rrr]);				}	/* 101 (Rn+Nn) */
		if constexpr (_mmm == 7) {			AGU::updateAddressRegister(r,-1,m,moduloMask[_rrr],modulo[_rrr]);		a = r;	}	/* 111 -(Rn)   */

		_r.var = r;

		return a;
	}

	inline TWord DSP::decode_MMMRRR_read( TWord _mmm, TWord _rrr )
	{
		switch(_mmm << 3 | _rrr)
		{
		case MMMRRR_AbsAddr:		return fetchOpWordB();		// absolute address
		case MMMRRR_ImmediateData:	return fetchOpWordB();		// immediate data
		}

		const TWord regIdx = _rrr;

		const TReg24	_n = reg.n[regIdx];
		TReg24&			_r = reg.r[regIdx];
		const TReg24	_m = reg.m[regIdx];

		TWord r = _r.toWord();

		TWord a;

		switch( _mmm )
		{
		case 0:	/* 000 (Rn)-Nn */	a = r;		AGU::updateAddressRegister(r,-_n.var,_m.var,moduloMask[regIdx],modulo[regIdx]);					break;
		case 1:	/* 001 (Rn)+Nn */	a = r;		AGU::updateAddressRegister(r,+_n.var,_m.var,moduloMask[regIdx],modulo[regIdx]);					break;
		case 2:	/* 010 (Rn)-   */	a = r;		AGU::updateAddressRegister(r,-1,_m.var,moduloMask[regIdx],modulo[regIdx]);						break;
		case 3:	/* 011 (Rn)+   */	a = r;		AGU::updateAddressRegister(r,+1,_m.var,moduloMask[regIdx],modulo[regIdx]);						break;
		case 4:	/* 100 (Rn)    */	a = r;																		break;
		case 5:	/* 101 (Rn+Nn) */	a = r;		AGU::updateAddressRegister(a,+_n.var, _m.var,moduloMask[regIdx],modulo[regIdx]);					break;
		// case 6: special case handled above, either immediate data or absolute address in extension word
		case 7:	/* 111 -(Rn)   */				AGU::updateAddressRegister(r,-1,_m.var,moduloMask[regIdx],modulo[regIdx]);			a = r;		break;

		default:
			assert(0 && "impossible to happen" );
			return 0xbadbad;
		}

		_r.var = r;

		assert( a >= 0x00000000 && a <= 0x00ffffff && "invalid memory address" );
		return a;
	}

	inline TWord DSP::decode_XMove_MMRRR( TWord _mm, TWord _rrr )
	{
		const TReg24	_n = reg.n[_rrr];
		TReg24&			_r = reg.r[_rrr];
		const TReg24	_m = reg.m[_rrr];

		TWord r = _r.toWord();

		TWord a;

		switch( _mm )
		{
		case 0:	/* 00 */	a = r;																				break;
		case 1:	/* 01 */	a = r;	AGU::updateAddressRegister(r,+_n.var,_m.var,moduloMask[_rrr],modulo[_rrr]);	break;
		case 2:	/* 10 */	a = r;	AGU::updateAddressRegister(r,-1,_m.var,moduloMask[_rrr],modulo[_rrr]);		break;
		case 3:	/* 11 */	a =	r;	AGU::updateAddressRegister(r,+1,_m.var,moduloMask[_rrr],modulo[_rrr]);		break;
		}

		_r.var = r;

		return a;
	}

	inline TWord DSP::decode_RRR_read( TWord _mmmrrr) const
	{
		const unsigned int regIdx = _mmmrrr&0x07;

		const TReg24& _r = reg.r[regIdx];

		return _r.var;
	}

	inline TWord DSP::decode_RRR_read( TWord _mmmrrr, int _shortDisplacement ) const
	{
		const unsigned int regIdx = _mmmrrr&0x07;

		const TReg24& _r = reg.r[regIdx];

		const int ea = _r.var + _shortDisplacement;

		return ea&0x00ffffff;
	}

	inline TReg24 DSP::decode_JJ_read(TWord jj) const
	{
		switch (jj)
		{
		case 0: return x0();
		case 1: return y0();
		case 2: return x1();
		case 3: return y1();
		}
		assert(0 && "unreachable, invalid JJ value");
		return TReg24(0xbadbad);
	}

	inline TReg56 DSP::decode_JJJ_read_56(TWord jjj, bool _b) const
	{
		TReg56 res;
		switch (jjj)
		{
		case 0:
		case 1: res = _b ? reg.b : reg.a;
			break;
		case 2: convert(res, reg.x);
			break;
		case 3: convert(res, reg.y);
			break;
		case 4: convert(res, x0());
			break;
		case 5: convert(res, y0());
			break;
		case 6: convert(res, x1());
			break;
		case 7: convert(res, y1());
			break;
		default:
			assert(0 && "unreachable, invalid JJJ value");
			return TReg56(static_cast<TReg56::MyType>(0xbadbadbadbadbadb));
		}
		return res;
	}

	inline void DSP::decode_JJJ_readwrite(TReg56& alu, TWord jjj, bool _b)
	{
		switch (jjj)
		{
		case 0:
		case 1: alu = _b ? reg.b : reg.a;
			return;
		case 2: convert(alu, reg.x);
			return;
		case 3: convert(alu, reg.y);
			return;
		case 4: convert(alu, x0());
			return;
		case 5: convert(alu, y0());
			return;
		case 6: convert(alu, x1());
			return;
		case 7: convert(alu, y1());
			return;
		default:
			assert(0 && "unreachable, invalid JJJ value");
		}
	}

	inline void DSP::decode_LLL_read(TWord _lll, TWord& x, TWord& y)
	{
		switch (_lll)
		{
		case 0: convert(x, a1());			convert(y, a0());			return;
		case 1: convert(x, b1());			convert(y, b0());			return;
		case 2: convert(x, x1());			convert(y, x0());			return;
		case 3: convert(x, y1());			convert(y, y0());			return;
		case 4:
			x = reg.a.var >> 24 & 0xffffff;
			y = reg.a.var & 0xffffff;
			return;
		case 5:
			x = reg.b.var >> 24 & 0xffffff;
			y = reg.b.var & 0xffffff;
			return;
		case 6: x = getA<TWord>();			y = getB<TWord>();			return;
		case 7: x = getB<TWord>();			y = getA<TWord>();			return;
		}
		assert(0 && "invalid LLL value");
	}

	inline void DSP::decode_LLL_write(TWord _lll, TReg24 x, TReg24 y)
	{
		switch (_lll)
		{
		case 0: a1(x);			a0(y);			return;
		case 1: b1(x);			b0(y);			return;
		case 2: x1(x);			x0(y);			return;
		case 3: y1(x);			y0(y);			return;
		case 4:
			{
				TReg48 xy;
				xy.var = static_cast<uint64_t>(x.var) << 24 | y.var;
				convert(reg.a, xy);
			}
			return;
		case 5:
			{
				TReg48 xy;
				xy.var = static_cast<uint64_t>(x.var) << 24 | y.var;
				convert(reg.b, xy);
			}
			return;
		case 6: setA(x);			setB(y);			return;
		case 7: setB(x);			setA(y);			return;
		}
		assert(0 && "invalid LLL value");
	}

	inline void DSP::decode_QQQQ_read(TReg24& _s1, TReg24& _s2, TWord _qqqq) const
	{
		switch (_qqqq)
		{
		case 0: _s1 = x0();		_s2 = x0();	return;
		case 1: _s1 = y0();		_s2 = y0();	return;
		case 2: _s1 = x1();		_s2 = x0();	return;
		case 3: _s1 = y1();		_s2 = y0();	return;

		case 4: _s1 = x0();		_s2 = y1();	return;
		case 5: _s1 = y0();		_s2 = x0();	return;
		case 6: _s1 = x1();		_s2 = y0();	return;
		case 7: _s1 = y1();		_s2 = x1();	return;

		case 8: _s1 = x1();		_s2 = x1();	return;
		case 9: _s1 = y1();		_s2 = y1();	return;
		case 10: _s1 = x0();	_s2 = x1();	return;
		case 11: _s1 = y0();	_s2 = y1();	return;

		case 12: _s1 = y1();	_s2 = x0();	return;
		case 13: _s1 = x0();	_s2 = y0();	return;
		case 14: _s1 = y0();	_s2 = x1();	return;
		case 15: _s1 = x1();	_s2 = y1();	return;
		}
		assert(0 && "invalid QQQQ value");
	}

	inline void DSP::decode_QQQ_read(TReg24& _s1, TReg24& _s2, TWord _qqq) const
	{
		switch (_qqq)
		{
		case 0: _s1 = x0(); _s2 = x0(); return;
		case 1: _s1 = y0(); _s2 = y0(); return;
		case 2: _s1 = x1(); _s2 = x0(); return;
		case 3: _s1 = y1(); _s2 = y0(); return;
		case 4: _s1 = x0(); _s2 = y1(); return;
		case 5: _s1 = y0(); _s2 = x0(); return;
		case 6: _s1 = x1(); _s2 = y0(); return;
		case 7: _s1 = y1(); _s2 = x1(); return;
		}
		assert(0 && "invalid QQQ value");
	}

	inline TReg24 DSP::decode_QQ_read(TWord _qq) const
	{
		switch (_qq)
		{
		case 0: return y1();
		case 1: return x0();
		case 2: return y0();
		case 3: return x1();
		}
		assert(0 && "invalid QQ value");
		return TReg24(static_cast<int>(0xbadbad));
	}

	inline TReg24 DSP::decode_qq_read(TWord _qq) const
	{
		switch (_qq)
		{
		case 0: return x0();
		case 1: return y0();
		case 2: return x1();
		case 3: return y1();
		}
		assert(0 && "invalid qq value");
		return TReg24(static_cast<int>(0xbadbad));
	}

	template<typename T> T DSP::decode_sss_read( TWord _sss ) const
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

	inline TWord DSP::decode_sssss(const TWord _sssss)
	{
		return 0x800000 >> _sssss;
	}
}
