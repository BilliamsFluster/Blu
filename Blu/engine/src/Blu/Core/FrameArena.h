#pragma once
#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

namespace Blu
{
	class FrameArena
	{
	public:
		explicit FrameArena(size_t capacity)
			: m_Storage(capacity)
		{
		}

		void* Allocate(size_t size, size_t alignment = alignof(std::max_align_t))
		{
			const size_t alignedOffset = (m_Offset + alignment - 1) & ~(alignment - 1);
			if (alignedOffset + size > m_Storage.size())
				throw std::bad_alloc();

			void* allocation = m_Storage.data() + alignedOffset;
			m_Offset = alignedOffset + size;
			if (m_Offset > m_HighWaterMark)
				m_HighWaterMark = m_Offset;
			return allocation;
		}

		template<typename T, typename... Args>
		T* Create(Args&&... args)
		{
			static_assert(std::is_trivially_destructible_v<T>, "FrameArena values must not require destruction");
			return new (Allocate(sizeof(T), alignof(T))) T(std::forward<Args>(args)...);
		}

		void Reset() { m_Offset = 0; }
		size_t GetBytesUsed() const { return m_Offset; }
		size_t GetCapacity() const { return m_Storage.size(); }
		size_t GetHighWaterMark() const { return m_HighWaterMark; }
		bool HasOutstandingAllocations() const { return m_Offset != 0; }

	private:
		std::vector<std::byte> m_Storage;
		size_t m_Offset = 0;
		size_t m_HighWaterMark = 0;
	};
}
