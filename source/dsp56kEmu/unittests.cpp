#include "unittests.h"

namespace dsp56k
{
	static DefaultMemoryValidator g_defaultMemoryValidator;

	UnitTests::UnitTests()
		: mem(g_defaultMemoryValidator, 0x100)
		, dsp(mem, &peripheralsX, &peripheralsY)
	{
	}

	void UnitTests::runAllTests()
	{
		aguModulo();
		aguMultiWrapModulo();
		aguBitreverse();

		abs();
		add();
		addShortImmediate();
		addLongImmediate();
		addl();
		addr();
		and_();
		andi();
		asl_D();
		asl_ii();
		asl_S1S2D();
		asr_D();
		asr_ii();
		asr_S1S2D();

		bchg_aa();
		bclr_ea();
		bclr_aa();
		bclr_qqpp();
		bclr_D();
		bset_aa();
		btst_aa();

		clr();
		cmp();
		dec();
		dmac();
		eor();
		extractu();
		ifcc();
		inc();
		insert();
		lra();
		lsl();
		lsr();
		lua_ea();
		lua_rn();
		mac_S();
		max();
		maxm();
		mpy();
		mpyr();
		mpy_SD();
		neg();
		not_();
		or_();
		ori();
		rnd();
		rol();
		sub();
		tfr();
		tcc();

		move();
		parallel();
	}

	void UnitTests::aguModulo()
	{
		runTest([&]()
		{
			dsp.set_m(0, 0x000fff);
			dsp.regs().r[0].var = 0x000f00;
			dsp.regs().n[0].var = 0x000200;

			emit(0x204800);	// move (r0)+n0
		}, [&]()
		{
			verify(dsp.regs().r[0] == 0x000100);
		});

		runTest([&]()
		{
			// edge case where N = modulo size but not block size
			dsp.set_m(5, 0x003ffd);
			dsp.regs().r[5].var = 0x09c000;
			dsp.regs().n[5].var = 0x003ffe;

			emit(0x204500);	// move (r5)-n5
		}, [&]()
		{
			verify(dsp.regs().r[5] == 0x9c000);
		});

		runTest([&]()
		{
			dsp.set_m(5, 0x003ffd);
			dsp.regs().r[5].var = 0x09c000;
			dsp.regs().n[5].var = 0x001000;

			emit(0x204500);	// move (r5)-n5
		}, [&]()
		{
			verify(dsp.regs().r[5] == 0x09effe);
		});

		runTest([&]()
		{
			// edge case where N is the size of a block
			dsp.set_m(5, 0x003ffd);
			dsp.regs().r[5].var = 0x09c000;
			dsp.regs().n[5].var = 0x004000;

			emit(0x204d00);	// move (r5)+n5
		}, [&]()
		{
			verify(dsp.regs().r[5] == 0x0a0000);
		});

		runTest([&]()
		{
			// undefined behaviour, tested in the simulator. It does modulo where masked and not-modulo outside of the mask
			dsp.set_m(5, 0x000080);
			dsp.regs().r[5].var = 0x000000;
			dsp.regs().n[5].var = 0x000190;

			emit(0x204d00);	// move (r5)+n5
		}, [&]()
		{
			verify(dsp.regs().r[5] == 0x00010f);
		});

		runTest([&]()
		{
			// negative n
			dsp.set_m(0, 0x003ffd);
			dsp.regs().r[0].var = 0x0bbc3a;
			dsp.regs().n[0].var = 0xffe9c7;

			emit(0x204800);	// move (r0)+n0
		}, [&]()
		{
			verify(dsp.regs().r[0] == 0x0ba601);
		});
	}

	void UnitTests::aguMultiWrapModulo()
	{
		for(uint32_t i=0; i<0x200; ++i)
		{
			runTest([&]()
			{
				dsp.set_m(0, 0x0080ff);
				dsp.regs().r[0].var = 0x123400 + (i & 0xff);

				dsp.set_m(1, 0x0080ff);
				dsp.regs().r[1].var = 0x123400 + (i & 0xff);
				dsp.regs().n[1].var = 0x88;

				dsp.set_m(2, 0x0080ff);
				dsp.regs().r[2].var = 0x123400 + (i & 0xff);
				dsp.regs().n[2].var = 0x100;

				dsp.set_m(3, 0x0080ff);
				dsp.regs().r[3].var = 0x123400;
				dsp.regs().n[3].var = i;

				dsp.set_m(4, 0x0080ff);
				dsp.regs().r[4].var = 0x123400 + ((-static_cast<int32_t>(i)) & 0xff);

				emit(0x205800);	// move (r0)+
				emit(0x204900);	// move (r1)+n1
				emit(0x204a00);	// move (r2)+n2
				emit(0x204300);	// move (r3)-n3
				emit(0x205400);	// move (r4)-
			}, [&]()
			{
				verify(dsp.regs().r[0] == 0x123400 + ((i + 1) & 0xff));
				verify(dsp.regs().r[1] == 0x123400 + (((i & 0xff) + 0x88) & 0xff));
				verify(dsp.regs().r[2] == 0x123400 + (i & 0xff));
				verify(dsp.regs().r[3] == 0x123400 + ((-static_cast<int32_t>(i)) & 0xff));
				verify(dsp.regs().r[4] == 0x123400 + ((-static_cast<int32_t>(i) - 1) & 0xff));
			});
		}
		runTest([&]()
		{
			dsp.x0(0x810f);

			emit(0x04c4a1);	// move x0,m1
			emit(0x04c4a1);	// move x0,m2

			dsp.regs().r[1].var = 0x3c8;
			dsp.regs().n[1].var = 5;
			dsp.set_m(1, 0x801f);

			dsp.regs().r[2].var = 0x3c8;
			dsp.regs().n[2].var = 1;
			dsp.set_m(2, 0x801f);

			emit(0x204900);	// move (r1)+n1
			emit(0x204a00);	// move (r2)+n2
		}, [&]()
		{
			verify(dsp.regs().r[1] == 0x3cd);
			verify(dsp.regs().r[2] == 0x3c9);
		});
	}

	void UnitTests::aguBitreverse()
	{
		static_assert(bitreverse24(0x800000) == 0x000001, "bitreverse function not working");
		static_assert(bitreverse24(0x400000) == 0x000002, "bitreverse function not working");
		static_assert(bitreverse24(0x200000) == 0x000004, "bitreverse function not working");
		static_assert(bitreverse24(0x100000) == 0x000008, "bitreverse function not working");

		static_assert(bitreverse24(0x080000) == 0x000010, "bitreverse function not working");
		static_assert(bitreverse24(0x040000) == 0x000020, "bitreverse function not working");
		static_assert(bitreverse24(0x020000) == 0x000040, "bitreverse function not working");
		static_assert(bitreverse24(0x010000) == 0x000080, "bitreverse function not working");

		static_assert(bitreverse24(0x008000) == 0x000100, "bitreverse function not working");
		static_assert(bitreverse24(0x004000) == 0x000200, "bitreverse function not working");
		static_assert(bitreverse24(0x002000) == 0x000400, "bitreverse function not working");
		static_assert(bitreverse24(0x001000) == 0x000800, "bitreverse function not working");

		static_assert(bitreverse24(0x000001) == 0x800000, "bitreverse function not working");
		static_assert(bitreverse24(0x000002) == 0x400000, "bitreverse function not working");
		static_assert(bitreverse24(0x000004) == 0x200000, "bitreverse function not working");
		static_assert(bitreverse24(0x000008) == 0x100000, "bitreverse function not working");

		static_assert(bitreverse24(0x000010) == 0x080000, "bitreverse function not working");
		static_assert(bitreverse24(0x000020) == 0x040000, "bitreverse function not working");
		static_assert(bitreverse24(0x000040) == 0x020000, "bitreverse function not working");
		static_assert(bitreverse24(0x000080) == 0x010000, "bitreverse function not working");

		static_assert(bitreverse24(0x000100) == 0x008000, "bitreverse function not working");
		static_assert(bitreverse24(0x000200) == 0x004000, "bitreverse function not working");
		static_assert(bitreverse24(0x000400) == 0x002000, "bitreverse function not working");
		static_assert(bitreverse24(0x000800) == 0x001000, "bitreverse function not working");

		auto run = [&](const TWord _rInit, const TWord _rInc, const TWord _expectedResult, bool _add)
		{
			runTest([&]()
			{
				dsp.set_m(0, 0);
				dsp.regs().r[0].var = _rInit;
				dsp.regs().n[0].var = _rInc;
				if(_add)
					emit(0x204800);	// move (r0)+n0
				else
					emit(0x204000);	// move (r0)-n0
			}, [&]()
			{
				verify(dsp.regs().r[0] == _expectedResult);
			});
		};

		run(0, 1, 1, true);
		run(1, 1, 0, true);

		run(0, 1, 1, false);
		run(1, 1, 0, false);

		run(0xaabbcc, 0x123456, 0xb99079, true);
		run(0xaabbcc, 0x123456, 0xb08d93, false);
	}

	void UnitTests::abs()
	{
		runTest([&]()
		{
			dsp.regs().a.var = 0x00ff112233445566;
			dsp.regs().b.var = 0x0000aabbccddeeff;

			emit(0x200026);	// abs a
			emit(0x20002e);	// abs b
		}, [&]()
		{
			verify(dsp.regs().a == 0x00EEDDCCBBAA9A);
			verify(dsp.regs().b == 0x0000aabbccddeeff);
		});
	}

	void UnitTests::add()
	{
		runTest([&]()
		{
			dsp.regs().a.var = 0x0001e000000000;
			dsp.regs().b.var = 0xfffe2000000000;

			// add b,a
			emit(0x200010);
		}, [&]()
		{
			verify(dsp.regs().a.var == 0);
			verify(dsp.sr_test(CCR_C));
			verify(dsp.sr_test(CCR_Z));
			verify(!dsp.sr_test(CCR_V));
		});
	}

	void UnitTests::addShortImmediate()
	{
		runTest([&]()
		{
			dsp.regs().a.var = 0;

			// add #<32,a
			emit(0x017280);
		}, [&]()
		{
			verify(dsp.regs().a.var == 0x00000032000000);
			verify(!dsp.sr_test(CCR_C));
			verify(!dsp.sr_test(CCR_Z));
			verify(!dsp.sr_test(CCR_V));
		});
	}

	void UnitTests::addLongImmediate()
	{
		runTest([&]()
		{
			dsp.regs().a.var = 0;
			dsp.regs().pc.var = 0;

			// add #>32,a, two op add with immediate in extension word
			emit(0x0140c0, 0x000032);
		}, [&]()
		{
			verify(dsp.regs().a.var == 0x00000032000000);
			verify(!dsp.sr_test(CCR_C));
			verify(!dsp.sr_test(CCR_Z));
			verify(!dsp.sr_test(CCR_V));
		});
	}

	void UnitTests::addl()
	{
		runTest([&]()
		{
			dsp.regs().a.var = 0x222222;
			dsp.regs().b.var = 0x333333;

			emit(0x20001a);
		}, [&]()
		{
			verify(dsp.regs().b.var == 0x888888);
			verify(!dsp.sr_test(CCR_C));
			verify(!dsp.sr_test(CCR_Z));
			verify(!dsp.sr_test(CCR_V));
		});
	}

	void UnitTests::addr()
	{
		runTest([&]()
		{
			dsp.regs().a.var = 0x004edffe000000;
			dsp.regs().b.var = 0xff89fe13000000;
			dsp.setSR(0x0800d0);							// (S L) U

			emit(0x200002);	// addr b,a
		}, [&]()
		{
			verify(dsp.regs().a.var == 0x0ffb16e12000000);
			verify(dsp.getSR().var == 0x0800c8);			// (S L) N
		});
	}

	void UnitTests::and_()
	{
		runTest([&]()
		{
			dsp.regs().a.var = 0xffcccccc112233;
			dsp.regs().x.var = 0x777777;

			dsp.regs().b.var = 0xaaaabbcc334455;
			dsp.regs().y.var = 0x667788000000;

			emit(0x200046);	// and x0,a
			emit(0x20007e);	// and y1,b
		}, [&]()
		{
			verify(dsp.regs().a.var == 0xff444444112233);
			verify(dsp.regs().b.var == 0xaa223388334455);
		});
	}

	void UnitTests::andi()
	{
		const auto srBackup = dsp.regs().sr;

		runTest([&]()
		{
			dsp.regs().omr.var = 0xff6666;
			dsp.regs().sr.var = 0xff4666;

			emit(0x0033ba);	// andi #$33,omr
			emit(0x0033bb);	// andi #$33,eom
			emit(0x0033b8);	// andi #$33,mr
			emit(0x0033b9);	// andi #$33,ccr
		}, [&]()
		{
			verify(dsp.regs().omr.var == 0xff2222);
			verify(dsp.regs().sr.var == 0xff0222);
		});

		dsp.setSR(srBackup);
	}

	void UnitTests::asl_D()
	{
		runTest([&]()
		{
			dsp.regs().a.var = 0xaaabcdef123456;
			dsp.regs().sr.var = 0;

			emit(0x200032);	// asl a
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0x55579bde2468ac);
			verify(!dsp.sr_test_noCache(CCR_Z));
			verify(dsp.sr_test_noCache(CCR_V));
			verify(dsp.sr_test_noCache(CCR_C));
		});

		runTest([&]()
		{
			dsp.regs().a.var = 0x00400000000000;
			dsp.regs().sr.var = 0;
			emit(0x200032);	// asl a
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0x00800000000000);
			verify(!dsp.sr_test_noCache(CCR_Z));
			verify(!dsp.sr_test_noCache(CCR_V));
			verify(!dsp.sr_test_noCache(CCR_C));
		});
	}

	void UnitTests::asl_ii()
	{
		runTest([&]()
		{
			dsp.regs().a.var = 0xaaabcdef123456;
			dsp.regs().sr.var = 0;
			emit(0x0c1d02);	// asl #1,a,a
		}, [&]()
		{
			verify(dsp.regs().a.var == 0x55579bde2468ac);
			verify(!dsp.sr_test_noCache(CCR_Z));
			verify(dsp.sr_test_noCache(CCR_V));
			verify(dsp.sr_test_noCache(CCR_C));
		});
	}

	void UnitTests::asl_S1S2D()
	{
		runTest([&]()
		{
			dsp.regs().x.var = ~0;
			dsp.regs().y.var = ~0;
			dsp.x0(0x4);
			dsp.y1(0x8);

			dsp.regs().a.var = 0x0011aabbccddeeff;
			dsp.regs().b.var = 0x00ff112233445566;

			emit(0x0c1e48);	// asl x0,a,a
			emit(0x0c1e5f);	// asl y1,b,b
		}, [&]()
		{
			verify(dsp.regs().a.var == 0x001aabbccddeeff0);
			verify(dsp.regs().b.var == 0x0011223344556600);
		});
	}

	void UnitTests::asr_D()
	{
		runTest([&]()
		{
			dsp.regs().a.var = 0x000599f2204000;
			dsp.regs().sr.var = 0;

			emit(0x200022);	// asr a
		}, [&]()
		{
			verify(dsp.regs().a.var == 0x0002ccf9102000);
		});
	}

	void UnitTests::asr_ii()
	{
		runTest([&]()
		{
			dsp.regs().a.var = 0x000599f2204000;
			emit(0x0c1c02);	// asr #1,a,a
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0x0002ccf9102000);
		});

		runTest([&]()
		{
			dsp.regs().a.var = 0xfffffdff000000;
			emit(0x0c1c2a);	// asr #15,a,a
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0xffffffffffeff8);
		});
	}

	void UnitTests::asr_S1S2D()
	{
		runTest([&]()
		{
			dsp.regs().x.var = ~0;
			dsp.regs().y.var = ~0;
			dsp.x0(0x4);
			dsp.y1(0x8);

			dsp.regs().a.var = 0x0011aabbccddeeff;
			dsp.regs().b.var = 0x00ff112233445566;

			emit(0x0c1e68);	// asr x0,a,a
			emit(0x0c1e7f);	// asr y1,b,b
		}, [&]()
		{
			verify(dsp.regs().a.var == 0x00011aabbccddeef);
			verify(dsp.regs().b.var == 0x00ffff1122334455);
		});

		runTest([&]()
		{
			dsp.regs().y.var = ~0;
			dsp.y1(0x9);

			dsp.regs().a.var = 0x00000200000000;
			dsp.regs().b.var = 0x00000007000000;

			emit(0x0c1e6f);	// asr y1,a,b
		}, [&]()
		{
			verify(dsp.regs().a.var == 0x00000200000000);
			verify(dsp.regs().b.var == 0x00000001000000);
		});
	}

	void UnitTests::bchg_aa()
	{
		runTest([&]()
		{
			dsp.memory().set(MemArea_X, 0x2, 0x556677);
			dsp.memory().set(MemArea_Y, 0x3, 0xddeeff);

			emit(0x0b0203);	// bchg #$3,x:<$2
			emit(0x0b0343);	// bchg #$3,y:<$3
		}, [&]()
		{
			const auto x = dsp.memory().get(MemArea_X, 0x2);
			const auto y = dsp.memory().get(MemArea_Y, 0x3);
			verify(x == 0x55667f);
			verify(y == 0xddeef7);
		});
	}

	void UnitTests::bclr_ea()
	{
		runTest([&]()
		{
			dsp.memory().set(MemArea_X, 0x11, 0xffffff);
			dsp.memory().set(MemArea_Y, 0x22, 0xffffff);

			dsp.regs().r[0].var = 0x11;
			dsp.regs().r[1].var = 0x22;

			dsp.regs().n[0].var = dsp.regs().n[1].var = 0;
			dsp.set_m(0, 0xffffff); dsp.set_m(1, 0xffffff);

			emit(0xa6014);	// bclr #$14,x:(r0)
			emit(0xa6150);	// bclr #$10,y:(r1)
		}, [&]()
		{
			const auto x = dsp.memory().get(MemArea_X, 0x11);
			const auto y = dsp.memory().get(MemArea_Y, 0x22);
			verify(x == 0xefffff);
			verify(y == 0xfeffff);
		});
	}

	void UnitTests::bclr_aa()
	{
		runTest([&]()
		{
			dsp.memory().set(MemArea_X, 0x11, 0xffaaaa);
			dsp.memory().set(MemArea_Y, 0x22, 0xffbbbb);

			emit(0xa1114);	// bclr #$14,x:<$11
			emit(0xa2250);	// bclr #$10,y:<$22
		}, [&]()
		{
			const auto x = dsp.memory().get(MemArea_X, 0x11);
			const auto y = dsp.memory().get(MemArea_Y, 0x22);
			verify(x == 0xefaaaa);
			verify(y == 0xfebbbb);
		});
	}

	void UnitTests::bclr_qqpp()
	{
		runTest([&]()
		{
			dsp.getPeriph(0)->write(0xffff90, 0x334455);
			dsp.getPeriph(0)->write(0xffffd0, 0x556677);

			emit(0x11002);	// bclr #$2,x:<<$ffff90	- bclr_qq
			emit(0xa9004);	// bclr #$4,x:<<$ffffd0 - bclr_pp
		}, [&]()
		{
			const auto a = dsp.getPeriph(0)->read(0xffff90, Bclr_qq);
			const auto b = dsp.getPeriph(0)->read(0xffffd0, Bclr_pp);
			verify(a == 0x334451);	// bit 2 cleared
			verify(b == 0x556667);	// bit 4 cleared
		});
	}

	void UnitTests::bclr_D()
	{
		runTest([&]()
		{
			dsp.regs().omr.var = 0xddeeff;
			emit(0x0afa47);	// bclr #$7,omr
		}, [&]()
		{
			verify(dsp.regs().omr.var == 0xddee7f);
		});
	}

	void UnitTests::bset_aa()
	{
		runTest([&]()
		{
			dsp.memory().set(MemArea_X, 0x2, 0x55667f);
			dsp.memory().set(MemArea_Y, 0x3, 0xddeef0);

			emit(0x0a0223);	// bset #$3,x:<$2
			emit(0x0a0363);	// bset #$3,y:<$3
		}, [&]()
		{
			const auto x = dsp.memory().get(MemArea_X, 0x2);
			const auto y = dsp.memory().get(MemArea_Y, 0x3);
			verify(x == 0x55667f);
			verify(y == 0xddeef8);
		});
	}

	void UnitTests::btst_aa()
	{
		runTest([&]()
		{
			dsp.memory().set(MemArea_X, 0x2, 0xaabbc4);

			emit(0x0b0222);	// btst #$2,x:<$2
		}, [&]()
		{
			verify(dsp.sr_test(CCR_C));
		});

		runTest([&]()
		{
			emit(0x0b0223);	// btst #$3,x:<$2
		}, [&]()
		{
			verify(!dsp.sr_test(CCR_C));
		});
	}

	void UnitTests::clr()
	{
		runTest([&]()
		{
			dsp.regs().b.var = 0x99aabbccddeeff;
			dsp.x0(0);
			dsp.regs().sr.var = 0x080000;

			emit(0x44f41b, 0x000128);		// clr b #>$128,x0
		},
			[&]()
		{
			verify(dsp.regs().b == 0);
			verify(dsp.x0() == 0x128);
			verify(dsp.sr_test(CCR_U));
			verify(dsp.sr_test(CCR_Z));
			verify(!dsp.sr_test(CCR_E));
			verify(!dsp.sr_test(CCR_N));
			verify(!dsp.sr_test(CCR_V));
		});

		runTest([&]()
		{
			dsp.regs().a.var = 0xbada55c0deba5e;
			emit(0x200013);					// clr a
		},
			[&]()
		{
			verify(dsp.regs().a == 0);
		});
	}

	void UnitTests::cmp()
	{
		runTest([&]()
		{
			dsp.regs().b.var = 0;
			dsp.b1(TReg24(0x123456));

			dsp.regs().x.var = 0;
			dsp.x0(TReg24(0x123456));

			emit(0x20004d);		// cmp x0,b
		},
			[&]()
		{
			verify(dsp.sr_test(CCR_Z));
			verify(!dsp.sr_test(CCR_N));
			verify(!dsp.sr_test(CCR_E));
			verify(!dsp.sr_test(CCR_V));
			verify(!dsp.sr_test(CCR_C));
		});
		runTest([&]()
		{
			dsp.x0(0xf00000);
			dsp.regs().a.var = 0xfff40000000000;
			dsp.setSR(0x0800d8);

			emit(0x200045);	// cmp x0,a
		},
			[&]()
		{
			verify(dsp.getSR().var == 0x0800d0);
		});
		runTest([&]()
		{

			dsp.setSR(0x080099);
			dsp.regs().a.var = 0xfffffc6c000000;
			emit(0x0140c5, 0x0000aa);	// cmp #>$aa,a
		},
			[&]()
		{
			verify(dsp.getSR().var == 0x080098);
		});
	}

	void UnitTests::dec()
	{
		runTest([&]()
		{
			dsp.regs().a.var = 2;
			emit(0x00000a);		// dec a
		},
			[&]()
		{
			verify(dsp.regs().a.var == 1);
			verify(!dsp.sr_test(CCR_Z));
			verify(!dsp.sr_test(CCR_N));
			verify(!dsp.sr_test(CCR_E));
			verify(!dsp.sr_test(CCR_V));
			verify(!dsp.sr_test(CCR_C));
		});
		runTest([&]()
		{
			dsp.regs().a.var = 1;
			emit(0x00000a);		// dec a
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0);
			verify(dsp.sr_test(CCR_Z));
			verify(!dsp.sr_test(CCR_N));
			verify(!dsp.sr_test(CCR_E));
			verify(!dsp.sr_test(CCR_V));
			verify(!dsp.sr_test(CCR_C));
		});
		runTest([&]()
		{
			dsp.regs().a.var = 0;
			emit(0x00000a);		// dec a
		},
			[&]()
		{
			verify(dsp.sr_test(static_cast<CCRMask>(CCR_N | CCR_C)));
			verify(!dsp.sr_test(static_cast<CCRMask>(CCR_Z | CCR_E | CCR_V)));
		});
	}

	void UnitTests::dmac()
	{
		runTest([&]()
		{
			dsp.regs().a.var = 0;
			dsp.x1(0x000020);
			dsp.y1(0x000020);
			emit(0x01248f);		// dmacss x1,y1,a
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0x800);
		});
		runTest([&]()
		{
			dsp.regs().a.var = 0xfff00000000000;
			dsp.x1(0x000020);
			dsp.y1(0x000020);
			emit(0x01248f);		// dmacss x1,y1,a
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0xfffffffff00800);
		});
		runTest([&]()
		{
			dsp.regs().a.var = 0x005f1bbfa0e440;
			dsp.regs().x.var = 0x015555555555;
			dsp.regs().y.var = 0x0000008ea9a0;
			emit(0x012586);		// dmacsu x1,y0,a
		}, [&]()
		{
			verify(dsp.regs().a.var == 0x00017c6effffff);
		});
	}

	void UnitTests::eor()
	{
		runTest([&]()
		{
			dsp.regs().a.var = 0x0f799428000000;
			dsp.x0(0x799428);

			emit(0x200043);		// eor x0,a
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0x0f000000000000);
		});

		runTest([&]()
		{
			dsp.regs().a.var = 0x0f000428000123;
			dsp.x0(0x799428);

			emit(0x200043);		// eor x0,a
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0x0f799000000123);
		});
	}

	void UnitTests::extractu()
	{
		runTest([&]()
		{
			dsp.regs().x.var = 0x4008000000;  // x1 = 0x4008  (width=4, offset=8)
			dsp.regs().a.var = 0xef00;
			dsp.regs().b.var = 0;

			// extractu x1,a,b  (width = 0x8, offset = 0x28)
			emit(0x0c1a8d);
		},
			[&]()
		{
			verify(dsp.regs().b.var == 0xf);
		});

		runTest([&]()
		{
			dsp.regs().a.var = 0;
			dsp.regs().b.var = 0xfff47555000000;
			dsp.setSR(0x0800d9);

			// extractu $8028,b,a
			emit(0x0c1890, 0x008028);
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0xf4);
			verify(dsp.getSR().var == 0x0800d0);
		});
	}

	void UnitTests::inc()
	{
		runTest([&]()
		{
			dsp.regs().a.var = 0x00ffffffffffffff;
			emit(0x000008);		// inc a
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0);
			verify(dsp.sr_test(static_cast<CCRMask>(CCR_C | CCR_Z)));
			verify(!dsp.sr_test(static_cast<CCRMask>(CCR_N | CCR_E | CCR_V)));
		});
		runTest([&]()
		{
			dsp.regs().a.var = 1;
			emit(0x000008);		// inc a
		},
			[&]()
		{
			verify(dsp.regs().a.var == 2);
			verify(!dsp.sr_test(static_cast<CCRMask>(CCR_Z | CCR_N | CCR_E | CCR_V | CCR_C)));
		});
	}

	void UnitTests::insert()
	{
		runTest([&]()
		{
			dsp.x1(0x123456);
			dsp.regs().a.var = 0x12aabbccddeeff;
			emit(0xc1960, 0x00c008);				// insert #$00c008,x1,a	; use 12 bits from x1 and insert into a at bit 8
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0x12aabbccd456ff);
		});

		runTest([&]()
		{
			dsp.x0(0x010028);						// control reg, 16 bits to position 40
			dsp.y1(0xabcdef);						// source
			dsp.regs().a.var = 0x12123456123456;	// dest
			emit(0xc1b78);							// insert x0,y1,a
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0xcdef3456123456);
		});
	}

	void UnitTests::lra()
	{
		runTest([&]()
		{
			dsp.regs().n[0].var = 0x4711;
			emit(0x044058, 0x00000a, 0x20);				// lra >*+$a,n0
		},
			[&]()
		{
			verify(dsp.regs().n[0].var == 0x2a);
		});
	}

	void UnitTests::lsl()
	{
		runTest([&]()
		{
			dsp.regs().a.var = 0xffaabbcc112233;
			emit(0x200033);				// lsl a
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0xff557798112233);
			verify(dsp.sr_test(CCR_C));
		});

		runTest([&]()
		{
			dsp.regs().a.var = 0xffaabbcc112233;
			emit(0x0c1e88);				// lsl #$4,a
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0xffabbcc0112233);
			verify(!dsp.sr_test(CCR_C));
		});

		runTest([&]()
		{
			dsp.x1(0x4);
			dsp.regs().a.var = 0xab112233445566;
			emit(0x0c1e1c);				// lsl x1,a
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0xab122330445566);
		});

		runTest([&]()
		{
			dsp.x1(0x1c);				// more than 24 bits should move in zeroes
			dsp.regs().a.var = 0xab112233445566;
			emit(0x0c1e1c);				// lsl x1,a
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0xab000000445566);
		});
	}

	void UnitTests::lsr()
	{
		runTest([&]()
		{
			dsp.regs().a.var = 0xffaabbcc112233;
			emit(0x200023);				// lsr a
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0xff555de6112233);
			verify(!dsp.sr_test(CCR_C));
		});

		runTest([&]()
		{
			dsp.regs().a.var = 0xffaabbcc112233;
			emit(0x0c1ec8);				// lsr #$4,a
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0xff0aabbc112233);
			verify(dsp.sr_test(CCR_C));
		});

		runTest([&]()
		{
			dsp.x1(0x4);
			dsp.regs().a.var = 0xab112233445566;
			emit(0x0c1e3c);				// lsr x1,a
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0xab011223445566);
		});

		runTest([&]()
		{
			dsp.x1(0x1c);				// more than 24 bits should move in zeroes
			dsp.regs().a.var = 0xab112233445566;
			emit(0x0c1e3c);				// lsr x1,a
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0xab000000445566);
		});
	}

	void UnitTests::lua_ea()
	{
		runTest([&]()
		{
			dsp.regs().r[0].var = 0x112233;
			dsp.regs().n[0].var = 0x001111;
			emit(0x045818);				// lua (r0)+,n0
		},
			[&]()
		{
			verify(dsp.regs().r[0].var == 0x112233);
			verify(dsp.regs().n[0].var == 0x112234);
		});

		runTest([&]()
		{
			dsp.regs().r[0].var = 0x112233;
			dsp.regs().n[0].var = 0x001111;
			emit(0x044818);				// lua (r0)+n0,n0
		},
			[&]()
		{
			verify(dsp.regs().r[0].var == 0x112233);
			verify(dsp.regs().n[0].var == 0x113344);
		});
	}

	void UnitTests::lua_rn()
	{
		runTest([&]()
		{
			dsp.regs().r[0].var = 0x0000f0;
			dsp.set_m(0, 0xffffff);

			emit(0x04180b);				// lua (r0+$30),n3
		},
			[&]()
		{
			verify(dsp.regs().r[0].var == 0x0000f0);
			verify(dsp.regs().n[3].var == 0x000120);
		});
		runTest([&]()
		{
			dsp.regs().r[0].var = 0x0000f0;
			dsp.set_m(0, 0x0000ff);

			emit(0x04180b);				// lua (r0+$30),n3
		},
			[&]()
		{
			verify(dsp.regs().r[0].var == 0x0000f0);
			verify(dsp.regs().n[3].var == 0x000020);
		});
		runTest([&]()
		{
			dsp.regs().r[0].var = 0x0000f0;
			dsp.set_m(0, 0xffffff);

			emit(0x0430f6);				// lua (r0-$11),r6
		},
			[&]()
		{
			verify(dsp.regs().r[0].var == 0x0000f0);
			verify(dsp.regs().r[6].var == 0x0000df);
		});
	}

	void UnitTests::mac_S()
	{
		runTest([&]()
		{
			dsp.x1(0x2);
			dsp.regs().a.var = 0x100;

			emit(0x0102f2);				// mac x1,#$2,a
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0x00000000800100);
		});
	}

	void UnitTests::max()
	{
		auto run = [&](uint64_t _a, uint64_t _b, bool aIsGreaterEqual)
		{
			runTest([&]()
			{
				dsp.regs().a.var = _a;
				dsp.regs().b.var = _b;

				emit(0x20001d);				// max a,b
			},
				[&]()
			{
				if(aIsGreaterEqual)
				{
					verify(dsp.regs().a.var == _a);
					verify(dsp.regs().b.var == _a);
					assert(!dsp.sr_test(CCR_C));
				}
				else
				{
					verify(dsp.regs().a.var == _a);
					verify(dsp.regs().b.var == _b);
					assert(dsp.sr_test(CCR_C));
				}
			});
		};

		run(1, 1, true);
		run(2, 1, true);
		run(1, 2, false);
		run(0xff112233445566, 0xffffffffffffff, false);
		run(0xffffffffffffff, 0x00123456123456, false);
		run(0x00123456123456, 0xffffffffffffff, true);
	}

	void UnitTests::maxm()
	{
		auto run = [&](int64_t _a, int64_t _b, bool aIsGreaterEqual)
		{
			_a &= 0xff'ffffff'ffffff;
			_b &= 0xff'ffffff'ffffff;

			runTest([&]()
			{
				dsp.regs().a.var = _a;
				dsp.regs().b.var = _b;

				emit(0x200015);				// maxm a,b
			},
				[&]()
			{
				if(aIsGreaterEqual)
				{
					verify(dsp.regs().a.var == _a);
					verify(dsp.regs().b.var == _a);
					assert(!dsp.sr_test(CCR_C));
				}
				else
				{
					verify(dsp.regs().a.var == _a);
					verify(dsp.regs().b.var == _b);
					assert(dsp.sr_test(CCR_C));
				}
			});
		};

		run(1, 1, true);
		run(2, 1, true);
		run(1, 2, false);
		run(-2, 1, true);
		run(-2, -5, false);
		run(0xff112233445566, 0xffffffffffffff, true);
		run(0xffffffffffffff, 0x00123456123456, false);
		run(0x00123456123456, 0xffffffffffffff, true);
	}

	void UnitTests::mpy()
	{
		runTest([&]()
		{
			dsp.x0(0x20);
			dsp.x1(0x20);

			emit(0x2000a0);				// mpy x0,x1,a
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0x000800);
		});

		runTest([&]()
		{
			dsp.x0(0xffffff);
			dsp.x1(0xffffff);

			emit(0x2000a0);				// mpy x0,x1,a
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0x2);
		});
	}

	void UnitTests::mpyr()
	{
		runTest([&]()
		{
			dsp.x0(0xef4e);
			dsp.y0(0x600000);
			dsp.setSR(0x0880d0);
			dsp.regs().omr.var = 0x004380;

			emit(0x2000d1);				// mpyr y0,x0,a
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0x0000b37a000000);
		});
	}

	void UnitTests::mpy_SD()
	{
		runTest([&]()
		{
			dsp.x1(0x2);
			dsp.regs().a.var = 0;

			emit(0x0102f0);				// mpy x1,#$2,a
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0x00000000800000);
		});
	}

	void UnitTests::neg()
	{
		runTest([&]()
		{
			dsp.regs().a.var = 1;

			emit(0x200036);				// neg a
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0xffffffffffffff);
			verify(dsp.sr_test(CCR_N));
			verify(!dsp.sr_test(CCR_Z));
		});

		runTest([&]()
		{
			dsp.regs().a.var = 0xfffffffffffffe;

			emit(0x200036);				// neg a
		},
			[&]()
		{
			verify(dsp.regs().a.var == 2);
			verify(!dsp.sr_test(CCR_N));
			verify(!dsp.sr_test(CCR_Z));
		});
	}

	void UnitTests::not_()
	{
		runTest([&]()
		{
			dsp.regs().a.var = 0x12555555123456;
			emit(0x200017);	// not a
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0x12aaaaaa123456);
		});

		runTest([&]()
		{
			dsp.regs().a.var = 0xffd8b38b000000;
			dsp.setSR(0x0800e8);
			emit(0x200017);	// not a
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0xff274c74000000);
			verify(dsp.regs().sr.var == 0x0800e0);
		});
	}

	void UnitTests::or_()
	{
		runTest([&]()
		{
			dsp.regs().a.var = 0xee222222555555;
			dsp.x0(0x444444);
			emit(0x200042);				// or x0,a
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0xee666666555555);
		});

		runTest([&]()
		{
			dsp.regs().a.var = 0xee222222555555;
			emit(0x140c2, 0x444444);	// or #$444444,a
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0xee666666555555);
		});

		runTest([&]()
		{
			dsp.regs().a.var = 0xee222222555555;
			emit(0x14482);				// or #$4,a
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0xee222226555555);
		});
	}

	void UnitTests::ori()
	{
		const auto srBackup = dsp.getSR();

		runTest([&]()
		{
			dsp.regs().omr.var = 0xff1111;
			dsp.regs().sr.var = 0xff1111;

			emit(0x0022fa);	// ori #$33,omr
			emit(0x0022fb);	// ori #$33,eom
			emit(0x0022f9);	// ori #$33,ccr
			emit(0x0022f8);	// ori #$33,mr
		},
			[&]()
		{
			verify(dsp.regs().omr.var == 0xff3333);
			verify(dsp.regs().sr.var == 0xff3333);
		});

		dsp.setSR(srBackup);
	}

	void UnitTests::rnd()
	{
		runTest([&]()
		{
			dsp.regs().a.var = 0x00222222333333;

			emit(0x200011);				// rnd a
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0x00222222000000);
		});

		runTest([&]()
		{
			dsp.regs().a.var = 0x00222222999999;

			emit(0x200011);				// rnd a
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0x00222223000000);
		});

		runTest([&]()
		{
			dsp.regs().b.var = 0xffff9538000000;

			emit(0x200019);				// rnd b
		},
			[&]()
		{
			verify(dsp.regs().b.var == 0xffff9538000000);
			verify(dsp.sr_test(CCR_N));
			verify(!dsp.sr_test(CCR_Z));
			verify(!dsp.sr_test(CCR_V));
		});
	}

	void UnitTests::rol()
	{
		runTest([&]()
		{
			dsp.regs().sr.var = 0;
			dsp.regs().a.var = 0xee112233ffeedd;

			emit(0x200037);				// rol a
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0xee224466ffeedd);
			verify(!dsp.sr_test(CCR_C));
		});
	}

	void UnitTests::sub()
	{
		runTest([&]()
		{
			dsp.regs().a.var = 0x00000000000001;
			dsp.regs().b.var = 0x00000000000002;

			emit(0x200014);		// sub b,a
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0xffffffffffffff);
			verify(dsp.sr_test(CCR_C));
			verify(!dsp.sr_test(CCR_V));
		});

		runTest([&]()
		{
			dsp.regs().a.var = 0x80000000000000;
			dsp.regs().b.var = 0x00000000000001;

			emit(0x200014);		// sub b,a
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0x7fffffffffffff);
			verify(!dsp.sr_test(CCR_C));
			verify(!dsp.sr_test(CCR_V));
		});

		runTest([&]()
		{
			dsp.regs().a.var = 0;
			dsp.x0(0x800000);

			emit(0x200044);		// sub x0,a
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0x00800000000000);
			verify(dsp.sr_test(CCR_C));
			verify(!dsp.sr_test(CCR_N));
		});
	}

	void UnitTests::tfr()
	{
		runTest([&]()
		{
			dsp.regs().a.var = 0x11223344556677;
			dsp.regs().b.var = 0;
			emit(0x200009);	// tfr a,b
		},
			[&]()
		{
			verify(dsp.regs().b.var == 0x11223344556677);
		});
	}

	void UnitTests::tcc()
	{
		// Tcc_S1D1

		runTest([&]()
		{
			dsp.regs().a.var = 0xaa112233445566;
			dsp.regs().b.var = 0;
			dsp.sr_set(CCR_Z);
			emit(0x022008);	// tne a,b
		},
			[&]()
		{
			verify(dsp.regs().b.var == 0);
		});

		runTest([&]()
		{
			dsp.regs().a.var = 0xbb112233445566;
			dsp.regs().b.var = 0;
			dsp.sr_clear(CCR_Z);
			emit(0x022008);	// tne a,b
		},
			[&]()
		{
			verify(dsp.regs().b.var == 0xbb112233445566);
		});

		// Tcc_S2D2

		runTest([&]()
		{
			dsp.regs().r[0].var = 0xaa1122;
			dsp.regs().r[1].var = 0x0;
			dsp.sr_set(CCR_Z);
			emit(0x022801);	// tne r0,r1
		},
			[&]()
		{
			verify(dsp.regs().r[1].var == 0);
		});

		runTest([&]()
		{
			dsp.regs().r[0].var = 0xbb1122;
			dsp.regs().r[1].var = 0x0;
			dsp.sr_clear(CCR_Z);
			emit(0x022801);	// tne r0,r1
		},
			[&]()
		{
			verify(dsp.regs().r[1].var == 0xbb1122);
		});

		// Tcc_S1D2S2D2

		runTest([&]()
		{
			dsp.regs().a.var = 0xaa112233445566;
			dsp.regs().b.var = 0;
			dsp.regs().r[0].var = 0xaa1122;
			dsp.regs().r[1].var = 0x0;
			dsp.sr_set(CCR_Z);
			emit(0x032009);	// tne a,b r0,r1
		},
			[&]()
		{
			verify(dsp.regs().b.var == 0);
			verify(dsp.regs().r[1].var == 0);
		});

		runTest([&]()
		{
			dsp.regs().a.var = 0xbb112233445566;
			dsp.regs().b.var = 0;
			dsp.regs().r[0].var = 0xbb1122;
			dsp.regs().r[1].var = 0x0;
			dsp.sr_clear(CCR_Z);
			emit(0x032009);	// tne a,b r0,r1
		},
			[&]()
		{
			verify(dsp.regs().b.var == 0xbb112233445566);
			verify(dsp.regs().r[1].var == 0xbb1122);
		});
	}

	void UnitTests::ifcc()
	{
		runTest([&]()
		{
			dsp.regs().a.var = 0;
			dsp.regs().b.var = 1;

			dsp.setSR(0);

			emit(0x202a10);	// add b,a ifeq
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0);
		});

		runTest([&]()
		{
			dsp.regs().a.var = 0;
			dsp.regs().b.var = 1;

			dsp.setSR(CCR_Z);

			emit(0x202a10);	// add b,a ifeq
		},
			[&]()
		{
			verify(dsp.regs().a.var == 1);
		});

		runTest([&]()
		{
			dsp.regs().a.var = 0xffffffff000000;
			dsp.regs().b.var = 0x0;

			dsp.setSR(CCR_Z);

			emit(0x200003);	// tst a
			emit(0x20310d);	// cmp a,b ifge.u
		},
			[&]()
		{
			verify(dsp.sr_test(CCR_N));
			verify(!dsp.sr_test(CCR_Z));
		});

		runTest([&]()
		{
			dsp.regs().a.var = 0x0;
			dsp.regs().b.var = 0x1;

			dsp.setSR(CCR_N);

			emit(0x200003);	// tst a
			emit(0x203105);	// cmp b,a ifge.u
		},
			[&]()
		{
			verify(dsp.sr_test(CCR_N));
			verify(!dsp.sr_test(CCR_Z));
		});
	}

	void UnitTests::move()
	{
		// op_Mover
		runTest([&]()
		{
			dsp.regs().a.var = 0x00112233445566;
			dsp.regs().n[2].var = 0;
			emit(0x21da00);	// move a,n2
		},		[&]()
		{
			verify(dsp.regs().n[2].var == 0x112233);
		});

		runTest([&]()
		{
			dsp.regs().a.var = 0x00445566aabbcc;
			dsp.regs().r[0].var = 0;
			emit(0x21d000);	// move a,r0
		},
			[&]()
		{
			verify(dsp.regs().r[0].var == 0x445566);
		});

		runTest([&]()
		{
			dsp.regs().a.var = 0;
			dsp.regs().b.var = 0x44aabbccddeeff;
			emit(0x21ee00);	// move b,a
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0x007fffff000000);
			verify(dsp.regs().b.var == 0x44aabbccddeeff);
		});

		runTest([&]()
		{
			dsp.regs().a.var = 0xff000000000000;
			dsp.regs().b.var = 0x77000000000000;
			emit(0x214400);	// move a2,x0
			emit(0x216600);	// move b2,y0
		},
			[&]()
		{
			verify(dsp.x0() == 0xffffff);
			verify(dsp.y0() == 0x000077);
		});

		runTest([&]()
		{
			dsp.regs().a.var = 0x00223344556677;
			dsp.y1(0xaabbcc);
			emit(0x21c700);	// move a,y1
		},
			[&]()
		{
			verify(dsp.y1() == 0x223344);
		});

		// op_Movem_ea
		runTest([&]()
		{
			dsp.regs().r[2].var = 0xa;
			dsp.regs().n[2].var = 0x5;
			dsp.memory().set(MemArea_P, 0xa + 0x5, 0x123456);
			emit(0x07ea92);	// move p:(r2+n2),r2
		},
			[&]()
		{
			verify(dsp.regs().r[2].var == 0x123456);
		});

		// op_Movex_ea
		runTest([&]()
		{
			dsp.regs().a.var = 0;
			dsp.memory().set(MemArea_X, 0x10, 0x223344);
			emit(0x56f000, 0x000010);	// move x:<<$10,a
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0x00223344000000);
		});

		runTest([&]()
		{
			emit(0x57f400, 0x03a800);	// move #>$3a800,b
		},
			[&]()
		{
			verify(dsp.regs().b.var == 0x0003a800000000);
		});

		runTest([&]()
		{
			dsp.regs().r[0].var = 0x11;
			dsp.memory().set(MemArea_X, 0x19, 0x11abcd);
			emit(0x02209f);	// move x:(r0+$8),b
		},
			[&]()
		{
			verify(dsp.regs().b.var == 0x0011abcd000000);
		});

		runTest([&]()
		{
			dsp.regs().b.var = 0x0011aabb000000;
			dsp.memory().set(MemArea_X, 0x07, 0);
			dsp.regs().r[0].var = 0x3;
			emit(0x02108f);	// move b,x:(r0+$4)
		},
			[&]()
		{
			const auto r = dsp.memory().get(MemArea_X, 0x7);
			verify(r == 0x11aabb);
		});

		// op_Move_xx
		runTest([&]()
		{
			dsp.regs().x.var = 0;
			emit(0x24ff00);	// move #$ff,x0
		},
			[&]()
		{
			verify(dsp.regs().x.var == 0x000000ff0000);
		});

		runTest([&]()
		{
			dsp.regs().a.var = 0;
			emit(0x2eff00);	// move #$ff,a
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0xffff0000000000);
		});

		// op_Movel_ea
		runTest([&]()
		{
			dsp.regs().x.var = 0xbadbadbadbad;
			dsp.regs().r[1].var = 0x10;
			dsp.memory().set(MemArea_X, 0x10, 0xaabbcc);
			dsp.memory().set(MemArea_Y, 0x10, 0xddeeff);

			emit(0x42d900);	// move l:(r1)+,x

			dsp.memory().set(MemArea_X, 0x3, 0x7f0000);
			dsp.memory().set(MemArea_Y, 0x3, 0x112233);
			dsp.regs().b.var = 0xffffeeddccbbaa;

			emit(0x498300);	// move l:$3,b
		},
			[&]()
		{
			verify(dsp.regs().x.var == 0xaabbccddeeff);
			verify(dsp.regs().b.var == 0x007f0000112233);
			verify(dsp.regs().r[1].var == 0x11);
		});

		runTest([&]()
		{
			dsp.regs().x.var = 0xaabbccddeeff;
			dsp.regs().y.var = 0x112233445566;
			dsp.regs().a.var = 0x00765432123456;
			dsp.regs().b.var = 0x00654321fedcba;
			dsp.regs().r[1].var = 0x10;
			dsp.regs().r[2].var = 0x15;
			dsp.regs().r[3].var = 0x20;
			dsp.regs().r[4].var = 0x25;
			dsp.memory().set(MemArea_X, 0x10, 0);	dsp.memory().set(MemArea_Y, 0x10, 0);
			dsp.memory().set(MemArea_X, 0x15, 0);	dsp.memory().set(MemArea_Y, 0x15, 0);
			dsp.memory().set(MemArea_X, 0x20, 0);	dsp.memory().set(MemArea_Y, 0x20, 0);
			dsp.memory().set(MemArea_X, 0x25, 0);	dsp.memory().set(MemArea_Y, 0x25, 0);
			emit(0x426100);	// move x,l:(r1)
			emit(0x436200);	// move y,l:(r2)
			emit(0x486300);	// move a,l:(r3)
			emit(0x496400);	// move b,l:(r4)
		},
			[&]()
		{
			verify(dsp.memory().get(MemArea_X, 0x10) == 0xaabbcc);	verify(dsp.memory().get(MemArea_Y, 0x10) == 0xddeeff);
			verify(dsp.memory().get(MemArea_X, 0x15) == 0x112233);	verify(dsp.memory().get(MemArea_Y, 0x15) == 0x445566);
			verify(dsp.memory().get(MemArea_X, 0x20) == 0x765432);	verify(dsp.memory().get(MemArea_Y, 0x20) == 0x123456);
			verify(dsp.memory().get(MemArea_X, 0x25) == 0x654321);	verify(dsp.memory().get(MemArea_Y, 0x25) == 0xfedcba);
		});

		// op_Movel_aa
		runTest([&]()
		{
			dsp.regs().a.var = 0;
			dsp.regs().b.var = 0;
			dsp.memory().set(MemArea_X, 0x3, 0x123456);
			dsp.memory().set(MemArea_Y, 0x3, 0x789abc);
			emit(0x4a8300);	// move l:<$3,ab
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0x00123456000000);
			verify(dsp.regs().b.var == 0x00789abc000000);
		});

		runTest([&]()
		{
			dsp.regs().y.var = 0;
			dsp.memory().set(MemArea_X, 0x4, 0x123456);
			dsp.memory().set(MemArea_Y, 0x4, 0x789abc);
			emit(0x438400);	// move l:<$4,y
		},
			[&]()
		{
			verify(dsp.regs().y.var == 0x00123456789abc);
		});

		// op_Movey_ea
		runTest([&]()
		{
			dsp.memory().set(MemArea_Y, 0x20, 0x334455);
			emit(0x4ff000, 0x000020);	// move y:>$2daf2,y1
		},
			[&]()
		{
			verify(dsp.y1() == 0x334455);
		});

		// op_Move_ea
		runTest([&]()
		{
			dsp.regs().r[4].var = 0x10;
			dsp.regs().n[4].var = 0x3;
			emit(0x204c00);	// move (r4)+n4
		},
			[&]()
		{
			verify(dsp.regs().r[4].var == 0x13);
		});

		runTest([&]()
		{
			dsp.regs().r[4].var = 0x13;
			emit(0x205c00);	// move (r4)+
		},
			[&]()
		{
			verify(dsp.regs().r[4].var == 0x14);
		});

		// op_Movex_aa
		runTest([&]()
		{
			dsp.memory().set(MemArea_X, 0x7, 0x654321);
			dsp.regs().r[2].var = 0;
			emit(0x628700);	// move x:<$7,r2
		},
			[&]()
		{
			verify(dsp.regs().r[2].var == 0x654321);
		});

		// op_Movey_aa
		runTest([&]()
		{
			dsp.regs().r[2].var = 0xfedcba;
			dsp.memory().set(MemArea_Y, 0x6, 0);
			emit(0x6a0600);	// move r2,y:<$6
		},
			[&]()
		{
			verify(dsp.memory().get(MemArea_Y, 0x6) == 0xfedcba);
		});

		// op_Movex_Rnxxxx
		runTest([&]()
		{
			dsp.regs().r[3].var = 0x3;
			dsp.regs().n[5].var = 0;
			dsp.memory().set(MemArea_X, 0x7, 0x223344);
			emit(0x0a73dd, 000004);	// move x:(r3+$4),n5
		},
			[&]()
		{
			verify(dsp.regs().r[3].var == 0x3);
			verify(dsp.regs().n[5].var == 0x223344);
		});
		
		runTest([&]()
		{
			dsp.regs().r[2].var = 0x15;
			dsp.regs().r[1].var = 0;
			dsp.memory().set(MemArea_X, 0x11, 0x456789);
			emit(0x0a72d1, 0xfffffc);	// move x:(r2-$4),r1
		},
			[&]()
		{
			verify(dsp.regs().r[1].var == 0x456789);
		});

		// op_Movey_Rnxxxx
		runTest([&]()
		{
			dsp.regs().r[2].var = 0x5;
			dsp.regs().n[3].var = 0x778899;
			dsp.memory().set(MemArea_Y, 0x9, 0);
			emit(0x0b729b, 000004);	// move n3,y:(r2+$4)
		},
			[&]()
		{
			verify(dsp.regs().r[2].var == 0x5);
			verify(dsp.memory().get(MemArea_Y, 0x9) == 0x778899);
		});

		// op_Movex_Rnxxx
		runTest([&]()
		{
			dsp.regs().r[3].var = 0x3;
			dsp.regs().a.var = 0;
			dsp.memory().set(MemArea_X, 0x7, 0x223344);
			emit(0x02139e);	// move x:(r3+$4),a
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0x00223344000000);
		});

		runTest([&]()
		{
			dsp.regs().r[2].var = 0x14;
			dsp.regs().a.var = 0;
			dsp.memory().set(MemArea_X, 0x10, 0x345678);
			emit(0x03f29e);	// move x:(r2-$4),a
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0x00345678000000);
		});

		runTest([&]()
		{
			dsp.regs().r[2].var = 0x11;
			dsp.set_m(2, 0x0f);
			dsp.regs().a.var = 0;
			dsp.memory().set(MemArea_X, 0x1d, 0x345678);
			emit(0x03f29e);	// move x:(r2-$4),a
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0x00345678000000);
			dsp.set_m(2, 0xffffff);
		});

		// op_Movey_Rnxxx
		runTest([&]()
		{
			dsp.regs().r[2].var = 0x5;
			dsp.regs().a.var = 0x00334455667788;
			dsp.memory().set(MemArea_Y, 0x9, 0);
			emit(0x0212ae);	// move a,y:(r2+$4)
		},
			[&]()
		{
			verify(dsp.memory().get(MemArea_Y, 0x9) == 0x334455);
		});

		// op_Movexr_ea
		runTest([&]()
		{
			dsp.regs().r[2].var = 0x5;
			dsp.regs().a.var = 0;
			dsp.regs().b.var = 0x00223344556677;
			dsp.regs().y.var = 0x111111222222;
			dsp.memory().set(MemArea_X, 0x5, 0xaabbcc);
			emit(0x1a9a00);	// move x:(r2)+,a b,y0
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0xffaabbcc000000);
			verify(dsp.regs().y.var == 0x111111223344);
			verify(dsp.regs().r[2].var == 0x6);
		});

		runTest([&]()
		{
			// test dynamic peripheral addressing
			peripheralsX.write(0xffffc5, 0x00c0de);
			dsp.regs().r[2].var = 0xffffc5;
			dsp.regs().a.var = 0;
			emit(0x1aa200);	// move x:(r2)+,a b,y0
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0x00c0de000000);
		});

		// op_Moveyr_ea
		runTest([&]()
		{
			dsp.regs().r[2].var = 0x5;
			dsp.regs().a.var = 0;
			dsp.regs().b.var = 0x00223344556677;
			dsp.regs().x.var = 0x111111222222;
			dsp.memory().set(MemArea_Y, 0x5, 0xddeeff);
			emit(0x1ada00);	// move b,x0 y:(r2)+,a
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0xffddeeff000000);
			verify(dsp.regs().x.var == 0x111111223344);
			verify(dsp.regs().r[2].var == 0x6);
		});

		// op_Movexr_A
		runTest([&]()
		{
			dsp.regs().r[1].var = 0x3;
			dsp.regs().a.var = 0x00223344556677;
			dsp.regs().x.var = 0x111111222222;
			dsp.memory().set(MemArea_X, 3, 0);
			emit(0x082100);	// move a,x:(r1) x0,a
		},
			[&]()
		{
			verify(dsp.regs().a.var == 0x00222222000000);
			verify(dsp.memory().get(MemArea_X, 3) == 0x223344);
		});

		// op_Moveyr_A
		runTest([&]()
		{
			dsp.regs().r[6].var = 0x4;
			dsp.regs().b.var = 0x00334455667788;
			dsp.regs().y.var = 0x444444555555;
			dsp.memory().set(MemArea_Y, 4, 0);
			emit(0x09a600);	// move b,y:(r6) y0,b
		},
			[&]()
		{
			verify(dsp.regs().b.var == 0x00555555000000);
			verify(dsp.memory().get(MemArea_Y, 4) == 0x334455);
		});

		// op_Movexy
		runTest([&]()
		{
			dsp.regs().r[2].var = 0x2;
			dsp.regs().r[6].var = 0x3;
			dsp.regs().n[2].var = 0x3;
			dsp.x0(0);
			dsp.y0(0);
			dsp.memory().set(MemArea_X, 2, 0x223344);
			dsp.memory().set(MemArea_Y, 3, 0xccddee);

			emit(0xf0ca00);	// move x:(r2)+n2,x0 y:(r6)+,y0
		},
			[&]()
		{
			verify(dsp.x0() == 0x223344);
			verify(dsp.y0() == 0xccddee);
			verify(dsp.regs().r[2].var == 0x5);
			verify(dsp.regs().r[6].var == 0x4);
		});

		runTest([&]()
		{
			dsp.regs().r[3].var = 0x6;
			dsp.regs().r[7].var = 0x7;
			dsp.x0(0x112233);
			dsp.y0(0x445566);
			dsp.memory().set(MemArea_X, 6, 0);
			dsp.memory().set(MemArea_Y, 7, 0);

			emit(0x806300);	// move x0,x:(r3) y0,y:(r7)
		},
			[&]()
		{
			verify(dsp.memory().get(MemArea_X, 6) == 0x112233);
			verify(dsp.memory().get(MemArea_Y, 7) == 0x445566);
		});

		// op_Movec_ea
		runTest([&]()
		{
			dsp.regs().r[0].var = 3;
			dsp.regs().omr.var = 0;
			dsp.memory().set(MemArea_X, 3, 0x112233);
			emit(0x05e03a);	// move x:(r0),omr
		},
			[&]()
		{
			verify(dsp.regs().omr == 0x112233);
		});

		const auto srBackup = dsp.regs().sr;

		// op_Movec_aa
		runTest([&]()
		{
			dsp.regs().sr.var = 0;
			dsp.memory().set(MemArea_X, 3, 0x223344);
			emit(0x058339);	// move x:<$3,sr
		},
			[&]()
		{
			verify(dsp.regs().sr == 0x223344);
		});

		dsp.setSR(srBackup);

		// op_Movec_S1D2
		runTest([&]()
		{
			dsp.regs().vba.var = 0;
			dsp.y1(0x334455);
			emit(0x04c7b0);	// move y1,vba
		},
			[&]()
		{
			verify(dsp.regs().vba.var == 0x334455);
		});

		// op_Movec_S1D2
		runTest([&]()
		{
			dsp.regs().ep.var = 0xaabbdd;
			dsp.x1(0);
			emit(0x445aa);	// move ep,x1
		},
			[&]()
		{
			verify(dsp.x1() == 0xaabbdd);
		});

		// op_Movec_ea with immediate data
		runTest([&]()
		{
			dsp.regs().lc.var = 0;
			emit(0x05f43f, 0xaabbcc);	// move #$aabbcc,lc
		},
			[&]()
		{
			verify(dsp.regs().lc.var == 0xaabbcc);
		});

		// op_Movec_xx
		runTest([&]()
		{
			dsp.regs().la.var = 0;
			emit(0x0555be);	// move #$55,la
		},
			[&]()
		{
			verify(dsp.regs().la.var == 0x55);
		});

		// op_Movep_ppea
		runTest([&]()
		{
			peripheralsX.write(0xffffc5, 0);
			emit(0x08f485, 0xffeeff);	// movep #>$112233,x:<<$ffffc5
		},
			[&]()
		{
			verify(dsp.memReadPeriph(MemArea_X, 0xffffc5, Movep_ppea) == 0xffeeff);
		});

		// op_Movep_Xqqea
		runTest([&]()
		{
			peripheralsX.write(0xffff85, 0);
			emit(0x07f405, 0x334455);	// movep #>$334455,x:<<$ffff85
		},
			[&]()
		{
			verify(dsp.memReadPeriph(MemArea_X, 0xffff85, Movep_Xqqea) == 0x334455);
		});

		// op_Movep_Yqqea
		runTest([&]()
		{
			peripheralsY.write(0xffff8c, 0);
			emit(0x07b48c, 0x556677);	// movep #>$556677,y:<<$ffff82
		},
			[&]()
		{
			verify(dsp.memReadPeriph(MemArea_Y, 0xffff8c, Movep_Yqqea) == 0x556677);
		});

		// op_Movep_SXqq
		runTest([&]()
		{
			peripheralsX.write(0xffff84, 0);
			dsp.y1(0x334455);
			emit(0x04c784);	// movep y1,x:<<$ffff84
		},
			[&]()
		{
			verify(dsp.memReadPeriph(MemArea_X, 0xffff84, Movep_SXqq) == 0x334455);
		});

		// op_Movep_SYqq
		runTest([&]()
		{
			peripheralsY.write(0xffff86, 0x112233);
			dsp.regs().b.var = 0;
			emit(0x044f26);	// movep y:<<$ffff86,b
		},
			[&]()
		{
			verify(dsp.regs().b.var == 0x00112233000000);
		});

		// op_Movep_Spp
		runTest([&]()
		{
			peripheralsY.write(0xffffc5, 0x8899aa);
			dsp.y1(0);
			emit(0x094705);	// movep y:<<$ffffc5,y1
		},
			[&]()
		{
			verify(dsp.y1() == 0x8899aa);
		});
	}

	void UnitTests::parallel()
	{
		runTest([&]()
		{
			dsp.regs().x.var = 0x000000010000;
			dsp.regs().a.var = 0x006c0000000000;
			dsp.regs().b.var = 0xbbbbbbbbbbbbbb;
			dsp.regs().y.var = 0x222222222222;

			emit(0x243c44);	// sub x0,a #$3c,x0
		},
			[&]()
		{
			verify(dsp.x0().var == 0x3c0000);
			verify(dsp.regs().a.var == 0x006b0000000000);
			verify(dsp.regs().b.var == 0xbbbbbbbbbbbbbb);
			verify(dsp.regs().y.var == 0x222222222222);
		});

		runTest([&]()
		{
			dsp.regs().x.var = 0x100000080000;
			dsp.regs().y.var = 0x000000200000;
			dsp.regs().a.var = 0x0002cdd6000000;
			dsp.regs().b.var = 0x0002a0a5000000;

			emit(0x210541);	// tfr x0,a a0,x1
		},
			[&]()
		{
			verify(dsp.regs().x.var == 0x000000080000);
			verify(dsp.regs().y.var == 0x000000200000);
			verify(dsp.regs().a.var == 0x00080000000000);
			verify(dsp.regs().b.var == 0x0002a0a5000000);
		});

		runTest([&]()
		{
			dsp.regs().x.var = 0x000000003339;
			dsp.regs().y.var = 0x65a1cb000000;
			dsp.regs().a.var = 0x00000000000000;
			dsp.regs().b.var = 0x00196871f4bc6a;

			emit(0x21cf51);	// tfr y0,a a,b
		},
			[&]()
		{
			verify(dsp.regs().x.var == 0x000000003339);
			verify(dsp.regs().y.var == 0x65a1cb000000);
			verify(dsp.regs().a.var == 0x00000000000000);
			verify(dsp.regs().b.var == 0x00000000000000);
		});

		runTest([&]()
		{
			dsp.regs().x.var = 0x111111222222;
			dsp.regs().y.var = 0x333333444444;
			dsp.regs().a.var = 0x55666666777777;
			dsp.regs().b.var = 0x88999999aaaaaa;

			emit(0x21ee59);	// tfr y0,b b,a
		},
			[&]()
		{
			verify(dsp.regs().x.var == 0x111111222222);
			verify(dsp.regs().y.var == 0x333333444444);
			verify(dsp.regs().a.var == 0xff800000000000);
			verify(dsp.regs().b.var == 0x00444444000000);
		});

		runTest([&]()
		{
			dsp.regs().x.var = 0x111111222222;
			dsp.regs().y.var = 0x333333444444;
			dsp.regs().a.var = 0x55666666777777;
			dsp.regs().b.var = 0x88999999aaaaaa;

			emit(0x210741);	// tfr x0,a a0,y1
		},
			[&]()
		{
			verify(dsp.regs().x.var == 0x111111222222);
			verify(dsp.regs().y.var == 0x777777444444);
			verify(dsp.regs().a.var == 0x00222222000000);
			verify(dsp.regs().b.var == 0x88999999aaaaaa);
		});
	}
}
