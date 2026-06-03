#pragma once
#include "UObject.h"
#include "Blu/Scene/Entity.h"

namespace Blu
{
	struct TransformComponent;

	// AActor is the base class for anything that exists in a scene.
	// It wraps an entt::entity and provides typed component accessors.
	// Subclass this (or APawn / ACharacter) and register it with NativeClassRegistry.
	class AActor : public UObject
	{
	public:
		AActor() = default;
		virtual ~AActor() = default;

		// Lifecycle — called by Scene at runtime start / stop / each frame.
		void BeginPlay() override {}
		void EndPlay()   override {}
		virtual void Tick(float deltaTime) {}
		virtual void OnBeginOverlap(AActor* other) {}

		// ── Component access ────────────────────────────────────────────────────
		TransformComponent& GetTransform();

		template<typename T>
		T& GetComponent() { return m_Entity.GetComponent<T>(); }

		template<typename T, typename... Args>
		T& AddComponent(Args&&... args) { return m_Entity.AddComponent<T>(std::forward<Args>(args)...); }

		template<typename T>
		bool HasComponent() { return m_Entity.HasComponent<T>(); }

		template<typename T>
		void RemoveComponent() { m_Entity.RemoveComponent<T>(); }

		// ── Identity ────────────────────────────────────────────────────────────
		Entity GetEntity() const { return m_Entity; }
		Scene* GetScene()  const { return m_Scene;  }

	private:
		Entity  m_Entity;
		Scene*  m_Scene = nullptr;
		friend class Scene;
		friend class ActorSystem;
	};
}
