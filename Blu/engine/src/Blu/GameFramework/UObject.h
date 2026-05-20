#pragma once
#include "Blu/Core/UUID.h"
#include <string>

namespace Blu
{
	class UObject
	{
	public:
		virtual ~UObject() = default;

		virtual void BeginPlay() {}
		virtual void EndPlay()   {}

		const UUID&        GetUUID() const { return m_UUID; }
		const std::string& GetName() const { return m_Name; }
		void               SetName(const std::string& name) { m_Name = name; }

	protected:
		UUID        m_UUID;
		std::string m_Name;
	};
}
