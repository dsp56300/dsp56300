#pragma once

#include <vector>

#include "dsp56kBase/mmuhelper.h"
#include "types.h"

namespace dsp56k
{
	// A range of DSP addresses below the external memory address that shows external memory, in P, X and Y alike. On
	// the chip, that is a chip select window over memory that decodes fewer address lines than the DSP drives
	struct MemoryMirror
	{
		TWord address;	// first DSP address of the mirror
		TWord size;		// number of words
		TWord source;	// DSP address of the external memory word that appears at 'address'
	};

#ifdef __ANDROID__
	class MemoryBuffer
	{
	public:
		MemoryBuffer(TWord _pSize, TWord _xySize, TWord _externalMemAddress, const std::vector<MemoryMirror>& _mirrors) {}
		~MemoryBuffer() {}

		bool isValid() const { return false; }

		TWord* ptrX() const { return nullptr; }
		TWord* ptrY() const { return nullptr; }
		TWord* ptrP() const { return nullptr; }
	};
#else
	class MemoryBuffer
	{
	public:
		MemoryBuffer(TWord _pSize, TWord _xySize, TWord _externalMemAddress, const std::vector<MemoryMirror>& _mirrors);
		~MemoryBuffer() = default;

		bool isValid() const { return m_isValid; }

		TWord* ptrX() const { return m_x; }
		TWord* ptrY() const { return m_y; }
		TWord* ptrP() const { return m_p; }

	private:
		MmuHelper m_mmu;

		TWord* m_x = nullptr;
		TWord* m_y = nullptr;
		TWord* m_p = nullptr;

		bool m_isValid = false;
	};
#endif
}
