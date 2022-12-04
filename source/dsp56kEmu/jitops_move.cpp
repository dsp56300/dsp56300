#include "jitops.h"

#include "jitops_move.inl"

namespace dsp56k
{
	void JitOps::op_Lra_xxxx(TWord op)
	{
		const auto ddddd = getFieldValue<Lra_xxxx, Field_ddddd>(op);
		const auto ea = m_pcCurrentOp + pcRelativeAddressExt<Lra_xxxx>();

		DspValue r(m_block, ea, DspValue::Immediate24);
		decode_dddddd_write(ddddd, r);
	}

	void JitOps::op_Move_Nop(TWord op)
	{
	}

	void JitOps::op_Move_xx(TWord op)
	{
		const auto ddddd = getFieldValue<Move_xx, Field_ddddd>(op);
		const auto iiiiiiii = getFieldValue<Move_xx, Field_iiiiiiii>(op);

		const DspValue i(m_block, iiiiiiii, DspValue::Immediate8);
		decode_dddddd_write(ddddd, i, true);
	}

	void JitOps::op_Mover(TWord op)
	{
		const auto eeeee = getFieldValue<Mover, Field_eeeee>(op);
		const auto ddddd = getFieldValue<Mover, Field_ddddd>(op);

		copy24ToDDDDDD<Mover>(op, ddddd, false, [&](DspValue& r)
		{
			decode_dddddd_read(r, eeeee);
		});
	}

	void JitOps::op_Move_ea(TWord op)
	{
		const auto mm = getFieldValue<Move_ea, Field_MM>(op);
		const auto rrr = getFieldValue<Move_ea, Field_RRR>(op);

		const auto unused = updateAddressRegister(mm, rrr, true, true);
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

	void JitOps::op_Movex_Rnxxxx(TWord op)
	{
		move_Rnxxxx<Movex_Rnxxxx>(op, MemArea_X);
	}

	void JitOps::op_Movey_Rnxxxx(TWord op)
	{
		move_Rnxxxx<Movey_Rnxxxx>(op, MemArea_Y);
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
		const TWord F = getFieldValue<Movexr_ea, Field_F>(op);	// true:Y1, false:Y0
		const TWord ff = getFieldValue<Movexr_ea, Field_ff>(op);
		const TWord write = getFieldValue<Movexr_ea, Field_W>(op);
		const TWord d = getFieldValue<Movexr_ea, Field_d>(op);

		// S2 D2 read
		DspValue ab(m_block);
		transferAluTo24(ab, d);
		bool rEqualsAb = false;

		DspValue r(m_block, UsePooledTemp);

		// S1 D1 read
		if (write)
		{
			readMem<Movexr_ea>(r, op, MemArea_X);
		}
		else
		{
			if(ff >= 2 && (ff-2) == d)
				rEqualsAb = true;
			else
				decode_ee_read(r, ff);
		}

		// S2 D2 write
		if (F)		setXY1(1, ab);
		else		setXY0(1, ab);

		if(!rEqualsAb)
			ab.release();

		// S1 D1 write
		if (write)
		{
			decode_ee_write(ff, r);
		}
		else
		{
			writeMem<Movexr_ea>(op, MemArea_X, rEqualsAb ? ab : r);
		}
	}

	void JitOps::op_Moveyr_ea(TWord op)
	{
		const bool e = getFieldValue<Moveyr_ea, Field_e>(op);	// true:X1, false:X0
		const TWord ff = getFieldValue<Moveyr_ea, Field_ff>(op);
		const bool write = getFieldValue<Moveyr_ea, Field_W>(op);
		const auto d = getFieldValue<Moveyr_ea, Field_d>(op);

		// S2 D2 read
		DspValue ab(m_block);
		transferAluTo24(ab, d);
		bool rEqualsAb = false;

		// S1 D1 read
		DspValue r(m_block, UsePooledTemp);

		if (write)
		{
			readMem<Moveyr_ea>(r, op, MemArea_Y);
		}
		else
		{
			if(ff >= 2 && (ff-2) == d)
				rEqualsAb = true;
			else
				decode_ff_read(r, ff);
		}

		// S2 D2 write
		{
			if (e)		setXY1(0, ab);
			else		setXY0(0, ab);
		}

		// S1 D1 write
		if (write)
		{
			decode_ff_write(ff, r);
		}
		else
		{
			writeMem<Moveyr_ea>(op, MemArea_Y, rEqualsAb ? ab : r);
		}
	}

	void JitOps::op_Movexr_A(TWord op)
	{
		const auto d = getFieldValue<Movexr_A, Field_d>(op);

		{
			DspValue ab(m_block);
			transferAluTo24(ab, d);

			writeMem<Movexr_A>(op, MemArea_X, ab);
		}
		{
			DspValue x0(m_block);
			getX0(x0, false);
			transfer24ToAlu(d, x0);
		}
	}

	void JitOps::op_Moveyr_A(TWord op)
	{
		const auto d = getFieldValue<Moveyr_A, Field_d>(op);

		{
			DspValue ab(m_block);
			transferAluTo24(ab, d);

			writeMem<Moveyr_A>(op, MemArea_Y, ab);
		}
		{
			DspValue y0(m_block);
			getY0(y0, false);
			transfer24ToAlu(d, y0);
		}
	}

	void JitOps::op_Movel_ea(TWord op)
	{
		move_L<Movel_ea>(op);
	}

	void JitOps::op_Movel_aa(TWord op)
	{
		const auto LLL = getFieldValue<Movel_aa, Field_L, Field_LL>(op);
		const auto write = getFieldValue<Movel_aa, Field_W>(op);
		const auto ea = getFieldValue<Movel_aa, Field_aaaaaa>(op);

		DspValue x(m_block);
		DspValue y(m_block);

		if (write)
		{
			m_block.mem().readDspMemory(x, y, ea);

			decode_LLL_write(LLL, x, y);
		}
		else
		{
			x.temp(DspValue::Temp24);
			y.temp(DspValue::Temp24);

			decode_LLL_read(LLL, x, y);

			m_block.mem().writeDspMemory(ea, x, y);
		}
	}

	void JitOps::op_Movexy(TWord op)
	{
		const auto MM = getFieldValue<Movexy, Field_MM>(op);
		const auto RRR = getFieldValue<Movexy, Field_RRR>(op);
		const auto mm = getFieldValue<Movexy, Field_mm>(op);
		auto rr = getFieldValue<Movexy, Field_rr>(op);
		const auto writeX = getFieldValue<Movexy, Field_W>(op);
		const auto writeY = getFieldValue<Movexy, Field_w>(op);
		const auto ee = getFieldValue<Movexy, Field_ee>(op);
		const auto ff = getFieldValue<Movexy, Field_ff>(op);

		const TWord regIdxOffset = RRR >= 4 ? 0 : 4;
		rr = (rr + regIdxOffset) & 7;

		if (!writeX && !writeY)
		{
			// we need to read the values first to prevent running out of temps
			DspValue rx(m_block, UsePooledTemp);
			DspValue ry(m_block, UsePooledTemp);

			if(ee == ff && ee >= 2)
			{
				// the same source alu is moved to two locations, read & convert it only once
				decode_ee_read(rx, ee);

				const auto eaX = decode_XMove_MMRRR(MM, RRR);
				const auto eaY = decode_XMove_MMRRR(mm, rr);

				m_block.mem().writeDspMemory(eaX, eaY, rx, rx);
			}
			else
			{
				decode_ee_read(rx, ee);
				decode_ff_read(ry, ff);

				const auto eaX = decode_XMove_MMRRR(MM, RRR);
				const auto eaY = decode_XMove_MMRRR(mm, rr);

				m_block.mem().writeDspMemory(eaX, eaY, rx, ry);
			}
			return;
		}

		if (writeX && writeY)
		{
			DspValue rx(m_block, UsePooledTemp);
			DspValue ry(m_block, UsePooledTemp);

			{
				const auto eaX = decode_XMove_MMRRR(MM, RRR);
				const auto eaY = decode_XMove_MMRRR(mm, rr);

				m_block.mem().readDspMemory(rx, ry, eaX, eaY);
			}

			decode_ee_write(ee, rx);
			decode_ff_write(ff, ry);
			return;
		}

		Jitmem::ScratchPMem basePtr(m_block);

		if (!writeX)
		{
			DspValue r(m_block, UsePooledTemp);
			decode_ee_read(r, ee);
			const auto eaX = decode_XMove_MMRRR(MM, RRR);
			m_block.mem().writeDspMemory(MemArea_X, eaX, r, basePtr);
		}
		if (!writeY)
		{
			DspValue r(m_block, UsePooledTemp);
			decode_ff_read(r, ff);
			const auto eaY = decode_XMove_MMRRR(mm, rr);
			m_block.mem().writeDspMemory(MemArea_Y, eaY, r, basePtr);
		}

		if (writeX)
		{
			DspValue r(m_block, UsePooledTemp);
			const auto eaX = decode_XMove_MMRRR(MM, RRR);
			m_block.mem().readDspMemory(r, MemArea_X, eaX, basePtr);
			decode_ee_write(ee, r);
		}

		if (writeY)
		{
			DspValue r(m_block, UsePooledTemp);
			const auto eaY = decode_XMove_MMRRR(mm, rr);
			m_block.mem().readDspMemory(r, MemArea_Y, eaY, basePtr);
			decode_ff_write(ff, r);
		}
	}

	void JitOps::op_Movec_ea(TWord op)
	{
		const auto ddddd = getFieldValue<Movec_ea, Field_DDDDD>(op);
		const auto write = getFieldValue<Movec_ea, Field_W>(op);

		if (write)
		{
			DspValue r(m_block);
			readMem<Movec_ea>(r, op);

			decode_ddddd_pcr_write(ddddd, r);
		}
		else
		{
			DspValue r(m_block);
			decode_ddddd_pcr_read(r, ddddd);

			writeMem<Movec_ea>(op, r);
		}
	}
	void JitOps::op_Movec_aa(TWord op)
	{
		const auto ddddd = getFieldValue<Movec_aa, Field_DDDDD>(op);
		const auto write = getFieldValue<Movec_aa, Field_W>(op);

		if (write)
		{
			DspValue r(m_block);
			readMem<Movec_aa>(r, op);
			decode_ddddd_pcr_write(ddddd, r);
		}
		else
		{
			const auto addr = getFieldValue<Movec_aa, Field_aaaaaa>(op);
			const auto area = getFieldValueMemArea<Movec_aa>(op);

			DspValue r(m_block);
			decode_ddddd_pcr_read(r, ddddd);
			m_block.mem().writeDspMemory(area, addr, r);
		}
	}
	void JitOps::op_Movec_S1D2(TWord op)
	{
		const auto write = getFieldValue<Movec_S1D2, Field_W>(op);
		const auto eeeeee = getFieldValue<Movec_S1D2, Field_eeeeee>(op);
		const auto ddddd = getFieldValue<Movec_S1D2, Field_DDDDD>(op);

		DspValue r(m_block);

		if (write)
		{
			decode_dddddd_read(r, eeeeee);
			decode_ddddd_pcr_write(ddddd, r);
		}
		else
		{
			decode_ddddd_pcr_read(r, ddddd);
			decode_dddddd_write(eeeeee, r);
		}
	}

	void JitOps::op_Movec_xx(TWord op)
	{
		const auto iiiiiiii = getFieldValue<Movec_xx, Field_iiiiiiii>(op);
		const auto ddddd = getFieldValue<Movec_xx, Field_DDDDD>(op);

		const DspValue r(m_block, iiiiiiii, DspValue::Immediate24);
		decode_ddddd_pcr_write(ddddd, r);
	}

	void JitOps::op_Movem_ea(TWord op)
	{
		const auto write = getFieldValue<Movem_ea, Field_W>(op);
		const auto dddddd = getFieldValue<Movem_ea, Field_dddddd>(op);

		if (write)
		{
			copy24ToDDDDDD<Movem_ea>(op, dddddd, UsePooledTemp, [&](DspValue& r)
			{
				readMem<Movem_ea>(r, op, MemArea_P);
			});
		}
		else
		{
			DspValue r(m_block, UsePooledTemp);

			decode_dddddd_read(r, dddddd);

			writePmem<Movem_ea>(op, r);
		}
	}

	void JitOps::op_Movep_ppea(TWord op)
	{
		const TWord pp = getFieldValue<Movep_ppea, Field_pppppp>(op) + 0xffffc0;
		const TWord mmmrrr = getFieldValue<Movep_ppea, Field_MMM, Field_RRR>(op);
		const auto write = getFieldValue<Movep_ppea, Field_W>(op);
		const EMemArea sp = getFieldValue<Movep_ppea, Field_s>(op) ? MemArea_Y : MemArea_X;
		const EMemArea sm = getFieldValueMemArea<Movep_ppea>(op);

		auto ea = effectiveAddress<Movep_ppea>(op);

		if (write)
		{
			if (mmmrrr == MMMRRR_ImmediateData)
			{
				m_block.mem().writePeriph(sp, pp, ea);
			}
			else
			{
				DspValue r(m_block);
				readMemOrPeriph(r, sm, ea, Movep_ppea);
				m_block.mem().writePeriph(sp, pp, r);
			}
		}
		else
		{
			DspValue r(m_block);
			m_block.mem().readPeriph(r, sp, pp, Movep_ppea);
			writeMemOrPeriph(sm, ea, r);
		}
	}

	void JitOps::op_Movep_Xqqea(TWord op)
	{
		movep_qqea<Movep_Xqqea>(op, MemArea_X);
	}
	void JitOps::op_Movep_Yqqea(TWord op)
	{
		movep_qqea<Movep_Yqqea>(op, MemArea_Y);
	}

	void JitOps::op_Movep_eapp(TWord op)
	{
		// 0000100sW1MMMRRR01pppppp
		const TWord		pppppp		= getFieldValue<Movep_eapp,Field_pppppp>(op);
		const EMemArea	periphArea	= getFieldValue<Movep_eapp,Field_s>(op) ? MemArea_Y : MemArea_X;
		const auto		write		= getFieldValue<Movep_eapp,Field_W>(op);

		DspValue v(m_block);

		if(write)
		{
			readMem<Movep_eapp>(v, op, MemArea_P);
			m_block.mem().writePeriph(periphArea, pppppp + 0xffffc0, v);
		}
		else
		{
			m_block.mem().readPeriph(v, periphArea, pppppp + 0xffffc0, Movep_eapp);

			writePmem<Movep_eapp>(op, v);
		}
	}

	void JitOps::op_Movep_SXqq(TWord op)
	{
		movep_sqq<Movep_SXqq>(op, MemArea_X);
	}

	void JitOps::op_Movep_SYqq(TWord op)
	{
		movep_sqq<Movep_SYqq>(op, MemArea_Y);
	}

	void JitOps::op_Movep_Spp(TWord op)
	{
		const TWord pppppp = getFieldValue<Movep_Spp, Field_pppppp>(op) + 0xffffc0;
		const TWord dddddd = getFieldValue<Movep_Spp, Field_dddddd>(op);
		const EMemArea area = getFieldValue<Movep_Spp, Field_s>(op) ? MemArea_Y : MemArea_X;
		const auto	write = getFieldValue<Movep_Spp, Field_W>(op);

		if (write)
		{
			DspValue r(m_block);
			decode_dddddd_read(r, dddddd);
			m_block.mem().writePeriph(area, pppppp, r);
		}
		else
		{
			copy24ToDDDDDD(dddddd, false, [&](DspValue& r)
			{
				m_block.mem().readPeriph(r, area, pppppp, Movep_Spp);
			}, false);
		}
	}

	void JitOps::copy24ToDDDDDD(const TWord _dddddd, const bool _usePooledTemp, const std::function<void(DspValue&)>& _readCallback, bool _readReg)
	{
		DspValue writeRef = decode_dddddd_ref(_dddddd, _readReg, true);

		if(writeRef.isRegValid() && writeRef.getBitCount() == 56)
		{
			const auto myReg = writeRef.getDspReg().dspReg();

			writeRef.reinterpretAs(DspValue::DspReg24, 24);
			_readCallback(writeRef);

			// read callback might return its own direct reference to a DSP register (for example: move r1,a), in this case we need to process it the regular way
			if(writeRef.getDspReg().dspReg() != myReg)
			{
				assert(writeRef.getBitCount() == 24);
				decode_dddddd_write(_dddddd, writeRef);
				return;
			}

			writeRef.convertTo56(writeRef.get(), writeRef.get());
			writeRef.reinterpretAs(DspValue::DspReg56, 56);
		}
		else if(writeRef.isRegValid() && writeRef.getBitCount() == 24)
		{
			const auto myReg = writeRef.getDspReg().dspReg();
			_readCallback(writeRef);

			// read callback might return its own direct reference to a DSP register (for example: move r1,a), in this case we need to process it the regular way
			if(writeRef.getDspReg().dspReg() != myReg)
			{
				assert(writeRef.getDspReg().dspReg() != JitDspRegPool::DspRegInvalid);
				decode_dddddd_write(_dddddd, writeRef);
			}

			const auto targetReg = writeRef.getDspReg().dspReg();

			if(targetReg >= JitDspRegPool::DspM0 && targetReg <= JitDspRegPool::DspM7)
				m_block.regs().setM(targetReg - JitDspRegPool::DspM0, writeRef);
		}
		else
		{
			writeRef.release();

			DspValue r(m_block, _usePooledTemp);
			_readCallback(r);
			decode_dddddd_write(_dddddd, r);
		}
	}
}