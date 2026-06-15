#pragma once
#include "entt.hpp"
#include "Component.h"
#include "Blu/Core/Timestep.h"
#include "Blu/Rendering/Renderer3D.h"
#include "Blu/Rendering/TimeOfDay.h"
#include <filesystem>
#include <string>
#include <vector>
#include <set>

class b2World;
namespace Blu
{
	class Entity;
	class AActor;
	class AGameMode;
	class ActorSystem;
	class PostProcess;
	class Skybox;
	class Physics3DWorld;

	struct SceneDiagnostics
	{
		uint32_t EntityCount = 0;
		uint32_t CameraCount = 0;
		uint32_t PrimaryCameraCount = 0;
		uint32_t MeshEntityCount = 0;
		uint32_t DrawableMeshCount = 0;
		uint32_t VisualOffsetCount = 0;
		uint32_t SpringArmCount = 0;
		uint32_t NativeScriptCount = 0;
		uint32_t AssetReferenceCount = 0;
		uint32_t MissingAssetCount = 0;
		uint32_t ExternalAssetCount = 0;
		uint32_t ImportedAssetCount = 0;
		uint32_t Rigidbody3DCount = 0;
		uint32_t StaticBody3DCount = 0;
		uint32_t DynamicBody3DCount = 0;
		uint32_t KinematicBody3DCount = 0;
		uint32_t RuntimePhysicsBody3DCount = 0;
		uint32_t BoxCollider3DCount = 0;
		uint32_t SphereCollider3DCount = 0;
		uint32_t CapsuleCollider3DCount = 0;
		uint32_t MeshCollider3DCount = 0;
		uint32_t MeshColliderTriangleCount = 0;
		uint32_t MissingCollider3DCount = 0;
		uint32_t InvalidMeshCollider3DCount = 0;
		uint32_t PhysicsBodyCreationFailureCount = 0;
		uint32_t CharacterControllerCount = 0;
		uint32_t RuntimeCharacterControllerCount = 0;
		uint32_t InvalidCharacterColliderCount = 0;
		uint32_t InteractableCount = 0;
		uint32_t PickupCount = 0;
		uint32_t ActiveZombieCount = 0;
		uint32_t UIRootCount = 0;
		uint32_t RuntimeUIWidgetCount = 0;
		bool RuntimeUIRendered = false;
		bool RuntimeUIMissingDocument = false;
		float RuntimeUIViewportWidth = 0.0f;
		float RuntimeUIViewportHeight = 0.0f;
		bool PlayerInputEnabled = false;
		bool EjectCameraActive = false;
		bool PossessedPawnGrounded = false;
		bool PossessedPawnHasVisualOffset = false;
		bool PossessedPawnHasStats = false;
		float PossessedPawnCapsuleRadius = 0.0f;
		float PossessedPawnCapsuleHalfHeight = 0.0f;
		float PossessedPawnHealth = 0.0f;
		float PossessedPawnMaxHealth = 0.0f;
		float PossessedPawnStamina = 0.0f;
		float PossessedPawnMaxStamina = 0.0f;
		glm::vec3 PossessedPawnFeetPosition = { 0.0f, 0.0f, 0.0f };
		glm::vec3 PossessedPawnVelocity = { 0.0f, 0.0f, 0.0f };
		std::string PrimaryCameraName;
		std::string PossessedPawnName;
		std::string NearbyInteractableName;
		std::string RuntimeUIDocumentPath;
		bool  PossessedPawnHasAmmo     = false;
		int   PossessedPawnAmmoInMag   = 0;
		int   PossessedPawnAmmoReserve = 0;
		bool  PossessedPawnReloading   = false;
		float HitmarkerTimer           = 0.0f; // >0 while the hitmarker should show
		// Game-mode HUD stats (filled by AGameMode::PopulateHUD; HasWaveHUD gates display)
		bool  HasWaveHUD               = false;
		int   CurrentWave              = 0;
		int   TotalWaves               = 0;
		int   ZombiesKilled            = 0;
		int   PlayerLives              = 0;
	};

	struct AssetDependency
	{
		std::string Type;
		std::string Path;
		std::string ResolvedPath;
		uint64_t SourceEntity = 0;
		std::string SourceTag;
		std::string SourceComponent;
		bool Exists = false;
		bool External = false;
		bool Imported = false;
	};

	struct SceneAssetManifest
	{
		std::vector<AssetDependency> Dependencies;
		uint32_t ReferencedCount = 0;
		uint32_t MissingCount = 0;
		uint32_t ExternalCount = 0;
		uint32_t ImportedCount = 0;
	};

	class Scene
	{
	public:
		Scene();
		~Scene();

		static Shared<Scene> Copy(Shared<Scene> scene);
		Entity CreateEntity(const std::string& name = std::string());
		Entity CreateEntityWithUUID(UUID uuid, const std::string& name = std::string());
		Entity GetPrimaryCameraEntity();
		Entity EnsurePrimaryCamera();
		Entity FindEntityByName(std::string_view name);
		Entity GetEntityByUUID(UUID id);
		AActor* FindActor(UUID id) const;
		void SetGameModeClassID(const std::string& classID) { m_GameModeClassID = classID; }
		const std::string& GetGameModeClassID() const { return m_GameModeClassID; }
		AGameMode* GetGameMode() const { return m_GameMode.get(); }
		template<typename... Components>
		auto GetAllEntitiesWith()
		{
			return m_Registry.view<Components...>();
		}
		
		Entity DuplicateEntity(Entity& targetEntity);
		const std::filesystem::path& GetSceneFilePath() const
		{
			return m_SceneFilePath;
		}
		void SetSceneFilePath(std::filesystem::path& filepath)
		{
			m_SceneFilePath = filepath;
		}

		void OnRuntimeStart();
		void OnPhysics2DStart();
		void OnPhysics3DStart();
		void OnPhysics3DStop();
		void OnRuntimeStop();

		void UpdateActiveCameraComponent(Timestep deltaTime);

		void DestroyEntity(Entity entity);
		void OnUpdateEditor(Timestep deltaTime, class EditorCamera& camera);
		void OnUpdateRuntime(Timestep deltaTime);
		void OnUpdatePaused(Timestep deltaTime);
		void OnSceneStep(int frames = 1);
		void OnUpdateStep();
		void OnViewportResize(float width, float height);
		bool IsScenePaused() { return m_ScenePaused; }
		void SetScenePaused(bool paused) {m_ScenePaused = paused; }
		Shared<class LightManager> GetLightManager() { return m_LightManager; }
		void SetUseShadows(bool use) { m_UseShadows = use; }
		bool GetUseShadows() const { return m_UseShadows; }
		void SetUsePostProcess(bool use) { m_UsePostProcess = use; }
		bool GetUsePostProcess() const { return m_UsePostProcess; }
		Shared<PostProcess> GetPostProcess() const { return m_PostProcess; }
		void SetUseSkybox(bool use) { m_UseSkybox = use; }
		bool GetUseSkybox() const { return m_UseSkybox; }
		Shared<Skybox> GetSkybox() const { return m_Skybox; }
		FogSettings& GetFog() { return m_Fog; }
		void SetFogEnabled(bool e) { m_Fog.Enabled = e; }
		TimeOfDayController& GetTimeOfDay() { return m_TimeOfDay; }
		void SetUseTimeOfDay(bool use) { m_UseTimeOfDay = use; }
		bool GetUseTimeOfDay() const { return m_UseTimeOfDay; }
		void SetPlayerInputEnabled(bool enabled);
		bool IsPlayerInputEnabled() const { return m_PlayerInputEnabled; }
		SceneDiagnostics GetDiagnostics();
		SceneAssetManifest CollectAssetManifest();
		bool GenerateStaticMeshCollision(Entity entity, std::string* outMessage = nullptr);
		bool RebuildTerrain(Entity entity, std::string* outMessage = nullptr);
		bool FitCharacterVisualToCapsule(Entity entity, std::string* outMessage = nullptr);
		bool ResetVisualOffset(Entity entity, std::string* outMessage = nullptr);
		bool SnapCharacterFeetToGround(Entity entity, std::string* outMessage = nullptr);
		Entity CloneEntityFrom(Entity source, const std::string& nameOverride = {});

		// Eject mode: game logic keeps running but viewport renders from the editor camera.
		void BeginEject(EditorCamera* cam) { m_EjectCamera = cam; }
		void EndEject()                    { m_EjectCamera = nullptr; }

	private:
		entt::registry m_Registry; // container for all of our entt components
		Unique<ActorSystem> m_ActorSystem;
		Unique<AGameMode> m_GameMode;
		std::string m_GameModeClassID;
		float m_ViewportWidth = 0.0f, m_ViewportHeight = 0.0f;
		std::unordered_map<UUID, entt::entity> m_EntityMap;
		std::filesystem::path m_SceneFilePath; 
		Shared<LightManager> m_LightManager;

		b2World* m_PhysicsWorld = nullptr;
		Physics3DWorld* m_Physics3DWorld = nullptr;
		Shared<PostProcess> m_PostProcess;
		Shared<Skybox>      m_Skybox;
		std::set<std::string> m_EditorFolders; // Outliner organisational folders (incl. empty)
		friend class Entity;
		friend class SceneSerializer;
		friend class SceneHierarchyPanel;
	private:
		struct PostProcessSettingsSnapshot
		{
			bool EnableBloom = true;
			float BloomThreshold = 1.0f;
			float BloomStrength = 0.05f;
			bool EnableFXAA = true;
			int Preview = 0;
			bool EnableSSAO = true;
			float SSAORadius = 0.5f;
			float SSAOBias = 0.025f;
			float SSAOPower = 1.5f;
			int SSAOSamples = 16;
			float SSAOStrength = 1.0f;
		};

		void Render2DPass(class EditorCamera& camera, Timestep deltaTime);
		void Render2DPass(Camera& camera, const glm::mat4& transform, Timestep deltaTime, bool updateParticles);
		void Render3DPass(class EditorCamera& camera);
		void Render3DPass(Camera& camera, const glm::mat4& transform);
		void RenderRuntimeUI();
		void ShadowPass(const std::vector<DirLightData>& dirLights,
		                const glm::mat4& cameraVP, float cameraNear, float cameraFar);
		void UpdateSpringArmCameras(float deltaTime);

		class EditorCamera* m_EjectCamera = nullptr;
		bool        m_ScenePaused    = false;
		bool        m_PlayerInputEnabled = true;
		int         m_StepFrames    = 0;
		uint32_t   m_Physics3DBodyCreationFailureCount = 0;
		bool        m_UseShadows    = false;
		bool        m_UsePostProcess= false;
		bool        m_UseSkybox     = false;
		bool        m_UseTimeOfDay  = false;
		bool        m_HasPostProcessSettings = false;
		PostProcessSettingsSnapshot m_PostProcessSettings;
		FogSettings             m_Fog;
		TimeOfDayController     m_TimeOfDay;
		float       m_ElapsedTime   = 0.0f;
		uint32_t   m_LastRuntimeUIRootCount = 0;
		uint32_t   m_LastRuntimeUIWidgetCount = 0;
		bool       m_LastRuntimeUIRendered = false;
		bool       m_LastRuntimeUIMissingDocument = false;
		float      m_LastRuntimeUIViewportWidth = 0.0f;
		float      m_LastRuntimeUIViewportHeight = 0.0f;
		std::string m_LastRuntimeUIDocumentPath;
	};
}
