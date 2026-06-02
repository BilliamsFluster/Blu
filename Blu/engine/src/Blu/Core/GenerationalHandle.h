#pragma once
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace Blu
{
	template<typename Tag>
	struct GenerationalHandle
	{
		static constexpr uint32_t InvalidIndex = std::numeric_limits<uint32_t>::max();

		uint32_t Index = InvalidIndex;
		uint32_t Generation = 0;

		explicit operator bool() const { return Index != InvalidIndex; }
		bool operator==(const GenerationalHandle&) const = default;
	};

	template<typename T, typename Tag>
	class GenerationalRegistry
	{
	public:
		using Handle = GenerationalHandle<Tag>;

		template<typename... Args>
		Handle Emplace(Args&&... args)
		{
			uint32_t index = 0;
			if (m_FreeIndices.empty())
			{
				index = (uint32_t)m_Slots.size();
				m_Slots.emplace_back();
			}
			else
			{
				index = m_FreeIndices.back();
				m_FreeIndices.pop_back();
			}

			Slot& slot = m_Slots[index];
			slot.Value.emplace(std::forward<Args>(args)...);
			++m_LiveCount;
			return { index, slot.Generation };
		}

		T* Get(Handle handle)
		{
			if (!IsAlive(handle))
				return nullptr;
			return &*m_Slots[handle.Index].Value;
		}

		const T* Get(Handle handle) const
		{
			if (!IsAlive(handle))
				return nullptr;
			return &*m_Slots[handle.Index].Value;
		}

		bool Destroy(Handle handle)
		{
			if (!IsAlive(handle))
				return false;

			Slot& slot = m_Slots[handle.Index];
			slot.Value.reset();
			++slot.Generation;
			m_FreeIndices.push_back(handle.Index);
			--m_LiveCount;
			return true;
		}

		bool IsAlive(Handle handle) const
		{
			return handle && handle.Index < m_Slots.size() &&
				m_Slots[handle.Index].Generation == handle.Generation &&
				m_Slots[handle.Index].Value.has_value();
		}

		size_t Size() const { return m_LiveCount; }

	private:
		struct Slot
		{
			std::optional<T> Value;
			uint32_t Generation = 1;
		};

		std::vector<Slot> m_Slots;
		std::vector<uint32_t> m_FreeIndices;
		size_t m_LiveCount = 0;
	};
}
