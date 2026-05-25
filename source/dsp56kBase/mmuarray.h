#pragma once

#include <cassert>
#include <cstring>
#include <functional>
#include <vector>

#include "mmuhelper.h"

namespace dsp56k
{
	/*
	MmuArray<T> provides a flat array interface that can be backed by MMU lazy allocation.

	MMU mode (when available):
	- Reserves the full virtual address range upfront
	- All blocks initially map to a shared "default page" filled by the fill function
	- When ensureBlockForIndex() is called, the block is remapped to private memory
	  and filled with the fill function, ready for writes
	- The base pointer is stable — never changes after init

	Non-MMU fallback:
	- Uses std::vector<T> with grow-on-demand semantics
	- ensureBlockForIndex() grows the vector if needed
	- The base pointer may change on growth (caller must handle via onResized callback)
	*/
	template<typename T>
	class MmuArray
	{
	public:
		// Function that fills a block of T elements
		using FillFunc = std::function<void(T*, size_t)>;

		// Callback fired when the backing memory is reallocated (non-MMU mode only)
		using ResizedCallback = std::function<void()>;

		MmuArray() = default;
		~MmuArray() = default;

		MmuArray(const MmuArray&) = delete;
		MmuArray& operator=(const MmuArray&) = delete;
		MmuArray(MmuArray&&) = delete;
		MmuArray& operator=(MmuArray&&) = delete;

		// Initialize the array
		// _maxSize: maximum number of elements (defines virtual address range in MMU mode)
		// _fillFunc: function to fill blocks with default values (called with pointer and count)
		// _blockSize: number of elements per MMU block (must be power of 2, ignored in non-MMU mode)
		// Returns true if MMU mode is active, false if using fallback
		bool init(size_t _maxSize, FillFunc _fillFunc, size_t _blockSize = 16384)
		{
			m_fillFunc = std::move(_fillFunc);
			m_maxSize = _maxSize;
			m_blockSize = _blockSize;

			assert((_blockSize & (_blockSize - 1)) == 0 && "block size must be power of 2");

			if (_maxSize == 0)
				return false;

			// Calculate number of blocks
			m_numBlocks = (_maxSize + _blockSize - 1) / _blockSize;

			// macOS now uses the Mach VM API (mach_vm_map with VM_FLAGS_OVERWRITE)
			// which bypasses the BSD layer's DEALLOC_GAP guard pages.

			// Try MMU path
			const auto totalElements = m_numBlocks * _blockSize; // round up to full blocks
			const auto totalBytes = totalElements * sizeof(T);

			auto* basePtr = m_mmu.reserveAddressRange(totalBytes);
			if (!basePtr)
				return initFallback();

			// Backing store: one block per actual block + one shared default block
			const auto blockBytes = _blockSize * sizeof(T);
			const auto backingBytes = (m_numBlocks + 1) * blockBytes;

			if (!m_mmu.createBackingStore(backingBytes))
				return initFallback();

			// The default block is at the end of the backing store
			m_defaultBlockOffset = m_numBlocks * blockBytes;

			// Map all blocks to the shared default block
			for (size_t i = 0; i < m_numBlocks; ++i)
			{
				auto* target = basePtr + i * _blockSize * sizeof(T);
				if (!m_mmu.mapRegion(m_defaultBlockOffset, blockBytes, target))
				{
					m_mmu.releaseAll();
					return initFallback();
				}
			}

			m_ptr = reinterpret_cast<T*>(basePtr);
			m_size = totalElements;

			// Fill the default block via block 0's mapping (all blocks share the same page)
			m_fillFunc(m_ptr, _blockSize);

			// Track which blocks are private
			m_blockIsPrivate.resize(m_numBlocks, false);
			m_useMmu = true;

			return true;
		}

		// Convenience: initialize with a default value (requires T to be copyable)
		bool init(size_t _maxSize, const T& _defaultValue, size_t _blockSize = 16384)
		{
			return init(_maxSize, [_defaultValue](T* _ptr, size_t _count)
			{
				for (size_t i = 0; i < _count; ++i)
					_ptr[i] = _defaultValue;
			}, _blockSize);
		}

		// Access — no bounds check, raw array speed
		T& operator[](size_t _index) { return m_ptr[_index]; }
		const T& operator[](size_t _index) const { return m_ptr[_index]; }

		// Raw pointer access to the underlying array
		T* data() { return m_ptr; }
		const T* data() const { return m_ptr; }

		size_t size() const { return m_size; }

		bool usesMMU() const { return m_useMmu; }

		// Ensure the block containing _index has private memory.
		// Returns true if a remap was performed or a resize happened.
		bool ensureBlockForIndex(size_t _index)
		{
			if (m_useMmu)
				return ensureBlockMmu(_index);
			return ensureBlockFallback(_index);
		}

		// Ensure size covers at least _index+1 entries (non-MMU grow, MMU no-op if in range)
		bool ensureSize(size_t _index)
		{
			if (m_useMmu)
			{
				if (_index < m_size)
					return false;
				// MMU: can't grow beyond initial max
				assert(false && "MmuArray: index exceeds MMU range");
				return false;
			}
			return ensureBlockFallback(_index);
		}

		void setResizedCallback(ResizedCallback _cb) { m_resizedCallback = std::move(_cb); }

		void clear()
		{
			if (m_useMmu)
			{
				m_mmu.releaseAll();
				m_ptr = nullptr;
				m_size = 0;
				m_blockIsPrivate.clear();
				m_useMmu = false;
			}
			else
			{
				m_fallback.clear();
				m_ptr = nullptr;
				m_size = 0;
			}
		}

	private:
		bool initFallback()
		{
			m_useMmu = false;
			// Don't pre-allocate in fallback — grow on demand like current behavior
			return false;
		}

		bool ensureBlockMmu(size_t _index)
		{
			if (_index >= m_size)
				return false;

			const auto blockIdx = _index / m_blockSize;

			if (m_blockIsPrivate[blockIdx])
				return false;

			// Remap this block from the shared default page to its own private backing
			const auto blockBytes = m_blockSize * sizeof(T);
			const auto backingOffset = blockIdx * blockBytes;
			auto* target = reinterpret_cast<uint8_t*>(m_ptr) + blockIdx * blockBytes;

			// Unmap the current (shared default) mapping for this block
			m_mmu.unmapRegion(target);

			// Map to the block's own private region in the backing store
			if (!m_mmu.mapRegion(backingOffset, blockBytes, target))
				return false;

			// Fill with default values
			m_fillFunc(m_ptr + blockIdx * m_blockSize, m_blockSize);

			m_blockIsPrivate[blockIdx] = true;
			return true;
		}

		bool ensureBlockFallback(size_t _index)
		{
			if (_index < m_size)
				return false;

			const auto oldSize = m_fallback.size();
			const auto newSize = _index + 1;
			m_fallback.resize(newSize);
			m_fillFunc(m_fallback.data() + oldSize, newSize - oldSize);
			m_ptr = m_fallback.data();
			m_size = m_fallback.size();

			if (m_resizedCallback)
				m_resizedCallback();

			return true;
		}

		MmuHelper m_mmu;
		T* m_ptr = nullptr;
		size_t m_size = 0;
		size_t m_maxSize = 0;
		size_t m_blockSize = 0;
		size_t m_numBlocks = 0;
		size_t m_defaultBlockOffset = 0;
		bool m_useMmu = false;

		std::vector<bool> m_blockIsPrivate;
		std::vector<T> m_fallback;
		FillFunc m_fillFunc;

		ResizedCallback m_resizedCallback;
	};
}
