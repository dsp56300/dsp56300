#pragma once

#include <cstddef>
#include <cstdint>
#include <map>

namespace dsp56k
{
#ifdef __ANDROID__
	class MmuHelper
	{
	public:
		MmuHelper() = default;
		~MmuHelper() = default;

		bool isValid() const { return false; }

		uint8_t* reserveAddressRange(size_t _byteSize) { return nullptr; }
		bool createBackingStore(size_t _byteSize) { return false; }
		void* mapRegion(size_t _backingByteOffset, size_t _byteSize, void* _targetAddr) { return nullptr; }
		bool unmapRegion(void* _addr) { return false; }
		void releaseAll() {}
	};
#else
	class MmuHelper
	{
	public:
#ifdef _WIN32
		using THandle = void*;
		static constexpr THandle InvalidHandle = nullptr;
#elif defined(__APPLE__)
		using THandle = unsigned int;  // mach_port_t
		static constexpr THandle InvalidHandle = 0;  // MACH_PORT_NULL
#else
		using THandle = int;
		static constexpr THandle InvalidHandle = -1;
#endif

		MmuHelper() = default;
		~MmuHelper();

		MmuHelper(const MmuHelper&) = delete;
		MmuHelper& operator=(const MmuHelper&) = delete;
		MmuHelper(MmuHelper&&) = delete;
		MmuHelper& operator=(MmuHelper&&) = delete;

		// Reserve a contiguous virtual address range without committing physical memory
		uint8_t* reserveAddressRange(size_t _byteSize);

		// Create a backing store (page file / shared memory) of the given size
		bool createBackingStore(size_t _byteSize);

		// Map a portion of the backing store at a specific virtual address
		// _backingByteOffset: offset into the backing store
		// _byteSize: number of bytes to map
		// _targetAddr: desired virtual address (must be within reserved range)
		// Returns the mapped address on success, nullptr on failure
		void* mapRegion(size_t _backingByteOffset, size_t _byteSize, void* _targetAddr);

		// Unmap a previously mapped region
		bool unmapRegion(void* _addr);

		// Release all resources (unmaps, frees address range, closes backing store)
		void releaseAll();

		bool isValid() const { return m_basePtr != nullptr && m_hBackingStore != InvalidHandle; }

		const std::map<void*, size_t>& getMappedRegions() const { return m_mappedRegions; }

	private:
		void freeAddressRange();
		void closeBackingStore();

#ifdef _WIN32
		bool m_usePlaceholders = false;
#endif

		THandle m_hBackingStore = InvalidHandle;

		uint8_t* m_basePtr = nullptr;
		size_t m_basePtrSize = 0;

		std::map<void*, size_t> m_mappedRegions; // addr => byte size
	};
#endif
}
