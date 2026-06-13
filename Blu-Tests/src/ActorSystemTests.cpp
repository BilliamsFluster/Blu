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
#include "Blu/Rendering/MaterialSystem.h"
#include "Blu/Rendering/MaterialGraph.h"
#include "Blu/Rendering/LightBufferData.h"
#include "Blu/Rendering/SceneRenderPipeline.h"
#include "Blu/Rendering/Terrain.h"
#include "Blu/UI/RuntimeUI.h"
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
		TestLifetimeUtilities();
		TestMaterialResolver();
		TestMaterialGraphCompiler();
		TestSceneRenderPipelinePlan();
		TestSharedLightBufferPacking();
		TestWorldAuthoringContracts();
		TestSceneVersioningAndRoundTrip();
		TestAudioBackendIsCompiled();
		TestAuthoredGameplaySliceAssets();
		TestJoltConfigurationCompatibility();
	}
	catch (const std::exception& error)
	{
		std::cerr << "Blu-Tests failure: " << error.what() << '\n';
		return 1;
	}
	std::cout << "Blu-Tests: actor lifecycle tests passed\n";
	return 0;
}
