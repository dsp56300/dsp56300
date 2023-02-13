#pragma once

#include <map>

#include "types.h"

namespace dsp56k
{
	class MemoryBuffer
	{
	public:
#ifdef _WIN32
		using THandle = void*;
		static constexpr THandle InvalidHandle = nullptr;
#else
		using THandle = int;
		static constexpr THandle InvalidHandle = -1;
#endif

		MemoryBuffer(TWord _pSize, TWord _xySize, TWord _externalMemAddress);
		~MemoryBuffer();

		bool isValid() const { return m_isValid; }

		TWord* ptrX() const { return m_x; }
		TWord* ptrY() const { return m_y; }
		TWord* ptrP() const { return m_p; }

	private:
		TWord* mapMem(TWord _offset, TWord _size, TWord* _ptr);
		bool unmapMem(TWord*& _ptr);

		TWord* createBasePtr(TWord _totalByteSize);
		void freeBasePtr();

		THandle createFileMapping(TWord _neededByteSize);
		void destroyFileMapping();

		THandle m_hFileMapping = InvalidHandle;

		TWord* m_x = nullptr;
		TWord* m_y = nullptr;
		TWord* m_p = nullptr;

		TWord* m_xShared = nullptr;
		TWord* m_yShared = nullptr;
		TWord* m_pShared = nullptr;

		bool m_isValid = false;
		uint8_t* m_basePtr = nullptr;
		TWord m_basePtrSize = 0;

		std::map<TWord*, TWord> m_mappedSizes;	// ptr => size
	};
}
