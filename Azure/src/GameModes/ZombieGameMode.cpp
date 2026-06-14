#include "ZombieGameMode.h"
#include "Blu/Core/Log.h"
#include "Blu/Scene/Scene.h"
#include "Blu/Scene/SceneManager.h"
#include "Blu/Scene/Entity.h"
#include "Blu/Scene/Component.h"
#include "Blu/Rendering/Mesh.h"
#include "Blu/Rendering/Material.h"
#include <glm/gtc/constants.hpp>
#include <cmath>

namespace Azure
{
	float ZombieGameMode::Rand01()
	{
		m_Seed = m_Seed * 1664525u + 1013904223u;
		return (float)((m_Seed >> 8) & 0xFFFFFF) / (float)0x1000000;
	}

	void ZombieGameMode::OnGameStart()
	{
		ZombiesKilled   = 0;
		CurrentWave     = 0;
		m_WaveActive    = false;
		m_NextWaveTimer = 0.0f;
		StartWave(1); // first wave immediately; later waves are timed in Tick
	}

	void ZombieGameMode::StartWave(int wave)
	{
		if (!GetScene())
			return;
		if (wave > TotalWaves)
		{
			CurrentWave = wave; // > TotalWaves ⇒ IsVictory()
			BLU_CORE_INFO("ZombieGameMode: all {0} waves cleared — VICTORY, returning to menu", TotalWaves);
			Blu::SceneManager::Get().RequestLoadScene("assets/scenes/MainMenu.blu"); // world traversal: back to menu
			return;
		}
		CurrentWave = wave;
		const int count = 3 + wave * 2; // escalating: 5, 7, 9, ...
		for (int i = 0; i < count; ++i)
		{
			float ang = (float)i / count * glm::two_pi<float>() + Rand01() * 0.7f;
			float rad = 11.0f + Rand01() * 3.0f;
			SpawnZombie(glm::vec3(std::cos(ang) * rad, 1.5f, std::sin(ang) * rad));
		}
		m_WaveActive = true;
		m_WaveGrace  = 0.6f; // let the new zombies instantiate before clear-checks
		BLU_CORE_INFO("ZombieGameMode: wave {0}/{1} — spawned {2} zombies", wave, TotalWaves, count);
	}

	void ZombieGameMode::SpawnZombie(const glm::vec3& pos)
	{
		Blu::Scene* scene = GetScene();
		if (!scene)
			return;

		Blu::Entity z = scene->CreateEntity("Zombie");
		if (!z.HasComponent<Blu::TransformComponent>())
			z.AddComponent<Blu::TransformComponent>();
		z.GetComponent<Blu::TransformComponent>().Translation = pos;

		z.AddComponent<Blu::ActorComponent>().ClassID = "Azure::ZombieTestActor";

		auto& vo      = z.AddComponent<Blu::VisualOffsetComponent>();
		vo.Translation = { 0.0f, 0.85f, 0.0f };
		vo.Scale       = { 0.6f, 1.7f, 0.6f };

		auto& mc      = z.AddComponent<Blu::MeshComponent>();
		mc.MeshData   = Blu::Mesh::CreateCube();
		mc.Primitive  = Blu::MeshComponent::PrimitiveType::Cube;
		mc.MaterialInstance = Blu::Material::Create();
		mc.MaterialInstance->AlbedoColor      = glm::vec4(0.35f, 0.56f, 0.30f, 1.0f);
		mc.MaterialInstance->Metallic         = 0.0f;
		mc.MaterialInstance->Roughness        = 0.7f;
		mc.MaterialInstance->EmissiveColor    = glm::vec3(0.05f, 0.15f, 0.05f);
		mc.MaterialInstance->EmissiveStrength = 0.15f;

		auto& cap      = z.AddComponent<Blu::CapsuleCollider3DComponent>();
		cap.Radius     = 0.3f;
		cap.HalfHeight = 0.55f;
		cap.Friction   = 0.5f;
		cap.Density    = 1000.0f;

		auto& ccc      = z.AddComponent<Blu::CharacterControllerComponent>();
		ccc.MoveSpeed  = 2.4f;
		ccc.JumpImpulse = 0.0f;
		ccc.StepHeight = 0.35f;
		ccc.SlopeLimit = 45.0f;
	}

	int ZombieGameMode::CountAliveZombies()
	{
		Blu::Scene* scene = GetScene();
		if (!scene)
			return 0;
		int alive = 0;
		auto view = scene->GetAllEntitiesWith<Blu::HealthComponent>();
		for (auto e : view)
			if (view.get<Blu::HealthComponent>(e).Health > 0.0f)
				++alive;
		return alive;
	}

	void ZombieGameMode::OnActorKilled(Blu::AActor*, Blu::AActor*)
	{
		++ZombiesKilled;
	}

	void ZombieGameMode::OnPlayerDeath(Blu::AActor*)
	{
		--PlayerLives;
	}

	void ZombieGameMode::Tick(float dt)
	{
		if (IsGameOver() || IsVictory())
			return;

		if (m_WaveGrace > 0.0f)
		{
			m_WaveGrace -= dt;
			return;
		}

		if (m_WaveActive)
		{
			if (CountAliveZombies() == 0)
			{
				m_WaveActive    = false;
				m_NextWaveTimer = 3.0f; // breather before the next wave
				BLU_CORE_INFO("ZombieGameMode: wave {0} cleared", CurrentWave);
			}
		}
		else
		{
			m_NextWaveTimer -= dt;
			if (m_NextWaveTimer <= 0.0f)
				StartWave(CurrentWave + 1);
		}
	}
}
