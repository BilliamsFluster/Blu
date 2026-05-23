#include "GameLayer.h"
#include "Blu/Scene/Scene.h"
#include "Blu/Scene/Component.h"
#include "Blu/Scene/SceneSerializer.h"
#include <filesystem>
#include <array>

namespace Azure
{
	void GameLayer::OnAttach()
	{
		m_Scene = std::make_shared<Blu::Scene>();

		bool loaded = false;
		const std::array<std::string, 5> scenePathCandidates = {
			"LoadedScenes/Main.blu",
			"Blu-Editor/LoadedScenes/Main.blu",
			"../Blu-Editor/LoadedScenes/Main.blu",
			"../../Blu-Editor/LoadedScenes/Main.blu",
			"../../../Blu-Editor/LoadedScenes/Main.blu"
		};

		for (const auto& scenePath : scenePathCandidates)
		{
			if (!std::filesystem::exists(scenePath))
				continue;

			Blu::SceneSerializer serializer(m_Scene);
			loaded = serializer.Deserialize(scenePath);
			if (loaded)
				break;
		}

		if (!loaded)
		{
			auto ground = m_Scene->CreateEntity("Ground");
			auto& groundTransform = ground.GetComponent<Blu::TransformComponent>();
			groundTransform.Translation = { 0.0f, -0.5f, 0.0f };
			groundTransform.Scale = { 50.0f, 1.0f, 50.0f };
			auto& groundMesh = ground.AddComponent<Blu::MeshComponent>();
			groundMesh.MeshData = Blu::Mesh::CreateCube();
			groundMesh.Primitive = Blu::MeshComponent::PrimitiveType::Cube;
			groundMesh.MaterialInstance = Blu::Material::Create();
			groundMesh.MaterialInstance->AlbedoColor = glm::vec4(0.45f, 0.42f, 0.40f, 1.0f);
			auto& groundRb = ground.AddComponent<Blu::Rigidbody3DComponent>();
			groundRb.Type = Blu::Rigidbody3DComponent::BodyType::Static;
			auto& groundCollider = ground.AddComponent<Blu::BoxCollider3DComponent>();
			groundCollider.HalfExtents = { 25.0f, 0.5f, 25.0f };

			auto player = m_Scene->CreateEntity("PlayerCharacter");
			auto& playerTransform = player.GetComponent<Blu::TransformComponent>();
			playerTransform.Translation = { 0.0f, 2.0f, 5.0f };
			playerTransform.Scale = { 0.5f, 1.0f, 0.5f };
			auto& playerMesh = player.AddComponent<Blu::MeshComponent>();
			playerMesh.MeshData = Blu::Mesh::CreateCube();
			playerMesh.Primitive = Blu::MeshComponent::PrimitiveType::Cube;
			playerMesh.MaterialInstance = Blu::Material::Create();
			playerMesh.MaterialInstance->AlbedoColor = glm::vec4(0.2f, 0.5f, 1.0f, 1.0f);

			auto& capsule = player.AddComponent<Blu::CapsuleCollider3DComponent>();
			capsule.Radius = 0.3f;
			capsule.HalfHeight = 0.55f;

			auto& arm = player.AddComponent<Blu::SpringArmComponent>();
			arm.ArmLength = 6.0f;
			arm.Pitch = -15.0f;
			arm.InheritYaw = false;
			arm.SocketOffset = glm::vec3(0.0f, 1.0f, 0.0f);
			arm.EnableLag = true;
			arm.PositionLagSpeed = 10.0f;

			auto& nsc = player.AddComponent<Blu::NativeScriptComponent>();
			nsc.ClassName = "PlayerCharacter";
		}

		m_Scene->EnsurePrimaryCamera();
		m_Scene->OnRuntimeStart();
		m_Scene->SetPlayerInputEnabled(true);
	}

	void GameLayer::OnDetach()
	{
		m_Scene->OnRuntimeStop();
	}

	void GameLayer::OnUpdate(Blu::Timestep ts)
	{
		m_Scene->OnUpdateRuntime(ts);
	}
}
