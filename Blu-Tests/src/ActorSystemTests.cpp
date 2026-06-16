#include "Blu/GameFramework/ActorSystem.h"
#include "Blu/GameFramework/NativeClassRegistry.h"
#include "Blu/Core/Log.h"
#include "Blu/Core/FrameArena.h"
#include "Blu/Core/GenerationalHandle.h"
#include "Blu/Audio/AudioEngine.h"
#include "Blu/Physics/Physics3DDiagnostics.h"
#include "Blu/Scene/Component.h"
#include "Blu/Scene/Scene.h"
#include "Blu/Scene/Entity.h"
#include "Blu/Scene/SceneSerializer.h"
#include "Blu/Rendering/AssetManager.h"
#include "Blu/Rendering/StaticMeshAsset.h"
#include "Blu/Rendering/MaterialAsset.h"
#include "Blu/Rendering/MaterialSystem.h"
#include "Blu/Rendering/MaterialGraph.h"
#include "Blu/Rendering/LightBufferData.h"
#include "Blu/Rendering/SceneRenderPipeline.h"
#include "Blu/Rendering/PostProcess.h"
#include "Blu/Rendering/Terrain.h"
#include "Blu/UI/RuntimeUI.h"
#include "Blu/Utils/FileSystemService.h"
#include "Blu/Project/Project.h"
#include "Blu/Debug/PerfStats.h"
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
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

	void TestAssetMetaStableHandles()
	{
		const std::filesystem::path testDirectory = std::filesystem::temp_directory_path() /
			("BluTestsAssetMeta-" + std::to_string((uint64_t)Blu::UUID()));
		auto& fileSystem = Blu::FileSystemService::Get();
		fileSystem.Reset();
		Require(fileSystem.Mount("project", testDirectory), "project mount failed");
		Require(fileSystem.Mount("cache", testDirectory / "cache"), "cache mount failed");
		Require(fileSystem.Write("project://assets/box.obj", "OBJ"), "mesh source write failed");

		auto& assets = Blu::AssetManager::Get();
		assets.Reset();
		assets.SetRegistryPath("cache://AssetRegistry.yaml");
		assets.Initialize();

		const Blu::AssetHandle first = assets.Import("project://assets/box.obj");
		Require((uint64_t)first != 0, "asset import returned an invalid handle");
		Require(fileSystem.Exists("project://assets/box.obj.meta"), "import did not write a .meta sidecar");

		// Simulate a fresh session with NO registry persisted: the handle must be
		// recovered from the .meta sidecar, not re-minted.
		assets.Reset();
		assets.Initialize();
		const Blu::AssetHandle recovered = assets.Import("project://assets/box.obj");
		Require((uint64_t)recovered == (uint64_t)first,
			"import did not recover the stable UUID from the .meta sidecar after registry loss");

		// Reimport preserves the handle and rejects stale handles.
		Require(assets.Reimport(first), "reimport of a known asset failed");
		Require(!assets.Reimport(Blu::AssetHandle(123456)), "reimport of a stale handle was not rejected");

		assets.Reset();
		fileSystem.Reset();
		std::error_code cleanupError;
		std::filesystem::remove_all(testDirectory, cleanupError);
	}

	void TestStaticMeshAssetTyping()
	{
		const std::filesystem::path testDirectory = std::filesystem::temp_directory_path() /
			("BluTestsMeshAsset-" + std::to_string((uint64_t)Blu::UUID()));
		auto& fileSystem = Blu::FileSystemService::Get();
		fileSystem.Reset();
		Require(fileSystem.Mount("project", testDirectory), "project mount failed");
		Require(fileSystem.Mount("cache", testDirectory / "cache"), "cache mount failed");
		Require(fileSystem.Write("project://assets/box.obj", "o box\n"), "mesh source write failed");

		auto& assets = Blu::AssetManager::Get();
		assets.Reset();
		assets.SetRegistryPath("cache://AssetRegistry.yaml");
		assets.Initialize();

		const Blu::AssetHandle handle = assets.Import("project://assets/box.obj");
		const Blu::AssetMetadata* metadata = assets.FindMetadata(handle);
		Require(metadata != nullptr && metadata->Type == Blu::AssetType::StaticMesh,
			"OBJ source was not classified as a StaticMesh asset");

		// Resolving the handle yields a StaticMeshAsset, but the geometry is loaded
		// lazily (ModelLoader needs a GPU device, absent in tests) — so LoadedModel
		// must still be null here.
		auto asset = assets.Load(handle);
		auto mesh = std::dynamic_pointer_cast<Blu::StaticMeshAsset>(asset);
		Require(mesh != nullptr, "static mesh handle did not resolve to a StaticMeshAsset");
		Require(mesh->LoadedModel == nullptr, "static mesh geometry should load lazily, not on handle resolve");

		assets.Reset();
		fileSystem.Reset();
		std::error_code cleanupError;
		std::filesystem::remove_all(testDirectory, cleanupError);
	}

	void TestMeshComponentModelHandleMigration()
	{
		const std::filesystem::path testDirectory = std::filesystem::temp_directory_path() /
			("BluTestsMeshHandle-" + std::to_string((uint64_t)Blu::UUID()));
		const std::filesystem::path scenePath = testDirectory / "MeshHandle.blu";
		auto& fileSystem = Blu::FileSystemService::Get();
		fileSystem.Reset();
		Require(fileSystem.Mount("project", testDirectory), "project mount failed");
		Require(fileSystem.Mount("cache", testDirectory / "cache"), "cache mount failed");
		Require(fileSystem.Write("project://assets/box.obj", "o box\n"), "mesh source write failed");

		auto& assets = Blu::AssetManager::Get();
		assets.Reset();
		assets.SetRegistryPath("cache://AssetRegistry.yaml");
		assets.Initialize();

		// A mesh entity referencing a source by path, with NO loaded geometry (headless:
		// serialization must not require a GPU). The serializer should mint + persist a
		// stable ModelHandle.
		auto scene = std::make_shared<Blu::Scene>();
		Blu::Entity entity = scene->CreateEntity("MeshEntity");
		auto& mesh = entity.AddComponent<Blu::MeshComponent>();
		mesh.FilePath = "project://assets/box.obj";

		Blu::SceneSerializer serializer(scene);
		serializer.Serialize(scenePath.string());

		std::ifstream serialized(scenePath);
		std::stringstream text;
		text << serialized.rdbuf();
		const std::string yaml = text.str();
		Require(yaml.find("ModelHandle:") != std::string::npos, "mesh component did not persist a ModelHandle");
		Require(yaml.find("box.obj") != std::string::npos, "mesh component did not persist its source path");

		const auto& meshAfter = entity.GetComponent<Blu::MeshComponent>();
		Require((uint64_t)meshAfter.ModelHandle != 0, "serializer did not mint a model handle");
		Require((uint64_t)meshAfter.ModelHandle == (uint64_t)assets.Import("project://assets/box.obj"),
			"persisted mesh handle is not the stable asset handle for its source");

		assets.Reset();
		fileSystem.Reset();
		std::error_code cleanupError;
		std::filesystem::remove_all(testDirectory, cleanupError);
	}

	// Validates the handle-based material load path (#17): a MeshComponent that references a .blumat by
	// MaterialHandle resolves it to the concrete Material at load time (handle -> LoadMaterial ->
	// ToMaterial), while a MeshComponent with no handle still loads its inline PBR_* values (the legacy
	// fallback). Meshes use a source path + Primitive::None so no GPU geometry is built headlessly.
	void TestMeshComponentMaterialHandle()
	{
		const std::filesystem::path testDirectory = std::filesystem::temp_directory_path() /
			("BluTestsMaterialHandle-" + std::to_string((uint64_t)Blu::UUID()));
		const std::filesystem::path scenePath = testDirectory / "MaterialHandle.blu";
		auto& fileSystem = Blu::FileSystemService::Get();
		fileSystem.Reset();
		Require(fileSystem.Mount("project", testDirectory), "project mount failed");
		Require(fileSystem.Mount("cache", testDirectory / "cache"), "cache mount failed");
		Require(fileSystem.Write("project://assets/meshA.obj", "o a\n"), "mesh A source write failed");
		Require(fileSystem.Write("project://assets/meshB.obj", "o b\n"), "mesh B source write failed");

		auto& assets = Blu::AssetManager::Get();
		assets.Reset();
		assets.SetRegistryPath("cache://AssetRegistry.yaml");
		assets.Initialize();

		// Author a distinctive .blumat and register it for a stable handle.
		Blu::MaterialAsset blumat("project://assets/handle.blumat");
		blumat.GetProperties().AlbedoColor = glm::vec4(0.2f, 0.6f, 0.9f, 1.0f);
		blumat.GetProperties().Metallic    = 0.77f;
		blumat.GetProperties().Roughness   = 0.13f;
		blumat.SetBlendMode(Blu::BlendMode::Masked);
		blumat.SetShadingModel(Blu::ShadingModel::Unlit);
		blumat.SetTwoSided(true);
		blumat.SetAlphaCutoff(0.4f);
		Require(blumat.SaveToFile("project://assets/handle.blumat"), "blumat save failed");
		const Blu::AssetHandle materialHandle = assets.Import("project://assets/handle.blumat");
		Require((uint64_t)materialHandle != 0, "material import did not yield a handle");

		// Entity A references the material by handle (no inline material). Entity B uses an inline
		// material with no handle — the legacy fallback path.
		auto scene = std::make_shared<Blu::Scene>();
		Blu::Entity handleMesh = scene->CreateEntity("HandleMesh");
		auto& mcA = handleMesh.AddComponent<Blu::MeshComponent>();
		mcA.FilePath = "project://assets/meshA.obj";
		mcA.MaterialHandle = materialHandle;

		Blu::Entity inlineMesh = scene->CreateEntity("InlineMesh");
		auto& mcB = inlineMesh.AddComponent<Blu::MeshComponent>();
		mcB.FilePath = "project://assets/meshB.obj";
		mcB.MaterialInstance = Blu::Material::Create();
		mcB.MaterialInstance->Metallic  = 0.05f;
		mcB.MaterialInstance->Roughness = 0.95f;

		Blu::SceneSerializer serializer(scene);
		serializer.Serialize(scenePath.string());

		std::ifstream serialized(scenePath);
		std::stringstream text; text << serialized.rdbuf();
		Require(text.str().find("MaterialHandle:") != std::string::npos, "scene did not persist a MaterialHandle");

		// Reload: the handle mesh must come back with the .blumat's material, the inline mesh with its own.
		auto reloaded = std::make_shared<Blu::Scene>();
		Blu::SceneSerializer reloadedSerializer(reloaded);
		Require(reloadedSerializer.Deserialize(scenePath.string()), "material-handle scene did not deserialize");

		auto approx = [](float a, float b) { return std::fabs(a - b) < 0.001f; };
		bool sawHandleMesh = false, sawInlineMesh = false;
		auto view = reloaded->GetAllEntitiesWith<Blu::MeshComponent>();
		for (auto e : view)
		{
			auto& mc = view.get<Blu::MeshComponent>(e);
			Require(mc.MaterialInstance != nullptr, "deserialized mesh has no material");
			if ((uint64_t)mc.MaterialHandle != 0)
			{
				sawHandleMesh = true;
				Require((uint64_t)mc.MaterialHandle == (uint64_t)materialHandle, "material handle did not round-trip");
				// Values must come from the .blumat via ToMaterial, not inline defaults.
				Require(approx(mc.MaterialInstance->Metallic, 0.77f),  "handle material lost metallic");
				Require(approx(mc.MaterialInstance->Roughness, 0.13f), "handle material lost roughness");
				Require(mc.MaterialInstance->Blend == Blu::BlendMode::Masked,     "handle material lost blend mode");
				Require(mc.MaterialInstance->Shading == Blu::ShadingModel::Unlit, "handle material lost shading model");
				Require(mc.MaterialInstance->TwoSided, "handle material lost two-sided flag");
			}
			else
			{
				sawInlineMesh = true;
				Require(approx(mc.MaterialInstance->Metallic, 0.05f),  "inline-fallback material lost metallic");
				Require(approx(mc.MaterialInstance->Roughness, 0.95f), "inline-fallback material lost roughness");
			}
		}
		Require(sawHandleMesh, "handle-based mesh was not found after reload");
		Require(sawInlineMesh, "inline-fallback mesh was not found after reload");

		assets.Reset();
		fileSystem.Reset();
		std::error_code cleanupError;
		std::filesystem::remove_all(testDirectory, cleanupError);
	}

	void TestAssetHandleMigrationAcrossComponents()
	{
		const std::filesystem::path testDirectory = std::filesystem::temp_directory_path() /
			("BluTestsHandlesAll-" + std::to_string((uint64_t)Blu::UUID()));
		const std::filesystem::path scenePath = testDirectory / "Handles.blu";
		auto& fileSystem = Blu::FileSystemService::Get();
		fileSystem.Reset();
		Require(fileSystem.Mount("project", testDirectory), "project mount failed");
		Require(fileSystem.Mount("cache", testDirectory / "cache"), "cache mount failed");
		Require(fileSystem.Write("project://assets/grass.obj", "o grass\n"), "foliage source write failed");
		Require(fileSystem.Write("project://assets/lod0.obj", "o lod\n"), "lod source write failed");
		Require(fileSystem.Write("project://assets/shot.wav", "RIFF"), "audio source write failed");

		auto& assets = Blu::AssetManager::Get();
		assets.Reset();
		assets.SetRegistryPath("cache://AssetRegistry.yaml");
		assets.Initialize();

		auto scene = std::make_shared<Blu::Scene>();
		Blu::Entity entity = scene->CreateEntity("AssetRefs");
		auto& foliage = entity.AddComponent<Blu::FoliageComponent>();
		foliage.FilePath = "project://assets/grass.obj";
		auto& audio = entity.AddComponent<Blu::AudioSourceComponent>();
		audio.FilePath = "project://assets/shot.wav";
		auto& lod = entity.AddComponent<Blu::MeshLODComponent>();
		Blu::LODEntry level;
		level.FilePath = "project://assets/lod0.obj";
		level.MaxDistance = 50.0f;
		lod.Levels.push_back(level);

		Blu::SceneSerializer serializer(scene);
		serializer.Serialize(scenePath.string());

		std::ifstream serialized(scenePath);
		std::stringstream text;
		text << serialized.rdbuf();
		const std::string yaml = text.str();
		Require(yaml.find("AudioHandle:") != std::string::npos, "audio source did not persist an AssetHandle");
		Require(yaml.find("ModelHandle:") != std::string::npos, "foliage/LOD did not persist an AssetHandle");

		Require((uint64_t)entity.GetComponent<Blu::FoliageComponent>().ModelHandle != 0, "foliage handle not minted");
		Require((uint64_t)entity.GetComponent<Blu::AudioSourceComponent>().AudioHandle != 0, "audio handle not minted");
		const auto& lodAfter = entity.GetComponent<Blu::MeshLODComponent>();
		Require(!lodAfter.Levels.empty() && (uint64_t)lodAfter.Levels[0].ModelHandle != 0, "LOD level handle not minted");

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

	void TestMaterialAssetPersistence()
	{
		const std::filesystem::path testDirectory = std::filesystem::temp_directory_path() /
			("BluTestsMaterialAsset-" + std::to_string((uint64_t)Blu::UUID()));
		auto& fileSystem = Blu::FileSystemService::Get();
		fileSystem.Reset();
		Require(fileSystem.Mount("project", testDirectory), "project mount failed");
		Require(fileSystem.Mount("cache", testDirectory / "cache"), "cache mount failed");

		auto& assets = Blu::AssetManager::Get();
		assets.Reset();
		assets.SetRegistryPath("cache://AssetRegistry.yaml");
		assets.Initialize();

		// Author a material and persist it as .blumat.
		Blu::MaterialAsset material("project://assets/rusty.blumat");
		material.GetProperties().Metallic = 0.9f;
		material.GetProperties().Roughness = 0.2f;
		material.GetProperties().AlbedoColor = glm::vec4(0.8f, 0.1f, 0.1f, 1.0f);
		material.SetNormalTexture(Blu::AssetHandle(4242));
		Require(material.SaveToFile("project://assets/rusty.blumat"), "material asset save failed");
		Require(fileSystem.Exists("project://assets/rusty.blumat"), ".blumat file was not written");

		// Round-trip through a fresh asset.
		Blu::MaterialAsset reloaded;
		Require(reloaded.LoadFromFile("project://assets/rusty.blumat"), "material asset load failed");
		Require(std::abs(reloaded.GetProperties().Metallic - 0.9f) < 0.001f, "metallic did not round-trip");
		Require(std::abs(reloaded.GetProperties().Roughness - 0.2f) < 0.001f, "roughness did not round-trip");
		Require(std::abs(reloaded.GetProperties().AlbedoColor.r - 0.8f) < 0.001f, "albedo did not round-trip");
		Require((uint64_t)reloaded.GetNormalTexture() == 4242, "normal texture handle did not round-trip");

		// AssetManager classifies and loads .blumat as a MaterialAsset.
		const Blu::AssetHandle handle = assets.Import("project://assets/rusty.blumat");
		const Blu::AssetMetadata* metadata = assets.FindMetadata(handle);
		Require(metadata != nullptr && metadata->Type == Blu::AssetType::Material, ".blumat was not classified as a Material asset");
		auto loaded = assets.LoadMaterial(handle);
		Require(loaded != nullptr, "AssetManager::LoadMaterial returned null for a .blumat handle");
		Require(std::abs(loaded->GetProperties().Metallic - 0.9f) < 0.001f, "AssetManager-loaded material lost its properties");
		Require((uint64_t)loaded->GetNormalTexture() == 4242, "AssetManager-loaded material lost its texture handle");

		assets.Reset();
		fileSystem.Reset();
		std::error_code cleanupError;
		std::filesystem::remove_all(testDirectory, cleanupError);
	}

	// Validates that MaterialAsset (.blumat) is now a LOSSLESS mirror of the concrete runtime
	// Material: blend/shading/two-sided/alpha-cutoff (previously dropped) survive a
	// Material -> MaterialAsset -> .blumat YAML -> MaterialAsset -> Material round-trip alongside
	// the scalar PBR values. This is the conversion spine for rendering handle-based .blumat
	// materials, so any field it silently drops would cause a visible regression.
	void TestMaterialAssetFullFidelity()
	{
		const std::filesystem::path testDirectory = std::filesystem::temp_directory_path() /
			("BluTestsMaterialFidelity-" + std::to_string((uint64_t)Blu::UUID()));
		auto& fileSystem = Blu::FileSystemService::Get();
		fileSystem.Reset();
		Require(fileSystem.Mount("project", testDirectory), "project mount failed");
		Require(fileSystem.Mount("cache", testDirectory / "cache"), "cache mount failed");

		// A concrete material with every field set to a distinctive non-default value, including
		// the render-state metadata that the old MaterialAsset schema could not represent.
		Blu::Material source;
		source.AlbedoColor      = glm::vec4(0.13f, 0.42f, 0.77f, 0.66f);
		source.Metallic         = 0.81f;
		source.Roughness        = 0.27f;
		source.AO               = 0.55f;
		source.EmissiveColor    = glm::vec3(0.9f, 0.2f, 0.05f);
		source.EmissiveStrength = 3.5f;
		source.Blend            = Blu::BlendMode::Masked;     // non-default
		source.Shading          = Blu::ShadingModel::Unlit;   // non-default
		source.TwoSided         = true;                       // non-default
		source.AlphaCutoff      = 0.33f;                      // non-default

		Blu::MaterialAsset asset = Blu::MaterialAsset::FromMaterial(source);
		Require(asset.SaveToFile("project://assets/full.blumat"), "full-fidelity material save failed");

		Blu::MaterialAsset reloaded;
		Require(reloaded.LoadFromFile("project://assets/full.blumat"), "full-fidelity material load failed");
		Blu::Shared<Blu::Material> result = reloaded.ToMaterial();
		Require(result != nullptr, "ToMaterial returned null");

		auto approx = [](float a, float b) { return std::fabs(a - b) < 0.001f; };
		Require(approx(result->AlbedoColor.r, 0.13f) && approx(result->AlbedoColor.a, 0.66f), "albedo (with alpha) did not round-trip");
		Require(approx(result->Metallic, 0.81f),  "metallic did not round-trip");
		Require(approx(result->Roughness, 0.27f), "roughness did not round-trip");
		Require(approx(result->AO, 0.55f),         "AO did not round-trip");
		Require(approx(result->EmissiveColor.g, 0.2f),  "emissive color did not round-trip");
		Require(approx(result->EmissiveStrength, 3.5f), "emissive strength did not round-trip");
		// The previously-lossy render-state metadata must now survive intact.
		Require(result->Blend == Blu::BlendMode::Masked,    "blend mode was dropped (was lossy before this change)");
		Require(result->Shading == Blu::ShadingModel::Unlit, "shading model was dropped");
		Require(result->TwoSided == true,                    "two-sided flag was dropped");
		Require(approx(result->AlphaCutoff, 0.33f),          "alpha cutoff was dropped");

		fileSystem.Reset();
		std::error_code cleanupError;
		std::filesystem::remove_all(testDirectory, cleanupError);
	}

	void TestMaterialResolver()
	{
		auto& resolver = Blu::MaterialResolver::Get();
		resolver.Clear();

		auto materialTemplate = std::make_shared<Blu::MaterialTemplate>();
		materialTemplate->Handle = Blu::AssetHandle(1001);
		materialTemplate->Blend = Blu::BlendMode::Masked;
		materialTemplate->TwoSided = true;
		materialTemplate->Defaults.Roughness = 0.8f;
		materialTemplate->Textures.Normal = Blu::AssetHandle(404);
		resolver.RegisterTemplate(materialTemplate);

		auto materialInstance = std::make_shared<Blu::MaterialInstance>();
		materialInstance->Handle = Blu::AssetHandle(1002);
		materialInstance->TemplateHandle = materialTemplate->Handle;
		materialInstance->Overrides.Roughness = 0.25f;
		resolver.RegisterInstance(materialInstance);

		Blu::MaterialRenderContext context;
		context.Path = Blu::RenderPath::Deferred;
		context.Skinned = true;
		const Blu::ResolvedMaterial resolved = resolver.Resolve(materialTemplate->Handle, materialInstance->Handle, context);
		Require(std::abs(resolved.Parameters.Roughness - 0.25f) < 0.001f, "material instance override did not win over template default");
		Require(resolved.Blend == Blu::BlendMode::Masked && resolved.TwoSided, "material template settings did not resolve");
		Require(resolved.Permutation.Has(Blu::MaterialPermutation::Deferred), "deferred material permutation was not selected");
		Require(resolved.Permutation.Has(Blu::MaterialPermutation::Skinned), "skinned material permutation was not selected");
		Require(resolved.Permutation.Has(Blu::MaterialPermutation::Masked), "masked material permutation was not selected");
		Require(!resolved.MissingTextureSlots.empty(), "missing material texture was not reported");

		materialTemplate->Blend = Blu::BlendMode::Transparent;
		const Blu::ResolvedMaterial transparent = resolver.Resolve(materialTemplate->Handle, materialInstance->Handle, context);
		Require(transparent.EffectivePath == Blu::RenderPath::Forward, "transparent deferred material did not fall back to forward");
		Require(transparent.Permutation.Has(Blu::MaterialPermutation::Transparent), "transparent material permutation was not selected");

		Blu::Material legacy;
		legacy.Roughness = 0.65f;
		const Blu::ResolvedMaterial legacyResolved = resolver.ResolveLegacy(legacy, {});
		Require(std::abs(legacyResolved.Parameters.Roughness - 0.65f) < 0.001f, "legacy material compatibility resolution failed");
	}

	void TestMaterialGraphCompiler()
	{
		Blu::MaterialGraph graph;
		graph.Blend = Blu::BlendMode::Masked;
		graph.TwoSided = true;
		const auto roughness = graph.AddScalarParameter("Roughness", 0.35f);
		const auto albedo = graph.AddVector4Parameter("Albedo", glm::vec4(0.2f, 0.4f, 0.6f, 1.0f));
		const auto normal = graph.AddTextureParameter("Normal", Blu::AssetHandle(404));
		graph.Connect(Blu::MaterialGraphInput::Roughness, roughness);
		graph.Connect(Blu::MaterialGraphInput::AlbedoColor, albedo);
		graph.Connect(Blu::MaterialGraphInput::NormalTexture, normal);

		const auto compiled = Blu::MaterialGraphCompiler::Compile(graph, Blu::AssetHandle(2001));
		Require(compiled.Succeeded(), "valid material graph did not compile");
		Require((uint64_t)compiled.Template->Handle == 2001, "material graph compiler lost the template handle");
		Require(std::abs(compiled.Template->Defaults.Roughness - 0.35f) < 0.001f, "material graph scalar did not compile");
		Require(compiled.Template->Defaults.AlbedoColor == glm::vec4(0.2f, 0.4f, 0.6f, 1.0f), "material graph vector did not compile");
		Require((uint64_t)compiled.Template->Textures.Normal == 404, "material graph texture did not compile");
		Require((compiled.Template->Features & (uint32_t)Blu::MaterialFeature::NormalMap) != 0, "material graph texture feature was not enabled");

		Blu::MaterialGraph invalidGraph;
		const auto wrongType = invalidGraph.AddScalarParameter("NotAColor", 1.0f);
		invalidGraph.Connect(Blu::MaterialGraphInput::AlbedoColor, wrongType);
		const auto invalid = Blu::MaterialGraphCompiler::Compile(invalidGraph, Blu::AssetHandle(2002));
		Require(!invalid.Succeeded() && !invalid.Diagnostics.empty(), "invalid material graph compiled without diagnostics");
	}

	void TestSceneRenderPipelinePlan()
	{
		const Blu::SceneRenderPipelinePlan deferred =
			Blu::BuildSceneRenderPipelinePlan(Blu::RenderPath::Deferred, Blu::RendererAPI::API::Direct3D);
		Require(deferred.UsesDeferred(), "DX11 deferred request did not select deferred rendering");
		Require(deferred.Stages.size() == 5, "DX11 deferred pipeline had an unexpected stage count");
		Require(deferred.Stages[0] == Blu::SceneRenderStage::GBufferGeometry, "DX11 deferred pipeline did not begin with G-buffer geometry");
		Require(deferred.Stages[1] == Blu::SceneRenderStage::DeferredLighting, "DX11 deferred pipeline did not light after geometry");
		Require(deferred.Stages[2] == Blu::SceneRenderStage::ForwardTransparent, "DX11 deferred pipeline did not retain transparent forward rendering");
		Require(deferred.Stages[3] == Blu::SceneRenderStage::Skybox, "DX11 deferred pipeline did not render the skybox before composition");
		Require(deferred.Stages[4] == Blu::SceneRenderStage::PostProcessComposition, "DX11 deferred pipeline did not finish with composition");

		const Blu::SceneRenderPipelinePlan openGLFallback =
			Blu::BuildSceneRenderPipelinePlan(Blu::RenderPath::Deferred, Blu::RendererAPI::API::OpenGL);
		Require(!openGLFallback.UsesDeferred(), "OpenGL selected the DX11-only deferred path");
		Require(openGLFallback.EffectivePath == Blu::RenderPath::Forward, "OpenGL deferred request did not fall back to forward rendering");
		Require(openGLFallback.Stages[0] == Blu::SceneRenderStage::ForwardOpaque, "OpenGL fallback did not preserve the forward path");
	}

	void TestSharedLightBufferPacking()
	{
		Blu::DirLightData directional;
		directional.Direction = { 0.0f, -1.0f, 0.0f };
		Blu::PointLightData point;
		point.Position = { 1.0f, 2.0f, 3.0f };
		Blu::SpotLightData spot;
		spot.Position = { 4.0f, 5.0f, 6.0f };

		const Blu::LightDataGPU packed = Blu::BuildLightDataGPU({ directional }, { point }, { spot });
		Require(packed.NumDirLights == 1 && packed.NumPointLights == 1 && packed.NumSpotLights == 1,
			"shared light buffer counts were not packed");
		Require(packed.PointLights[0].Position == point.Position, "shared point light position was not packed");
		Require(packed.SpotLights[0].Position == spot.Position, "shared spot light position was not packed");
	}

	void TestWorldAuthoringContracts()
	{
		Blu::TerrainSpec terrainSpec;
		terrainSpec.GridWidth = 2;
		terrainSpec.GridHeight = 3;
		terrainSpec.CellSize = 2.0f;
		const Blu::TerrainMeshData meshData = Blu::BuildTerrainMeshData(terrainSpec);
		Require(meshData.Vertices.size() == 12, "terrain CPU builder returned an unexpected vertex count");
		Require(meshData.Indices.size() == 36, "terrain CPU builder returned an unexpected index count");

		Blu::TerrainSpec invalidSpec;
		invalidSpec.GridWidth = 0;
		invalidSpec.GridHeight = -4;
		invalidSpec.CellSize = 0.0f;
		invalidSpec.HeightScale = -1.0f;
		const Blu::TerrainSpec sanitized = Blu::SanitizeTerrainSpec(invalidSpec);
		Require(sanitized.GridWidth == 1 && sanitized.GridHeight == 1, "terrain dimensions were not sanitized");
		Require(sanitized.CellSize > 0.0f && sanitized.HeightScale == 0.0f, "terrain scalar values were not sanitized");

		const std::filesystem::path testDirectory = std::filesystem::temp_directory_path() /
			("BluTestsWorldAuthoring-" + std::to_string((uint64_t)Blu::UUID()));
		const std::filesystem::path scenePath = testDirectory / "WorldAuthoring.blu";
		std::filesystem::create_directories(testDirectory);

		auto scene = std::make_shared<Blu::Scene>();
		Blu::Entity entity = scene->CreateEntity("AuthoredWorldSystems");
		entity.AddComponent<Blu::TerrainComponent>().Spec = terrainSpec;
		auto& animator = entity.AddComponent<Blu::AnimatorComponent>();
		animator.CurrentClipIndex = 3;
		animator.CurrentTime = 12.5f;
		animator.Playing = false;
		animator.Loop = false;
		animator.SpeedScale = 0.75f;

		Blu::SceneSerializer serializer(scene);
		serializer.Serialize(scenePath.string());
		std::ifstream serializedScene(scenePath);
		std::stringstream serializedText;
		serializedText << serializedScene.rdbuf();
		const std::string yaml = serializedText.str();
		Require(yaml.find("TerrainComponent:") != std::string::npos, "terrain descriptor was not serialized");
		Require(yaml.find("GridWidth: 2") != std::string::npos, "terrain width was not serialized");
		Require(yaml.find("AnimatorComponent:") != std::string::npos, "animator authoring state was not serialized");
		Require(yaml.find("Playing: false") != std::string::npos, "animator playback state was not serialized");

		std::error_code cleanupError;
		std::filesystem::remove_all(testDirectory, cleanupError);
	}

	void TestSceneVersioningAndRoundTrip()
	{
		const std::filesystem::path testDirectory = std::filesystem::temp_directory_path() /
			("BluTestsSceneRoundTrip-" + std::to_string((uint64_t)Blu::UUID()));
		const std::filesystem::path scenePath = testDirectory / "RoundTrip.blu";
		std::filesystem::create_directories(testDirectory);

		auto readFile = [](const std::filesystem::path& path)
		{
			std::ifstream file(path);
			std::stringstream buffer;
			buffer << file.rdbuf();
			return buffer.str();
		};

		// Build a small, deterministic single-entity scene (stable serialization order).
		auto scene = std::make_shared<Blu::Scene>();
		Blu::Entity entity = scene->CreateEntity("RoundTripEntity");
		if (entity.HasComponent<Blu::TransformComponent>())
		{
			auto& transform = entity.GetComponent<Blu::TransformComponent>();
			transform.Translation = { 1.0f, 2.0f, 3.0f };
			transform.Scale = { 2.0f, 2.0f, 2.0f };
		}
		auto& animator = entity.AddComponent<Blu::AnimatorComponent>();
		animator.CurrentClipIndex = 2;
		animator.CurrentTime = 5.0f;
		animator.Playing = false;
		animator.Loop = true;
		animator.SpeedScale = 1.5f;

		Blu::SceneSerializer serializer(scene);
		serializer.Serialize(scenePath.string());
		const std::string firstPass = readFile(scenePath);
		Require(firstPass.find("SceneVersion: 1") != std::string::npos,
			"serialized scene did not record the current SceneVersion");

		// Round trip: deserialize then re-serialize to the same path; bytes must match
		// (re-using the path keeps the embedded "Scene:" value identical across passes).
		auto reloaded = std::make_shared<Blu::Scene>();
		Blu::SceneSerializer reloadedSerializer(reloaded);
		Require(reloadedSerializer.Deserialize(scenePath.string()), "round-trip scene did not deserialize");
		Require(reloadedSerializer.GetLoadedSceneVersion() == Blu::SceneSerializer::kCurrentSceneVersion,
			"versioned scene did not report the current version on load");
		reloadedSerializer.Serialize(scenePath.string());
		const std::string secondPass = readFile(scenePath);
		Require(firstPass == secondPass, "scene serialization was not stable across a round trip");

		// Legacy scenes (no SceneVersion key) must be treated as version 0.
		const std::filesystem::path legacyPath = testDirectory / "Legacy.blu";
		{
			std::ofstream legacy(legacyPath);
			legacy << "Scene: Legacy\nEntities:\n  - Entity: 7\n    TagComponent:\n      Tag: Old\n";
		}
		auto legacyScene = std::make_shared<Blu::Scene>();
		Blu::SceneSerializer legacySerializer(legacyScene);
		Require(legacySerializer.Deserialize(legacyPath.string()), "legacy scene did not deserialize");
		Require(legacySerializer.GetLoadedSceneVersion() == 0, "legacy scene was not treated as version 0");

		std::error_code cleanupError;
		std::filesystem::remove_all(testDirectory, cleanupError);
	}

	void TestAudioBackendIsCompiled()
	{
		Require(Blu::AudioEngine::Get().IsBackendCompiled(), "miniaudio backend was not compiled into Blu");
	}

	static bool HUDHasBinding(const std::vector<Blu::UIWidget>& widgets, Blu::UIBinding binding)
	{
		for (const auto& w : widgets)
		{
			if (w.Binding == binding) return true;
			if (HUDHasBinding(w.Children, binding)) return true;
		}
		return false;
	}

	void TestAuthoredGameplaySliceAssets()
	{
		Blu::UIDocument hud;
		Require(Blu::RuntimeUI::LoadDocument("assets/ui/GameplayHUD.bluui", hud), "authored gameplay HUD did not load");
		Require(!hud.Widgets.empty(), "authored gameplay HUD has no widgets");
		// Wave/Score/Lives bindings must author + parse (Phase 13 HUD additions).
		Require(HUDHasBinding(hud.Widgets, Blu::UIBinding::Wave),  "gameplay HUD missing Wave binding");
		Require(HUDHasBinding(hud.Widgets, Blu::UIBinding::Score), "gameplay HUD missing Score binding");
		Require(HUDHasBinding(hud.Widgets, Blu::UIBinding::Lives), "gameplay HUD missing Lives binding");

		// Find the repo root (dir containing Blu.sln) by walking up from CWD, so this passes
		// regardless of the working directory (Visual Studio launches from the project/bin
		// folder, not the repo root) and independent of any virtual mounts other tests re-point.
		std::filesystem::path repoRoot = std::filesystem::current_path();
		for (std::filesystem::path p = repoRoot; !p.empty(); p = p.parent_path())
		{
			std::error_code ec;
			if (std::filesystem::exists(p / "Blu.sln", ec)) { repoRoot = p; break; }
			if (p == p.root_path()) break;
		}
		std::ifstream scene(repoRoot / "Blu-Editor" / "assets" / "scenes" / "GameplayCoreTest.blu");
		std::stringstream sceneText;
		sceneText << scene.rdbuf();
		const std::string yaml = sceneText.str();
		Require(yaml.find("ActorComponent:") != std::string::npos, "gameplay slice does not use native actor components");
		Require(yaml.find("NativeScriptComponent:") == std::string::npos, "gameplay slice still contains legacy native script components");
		Require(yaml.find("TerrainComponent:") != std::string::npos, "gameplay slice does not persist terrain");
	}

	void TestJoltConfigurationCompatibility()
	{
		Require(Blu::IsJoltConfigurationCompatible(), "Blu and Jolt were compiled with incompatible configuration defines");
	}

	// CPU mirror of the decal projection in PostProcess_Decals.hlsl: transform a world point into
	// the decal's local unit box and compute the radial blend alpha. Validates the box test +
	// radial falloff that the shader uses, headlessly.
	static float DecalAlpha(const glm::mat4& invWorld, glm::vec3 worldPos, float opacity, float falloff)
	{
		glm::vec3 local = glm::vec3(invWorld * glm::vec4(worldPos, 1.0f));
		glm::vec3 a = glm::abs(local);
		if (a.x > 0.5f || a.y > 0.5f || a.z > 0.5f) return 0.0f;
		auto smooth = [](float e0, float e1, float x) { float t = glm::clamp((x - e0) / (e1 - e0), 0.0f, 1.0f); return t * t * (3.0f - 2.0f * t); };
		float r     = glm::length(glm::vec2(local.x, local.z)) * 2.0f;
		float edge  = 1.0f - smooth(1.0f - glm::clamp(falloff, 0.0f, 1.0f), 1.0f, r);
		float fadeY = 1.0f - smooth(0.3f, 0.5f, a.y);
		return glm::clamp(edge * fadeY * opacity, 0.0f, 1.0f);
	}

	void TestDecalProjection()
	{
		auto approx = [](float a, float b) { return std::fabs(a - b) < 1e-3f; };
		const glm::mat4 unit(1.0f); // unit box at origin

		// Centre of the box: full radial weight -> alpha == opacity.
		Require(approx(DecalAlpha(unit, glm::vec3(0, 0, 0), 0.85f, 0.55f), 0.85f), "decal centre alpha should equal opacity");
		// Outside the box (X and Y) -> no decal.
		Require(DecalAlpha(unit, glm::vec3(2, 0, 0), 0.85f, 0.55f) == 0.0f, "point outside decal box (x) -> 0");
		Require(DecalAlpha(unit, glm::vec3(0, 2, 0), 0.85f, 0.55f) == 0.0f, "point above decal box (y) -> 0");

		// A 4 x 1 x 4 box: InvWorld scales world XZ by 1/4 into the unit box.
		glm::mat4 inv(1.0f); inv[0][0] = 0.25f; inv[2][2] = 0.25f;
		Require(DecalAlpha(inv, glm::vec3(1.5f, 0, 0), 0.85f, 0.55f) > 0.0f, "point inside scaled decal box -> > 0");
		Require(DecalAlpha(inv, glm::vec3(2.5f, 0, 0), 0.85f, 0.55f) == 0.0f, "point outside scaled decal box -> 0");
	}

	// Validates Scene::UpdateDecalLifetimes — the per-frame ager that retires the transient blood /
	// bullet-hole decals actors spawn on impact. Transient decals (Lifetime >= 0) count down and are
	// destroyed once expired; editor-placed permanent decals (Lifetime < 0) must never age out.
	void TestDecalLifetime()
	{
		auto scene = std::make_shared<Blu::Scene>();

		Blu::Entity transient = scene->CreateEntity("Transient");
		transient.AddComponent<Blu::DecalComponent>().Lifetime = 1.0f;
		Blu::Entity permanent = scene->CreateEntity("Permanent");
		permanent.AddComponent<Blu::DecalComponent>().Lifetime = -1.0f;

		auto countDecals = [&]() {
			int n = 0;
			auto view = scene->GetAllEntitiesWith<Blu::DecalComponent>();
			for (auto e : view) { (void)e; ++n; }
			return n;
		};
		Require(countDecals() == 2, "expected 2 decals before aging");

		scene->UpdateDecalLifetimes(0.6f); // transient 1.0 -> 0.4 (still alive)
		Require(countDecals() == 2, "transient decal should survive while Lifetime > 0");

		scene->UpdateDecalLifetimes(0.6f); // transient 0.4 -> -0.2 (expired, destroyed)
		Require(countDecals() == 1, "expired transient decal should be destroyed");

		// The survivor must be the permanent (Lifetime < 0) decal.
		auto view = scene->GetAllEntitiesWith<Blu::DecalComponent>();
		for (auto e : view)
		{
			Blu::Entity ent{ e, scene.get() };
			Require(ent.GetComponent<Blu::DecalComponent>().Lifetime < 0.0f,
				"permanent decal (Lifetime < 0) must never age out");
		}

		// A permanent decal stays put no matter how long the level runs.
		scene->UpdateDecalLifetimes(100.0f);
		Require(countDecals() == 1, "permanent decal should persist across a long tick");
	}

	// Validates the surface-alignment math for Phase-2 bullet-hole decals. A decal's local +Y is its
	// projection (stamp) axis. On a world hit PlayerCharacter stores Rotation = eulerAngles(rotation(
	// +Y, surfaceNormal)); the decal gather rebuilds world = translate * toMat4(quat(Rotation)) * scale.
	// Round-tripping that pipeline must carry local +Y onto the surface normal for every wall/floor
	// orientation — otherwise bullet holes float at wrong angles instead of lying flat on the surface.
	void TestBulletHoleDecalOrientation()
	{
		auto alignmentWith = [](const glm::vec3& nIn) {
			glm::vec3 n     = glm::normalize(nIn);
			glm::quat q     = glm::rotation(glm::vec3(0.0f, 1.0f, 0.0f), n); // PlayerCharacter::UpdateProjectiles
			glm::vec3 euler = glm::eulerAngles(q);                           // stored in TransformComponent.Rotation
			glm::mat4 world = glm::toMat4(glm::quat(euler));                 // Scene decal-gather reconstruction
			glm::vec3 mapped = glm::normalize(glm::vec3(world * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f)));
			return glm::dot(mapped, n); // 1.0 == perfectly aligned
		};
		Require(alignmentWith(glm::vec3(0, 0, -1))        > 0.999f, "decal +Y should align to a -Z wall normal");
		Require(alignmentWith(glm::vec3(0, 0,  1))        > 0.999f, "decal +Y should align to a +Z wall normal");
		Require(alignmentWith(glm::vec3(1, 0,  0))        > 0.999f, "decal +Y should align to a +X wall normal");
		Require(alignmentWith(glm::vec3(0, 1,  0))        > 0.999f, "decal +Y should align to a floor (+Y) normal");
		Require(alignmentWith(glm::vec3(0, -1, 0))        > 0.999f, "decal +Y should align to a ceiling (-Y) normal");
		Require(alignmentWith(glm::vec3(0.3f, 0.8f, -0.5f)) > 0.999f, "decal +Y should align to an angled normal");
	}

	// Validates SelectNearestDecals — the gather-time trim that honors the decal pass's GPU cap by
	// keeping the decals nearest the camera (so impacts around the player always render) instead of an
	// arbitrary ECS-iteration-order subset once a firefight pushes the count past the cap.
	void TestDecalNearestSelection()
	{
		std::vector<Blu::DecalGPU> decals;
		std::vector<glm::vec3> positions;
		// 5 decals along +X at distances 1..5 from the origin; tag each via Opacity = distance.
		for (int i = 1; i <= 5; ++i)
		{
			Blu::DecalGPU g; g.Opacity = (float)i;
			decals.push_back(g);
			positions.push_back(glm::vec3((float)i, 0.0f, 0.0f));
		}
		Blu::SelectNearestDecals(decals, positions, glm::vec3(0.0f), 3);
		Require(decals.size() == 3, "selection should trim to maxCount");
		Require(positions.size() == 3, "parallel position list should trim with the decals");
		// The 3 nearest are at distance 1,2,3 (tags 1,2,3); the far ones (4,5) must be dropped.
		float maxTag = 0.0f;
		for (auto& d : decals) if (d.Opacity > maxTag) maxTag = d.Opacity;
		Require(maxTag <= 3.0f + 0.001f, "selection kept a farther decal over a nearer one");

		// No-op when already within the cap.
		std::vector<Blu::DecalGPU> few(2);
		std::vector<glm::vec3> fewPos(2, glm::vec3(0.0f));
		Blu::SelectNearestDecals(few, fewPos, glm::vec3(0.0f), 32);
		Require(few.size() == 2, "selection must not grow or trim when within the cap");
	}

	// CPU mirror of the analytic ray-segment overlap in PostProcess_FogVolume.hlsl. Validates the
	// fog-integration math headlessly — the shader uses the identical algorithm, so getting these
	// geometric cases right is the high-confidence check that localized fog accumulates correctly.
	static float FogBoxOverlap(glm::vec3 ro, glm::vec3 rd, float segLen, glm::vec3 center, glm::vec3 halfExt)
	{
		// Mirror the shader's axis-parallel guard (avoids 0*inf = NaN on a slab face).
		glm::vec3 safeRd(std::fabs(rd.x) < 1e-6f ? 1e-6f : rd.x,
		                 std::fabs(rd.y) < 1e-6f ? 1e-6f : rd.y,
		                 std::fabs(rd.z) < 1e-6f ? 1e-6f : rd.z);
		glm::vec3 inv = 1.0f / safeRd;
		glm::vec3 t0  = (center - halfExt - ro) * inv;
		glm::vec3 t1  = (center + halfExt - ro) * inv;
		glm::vec3 tlo = glm::min(t0, t1);
		glm::vec3 thi = glm::max(t0, t1);
		float tEnter = std::fmax(std::fmax(std::fmax(tlo.x, tlo.y), tlo.z), 0.0f);
		float tExit  = std::fmin(std::fmin(std::fmin(thi.x, thi.y), thi.z), segLen);
		return std::fmax(0.0f, tExit - tEnter);
	}
	static float FogSphereOverlap(glm::vec3 ro, glm::vec3 rd, float segLen, glm::vec3 center, float radius)
	{
		glm::vec3 oc = ro - center;
		float b = glm::dot(oc, rd);
		float c = glm::dot(oc, oc) - radius * radius;
		float disc = b * b - c;
		if (disc < 0.0f) return 0.0f;
		float s = std::sqrt(disc);
		float tEnter = std::fmax(-b - s, 0.0f);
		float tExit  = std::fmin(-b + s, segLen);
		return std::fmax(0.0f, tExit - tEnter);
	}

	void TestFogVolumeRayMath()
	{
		auto approx = [](float a, float b) { return std::fabs(a - b) < 1e-3f; };
		const glm::vec3 ro(0.0f), rd(1.0f, 0.0f, 0.0f);

		// Box centered at x=10, half-extents 2 → spans [8,12]. Full ray passes through: overlap 4.
		Require(approx(FogBoxOverlap(ro, rd, 100.0f, glm::vec3(10, 0, 0), glm::vec3(2)), 4.0f), "box full overlap should be 4");
		// Surface before the box (segLen=8) → ray ends at the entry plane → overlap 0.
		Require(approx(FogBoxOverlap(ro, rd, 8.0f, glm::vec3(10, 0, 0), glm::vec3(2)), 0.0f), "box overlap clipped to segLen should be 0");
		// Surface inside the box (segLen=10) → covers [8,10] → overlap 2.
		Require(approx(FogBoxOverlap(ro, rd, 10.0f, glm::vec3(10, 0, 0), glm::vec3(2)), 2.0f), "box partial overlap should be 2");
		// Ray offset in Y beyond the box → miss → overlap 0.
		Require(approx(FogBoxOverlap(glm::vec3(0, 10, 0), rd, 100.0f, glm::vec3(10, 0, 0), glm::vec3(2)), 0.0f), "box miss should be 0");

		// Sphere centered at x=10 radius 3 → spans [7,13]. Full ray: overlap 6.
		Require(approx(FogSphereOverlap(ro, rd, 100.0f, glm::vec3(10, 0, 0), 3.0f), 6.0f), "sphere full overlap should be 6");
		// Surface inside the sphere (segLen=10) → covers [7,10] → overlap 3.
		Require(approx(FogSphereOverlap(ro, rd, 10.0f, glm::vec3(10, 0, 0), 3.0f), 3.0f), "sphere partial overlap should be 3");
		// Ray offset in Y beyond the radius → miss → overlap 0.
		Require(approx(FogSphereOverlap(glm::vec3(0, 10, 0), rd, 100.0f, glm::vec3(10, 0, 0), 3.0f), 0.0f), "sphere miss should be 0");

		// Axis-parallel ray (zero rd components) straight through a box must not NaN; full chord = 4.
		Require(approx(FogBoxOverlap(glm::vec3(0, 0, -10), glm::vec3(0, 0, 1), 100.0f, glm::vec3(0), glm::vec3(2)), 4.0f),
			"axis-parallel ray through box should be 4");
		// Ray grazing exactly along a box face (the degenerate 0*inf case) must stay finite.
		{
			float g = FogBoxOverlap(glm::vec3(2, 0, -10), glm::vec3(0, 0, 1), 100.0f, glm::vec3(0), glm::vec3(2));
			Require(g == g && g >= 0.0f && g <= 4.0f, "grazing-face ray must be finite (no NaN)");
		}
	}

	// The process-memory query backing the editor's live memory readout must return real numbers.
	void TestPerfStatsMemory()
	{
		Blu::Perf::MemoryInfo mem = Blu::Perf::QueryProcessMemory();
		Require(mem.WorkingSetBytes > 0, "process working set should be > 0");
		Require(mem.PrivateBytes > 0, "process private bytes should be > 0");
		Require(mem.PeakWorkingSetBytes >= mem.WorkingSetBytes, "peak working set should be >= current");
	}

	// Validates Animator::BlendClips crossfade math (Phase 10a): per-bone matrix lerp at the
	// transition endpoints and midpoint, with clamping. The Scene tick drives this during a PlayClip().
	void TestAnimatorBlend()
	{
		auto approx = [](float a, float b) { return std::fabs(a - b) < 1e-4f; };
		std::vector<glm::mat4> a(8, glm::mat4(1.0f)); // identity (diagonal 1)
		std::vector<glm::mat4> b(8, glm::mat4(3.0f)); // diagonal 3
		std::vector<glm::mat4> out;

		Blu::Animator::BlendClips(0.0f, a, b, out);
		Require(out.size() == 8, "BlendClips output size mismatch");
		Require(approx(out[0][0][0], 1.0f), "t=0 should equal clip A");
		Blu::Animator::BlendClips(1.0f, a, b, out);
		Require(approx(out[0][0][0], 3.0f), "t=1 should equal clip B");
		Blu::Animator::BlendClips(0.5f, a, b, out);
		Require(approx(out[0][0][0], 2.0f), "t=0.5 should be the midpoint");
		Blu::Animator::BlendClips(2.0f, a, b, out);
		Require(approx(out[0][0][0], 3.0f), "t>1 should clamp to clip B");
		Blu::Animator::BlendClips(-1.0f, a, b, out);
		Require(approx(out[0][0][0], 1.0f), "t<0 should clamp to clip A");
	}

	// Verifies the project layer that backs the editor's "--project" launch: a .bluproj round-trips,
	// loading it re-points the project:// and cache:// virtual mounts at the project (so assets and
	// the asset registry become project-scoped), derived paths resolve, and a failed load is inert.
	void TestProjectManagerLoadAndMounts()
	{
		namespace fs = std::filesystem;
		auto& fileSystem = Blu::FileSystemService::Get();

		// Capture the current default project root (from the mount table, which stores the clean
		// canonicalized path) so we can restore the default mounts when we're done.
		const fs::path originalProjectRoot = []
		{
			const auto& mounts = Blu::FileSystemService::Get().GetMounts();
			auto it = mounts.find("project");
			return it != mounts.end() ? it->second : fs::path{};
		}();

		std::error_code ec;
		const fs::path projectRoot = fs::temp_directory_path() / "BluProjectManagerTest";
		fs::remove_all(projectRoot, ec);
		fs::create_directories(projectRoot / "assets" / "scenes", ec);

		// Author a minimal startup scene and a .bluproj that points at it.
		{
			std::ofstream scene(projectRoot / "assets" / "scenes" / "Sample.blu");
			scene << "Scene: Sample\nSceneVersion: 1\nEntities:\n  []\n";
		}
		Blu::Project authored;
		authored.Name = "SampleProject";
		authored.AssetsDirectory = "assets";
		authored.StartupScene = "assets/scenes/Sample.blu";
		const fs::path manifest = projectRoot / "SampleProject.bluproj";
		Require(Blu::ProjectManager::SaveProject(authored, manifest), "SaveProject failed to write the manifest");
		Require(fs::exists(manifest), "manifest was not written to disk");

		// Load from the directory (exercises single-manifest discovery) and confirm activation.
		auto& projects = Blu::ProjectManager::Get();
		Require(projects.LoadProject(projectRoot), "LoadProject failed for a valid project directory");
		Require(projects.HasActiveProject(), "project did not become active after a successful load");
		Require(projects.GetActiveProject().Name == "SampleProject", "project name did not round-trip through the manifest");

		// project:// and cache:// must now point under the project root. Inspect the mount table
		// directly (its stored paths are clean — Resolve("x://") appends an empty component that can
		// leave a trailing separator, which would confuse filename()/parent_path()).
		const auto& mounts = fileSystem.GetMounts();
		auto projectMount = mounts.find("project");
		auto cacheMount = mounts.find("cache");
		Require(projectMount != mounts.end(), "project mount is missing after load");
		Require(cacheMount != mounts.end(), "cache mount is missing after load");
		Require(fs::equivalent(projectMount->second, projectRoot, ec) && !ec, "project:// was not re-pointed to the project root");
		Require(cacheMount->second.filename() == ".cache", "cache:// did not resolve to <project>/.cache");
		Require(fs::equivalent(cacheMount->second.parent_path(), projectRoot, ec) && !ec, "cache:// was not re-pointed under the project root");

		// Derived paths.
		Require(fs::equivalent(projects.GetAssetsPath(), projectRoot / "assets", ec) && !ec, "GetAssetsPath did not resolve to <root>/assets");
		Require(fs::exists(projects.GetStartupScenePath()), "GetStartupScenePath did not resolve to an existing scene file");

		// A failed load must leave the previously active project untouched.
		Require(!projects.LoadProject(projectRoot / "DoesNotExist.bluproj"), "LoadProject should fail for a missing manifest");
		Require(projects.HasActiveProject() && projects.GetActiveProject().Name == "SampleProject",
			"a failed load must not clobber the active project");

		// Restore global state so we don't disturb any later tests.
		projects.Clear();
		fileSystem.Reset();
		if (!originalProjectRoot.empty())
			fileSystem.MountDefaults(originalProjectRoot);
		fs::remove_all(projectRoot, ec);
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
		TestAssetMetaStableHandles();
		TestStaticMeshAssetTyping();
		TestMeshComponentModelHandleMigration();
		TestAssetHandleMigrationAcrossComponents();
		TestLifetimeUtilities();
		TestMaterialResolver();
		TestMaterialAssetPersistence();
		TestMaterialAssetFullFidelity();
		TestMeshComponentMaterialHandle();
		TestMaterialGraphCompiler();
		TestSceneRenderPipelinePlan();
		TestSharedLightBufferPacking();
		TestWorldAuthoringContracts();
		TestSceneVersioningAndRoundTrip();
		TestAudioBackendIsCompiled();
		TestAuthoredGameplaySliceAssets();
		TestJoltConfigurationCompatibility();
		TestProjectManagerLoadAndMounts();
		TestFogVolumeRayMath();
		TestDecalProjection();
		TestDecalLifetime();
		TestBulletHoleDecalOrientation();
		TestDecalNearestSelection();
		TestAnimatorBlend();
		TestPerfStatsMemory();
	}
	catch (const std::exception& error)
	{
		std::cerr << "Blu-Tests failure: " << error.what() << '\n';
		return 1;
	}
	std::cout << "Blu-Tests: actor lifecycle tests passed\n";
	return 0;
}
