#pragma once

namespace dsp56k
{
	void JitOps::op_Move_Nop(TWord op)
	{
	}

	void JitOps::op_Move_xx(TWord op)
	{
		const auto ddddd = getFieldValue<Move_xx, Field_ddddd>(op);
		const auto iiiiiiii = getFieldValue<Move_xx, Field_iiiiiiii>(op);

		const RegGP i(m_block);
		m_asm.mov(i.get().r32(), asmjit::Imm(iiiiiiii));
		decode_dddddd_write(ddddd, i.get().r32());
	}

	void JitOps::op_Mover(TWord op)
	{
		const auto eeeee = getFieldValue<Mover,Field_eeeee>(op);
		const auto ddddd = getFieldValue<Mover,Field_ddddd>(op);

		const RegGP r(m_block);
		decode_dddddd_read(r.get().r32(), eeeee);
		decode_dddddd_write(ddddd, r.get().r32());
	}

	void JitOps::op_Move_ea(TWord op)
	{
		const auto mm  = getFieldValue<Move_ea, Field_MM>(op);
		const auto rrr = getFieldValue<Move_ea, Field_RRR>(op);

		const RegGP unused(m_block);
		updateAddressRegister(unused, mm, rrr, true, true);
	}

	template<Instruction Inst> void JitOps::move_ddddd_MMMRRR(const TWord _op, const EMemArea _area)
	{
		const TWord ddddd = getFieldValue<Inst,Field_dd, Field_ddd>(_op);
		const auto mmm = getFieldValue<Inst,Field_MMM>(_op);
		const auto rrr = getFieldValue<Inst,Field_RRR>(_op);
		const auto write = getFieldValue<Inst,Field_W>(_op);

		const RegGP r(m_block);

		if (write)
		{
			readMem<Inst>(r, _op, _area);
			decode_dddddd_write(ddddd, r.get().r32());
		}
		else
		{
			decode_dddddd_read(r.get().r32(), ddddd);
			writeMem<Inst>(_op, _area, r);
		}
	}
	template<Instruction Inst> void JitOps::move_ddddd_absAddr(TWord _op, const EMemArea _area)
	{
		const auto ddddd = getFieldValue<Inst,Field_dd, Field_ddd>(_op);
		const auto write = getFieldValue<Inst,Field_W>(_op);
		
		const RegGP r(m_block);

		if (write)
		{
			readMem<Inst>(r, _op, _area);
			decode_dddddd_write(ddddd, r.get().r32());
		}
		else
		{
			decode_dddddd_read(r.get().r32(), ddddd);
			writeMem<Inst>(_op, _area, r);
		}
	}

	void JitOps::op_Movex_ea(TWord op)
	{
		move_ddddd_MMMRRR<Movex_ea>(op, MemArea_X);
	}

	void JitOps::op_Movey_ea(TWord op)
	{
		move_ddddd_MMMRRR<Movey_ea>(op, MemArea_Y);
	}

	void JitOps::op_Movex_aa(TWord op)
	{
		move_ddddd_absAddr<Movex_aa>(op, MemArea_X);
	}

	void JitOps::op_Movey_aa(TWord op)
	{
		move_ddddd_absAddr<Movey_aa>(op, MemArea_Y);
	}

	template<Instruction Inst> void JitOps::move_Rnxxxx(TWord op, EMemArea _area)
	{
		const TWord DDDDDD	= getFieldValue<Inst,Field_DDDDDD>(op);
		const auto	write	= getFieldValue<Inst,Field_W>(op);
		const TWord rrr		= getFieldValue<Inst,Field_RRR>(op);

		const int shortDisplacement = pcRelativeAddressExt<Inst>();
		RegGP ea(m_block);
		decode_RRR_read(ea.get(), rrr, shortDisplacement);

		const RegGP r(m_block);

		if( write )
		{
			readMemOrPeriph(r, _area, ea);
			decode_dddddd_write(DDDDDD, r.get().r32());
		}
		else
		{
			decode_dddddd_read(r.get().r32(), DDDDDD);
			writeMemOrPeriph(_area, ea, r);
		}
	}

	void JitOps::op_Movex_Rnxxxx(TWord op)
	{
		move_Rnxxxx<Movex_Rnxxxx>(op, MemArea_X);
	}

	void JitOps::op_Movey_Rnxxxx(TWord op)
	{
		move_Rnxxxx<Movey_Rnxxxx>(op, MemArea_Y);
	}

	template<Instruction Inst> void JitOps::move_Rnxxx(TWord op, EMemArea _area)
	{
		const TWord ddddd	= getFieldValue<Inst,Field_DDDD>(op);
		const auto write	= getFieldValue<Inst,Field_W>(op);
		const auto aaaaaaa	= getFieldValue<Inst,Field_aaaaaa, Field_a>(op);
		const auto rrr		= getFieldValue<Inst,Field_RRR>(op);

		const int shortDisplacement = signextend<int,7>(aaaaaaa);

		RegGP ea(m_block);
		decode_RRR_read( ea, rrr, shortDisplacement );

		RegGP r(m_block);

		if( write )
		{
			readMemOrPeriph(r, _area, ea);
			decode_dddddd_write(ddddd, r.get().r32());
		}
		else
		{
			decode_dddddd_read(r.get().r32(), ddddd);
			writeMemOrPeriph(_area, ea, r);
		}		
	}

	void JitOps::op_Movex_Rnxxx(TWord op)
	{
		move_Rnxxx<Movex_Rnxxx>(op, MemArea_X);
	}

	void JitOps::op_Movey_Rnxxx(TWord op)
	{
		move_Rnxxx<Movey_Rnxxx>(op, MemArea_Y);
	}

	void JitOps::op_Movexr_ea(TWord op)
	{
		const TWord F		= getFieldValue<Movexr_ea,Field_F>(op);	// true:Y1, false:Y0
		const TWord ff		= getFieldValue<Movexr_ea,Field_ff>(op);
		const TWord write	= getFieldValue<Movexr_ea,Field_W>(op);
		const TWord d		= getFieldValue<Movexr_ea,Field_d>(op);

		{
			// S2 D2 move
			const RegGP ab(m_block);
			transferAluTo24(ab, d);

			if(F)		m_dspRegs.setXY1(1, ab);
			else		m_dspRegs.setXY0(1, ab);			
		}
		
		RegGP r(m_block);

		// S1/D1 move
		if( write )
		{
			readMem<Movexr_ea>(r, op, MemArea_X);

			decode_ee_write(ff, r);
		}
		else
		{
			
			decode_ff_read(r, ff);
			
			RegGP ea(m_block);
			effectiveAddress<Movexr_ea>(ea, op);
			writeMemOrPeriph(MemArea_X, ea, r);
		}
	}

	void JitOps::op_Moveyr_ea(TWord op)
	{
		const bool e		= getFieldValue<Moveyr_ea,Field_e>(op);	// true:X1, false:X0
		const TWord ff		= getFieldValue<Moveyr_ea,Field_ff>(op);
		const bool write	= getFieldValue<Moveyr_ea,Field_W>(op);
		const bool d		= getFieldValue<Moveyr_ea,Field_d>(op);

		{
			// S2 D2 move
			const RegGP ab(m_block);
			transferAluTo24(ab, d);

			if( e )		m_dspRegs.setXY1(0, ab);
			else		m_dspRegs.setXY0(0, ab);
		}
	
		// S1/D1 move
		RegGP r(m_block);
		if( write )
		{
			readMem<Moveyr_ea>(r, op, MemArea_Y);
			decode_ff_write( ff, r );
		}
		else
		{
			RegGP ea(m_block);
			effectiveAddress<Moveyr_ea>(ea, op);
			writeMemOrPeriph(MemArea_Y, ea, r);
		}
	}

	void JitOps::op_Movexr_A(TWord op)
	{
		const auto d = getFieldValue<Movexr_A,Field_d>(op);

		{
			// S1/D1 move

			RegGP ea(m_block);
			effectiveAddress<Movexr_A>(ea, op);

			const RegGP ab(m_block);
			transferAluTo24(ab, d);
			writeMemOrPeriph(MemArea_X, ea, ab);
		}

		{
			// S2/D2 move
			RegGP x0(m_block);
			m_dspRegs.getX0(x0);
			transfer24ToAlu(d, x0);
		}
	}

	void JitOps::op_Moveyr_A(TWord op)
	{
		const auto d = getFieldValue<Moveyr_A,Field_d>(op);

		{
			RegGP ea(m_block);
			effectiveAddress<Moveyr_A>(ea, op);

			const RegGP ab(m_block);
			transferAluTo24(ab, d);

			writeMemOrPeriph(MemArea_Y, ea, ab);
		}

		{
			RegGP y0(m_block);
			m_dspRegs.getY0(y0);
			transfer24ToAlu(d, y0);
		}
	}

	template<Instruction Inst> void JitOps::move_L(TWord op)
	{
		const auto LLL		= getFieldValue<Inst,Field_L, Field_LL>(op);
		const auto write	= getFieldValue<Inst,Field_W>(op);

		const RegGP ea(m_block);
		effectiveAddress<Inst>(ea, op);

		const RegGP x(m_block);
		const RegGP y(m_block);

		if( write )
		{
			readMemOrPeriph(x, MemArea_X, ea);
			readMemOrPeriph(y, MemArea_Y, ea);

			decode_LLL_write(LLL,  x.get().r32(),y.get().r32());
		}
		else
		{
			decode_LLL_read(LLL, x.get().r32(),y.get().r32());

			writeMemOrPeriph(MemArea_X, ea, x);
			writeMemOrPeriph(MemArea_Y, ea, y);
		}
	}

	inline void JitOps::op_Movel_ea(TWord op)
	{
		move_L<Movel_ea>(op);
	}

	inline void JitOps::op_Movel_aa(TWord op)
	{
		const auto LLL		= getFieldValue<Movel_aa,Field_L, Field_LL>(op);
		const auto write	= getFieldValue<Movel_aa,Field_W>(op);
		const auto ea		= getFieldValue<Movel_aa,Field_aaaaaa>(op);

		const RegGP x(m_block);
		const RegGP y(m_block);

		if( write )
		{
			m_block.mem().readDspMemory(x, MemArea_X, ea);
			m_block.mem().readDspMemory(y, MemArea_Y, ea);

			decode_LLL_write(LLL,  x.get().r32(),y.get().r32());
		}
		else
		{
			decode_LLL_read(LLL, x.get().r32(),y.get().r32());

			m_block.mem().writeDspMemory(MemArea_X, ea, x);
			m_block.mem().writeDspMemory(MemArea_Y, ea, y);
		}
	}

	inline void JitOps::op_Movexy(TWord op)
	{
		const auto MM		= getFieldValue<Movexy,Field_MM>(op);
		const auto RRR		= getFieldValue<Movexy,Field_RRR>(op);
		const auto mm		= getFieldValue<Movexy,Field_mm>(op);
		const auto rr		= getFieldValue<Movexy,Field_rr>(op);
		const auto writeX	= getFieldValue<Movexy,Field_W>(op);
		const auto writeY	= getFieldValue<Movexy,Field_w>(op);
		const auto ee		= getFieldValue<Movexy,Field_ee>(op);
		const auto ff		= getFieldValue<Movexy,Field_ff>(op);

		RegGP eaX(m_block);
		decode_XMove_MMRRR( eaX, MM, RRR );

		const TWord regIdxOffset = RRR >= 4 ? 0 : 4;

		const RegGP eaY(m_block);
		decode_XMove_MMRRR( eaY, mm, (rr + regIdxOffset) & 7 );

		if(!writeX)
		{
			const RegGP r(m_block);
			decode_ee_read( r, ee );
			writeMemOrPeriph(MemArea_X, eaX, r);
		}
		if(!writeY)
		{
			const RegGP r(m_block);
			decode_ff_read( r, ff );
			writeMemOrPeriph( MemArea_Y, eaY, r);
		}

		if( writeX )
		{
			const RegGP r(m_block);
			readMemOrPeriph(r, MemArea_X, eaX);
			decode_ee_write( ee, r );
		}
		if( writeY )
		{
			const RegGP r(m_block);
			readMemOrPeriph(r, MemArea_Y, eaY);
			decode_ff_write( ff, r);
		}
	}

	inline void JitOps::op_Movec_ea(TWord op)
	{
		const auto ddddd = getFieldValue<Movec_ea,Field_DDDDD>(op);
		const auto write = getFieldValue<Movec_ea,Field_W>(op);

		if( write )
		{
			const RegGP r(m_block);
			readMem<Movec_ea>(r, op);

			decode_ddddd_pcr_write( ddddd, r.get().r32());
		}
		else
		{
			const auto area = getFieldValueMemArea<Movec_ea>(op);

			RegGP ea(m_block);
			effectiveAddress<Movec_ea>(ea, op);

			const RegGP r(m_block);
			decode_ddddd_pcr_read(r.get().r32(), ddddd);

			writeMemOrPeriph(area, ea, r);
		}
	}
	inline void JitOps::op_Movec_aa(TWord op)
	{
		const auto ddddd	= getFieldValue<Movec_aa,Field_DDDDD>(op);
		const auto write	= getFieldValue<Movec_aa,Field_W>(op);

		if( write )
		{
			RegGP r(m_block);
			readMem<Movec_aa>(r, op);
			decode_ddddd_pcr_write( ddddd, r.get().r32() );
		}
		else
		{
			const auto addr = getFieldValue<Movec_aa,Field_aaaaaa>(op);
			const auto area = getFieldValueMemArea<Movec_aa>(op);

			const RegGP r(m_block);
			decode_ddddd_pcr_read(r.get().r32(), ddddd);
			m_block.mem().writeDspMemory(area, addr, r);
		}
	}
	inline void JitOps::op_Movec_S1D2(TWord op)
	{
		const auto write	= getFieldValue<Movec_S1D2,Field_W>(op);
		const auto eeeeee	= getFieldValue<Movec_S1D2,Field_eeeeee>(op);
		const auto ddddd	= getFieldValue<Movec_S1D2,Field_DDDDD>(op);

		const RegGP r(m_block);

		if( write )
		{
			decode_dddddd_read(r.get().r32(), eeeeee);
			decode_ddddd_pcr_write(ddddd, r.get().r32());
		}
		else
		{
			decode_ddddd_pcr_read(r.get().r32(), ddddd);
			decode_dddddd_write(eeeeee, r.get().r32());
		}
	}

	inline void JitOps::op_Movec_xx(TWord op)
	{
		const auto iiiiiiii	= getFieldValue<Movec_xx, Field_iiiiiiii>(op);
		const auto ddddd	= getFieldValue<Movec_xx,Field_DDDDD>(op);

		const RegGP r(m_block);
		m_asm.mov(r.get().r32(), asmjit::Imm(iiiiiiii));
		decode_ddddd_pcr_write( ddddd, r.get().r32());
	}
}
