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
}
