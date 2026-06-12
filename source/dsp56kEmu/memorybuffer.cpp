#include "memorybuffer.h"

#ifndef __ANDROID__

#include <algorithm>

#include "types.h"
#include "memory.h"

/*

If supported, we use the host MMU to create a memory map to match the DSP layout

The DSP has three different areas of memory, named X, Y and P that are separated from each other.
However, if an external SRAM is used, the three areas are bridged, any value that you write to
X memory will also appear in Y and P memory, and vice versa.

DSP Memory Layout $000000 - $ffffff:

          +--------------------+-------------------------+--------------------------+--+
          | X Memory           |                         |                          |  |
          +--------------------+                         |                          |  |
          | Y Memory           |      X/Y/P bridged      | invalid DSP memory range |  |
          +--------------------+                         |                          |  |
          | P Memory           |                         |                          |  |
          +--------------------+-------------------------+--------------------------+--+
         /                    /                         /                          /  /
        /                    /                         /                          /  /
       $000000              /                         /                          /   $ffffff Maximum addressable value
                       SRAM start                SRAM end                       /
                       (arbitrary addresses, vary per device)                   $ffff80 Peripherals start

Due to this complex layout, the emulator needs to check for every memory access
if the address is within bridged SRAM or in DSP onchip RAM and decide which
memory buffer to use.

We use the host MMU to create a memory mapping so that this step is no longer required by providing
three different address ranges that are mapped to the same physical memory address for any address
that is >= SRAM start

Another thing that we do is to map even more memory to cover the whole DSP address range
of $000000 - $ffffff. We use this to eliminate the need to check if memory accesses are out of
bounds due to bad DSP code.

For this, we append a block of memory behind the regular memory in each area.

The memory layout that the emulator uses looks like this:

    +---------------------------+---------------------------+---------------------------+
    | P Memory         | ###### | X Memory         | ###### | Y Memory         | ###### |
    +---------------------------+---------------------------+---------------------------+

##### is the DSP address range up to $ffffff that is above the valid DSP memory. Any out of
bounds access will use this scratch area, not harmful if written to or read from but not
affecting the regular, valid DSP memory.

*/

namespace dsp56k
{
	MemoryBuffer::MemoryBuffer(TWord _pSize, TWord _xySize, TWord _externalMemAddress)
	{
		const auto usedAreaSize = std::max(_pSize, _xySize);

		if(_externalMemAddress == 0 || _externalMemAddress >= usedAreaSize)
			return;

		constexpr TWord totalDspAreaSize = 0x1000000;
		constexpr TWord totalDspAreaByteSize = sizeof(TWord) * totalDspAreaSize;
		constexpr TWord invalidDspMemoryBlockSize = 0x100000;

		const auto externalAreaSize = usedAreaSize - _externalMemAddress;

		// to prevent that the DSP code has to check for valid memory addresses, we map the entire DSP address range to memory
		// we add a block above the external memory that every invalid DSP address will point into
		constexpr auto totalAddressRange = 3 * totalDspAreaByteSize;

		auto* basePtr = reinterpret_cast<TWord*>(m_mmu.reserveAddressRange(totalAddressRange * sizeof(TWord)));

		if(!basePtr)
			return;

		const auto neededSize = Memory::calcMemSize(_pSize, _xySize, _externalMemAddress) + invalidDspMemoryBlockSize;
		const auto neededByteSize = neededSize * static_cast<TWord>(sizeof(TWord));

		if (!m_mmu.createBackingStore(neededByteSize))
			return;

		// define host pointers for all three memory regions
		auto* hostPtrX = basePtr;
		auto* hostPtrY = basePtr + totalDspAreaSize;
		auto* hostPtrP = basePtr + (totalDspAreaSize<<1);

		size_t backingByteOffset = 0;

		// map memory for internal X, Y and P memory. They point to unique memory and are separated
		m_x = static_cast<TWord*>(m_mmu.mapRegion(backingByteOffset, _externalMemAddress * sizeof(TWord), hostPtrX));	backingByteOffset += _externalMemAddress * sizeof(TWord);
		m_y = static_cast<TWord*>(m_mmu.mapRegion(backingByteOffset, _externalMemAddress * sizeof(TWord), hostPtrY));	backingByteOffset += _externalMemAddress * sizeof(TWord);
		m_p = static_cast<TWord*>(m_mmu.mapRegion(backingByteOffset, _externalMemAddress * sizeof(TWord), hostPtrP));	backingByteOffset += _externalMemAddress * sizeof(TWord);

		if(!m_x || !m_y || !m_p)
			return;

		hostPtrX += _externalMemAddress;
		hostPtrY += _externalMemAddress;
		hostPtrP += _externalMemAddress;

		// now map memory pointers for X, Y and P that are in external SRAM and are shared
		// The host sees them as separate memory addresses that are next to the next to internal XYP pointers
		// but in reality they point to the same portion of physical memory
		auto xShared = m_mmu.mapRegion(backingByteOffset, externalAreaSize * sizeof(TWord), hostPtrX);
		auto yShared = m_mmu.mapRegion(backingByteOffset, externalAreaSize * sizeof(TWord), hostPtrY);
		auto pShared = m_mmu.mapRegion(backingByteOffset, externalAreaSize * sizeof(TWord), hostPtrP);

		if(!xShared || !yShared || !pShared)
			return;

		hostPtrX += externalAreaSize;
		hostPtrY += externalAreaSize;
		hostPtrP += externalAreaSize;

		backingByteOffset += externalAreaSize * sizeof(TWord);

		auto actualDspAddress = _externalMemAddress + externalAreaSize;
		while(actualDspAddress < totalDspAreaSize)
		{
			auto size = std::min(totalDspAreaSize - actualDspAddress, invalidDspMemoryBlockSize);

			const auto px = m_mmu.mapRegion(backingByteOffset, size * sizeof(TWord), hostPtrX);
			const auto py = m_mmu.mapRegion(backingByteOffset, size * sizeof(TWord), hostPtrY);
			const auto pp = m_mmu.mapRegion(backingByteOffset, size * sizeof(TWord), hostPtrP);

			if(!px || !py || !pp)
				return;

			hostPtrX += invalidDspMemoryBlockSize;
			hostPtrY += invalidDspMemoryBlockSize;
			hostPtrP += invalidDspMemoryBlockSize;

			actualDspAddress += invalidDspMemoryBlockSize;
		}

		// test if its working

		m_x[_externalMemAddress-1] = 0x111111;
		m_y[_externalMemAddress-1] = 0x222222;
		m_p[_externalMemAddress-1] = 0x333333;

		for(auto i=0; i<3; ++i)
		{
			m_x[_externalMemAddress+i] = 0xabcdef;
			m_y[_externalMemAddress+i] = 0xabcdef;
			m_p[_externalMemAddress+i] = 0xabcdef;
		}

		m_x[_externalMemAddress+0] = 0x444444;
		m_y[_externalMemAddress+1] = 0x555555;
		m_p[_externalMemAddress+2] = 0x666666;

		if( m_x[_externalMemAddress-1] == 0x111111 &&
			m_y[_externalMemAddress-1] == 0x222222 &&
			m_p[_externalMemAddress-1] == 0x333333 &&

			m_x[_externalMemAddress+0] == 0x444444 && m_y[_externalMemAddress+0] == 0x444444 && m_p[_externalMemAddress+0] == 0x444444 &&
			m_x[_externalMemAddress+1] == 0x555555 && m_y[_externalMemAddress+1] == 0x555555 && m_p[_externalMemAddress+1] == 0x555555 &&
			m_x[_externalMemAddress+2] == 0x666666 && m_y[_externalMemAddress+2] == 0x666666 && m_p[_externalMemAddress+2] == 0x666666
			)
		{
			LOG("MMU based DSP memory setup successful");
			m_isValid = true;
		}
		else
		{
			LOG("MMU based DSP memory setup FAILED, test has failed:");

			LOG("m_x[_externalMemAddress-1] == 0x111111 ? = " << HEX(m_x[_externalMemAddress-1]));
			LOG("m_y[_externalMemAddress-1] == 0x222222 ? = " << HEX(m_y[_externalMemAddress-1]));
			LOG("m_p[_externalMemAddress-1] == 0x333333 ? = " << HEX(m_p[_externalMemAddress-1]));

			LOG("m_x[_externalMemAddress+0] == 0x444444 ? = " << HEX(m_x[_externalMemAddress+0]));
			LOG("m_y[_externalMemAddress+0] == 0x444444 ? = " << HEX(m_y[_externalMemAddress+0]));
			LOG("m_p[_externalMemAddress+0] == 0x444444 ? = " << HEX(m_p[_externalMemAddress+0]));
			LOG("m_x[_externalMemAddress+1] == 0x555555 ? = " << HEX(m_x[_externalMemAddress+1]));
			LOG("m_y[_externalMemAddress+1] == 0x555555 ? = " << HEX(m_y[_externalMemAddress+1]));
			LOG("m_p[_externalMemAddress+1] == 0x555555 ? = " << HEX(m_p[_externalMemAddress+1]));
			LOG("m_x[_externalMemAddress+2] == 0x666666 ? = " << HEX(m_x[_externalMemAddress+2]));
			LOG("m_y[_externalMemAddress+2] == 0x666666 ? = " << HEX(m_y[_externalMemAddress+2]));
			LOG("m_p[_externalMemAddress+2] == 0x666666 ? = " << HEX(m_p[_externalMemAddress+2]));

			void* last = nullptr;
			for (const auto& [ptr, size] : m_mmu.getMappedRegions())
			{
				LOG("Mapped: at " << HEX(ptr) << ", size " << HEX(size) << ", ptrDiff " << HEX(static_cast<uint8_t*>(ptr) - static_cast<uint8_t*>(last)));
				last = ptr;
			}
		}
	}
}

#endif
