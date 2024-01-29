#pragma once

#include "jitops.h"
#include "jitops_mem.inl"

namespace dsp56k
{
	template<Instruction Inst> void JitOps::move_ddddd_MMMRRR(const TWord _op, const EMemArea _area)
	{
		const TWord ddddd = getFieldValue<Inst,Field_dd, Field_ddd>(_op);
		const auto mmm = getFieldValue<Inst,Field_MMM>(_op);
		const auto rrr = getFieldValue<Inst,Field_RRR>(_op);
		const auto write = getFieldValue<Inst,Field_W>(_op);

		if (write)
		{
			copy24ToDDDDDD<Inst>(_op, ddddd, UsePooledTemp, [&](DspValue& r)
			{
				readMem<Inst>(r, _op, _area);
			});
		}
		else
		{
			if((m_repMode == RepLoop || m_repMode == RepLast) && !hasAluOp(_op))
			{
				writeMem<Inst>(_op, _area, m_repTemps.front());
			}
			else
			{
				DspValue r(m_block, UsePooledTemp);

				decode_dddddd_read(r, ddddd);

				writeMem<Inst>(_op, _area, r);

				if(m_repMode == RepFirst && !hasAluOp(_op))
					m_repTemps.emplace_back(std::move(r));
			}
		}
	}
	template<Instruction Inst> void JitOps::move_ddddd_absAddr(TWord _op, const EMemArea _area)
	{
		const auto ddddd = getFieldValue<Inst,Field_dd, Field_ddd>(_op);
		const auto write = getFieldValue<Inst,Field_W>(_op);
		
		if (write)
		{
			copy24ToDDDDDD<Inst>(_op, ddddd, false, [&](DspValue& r)
			{
				readMem<Inst>(r, _op, _area);
			});
		}
		else
		{
			if((m_repMode == RepLoop || m_repMode == RepLast) && !hasAluOp(_op))
			{
				writeMem<Inst>(_op, _area, m_repTemps.front());
			}
			else
			{
				DspValue r(m_block);

				decode_dddddd_read(r, ddddd);
				writeMem<Inst>(_op, _area, r);

				if(m_repMode == RepFirst && !hasAluOp(_op))
					m_repTemps.emplace_back(std::move(r));
			}
		}
	}

	template<Instruction Inst> void JitOps::move_Rnxxxx(TWord op, EMemArea _area)
	{
		const TWord DDDDDD	= getFieldValue<Inst,Field_DDDDDD>(op);
		const auto	write	= getFieldValue<Inst,Field_W>(op);
		const TWord rrr		= getFieldValue<Inst,Field_RRR>(op);

		const int shortDisplacement = pcRelativeAddressExt<Inst>();

		const auto ea = decode_RRR_read(rrr, shortDisplacement);

		if( write )
		{
			copy24ToDDDDDD<Inst>(op, DDDDDD, false, [&](DspValue& r)
			{
				readMemOrPeriph(r, _area, ea, Inst);
			});
		}
		else
		{
			DspValue value(m_block);
			decode_dddddd_read(value, DDDDDD);
			writeMemOrPeriph(_area, ea, value);
		}
	}

	template<Instruction Inst> void JitOps::move_Rnxxx(TWord op, EMemArea _area)
	{
		const TWord ddddd	= getFieldValue<Inst,Field_DDDD>(op);
		const auto write	= getFieldValue<Inst,Field_W>(op);
		const auto aaaaaaa	= getFieldValue<Inst,Field_aaaaaa, Field_a>(op);
		const auto rrr		= getFieldValue<Inst,Field_RRR>(op);

		const int shortDisplacement = signextend<int,7>(aaaaaaa);

		const auto ea = decode_RRR_read(rrr, shortDisplacement);

		if( write )
		{
			copy24ToDDDDDD(ddddd, false, [&](DspValue& r)
			{
				readMemOrPeriph(r, _area, ea, Inst);
			});
		}
		else
		{
			DspValue r(m_block);
			decode_dddddd_read(r, ddddd);
			writeMemOrPeriph(_area, ea, r);
		}
	}

	template<Instruction Inst> void JitOps::move_L(TWord op)
	{
		const auto LLL		= getFieldValue<Inst,Field_L, Field_LL>(op);
		const auto write	= getFieldValue<Inst,Field_W>(op);
		const auto eaType	= effectiveAddressType<Inst>(op);

		DspValue x(m_block, UsePooledTemp);
		DspValue y(m_block);

		if( write )
		{
			switch (eaType)
			{
			case Immediate:
				x.set(getOpWordB(), DspValue::Immediate24);
				y.set(m_opWordB, DspValue::Immediate24);
				x.toTemp();	// very unlikely that this even happens, decode_LLL_write doesn't handle immediates
				y.toTemp();
				break;
			case Peripherals:
				m_block.mem().readPeriph(x, MemArea_X, getOpWordB(), Inst);
				m_block.mem().readPeriph(y, MemArea_Y, m_opWordB, Inst);
				break;
			case Memory:
				m_block.mem().readDspMemory(x, y, getOpWordB());
				break;
			case Dynamic:
				{
					const auto ea = effectiveAddress<Inst>(op);
					m_block.mem().readDspMemory(x, y, ea);
//					readMemOrPeriph(x, MemArea_X, ea, Inst);
//					readMemOrPeriph(y, MemArea_Y, ea, Inst);
				}
				break;
			}

			decode_LLL_write(LLL, std::move(x), std::move(y));
		}
		else
		{
			auto writeXY = [this, eaType, op](DspValue& _x, DspValue& _y)
			{
				switch (eaType)
				{
				case Immediate:
					assert(false && "unable to write to immediate");
					getOpWordB();
					break;
				case Peripherals:
					m_block.mem().writePeriph(MemArea_X, getOpWordB(), _x);
					m_block.mem().writePeriph(MemArea_Y, m_opWordB, _y);
					break;
				case Memory:
					m_block.mem().writeDspMemory(getOpWordB(), _x, _y);
					break;
				case Dynamic:
					{
						const auto ea = effectiveAddress<Inst>(op);
						m_block.mem().writeDspMemory(ea, _x, _y);
	//					writeMemOrPeriph(MemArea_X, ea, _x);
	//					writeMemOrPeriph(MemArea_Y, ea, _y);
					}
					break;
				}
			};

			if((m_repMode == RepLoop || m_repMode == RepLast) && !hasAluOp(op))
			{
				writeXY(m_repTemps[0], m_repTemps[1]);
			}
			else
			{
				decode_LLL_read(LLL, x, y);
				writeXY(x,y);

				if(m_repMode == RepFirst && !hasAluOp(op))
				{
					m_repTemps.emplace_back(std::move(x));
					m_repTemps.emplace_back(std::move(y));
				}
			}
		}
	}

	template <Instruction Inst> void JitOps::movep_qqea(TWord op, const EMemArea _area)
	{
		const auto qAddr	= getFieldValue<Inst,Field_qqqqqq>(op) + 0xffff80;
		const auto write	= getFieldValue<Inst,Field_W>(op);

		DspValue r(m_block);

		if( write )
		{
			readMem<Inst>(r, op);
			m_block.mem().writePeriph(_area, qAddr, r);
		}
		else
		{
			m_block.mem().readPeriph(r, _area, qAddr, Inst);
			writeMem<Inst>(op, r);
		}
	}

	template <Instruction Inst> void JitOps::movep_sqq(TWord op, const EMemArea _area)
	{
		const TWord addr	= getFieldValue<Inst,Field_q, Field_qqqqq>(op) + 0xffff80;
		const TWord dddddd	= getFieldValue<Inst,Field_dddddd>(op);
		const auto	write	= getFieldValue<Inst,Field_W>(op);

		if( write )
		{
			DspValue r(m_block);
			decode_dddddd_read(r, dddddd);
			m_block.mem().writePeriph(_area, addr, r);
		}
		else
		{
			copy24ToDDDDDD(dddddd, false, [&](DspValue& r)
			{
				m_block.mem().readPeriph(r,_area, addr, Inst);
			});
		}
	}
	template<Instruction Inst> void JitOps::copy24ToDDDDDD(const TWord _opA, const TWord _dddddd, bool _usePooledTemp, const std::function<void(DspValue&)>& _readCallback)
	{
		const auto regWrite = getRegisterDDDDDD(_dddddd);

		auto read = RegisterMask::None;
		auto written = RegisterMask::None;

		getRegisters(written, read, Inst, _opA);

		const auto writeMask = convert(regWrite);

		const auto readWrite = (writeMask & read) != RegisterMask::None;

		copy24ToDDDDDD(_dddddd, _usePooledTemp, _readCallback, readWrite);
	}
}
