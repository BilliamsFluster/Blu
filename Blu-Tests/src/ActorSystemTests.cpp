#include "Blu/GameFramework/ActorSystem.h"
#include "Blu/GameFramework/NativeClassRegistry.h"
#include "Blu/Core/Log.h"
#include "Blu/Core/FrameArena.h"
#include "Blu/Core/GenerationalHandle.h"
#include "Blu/Scene/Component.h"
#include "Blu/Scene/Scene.h"
#include "Blu/Scene/SceneSerializer.h"
#include "Blu/Rendering/AssetManager.h"
#include "Blu/Utils/FileSystemService.h"
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
		migratedScene.close();
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
		std::filesystem::remove_all(testDirectory, cleanupError);
	}

	void TestMountedFilesystemAndAssetRegistry()
	{
		const std::filesystem::path testDirectory = std::filesystem::temp_directory_path() /
			("BluTestsFileSystem-" + std::to_string((uint64_t)Blu::UUID()));
		auto& fileSystem = Blu::FileSystemService::Get();
		fileSystem.Reset();
		Require(fileSystem.Mount("project", testDirectory), "project mount failed");
		Require(fileSystem.Mount("cache", testDirectory / "cache"), "cache mount failed");
		Require(fileSystem.Write("project://assets/Source.asset", "source"), "mounted write failed");
		Require(fileSystem.Write("project://assets/Dependency.asset", "dependency"), "dependency write failed");
		Require(fileSystem.Exists("project://assets/Source.asset"), "mounted exists failed");
		Require(fileSystem.Resolve("project://../Escape.asset").empty(), "mounted traversal was not rejected");

		std::string contents;
		Require(fileSystem.Read("project://assets/Source.asset", contents), "mounted read failed");
		Require(contents == "source", "mounted read returned incorrect contents");

		auto& assets = Blu::AssetManager::Get();
		assets.Reset();
		assets.SetRegistryPath("cache://AssetRegistry.yaml");
		assets.Initialize();
		const Blu::AssetHandle source = assets.Import("project://assets/Source.asset");
		const Blu::AssetHandle dependency = assets.Import("project://assets/Dependency.asset");
		Require((uint64_t)source != 0 && (uint64_t)dependency != 0, "asset import returned an invalid handle");
		Require(assets.AddDependency(source, dependency), "asset dependency tracking failed");
		Require(assets.SaveRegistry(), "asset registry save failed");
		assets.Shutdown();

		assets.Initialize();
		const Blu::AssetMetadata* metadata = assets.FindMetadata(source);
		Require(metadata != nullptr, "asset registry round trip lost metadata");
		Require(metadata->Dependencies.size() == 1 && (uint64_t)metadata->Dependencies[0] == (uint64_t)dependency,
			"asset registry round trip lost dependencies");
		Require((uint64_t)assets.Import("project://assets/Source.asset") == (uint64_t)source,
			"asset registry did not preserve stable handles");

		Require(assets.Load(source) != nullptr, "asset metadata did not load");
		Require(assets.Load(source) != nullptr, "asset cache did not return a second reference");
		Require(assets.GetReferenceCount(source) == 2, "asset cache reference count was incorrect");
		assets.Release(source);
		assets.Release(source);
		Require(assets.GetLoadedAssetCount() == 0, "asset cache did not release its final reference");
		Require(assets.Load(Blu::AssetHandle(999999)) == nullptr, "stale asset handle loaded unexpectedly");
		Require(!assets.GetDiagnostics().empty(), "stale asset handle did not emit a diagnostic");

		assets.Reset();
		fileSystem.Reset();
		std::error_code cleanupError;
		std::filesystem::remove_all(testDirectory, cleanupError);
	}

	void TestLifetimeUtilities()
	{
		struct TestSlot;
		struct TestValue
		{
			int Value = 0;
		};

		Blu::GenerationalRegistry<TestValue, TestSlot> registry;
		const auto first = registry.Emplace(TestValue{ 7 });
		Require(registry.Get(first) && registry.Get(first)->Value == 7, "generational registry did not return its live value");
		Require(registry.Destroy(first), "generational registry did not destroy a live handle");
		Require(registry.Get(first) == nullptr, "generational registry accepted a stale handle");
		const auto second = registry.Emplace(TestValue{ 9 });
		Require(second.Index == first.Index && second.Generation != first.Generation, "generational registry did not advance generation");

		Blu::FrameArena arena(128);
		auto* value = arena.Create<TestValue>(TestValue{ 11 });
		Require(value->Value == 11 && arena.HasOutstandingAllocations(), "frame arena allocation failed");
		arena.Reset();
		Require(arena.GetBytesUsed() == 0 && arena.GetHighWaterMark() >= sizeof(TestValue), "frame arena reset diagnostics failed");
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
		TestMountedFilesystemAndAssetRegistry();
		TestLifetimeUtilities();
	}
	catch (const std::exception& error)
	{
		std::cerr << "Blu-Tests failure: " << error.what() << '\n';
		return 1;
	}
	std::cout << "Blu-Tests: actor lifecycle tests passed\n";
	return 0;
}
