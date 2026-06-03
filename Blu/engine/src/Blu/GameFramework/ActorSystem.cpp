#include "Blupch.h"
#include "ActorSystem.h"
#include "NativeClassRegistry.h"
#include "Blu/Core/Log.h"
#include "Blu/Scene/Component.h"
#include "Blu/Scene/Entity.h"
#include "Blu/Scene/Scene.h"
#include <algorithm>
#include <vector>

namespace Blu
{
	ActorSystem::ActorSystem(Scene& scene)
		: m_Scene(scene)
	{
	}

	ActorSystem::~ActorSystem()
	{
		Stop();
	}

	void ActorSystem::Start()
	{
		if (m_Running)
			return;

		m_Running = true;
		SyncActors();
	}

	void ActorSystem::Tick(float deltaTime)
	{
		if (!m_Running)
			return;

		SyncActors();
		for (const auto& [actorEntityID, actor] : m_Actors)
		{
			if (!m_DestroyQueue.contains(actorEntityID))
				actor->Tick(deltaTime);
		}
		FlushDestroyQueue();
	}

	void ActorSystem::Stop()
	{
		for (auto& [_, actor] : m_Actors)
			actor->EndPlay();

		m_Actors.clear();
		m_ActorClassIDs.clear();
		m_DestroyQueue.clear();
		m_DisabledActorClassIDs.clear();
		m_MissingClasses.clear();
		m_Running = false;
	}

	void ActorSystem::QueueDestroy(UUID actorEntityID)
	{
		if (m_Actors.contains(actorEntityID))
		{
			m_DestroyQueue.insert(actorEntityID);
			m_DisabledActorClassIDs[actorEntityID] = m_ActorClassIDs.at(actorEntityID);
		}
	}

	AActor* ActorSystem::FindActor(UUID actorEntityID) const
	{
		auto actor = m_Actors.find(actorEntityID);
		return actor != m_Actors.end() ? actor->second.get() : nullptr;
	}

	void ActorSystem::SyncActors()
	{
		std::vector<UUID> removedActors;
		for (const auto& [actorEntityID, _] : m_ActorClassIDs)
		{
			Entity entity = m_Scene.GetEntityByUUID(actorEntityID);
			if (!entity || !entity.HasComponent<ActorComponent>())
				removedActors.push_back(actorEntityID);
		}
		for (UUID actorEntityID : removedActors)
		{
			DestroyNow(actorEntityID);
			m_DisabledActorClassIDs.erase(actorEntityID);
		}

		auto view = m_Scene.GetAllEntitiesWith<ActorComponent, IDComponent>();
		for (auto entity : view)
		{
			const auto& [actorComponent, idComponent] = view.get<ActorComponent, IDComponent>(entity);
			const NativeClassID resolvedClassID = NativeClassRegistry::Get().ResolveClassID(actorComponent.ClassID);
			auto disabledClass = m_DisabledActorClassIDs.find(idComponent.ID);
			if (disabledClass != m_DisabledActorClassIDs.end() && disabledClass->second != resolvedClassID)
				m_DisabledActorClassIDs.erase(disabledClass);
			auto existingClass = m_ActorClassIDs.find(idComponent.ID);
			if (existingClass != m_ActorClassIDs.end() && existingClass->second != resolvedClassID)
			{
				DestroyNow(idComponent.ID);
				m_DisabledActorClassIDs.erase(idComponent.ID);
			}

			if (!actorComponent.ClassID.empty() && !m_Actors.contains(idComponent.ID) &&
				!m_DisabledActorClassIDs.contains(idComponent.ID) && !m_MissingClasses.contains(idComponent.ID))
				CreateActor(idComponent.ID);
		}
	}

	void ActorSystem::CreateActor(UUID actorEntityID)
	{
		Entity entity = m_Scene.GetEntityByUUID(actorEntityID);
		if (!entity || !entity.HasComponent<ActorComponent>())
			return;

		const ActorComponent& actorComponent = entity.GetComponent<ActorComponent>();
		Unique<AActor> actor = NativeClassRegistry::Get().Create<AActor>(actorComponent.ClassID);
		if (!actor)
		{
			if (m_MissingClasses.insert(actorEntityID).second)
				BLU_CORE_WARN("ActorSystem: native actor class '{0}' is not registered", actorComponent.ClassID);
			return;
		}

		actor->m_Entity = entity;
		actor->m_Scene = &m_Scene;

		if (const NativeClassDescriptor* descriptor = NativeClassRegistry::Get().FindDescriptor(actorComponent.ClassID))
		{
			for (const auto& [propertyName, value] : actorComponent.Overrides)
			{
				auto property = std::find_if(descriptor->Properties.begin(), descriptor->Properties.end(),
					[&propertyName](const NativePropertyDescriptor& candidate) { return candidate.Name == propertyName; });
				if (property == descriptor->Properties.end() || !property->Apply(*actor, value))
					BLU_CORE_WARN("ActorSystem: failed to apply override '{0}' to '{1}'", propertyName, descriptor->ID);
			}
		}

		const NativeClassID resolvedClassID = NativeClassRegistry::Get().ResolveClassID(actorComponent.ClassID);
		m_ActorClassIDs[actorEntityID] = resolvedClassID;
		m_MissingClasses.erase(actorEntityID);
		AActor* instance = actor.get();
		m_Actors[actorEntityID] = std::move(actor);
		instance->BeginPlay();
	}

	void ActorSystem::DestroyNow(UUID actorEntityID)
	{
		auto actor = m_Actors.find(actorEntityID);
		if (actor != m_Actors.end())
		{
			actor->second->EndPlay();
			m_Actors.erase(actor);
		}
		m_ActorClassIDs.erase(actorEntityID);
		m_DestroyQueue.erase(actorEntityID);
		m_MissingClasses.erase(actorEntityID);
	}

	void ActorSystem::FlushDestroyQueue()
	{
		const auto destroyQueue = m_DestroyQueue;
		for (UUID actorEntityID : destroyQueue)
			DestroyNow(actorEntityID);
	}
}
