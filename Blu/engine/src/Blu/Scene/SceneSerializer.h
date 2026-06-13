#pragma once
#include "Scene.h"
#include "Entity.h"
#include "Blu/Core/Core.h"

namespace Blu
{
	class SceneSerializer
	{
	public:
		// Current scene-format version written by Serialize(). Scenes saved before
		// versioning lack the "SceneVersion" key and are treated as version 0 (legacy).
		// Bump when the on-disk format changes so Deserialize() can migrate older scenes.
		static constexpr int kCurrentSceneVersion = 1;

		SceneSerializer(const Shared<Scene>& scene);

		void Serialize(const std::string& filepath);
		void SerializeBinary(const std::string& filepath);
		void SerializeLoadedScene(const std::string& filepath);
		bool SerializePrefab(Entity entity, const std::string& filepath);
		bool DeserializePrefab(const std::string& filepath, Entity* outEntity = nullptr);

		bool Deserialize(const std::string& filepath);
		std::string DeserializeLoadedScene();

		bool DeserializeBinary(const std::string& filepath);

		// Format version of the most recently deserialized scene (0 = legacy/pre-versioning).
		// Phase 1+ migrations (path-based → UUID-handle asset refs) branch on this.
		int GetLoadedSceneVersion() const { return m_SceneVersion; }

	private:
		Shared<Scene> m_Scene;
		int m_SceneVersion = 0;
	};
}
