#include "memorybuffer.h"

#include <algorithm>

#include "types.h"
#include "memory.h"

#ifdef _WIN32
#	define NOMINMAX
#	define NOSERVICE
#	define WIN32_LEAN_AND_MEAN
#	include <Windows.h>
#else
#	include <fcntl.h>
#	include <unistd.h>
#	include <sys/mman.h>
#	include <sys/stat.h>
#	include <sstream>
#endif

/*

What we try in this class is to use the host MMU to create a memory map to match the DSP layout

The DSP has three different areas of memory, named X, Y and P that are separated from each other.
However, if an external SRAM is used, the three areas are bridged, any value that you write to
X memory will also appear in Y and P memory, and vice versa.

DSP Memory Layout:

          +---------------------------+-----------------------------------+
          | X Memory                  |                                   |
          +---------------------------+                                   |
          | Y Memory                  |        X/Y/P bridged              |
          +---------------------------+                                   |
          | P Memory                  |                                   |
          +---------------------------+-----------------------------------+
		                             /
		                            /
		                           /
               SRAM address (arbitrary address, varies per device)

Due to this complex layout, the emulator needs to check for every memory access
if the address is within bridged SRAM or in DSP onchip RAM and decide which
memory buffer to use.

We use the host MMU to create a memory mapping so that this step is no longer required by providing
three different address ranges that are mapped to the same physical memory address for any address
that is >= SRAM start

*/

namespace dsp56k
{
	MemoryBuffer::MemoryBuffer(TWord _pSize, TWord _xySize, TWord _externalMemAddress)
	{
		const auto maxAreaSize = std::max(_pSize, _xySize);

		if(_externalMemAddress == 0 || _externalMemAddress >= maxAreaSize)
			return;

		const auto areaByteSize = maxAreaSize * static_cast<TWord>(sizeof(TWord));
		const auto totalByteSize = 3 * areaByteSize;
		const auto sharedSize = maxAreaSize - _externalMemAddress;

		// The Windows way: Allocate a buffer via VirtualAlloc, release it before mapping any buffer but keep the base ptr. Use that base ptr to map views and pray that they re still free to use
		// The Linux way: Allocate a buffer via mmap, keep that buffer and use subsequent mmaps to grab a portion of the existing buffer
		auto* basePtr = createBasePtr(totalByteSize);

		if(!basePtr)
			return;

		const auto neededSize = Memory::calcMemSize(_pSize, _xySize, _externalMemAddress);
		const auto neededByteSize = neededSize * static_cast<TWord>(sizeof(TWord));

		m_hFileMapping = createFileMapping(neededByteSize);
		if(m_hFileMapping == InvalidHandle)
			return;

#ifdef _WIN32
		freeBasePtr();
#endif

		auto* ptrX = basePtr;
		auto* ptrY = basePtr + maxAreaSize;
		auto* ptrP = basePtr + (maxAreaSize<<1);

		TWord offset = 0;

		m_x = mapMem(offset, _externalMemAddress, ptrX);	offset += _externalMemAddress;
		m_y = mapMem(offset, _externalMemAddress, ptrY);	offset += _externalMemAddress;
		m_p = mapMem(offset, _externalMemAddress, ptrP);	offset += _externalMemAddress;

		if(!m_x || !m_y || !m_p)
			return;

		auto* ptrXshared = ptrX + _externalMemAddress;
		auto* ptrYshared = ptrY + _externalMemAddress;
		auto* ptrPshared = ptrP + _externalMemAddress;

		m_xShared = mapMem(offset, sharedSize, ptrXshared);
		m_yShared = mapMem(offset, sharedSize, ptrYshared);
		m_pShared = mapMem(offset, sharedSize, ptrPshared);

		if(!m_xShared || !m_yShared || !m_pShared)
			return;

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
			LOG("MMU based DSP memory setup FAILED");
		}
	}

	MemoryBuffer::~MemoryBuffer()
	{
		unmapMem(m_pShared);
		unmapMem(m_yShared);
		unmapMem(m_xShared);
		unmapMem(m_p);
		unmapMem(m_y);
		unmapMem(m_x);

		freeBasePtr();

		destroyFileMapping();
	}
#ifdef _WIN32
	TWord* MemoryBuffer::mapMem(TWord _offset, TWord _size, TWord* _ptr)
	{
		auto* p = static_cast<TWord*>(MapViewOfFileEx(m_hFileMapping, FILE_MAP_READ | FILE_MAP_WRITE, 0, _offset * sizeof(TWord), _size * sizeof(TWord), _ptr));
		if(p == _ptr)
		{
			m_mappedSizes.insert(std::make_pair(p, _size));
			return p;
		}

		const auto err = GetLastError();
		LOG("Failed to create memory mapping, err " << err);
		return nullptr;
	}

	bool MemoryBuffer::unmapMem(TWord*& _ptr)
	{
		const auto res = UnmapViewOfFile(_ptr);
		m_mappedSizes.erase(_ptr);
		_ptr = nullptr;
		return res;
	}

	TWord* MemoryBuffer::createBasePtr(const TWord _totalByteSize)
	{
		auto* res = static_cast<TWord*>(VirtualAlloc(nullptr, _totalByteSize, MEM_RESERVE, PAGE_READWRITE));
		m_basePtr = reinterpret_cast<uint8_t*>(res);
		return res;
	}

	void MemoryBuffer::freeBasePtr()
	{
		if(m_basePtr == nullptr)
			return;
		VirtualFree(m_basePtr, 0, MEM_RELEASE);
		m_basePtr = nullptr;
	}

	MemoryBuffer::THandle MemoryBuffer::createFileMapping(const TWord _neededByteSize)
	{
		return CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, _neededByteSize, nullptr);
	}

	void MemoryBuffer::destroyFileMapping()
	{
		if(m_hFileMapping == InvalidHandle)
			return;
		CloseHandle(m_hFileMapping);
		m_hFileMapping = InvalidHandle;
	}
#else
	TWord* MemoryBuffer::mapMem(TWord _offset, TWord _size, TWord* _ptr)
	{
		errno = 0;
		auto* p = static_cast<TWord*>(mmap(_ptr, _size * sizeof(TWord), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, m_hFileMapping, _offset * sizeof(TWord)));
		if(p == _ptr)
		{
			mlock(p, _size * sizeof(TWord));
			m_mappedSizes.insert(std::make_pair(p, _size));
			return p;
		}

		LOG("Failed to create memory mapping, err " << errno << ", ptr=" << HEX(p) << " but requested ptr is " << HEX(_ptr));
		return nullptr;
	}

	bool MemoryBuffer::unmapMem(TWord*& _ptr)
	{
		auto it = m_mappedSizes.find(_ptr);
		if(it == m_mappedSizes.end())
		{
			LOG("FAILED to unmap memory, pointer not found in map");
			return false;
		}
		munlock(_ptr, it->second * sizeof(TWord));
		const auto res = munmap(_ptr, it->second * sizeof(TWord)) == 0;
		m_mappedSizes.erase(it);
		_ptr = nullptr;
		if(!res)
			LOG("FAILED to unmap memory, err " << errno);
		return res;
	}

	TWord* MemoryBuffer::createBasePtr(const TWord _totalByteSize)
	{
		auto* ptr = mmap(nullptr, _totalByteSize, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, InvalidHandle, 0);
		if(!ptr)
			return nullptr;
		m_basePtr = reinterpret_cast<uint8_t*>(ptr);
		m_basePtrSize = _totalByteSize;
		return reinterpret_cast<TWord*>(ptr);
	}

	void MemoryBuffer::freeBasePtr()
	{
		if(m_basePtr == nullptr)
			return;
		errno = 0;
		if(munmap(m_basePtr, m_basePtrSize))
			LOG("Failed to unmap base ptr, err=" << errno);
		m_basePtr = nullptr;
		m_basePtrSize = 0;
	}

	MemoryBuffer::THandle MemoryBuffer::createFileMapping(const TWord _neededByteSize)
	{
		static uint32_t g_uid = 0;

		std::stringstream name;
		name << "dsp56300_" << (uint64_t)this << '_' << g_uid++;
		const std::string na(name.str());
		const char* n = na.c_str();

		int fd = shm_open(n, O_RDWR | O_CREAT | O_EXCL, 0600);
		if (fd == InvalidHandle)
		{
			LOG("shm_open failed, err " << errno);
		    return InvalidHandle;
		}
		shm_unlink(n);
		if(ftruncate(fd, _neededByteSize))
			LOG("Failed to truncate to size of " << _neededByteSize);
		return fd;
	}

	void MemoryBuffer::destroyFileMapping()
	{
		if(m_hFileMapping == InvalidHandle)
		{
			LOG("Cannot close file, already closed");
			return;
		}
		if(close(m_hFileMapping))
			LOG("Failed to close file descriptor " << m_hFileMapping);
		m_hFileMapping = InvalidHandle;
	}
#endif
}
