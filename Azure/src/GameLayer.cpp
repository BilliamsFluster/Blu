#include "GameLayer.h"
#include "Blu/Scene/Scene.h"
#include "Blu/Scene/Component.h"
#include "Blu/Scene/SceneSerializer.h"
#include "Blu/Events/WindowEvent.h"
#include <filesystem>
#include <array>
#include <fstream>
#include <vector>

namespace Azure
{
	namespace
	{
		// Trim leading/trailing whitespace.
		static std::string Trim(const std::string& s)
		{
			size_t b = s.find_first_not_of(" \t\r\n");
			size_t e = s.find_last_not_of(" \t\r\n");
			return (b == std::string::npos) ? std::string() : s.substr(b, e - b + 1);
		}

		// Read "StartupScene: <path>" from a minimal Game.config text file (one key
		// per line, '#' comments). Dependency-free so Azure needn't link yaml-cpp.
		static std::string ReadConfigStartupScene()
		{
			const std::array<std::string, 4> cfgCandidates = {
				"Game.config", "Azure/Game.config", "../Azure/Game.config", "../../Azure/Game.config"
			};
			for (const auto& cfg : cfgCandidates)
			{
				if (!std::filesystem::exists(cfg))
					continue;
				std::ifstream in(cfg);
				std::string line;
				while (std::getline(in, line))
				{
					std::string t = Trim(line);
					if (t.empty() || t[0] == '#')
						continue;
					if (t.rfind("StartupScene", 0) == 0)
					{
						size_t colon = t.find(':');
						if (colon != std::string::npos)
						{
							std::string value = Trim(t.substr(colon + 1));
							if (!value.empty())
								return value;
						}
					}
				}
			}
			return {};
		}

		// Resolve the startup scene: command-line arg (first *.blu) overrides
		// Game.config's StartupScene, which overrides the built-in default.
		static std::string ResolveStartupScene()
		{
			const auto& args = Blu::Application::GetCommandLineArgs();
			for (size_t i = 1; i < args.size(); ++i)
			{
				const std::string& a = args[i];
				if (a.size() > 4 && a.substr(a.size() - 4) == ".blu")
					return a;
			}

			std::string fromConfig = ReadConfigStartupScene();
			if (!fromConfig.empty())
				return fromConfig;

			return "LoadedScenes/Main.blu";
		}

		// Expand a (possibly relative) scene path into candidate locations so it
		// resolves regardless of the process working directory.
		static std::vector<std::string> ExpandSceneCandidates(const std::string& path)
		{
			std::vector<std::string> out;
			out.push_back(path);
			if (!std::filesystem::path(path).is_absolute())
			{
				const char* prefixes[] = {
					"Blu-Editor/", "../Blu-Editor/", "../../Blu-Editor/",
					"Azure/", "../Azure/", "../../Azure/"
				};
				for (const char* p : prefixes)
					out.push_back(std::string(p) + path);
			}
			return out;
		}
	}

	void GameLayer::OnAttach()
	{
		m_Scene = std::make_shared<Blu::Scene>();

		bool loaded = false;
		const std::string requestedScene = ResolveStartupScene();

		for (const auto& scenePath : ExpandSceneCandidates(requestedScene))
		{
			if (!std::filesystem::exists(scenePath))
				continue;

			Blu::SceneSerializer serializer(m_Scene);
			loaded = serializer.Deserialize(scenePath);
			if (loaded)
			{
				BLU_INFO("GameLayer: loaded startup scene '{0}'", scenePath);
				break;
			}
		}

		if (!loaded)
			BLU_WARN("GameLayer: could not load scene '{0}', using procedural fallback", requestedScene);

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
			playerTransform.Scale = { 1.0f, 1.0f, 1.0f };
			auto& playerMesh = player.AddComponent<Blu::MeshComponent>();
			playerMesh.MeshData = Blu::Mesh::CreateCube();
			playerMesh.Primitive = Blu::MeshComponent::PrimitiveType::Cube;
			playerMesh.MaterialInstance = Blu::Material::Create();
			playerMesh.MaterialInstance->AlbedoColor = glm::vec4(0.2f, 0.5f, 1.0f, 1.0f);

			auto& capsule = player.AddComponent<Blu::CapsuleCollider3DComponent>();
			capsule.Radius = 0.3f;
			capsule.HalfHeight = 0.55f;
			player.AddComponent<Blu::CharacterControllerComponent>();
			auto& visual = player.AddComponent<Blu::VisualOffsetComponent>();
			visual.Translation = glm::vec3(0.0f, capsule.HalfHeight + capsule.Radius, 0.0f);
			visual.Scale = glm::vec3(capsule.Radius * 2.0f, (capsule.HalfHeight + capsule.Radius) * 2.0f, capsule.Radius * 2.0f);

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

		bool hasUIRoot = false;
		for (auto entity : m_Scene->GetAllEntitiesWith<Blu::UIRootComponent>())
		{
			(void)entity;
			hasUIRoot = true;
			break;
		}
		if (!hasUIRoot)
		{
			auto hud = m_Scene->CreateEntity("GameplayHUD");
			auto& ui = hud.AddComponent<Blu::UIRootComponent>();
			ui.DocumentPath = "assets/ui/GameplayHUD.bluui";
			ui.Visible = true;
		}

		m_Scene->EnsurePrimaryCamera();

		// Seed the viewport size from the real window. Without this the scene's
		// viewport stays at 0x0 and RuntimeUI::RenderDocument early-returns, so the
		// HUD never draws and camera aspect ratios are wrong.
		auto& window = Blu::Application::Get().GetWindow();
		m_Scene->OnViewportResize((float)window.GetWidth(), (float)window.GetHeight());

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

	void GameLayer::OnEvent(Blu::Events::Event& event)
	{
		if (event.GetType() == Blu::Events::Event::Type::WindowResize)
		{
			auto& e = static_cast<Blu::Events::WindowResizeEvent&>(event);
			if (e.GetWidth() > 0.0f && e.GetHeight() > 0.0f)
				m_Scene->OnViewportResize(e.GetWidth(), e.GetHeight());
		}
	}

	void GameLayer::OnGuiDraw()
	{
	}
}
