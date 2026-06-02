#include "Blu/GameFramework/ActorSystem.h"
#include "Blu/GameFramework/NativeClassRegistry.h"
#include "Blu/Core/Log.h"
#include "Blu/Scene/Component.h"
#include "Blu/Scene/Scene.h"
#include "Blu/Scene/SceneSerializer.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace
{
	void Require(bool condition, const char* message)
	{
		if (!condition)
			throw std::runtime_error(message);
	}

	class LifecycleActor final : public Blu::AActor
	{
	public:
		static inline int BeginPlayCount = 0;
		static inline int TickCount = 0;
		static inline int EndPlayCount = 0;
		static inline bool OverrideAppliedBeforeBeginPlay = false;

		float Speed = 1.0f;

		void BeginPlay() override
		{
			++BeginPlayCount;
			OverrideAppliedBeforeBeginPlay = std::abs(Speed - 3.5f) < 0.001f;
		}

		void Tick(float) override
		{
			++TickCount;
		}

		void EndPlay() override
		{
			++EndPlayCount;
		}
	};

	class TestGameMode final : public Blu::AGameMode
	{
	};

	void ResetLifecycleCounts()
	{
		LifecycleActor::BeginPlayCount = 0;
		LifecycleActor::TickCount = 0;
		LifecycleActor::EndPlayCount = 0;
		LifecycleActor::OverrideAppliedBeforeBeginPlay = false;
	}

	void TestActorLifecycleAndDeferredDestroy()
	{
		ResetLifecycleCounts();
		auto& registry = Blu::NativeClassRegistry::Get();
		registry.Clear();
		registry.RegisterActor<LifecycleActor>(
			"Tests::LifecycleActor",
			"Lifecycle Actor",
			"Tests",
			{ "LegacyLifecycleActor" },
			{ Blu::MakeNativeProperty<LifecycleActor>("Speed", &LifecycleActor::Speed) });

		Require(registry.ResolveClassID("LegacyLifecycleActor") == "Tests::LifecycleActor", "legacy actor alias did not resolve");

		auto scene = std::make_shared<Blu::Scene>();
		Blu::Entity entity = scene->CreateEntity("Lifecycle");
		auto& actorComponent = entity.AddComponent<Blu::ActorComponent>();
		actorComponent.ClassID = "LegacyLifecycleActor";
		actorComponent.Overrides["Speed"] = 3.5f;

		Blu::ActorSystem actorSystem(*scene);
		actorSystem.Start();
		Require(LifecycleActor::BeginPlayCount == 1, "actor BeginPlay count was incorrect");
		Require(LifecycleActor::OverrideAppliedBeforeBeginPlay, "property override was not applied before BeginPlay");
		Require(actorSystem.FindActor(entity.GetUUID()) != nullptr, "actor was not created");

		actorSystem.Tick(1.0f / 60.0f);
		Require(LifecycleActor::TickCount == 1, "actor Tick count was incorrect");

		actorSystem.QueueDestroy(entity.GetUUID());
		Require(actorSystem.FindActor(entity.GetUUID()) != nullptr, "actor destruction was not deferred");
		actorSystem.Tick(1.0f / 60.0f);
		Require(LifecycleActor::EndPlayCount == 1, "actor EndPlay count was incorrect");
		Require(actorSystem.FindActor(entity.GetUUID()) == nullptr, "actor was not destroyed");
		actorSystem.Tick(1.0f / 60.0f);
		Require(actorSystem.FindActor(entity.GetUUID()) == nullptr, "destroyed actor was recreated");

		actorSystem.Stop();
		Require(LifecycleActor::EndPlayCount == 1, "actor EndPlay ran more than once");
	}

	void TestMissingClassDoesNotCreateActor()
	{
		auto scene = std::make_shared<Blu::Scene>();
		Blu::Entity entity = scene->CreateEntity("Missing");
		entity.AddComponent<Blu::ActorComponent>().ClassID = "Tests::MissingActor";

		Blu::ActorSystem actorSystem(*scene);
		actorSystem.Start();
		Require(actorSystem.FindActor(entity.GetUUID()) == nullptr, "missing native class created an actor");
		actorSystem.Stop();
	}

	void TestGameModeRegistration()
	{
		auto& registry = Blu::NativeClassRegistry::Get();
		registry.RegisterGameMode<TestGameMode>(
			"Tests::GameMode", "Test Game Mode", "Tests", { "LegacyTestGameMode" });

		const auto* descriptor = registry.FindDescriptor("LegacyTestGameMode");
		Require(descriptor != nullptr, "game mode descriptor was not registered");
		Require(descriptor->Kind == Blu::NativeClassKind::GameMode, "game mode descriptor has the wrong kind");
		Require(registry.Create<Blu::AGameMode>("Tests::GameMode") != nullptr, "game mode could not be created");
	}

	void TestLegacySceneMigrationWritesActorComponent()
	{
		const std::filesystem::path testDirectory = std::filesystem::temp_directory_path() /
			("BluTestsNativeActorMigration-" + std::to_string((uint64_t)Blu::UUID()));
		const std::filesystem::path legacyScenePath = testDirectory / "Legacy.blu";
		const std::filesystem::path migratedScenePath = testDirectory / "Migrated.blu";
		std::filesystem::create_directories(testDirectory);

		{
			std::ofstream legacyScene(legacyScenePath);
			legacyScene
				<< "Scene: Legacy\n"
				<< "Entities:\n"
				<< "  - Entity: 42\n"
				<< "    TagComponent:\n"
				<< "      Tag: LegacyLifecycle\n"
				<< "    NativeScriptComponent:\n"
				<< "      ClassName: LegacyLifecycleActor\n";
		}

		auto scene = std::make_shared<Blu::Scene>();
		scene->SetGameModeClassID("Tests::GameMode");
		Blu::SceneSerializer serializer(scene);
		Require(serializer.Deserialize(legacyScenePath.string()), "legacy scene did not deserialize");

		Blu::Entity entity = scene->GetEntityByUUID(Blu::UUID(42));
		Require(entity, "legacy scene entity was not loaded");
		Require(entity.HasComponent<Blu::ActorComponent>(), "legacy native script did not migrate to ActorComponent");
		auto& actorComponent = entity.GetComponent<Blu::ActorComponent>();
		Require(actorComponent.ClassID == "Tests::LifecycleActor", "legacy class name did not resolve to stable ID");
		actorComponent.Overrides["Speed"] = 4.25f;

		serializer.Serialize(migratedScenePath.string());
		std::ifstream migratedScene(migratedScenePath);
		std::stringstream migratedText;
		migratedText << migratedScene.rdbuf();
		Require(migratedText.str().find("ActorComponent:") != std::string::npos, "migrated scene did not write ActorComponent");
		Require(migratedText.str().find("NativeScriptComponent:") == std::string::npos, "migrated scene wrote legacy NativeScriptComponent");

		auto reloadedScene = std::make_shared<Blu::Scene>();
		Blu::SceneSerializer reloadedSerializer(reloadedScene);
		Require(reloadedSerializer.Deserialize(migratedScenePath.string()), "migrated scene did not deserialize");
		Blu::Entity reloadedEntity = reloadedScene->GetEntityByUUID(Blu::UUID(42));
		Require(reloadedEntity, "migrated entity was not loaded");
		Require(reloadedScene->GetGameModeClassID() == "Tests::GameMode", "game mode class ID did not persist");
		const auto& reloadedActorComponent = reloadedEntity.GetComponent<Blu::ActorComponent>();
		Require(reloadedActorComponent.ClassID == "Tests::LifecycleActor", "actor class ID did not persist");
		Require(std::abs(std::get<float>(reloadedActorComponent.Overrides.at("Speed")) - 4.25f) < 0.001f, "actor override did not persist");

		std::error_code cleanupError;
		std::filesystem::remove(legacyScenePath, cleanupError);
		cleanupError.clear();
		std::filesystem::remove(migratedScenePath, cleanupError);
		cleanupError.clear();
		std::filesystem::remove(testDirectory / "Migrated.assets.yaml", cleanupError);
		cleanupError.clear();
		std::filesystem::remove(testDirectory, cleanupError);
	}
}

int main()
{
	Blu::Log::Init();
	try
	{
		TestActorLifecycleAndDeferredDestroy();
		TestMissingClassDoesNotCreateActor();
		TestGameModeRegistration();
		TestLegacySceneMigrationWritesActorComponent();
	}
	catch (const std::exception& error)
	{
		std::cerr << "Blu-Tests failure: " << error.what() << '\n';
		return 1;
	}
	std::cout << "Blu-Tests: actor lifecycle tests passed\n";
	return 0;
}
