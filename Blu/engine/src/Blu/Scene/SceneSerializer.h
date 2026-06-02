#pragma once
#include "Scene.h"
#include "Entity.h"
#include "Blu/Core/Core.h"

namespace Blu
{
	class SceneSerializer
	{
	public:

		SceneSerializer(const Shared<Scene>& scene);

		void Serialize(const std::string& filepath);
		void SerializeBinary(const std::string& filepath);
		void SerializeLoadedScene(const std::string& filepath);
		bool SerializePrefab(Entity entity, const std::string& filepath);
		bool DeserializePrefab(const std::string& filepath, Entity* outEntity = nullptr);

		bool Deserialize(const std::string& filepath);
		std::string DeserializeLoadedScene();

		bool DeserializeBinary(const std::string& filepath);

	private:
		Shared<Scene> m_Scene;
	};
}
