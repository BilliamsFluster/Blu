#include "ZombieTestActor.h"
#include "Blu/Core/Log.h"
#include "Blu/Scene/Component.h"
#include "Blu/Scene/Entity.h"
#include "Blu/Scene/Scene.h"
#include "Blu/GameFramework/AGameMode.h"
#include <glm/gtx/norm.hpp>
#include <algorithm>
#include <limits>

namespace Azure
{
	void ZombieTestActor::BeginPlay()
	{
		ACharacter::BeginPlay();
		SetMoveSpeed(2.4f);
		if (!HasComponent<Blu::HealthComponent>())
			AddComponent<Blu::HealthComponent>(MaxHealth);
	}

	void ZombieTestActor::Tick(float dt)
	{
		if (m_Dead)
			return;

		if (m_AttackCooldown > 0.0f)
			m_AttackCooldown -= dt;

		Blu::Scene* scene = GetScene();
		if (!scene || !HasComponent<Blu::TransformComponent>())
			return;

		// Death: a projectile drained our health. Notify the game mode once and retire
		// the body (hidden + inert). Avoids destroying the entity from inside its own
		// tick (which could free the actor mid-frame).
		if (HasComponent<Blu::HealthComponent>() && GetComponent<Blu::HealthComponent>().Health <= 0.0f)
		{
			m_Dead = true;
			if (Blu::AGameMode* gameMode = scene->GetGameMode())
				gameMode->OnActorKilled(this, nullptr);
			GetTransform().Scale = glm::vec3(0.0f);  // hide the corpse
			BLU_CORE_INFO("ZombieTestActor: killed");
			return;
		}

		const glm::vec3 zombiePos = GetTransform().Translation;
		Blu::Entity target;
		float bestDistSq = std::numeric_limits<float>::max();
		const float detectRangeSq = DetectRange * DetectRange;

		auto players = scene->GetAllEntitiesWith<Blu::TransformComponent, Blu::PlayerStatsComponent>();
		for (auto e : players)
		{
			auto&& [transform, stats] = players.get<Blu::TransformComponent, Blu::PlayerStatsComponent>(e);
			if (stats.Health <= 0.0f)
				continue;

			float distSq = glm::length2(transform.Translation - zombiePos);
			if (distSq <= detectRangeSq && distSq < bestDistSq)
			{
				bestDistSq = distSq;
				target = Blu::Entity{ e, scene };
			}
		}

		if (!target)
			return;

		auto& targetTransform = target.GetComponent<Blu::TransformComponent>();
		glm::vec3 toTarget = targetTransform.Translation - zombiePos;
		toTarget.y = 0.0f;
		const float distSq = glm::length2(toTarget);

		if (distSq > AttackRange * AttackRange)
		{
			if (distSq > 0.0001f)
				Move(glm::normalize(toTarget));
			return;
		}

		if (m_AttackCooldown <= 0.0f && target.HasComponent<Blu::PlayerStatsComponent>())
		{
			auto& stats = target.GetComponent<Blu::PlayerStatsComponent>();
			stats.Health = std::max(0.0f, stats.Health - AttackDamage);
			m_AttackCooldown = AttackCooldownSeconds;
			BLU_CORE_INFO("ZombieTestActor: dealt {0} damage, player health {1}/{2}",
				AttackDamage, stats.Health, stats.MaxHealth);
		}
	}
}
