#include "GameLayer.h"
#include "Blu/Scene/Scene.h"
#include "Blu/Scene/Component.h"

namespace Azure
{
	void GameLayer::OnAttach()
	{
		m_Scene = std::make_shared<Blu::Scene>();

		// Minimal camera so the renderer doesn't assert on missing primary camera.
		auto cam = m_Scene->CreateEntity("Camera");
		auto& cc  = cam.AddComponent<Blu::CameraComponent>();
		cc.Primary = true;

		// Player entity — resolved at runtime via ActorRegistry.
		auto player = m_Scene->CreateEntity("Player");
		auto& nsc   = player.AddComponent<Blu::NativeScriptComponent>();
		nsc.ClassName = "PlayerCharacter";

		m_Scene->OnRuntimeStart();
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
