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
		// Unmap all mapped regions first, restoring individual placeholders
		while (!m_mappedRegions.empty())
		{
			auto* addr = m_mappedRegions.begin()->first;
			unmapRegion(addr);
		}

		// After unmapping, each block is an independent placeholder created
		// by unmapRegion's VirtualAlloc2 call. VirtualFree(base, 0, MEM_RELEASE)
		// would only free the first one, leaking blocks 1..N. Coalesce them
		// back into a single placeholder first so the entire range can be
		// released at once.
		if (m_usePlaceholders && m_basePtr && m_basePtrSize)
			VirtualFree(m_basePtr, m_basePtrSize, MEM_RELEASE | MEM_COALESCE_PLACEHOLDERS);

		freeAddressRange();
		closeBackingStore();
	}

#ifdef _WIN32

	// ---- Placeholder API (Windows 10 1803+) ----
	// Loaded at runtime to maintain Win7/8 compatibility.
	// Eliminates the race condition in the legacy VirtualAlloc/VirtualFree/MapViewOfFileEx path.
	namespace
	{
		struct PlaceholderApi
		{
			decltype(&VirtualAlloc2) virtualAlloc2 = nullptr;
			decltype(&MapViewOfFile3) mapViewOfFile3 = nullptr;
			decltype(&UnmapViewOfFile2) unmapViewOfFile2 = nullptr;

			bool isSupported() const { return virtualAlloc2 != nullptr && mapViewOfFile3 != nullptr && unmapViewOfFile2 != nullptr; }

			static PlaceholderApi& instance()
			{
				static PlaceholderApi api;
				return api;
			}

		private:
			PlaceholderApi()
			{
				auto* hModule = GetModuleHandleW(L"kernelbase.dll");
				if (!hModule)
					return;

				virtualAlloc2 = reinterpret_cast<decltype(this->virtualAlloc2)>(GetProcAddress(hModule, "VirtualAlloc2"));
				mapViewOfFile3 = reinterpret_cast<decltype(this->mapViewOfFile3)>(GetProcAddress(hModule, "MapViewOfFile3"));
				unmapViewOfFile2 = reinterpret_cast<decltype(this->unmapViewOfFile2)>(GetProcAddress(hModule, "UnmapViewOfFile2"));

				if (isSupported())
					LOG("MmuHelper: Placeholder API available (VirtualAlloc2/MapViewOfFile3)");
			}
		};
	}

	// ---- MmuHelper Windows implementation ----

	uint8_t* MmuHelper::reserveAddressRange(const size_t _byteSize)
	{
		auto& api = PlaceholderApi::instance();

		if (api.isSupported())
		{
			auto* res = static_cast<uint8_t*>(api.virtualAlloc2(nullptr, nullptr, _byteSize, MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS, nullptr, 0));
			if (res)
			{
				m_basePtr = res;
				m_basePtrSize = _byteSize;
				m_usePlaceholders = true;
				return res;
			}
			LOG("MmuHelper: VirtualAlloc2 with placeholder failed, falling back to legacy path");
		}

		// Legacy path
		auto* res = static_cast<uint8_t*>(VirtualAlloc(nullptr, _byteSize, MEM_RESERVE, PAGE_READWRITE));
		m_basePtr = res;
		m_basePtrSize = _byteSize;
		m_usePlaceholders = false;
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

		if (!m_usePlaceholders)
		{
			// Legacy path: free the reservation so MapViewOfFileEx can use those addresses.
			// This creates a race window where another thread could claim the address range.
			freeAddressRange();
		}

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
		if (m_usePlaceholders)
		{
			// Split the placeholder at _targetAddr so we can replace just this portion.
			VirtualFree(_targetAddr, _byteSize, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER);

			auto& api = PlaceholderApi::instance();
			auto* p = api.mapViewOfFile3(m_hBackingStore, nullptr, _targetAddr, _backingByteOffset, _byteSize, MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, nullptr, 0);
			if (p == _targetAddr)
			{
				m_mappedRegions.insert(std::make_pair(p, _byteSize));
				return p;
			}

			const auto err = GetLastError();
			LOG("MmuHelper: MapViewOfFile3 failed, err " << err);
			return nullptr;
		}

		// Legacy path
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

		m_mappedRegions.erase(it);

		if (m_usePlaceholders)
		{
			// Atomically unmap and restore the placeholder. The previous
			// two-step approach (UnmapViewOfFile + VirtualAlloc2) left a
			// window where the address was free and another thread could
			// claim it, causing MapViewOfFile3 to later fail with
			// ERROR_INVALID_ADDRESS.
			auto& api = PlaceholderApi::instance();
			return api.unmapViewOfFile2(GetCurrentProcess(), _addr, MEM_PRESERVE_PLACEHOLDER) != 0;
		}

		return UnmapViewOfFile(_addr) != 0;
	}

#elif defined(__APPLE__)

	// ---- macOS Mach VM implementation ----
	// macOS inserts DEALLOC_GAP guard pages when mmap(MAP_FIXED) remaps portions
	// of an existing mapping, causing EXC_GUARD kills. The Mach VM API operates
	// below the BSD layer and does not trigger guard pages.

#include <mach/mach.h>

	uint8_t* MmuHelper::reserveAddressRange(const size_t _byteSize)
	{
		mach_vm_address_t addr = 0;
		const auto kr = mach_vm_allocate(mach_task_self(), &addr, _byteSize, VM_FLAGS_ANYWHERE);
		if (kr != KERN_SUCCESS)
		{
			LOG("MmuHelper: mach_vm_allocate reserve failed, kr=" << kr);
			return nullptr;
		}
		m_basePtr = reinterpret_cast<uint8_t*>(addr);
		m_basePtrSize = _byteSize;
		return m_basePtr;
	}

	void MmuHelper::freeAddressRange()
	{
		if (m_basePtr == nullptr)
			return;
		mach_vm_deallocate(mach_task_self(), reinterpret_cast<mach_vm_address_t>(m_basePtr), m_basePtrSize);
		m_basePtr = nullptr;
		m_basePtrSize = 0;
	}

	bool MmuHelper::createBackingStore(const size_t _byteSize)
	{
		// Allocate backing memory and create a named memory entry port from it.
		// The memory entry allows mapping the same physical pages at multiple
		// virtual addresses (shared default page pattern).
		mach_vm_address_t backingAddr = 0;
		auto kr = mach_vm_allocate(mach_task_self(), &backingAddr, _byteSize, VM_FLAGS_ANYWHERE);
		if (kr != KERN_SUCCESS)
		{
			LOG("MmuHelper: mach_vm_allocate backing failed, kr=" << kr);
			return false;
		}

		memory_object_size_t entrySize = _byteSize;
		mach_port_t entryPort = MACH_PORT_NULL;
		kr = mach_make_memory_entry_64(mach_task_self(), &entrySize, backingAddr,
			VM_PROT_READ | VM_PROT_WRITE, &entryPort, MACH_PORT_NULL);

		// Free the original allocation — the memory entry retains the pages
		mach_vm_deallocate(mach_task_self(), backingAddr, _byteSize);

		if (kr != KERN_SUCCESS || entryPort == MACH_PORT_NULL)
		{
			LOG("MmuHelper: mach_make_memory_entry_64 failed, kr=" << kr);
			return false;
		}

		m_hBackingStore = static_cast<THandle>(entryPort);
		return true;
	}

	void MmuHelper::closeBackingStore()
	{
		if (m_hBackingStore == InvalidHandle)
			return;
		mach_port_deallocate(mach_task_self(), static_cast<mach_port_t>(m_hBackingStore));
		m_hBackingStore = InvalidHandle;
	}

	void* MmuHelper::mapRegion(const size_t _backingByteOffset, const size_t _byteSize, void* _targetAddr)
	{
		// Use mach_vm_map with VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE to atomically
		// replace whatever is at the target address. This bypasses the BSD mmap
		// layer and does not trigger DEALLOC_GAP guard pages.
		mach_vm_address_t addr = reinterpret_cast<mach_vm_address_t>(_targetAddr);
		const auto kr = mach_vm_map(mach_task_self(), &addr, _byteSize, 0,
			VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE,
			static_cast<mach_port_t>(m_hBackingStore),
			_backingByteOffset, FALSE,
			VM_PROT_READ | VM_PROT_WRITE,
			VM_PROT_READ | VM_PROT_WRITE,
			VM_INHERIT_NONE);

		if (kr != KERN_SUCCESS)
		{
			LOG("MmuHelper: mach_vm_map failed, kr=" << kr);
			return nullptr;
		}

		m_mappedRegions.insert(std::make_pair(_targetAddr, _byteSize));
		return _targetAddr;
	}

	bool MmuHelper::unmapRegion(void* _addr)
	{
		const auto it = m_mappedRegions.find(_addr);
		if (it == m_mappedRegions.end())
			return false;

		const auto size = it->second;
		m_mappedRegions.erase(it);

		// Deallocate the mapping. The address becomes free, but ensureBlockMmu
		// immediately calls mapRegion which uses VM_FLAGS_OVERWRITE, so there
		// is no window for another thread to claim the address (the overwrite
		// flag atomically replaces whatever is at the target).
		// For the releaseAll path, freeAddressRange deallocates the entire range.
		const auto kr = mach_vm_deallocate(mach_task_self(), reinterpret_cast<mach_vm_address_t>(_addr), size);
		return kr == KERN_SUCCESS;
	}

#else // Linux

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
