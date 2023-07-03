#pragma once
#include "Scene.h"
#include "Blu/Core/Core.h"

namespace Blu
{
	class SceneSerializer
	{
	public:

		SceneSerializer(const Shared<Scene>& scene);

		void Serialize(const std::string& filepath);
		void SerializeBinary(const std::string& filepath);

		bool Deserialize(const std::string& filepath);
		bool DeserializeBinary(const std::string& filepath);

	private:
		Shared<Scene> m_Scene;
	};
}