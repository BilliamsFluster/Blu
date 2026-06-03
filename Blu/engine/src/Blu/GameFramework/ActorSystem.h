#pragma once
#include "Blu/Core/Core.h"
#include "Blu/Core/UUID.h"
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace Blu
{
	class AActor;
	class Scene;

	class ActorSystem
	{
	public:
		explicit ActorSystem(Scene& scene);
		~ActorSystem();

		void Start();
		void Tick(float deltaTime);
		void Stop();
		void QueueDestroy(UUID actorEntityID);
		AActor* FindActor(UUID actorEntityID) const;

	private:
		void SyncActors();
		void CreateActor(UUID actorEntityID);
		void DestroyNow(UUID actorEntityID);
		void FlushDestroyQueue();

		Scene& m_Scene;
		bool m_Running = false;
		std::unordered_map<UUID, Unique<AActor>> m_Actors;
		std::unordered_map<UUID, std::string> m_ActorClassIDs;
		std::unordered_set<UUID> m_DestroyQueue;
		std::unordered_map<UUID, std::string> m_DisabledActorClassIDs;
		std::unordered_set<UUID> m_MissingClasses;

		friend class Scene;
	};
}
