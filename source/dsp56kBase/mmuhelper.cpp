#include "mmuhelper.h"

#ifndef __ANDROID__

#include "logging.h"

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

namespace dsp56k
{
	MmuHelper::~MmuHelper()
	{
		releaseAll();
	}

	void MmuHelper::releaseAll()
	{
		// Unmap all mapped regions first
		while (!m_mappedRegions.empty())
		{
			auto* addr = m_mappedRegions.begin()->first;
			unmapRegion(addr);
		}

		freeAddressRange();
		closeBackingStore();
	}

#ifdef _WIN32

	uint8_t* MmuHelper::reserveAddressRange(const size_t _byteSize)
	{
		auto* res = static_cast<uint8_t*>(VirtualAlloc(nullptr, _byteSize, MEM_RESERVE, PAGE_READWRITE));
		m_basePtr = res;
		m_basePtrSize = _byteSize;
		return res;
	}

	void MmuHelper::freeAddressRange()
	{
		if (m_basePtr == nullptr)
			return;
		VirtualFree(m_basePtr, 0, MEM_RELEASE);
		m_basePtr = nullptr;
		m_basePtrSize = 0;
	}

	bool MmuHelper::createBackingStore(const size_t _byteSize)
	{
		m_hBackingStore = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, static_cast<DWORD>(_byteSize), nullptr);
		if (m_hBackingStore == InvalidHandle)
		{
			LOG("MmuHelper: Failed to create file mapping");
			return false;
		}

		// On Windows, the base pointer must be freed before mapping views at those addresses
		freeAddressRange();

		return true;
	}

	void MmuHelper::closeBackingStore()
	{
		if (m_hBackingStore == InvalidHandle)
			return;
		CloseHandle(m_hBackingStore);
		m_hBackingStore = InvalidHandle;
	}

	void* MmuHelper::mapRegion(const size_t _backingByteOffset, const size_t _byteSize, void* _targetAddr)
	{
		auto* p = MapViewOfFileEx(m_hBackingStore, FILE_MAP_READ | FILE_MAP_WRITE, 0, static_cast<DWORD>(_backingByteOffset), _byteSize, _targetAddr);
		if (p == _targetAddr)
		{
			m_mappedRegions.insert(std::make_pair(p, _byteSize));
			return p;
		}

		const auto err = GetLastError();
		LOG("MmuHelper: Failed to create memory mapping, err " << err);
		return nullptr;
	}

	bool MmuHelper::unmapRegion(void* _addr)
	{
		const auto it = m_mappedRegions.find(_addr);
		if (it == m_mappedRegions.end())
			return false;

		const auto res = UnmapViewOfFile(_addr);
		m_mappedRegions.erase(it);
		return res != 0;
	}

#else // Linux / macOS

	uint8_t* MmuHelper::reserveAddressRange(const size_t _byteSize)
	{
		auto* ptr = mmap(nullptr, _byteSize, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, InvalidHandle, 0);
		if (ptr == MAP_FAILED)
		{
			LOG("MmuHelper: mmap failed to reserve address range");
			return nullptr;
		}
		m_basePtr = reinterpret_cast<uint8_t*>(ptr);
		m_basePtrSize = _byteSize;
		return m_basePtr;
	}

	void MmuHelper::freeAddressRange()
	{
		if (m_basePtr == nullptr)
			return;
		// On Linux, the base allocation is replaced by individual mappings.
		// We do not munmap the whole block as it was already overwritten by
		// the individual mapRegion calls. Just clear the pointer.
		m_basePtr = nullptr;
		m_basePtrSize = 0;
	}

	bool MmuHelper::createBackingStore(const size_t _byteSize)
	{
		static uint32_t g_uid = 0;

		std::stringstream name;
		name << "dsp56300_" << reinterpret_cast<uint64_t>(this) << '_' << g_uid++;
		const std::string na(name.str());

		int fd = shm_open(na.c_str(), O_RDWR | O_CREAT | O_EXCL, 0600);
		if (fd == InvalidHandle)
		{
			LOG("MmuHelper: shm_open failed, err " << errno);
			return false;
		}
		shm_unlink(na.c_str());
		if (ftruncate(fd, static_cast<off_t>(_byteSize)))
		{
			LOG("MmuHelper: Failed to ftruncate to size of " << _byteSize);
		}

		m_hBackingStore = fd;
		return true;
	}

	void MmuHelper::closeBackingStore()
	{
		if (m_hBackingStore == InvalidHandle)
			return;
		if (close(m_hBackingStore))
			LOG("MmuHelper: Failed to close file descriptor " << m_hBackingStore);
		m_hBackingStore = InvalidHandle;
	}

	void* MmuHelper::mapRegion(const size_t _backingByteOffset, const size_t _byteSize, void* _targetAddr)
	{
		errno = 0;
		auto* p = mmap(_targetAddr, _byteSize, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, m_hBackingStore, static_cast<off_t>(_backingByteOffset));
		if (p == _targetAddr)
		{
			mlock(p, _byteSize);
			m_mappedRegions.insert(std::make_pair(p, _byteSize));
			return p;
		}

		LOG("MmuHelper: Failed to create memory mapping, err " << errno << ", ptr=" << p << " but requested " << _targetAddr);
		return nullptr;
	}

	bool MmuHelper::unmapRegion(void* _addr)
	{
		const auto it = m_mappedRegions.find(_addr);
		if (it == m_mappedRegions.end())
		{
			LOG("MmuHelper: Failed to unmap memory, pointer not found");
			return false;
		}
		munlock(_addr, it->second);
		const auto res = munmap(_addr, it->second) == 0;
		m_mappedRegions.erase(it);
		if (!res)
			LOG("MmuHelper: Failed to unmap memory, err " << errno);
		return res;
	}

#endif
}

#endif
