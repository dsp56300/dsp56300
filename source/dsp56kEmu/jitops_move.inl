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
}
