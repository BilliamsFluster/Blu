#include "Blu/GameFramework/ActorSystem.h"
#include "Blu/GameFramework/NativeClassRegistry.h"
#include "Blu/Core/Log.h"
#include "Blu/Core/FrameArena.h"
#include "Blu/Core/GenerationalHandle.h"
#include "Blu/Audio/AudioEngine.h"
#include "Blu/Physics/Physics3DDiagnostics.h"
#include "Blu/Scene/Component.h"
#include "Blu/Scene/Scene.h"
#include "Blu/Scene/SceneSerializer.h"
#include "Blu/Rendering/AssetManager.h"
#include "Blu/Rendering/StaticMeshAsset.h"
#include "Blu/Rendering/MaterialAsset.h"
#include "Blu/Rendering/MaterialSystem.h"
#include "Blu/Rendering/MaterialGraph.h"
#include "Blu/Rendering/LightBufferData.h"
#include "Blu/Rendering/SceneRenderPipeline.h"
#include "Blu/Rendering/Terrain.h"
#include "Blu/UI/RuntimeUI.h"
#include "Blu/Utils/FileSystemService.h"
#include "Blu/Project/Project.h"
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

	void TestAuthoredGameplaySliceAssets()
	{
		Blu::UIDocument hud;
		Require(Blu::RuntimeUI::LoadDocument("assets/ui/GameplayHUD.bluui", hud), "authored gameplay HUD did not load");
		Require(!hud.Widgets.empty(), "authored gameplay HUD has no widgets");

		std::ifstream scene("Blu-Editor/assets/scenes/GameplayCoreTest.blu");
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
	}
	catch (const std::exception& error)
	{
		std::cerr << "Blu-Tests failure: " << error.what() << '\n';
		return 1;
	}
	std::cout << "Blu-Tests: actor lifecycle tests passed\n";
	return 0;
}
