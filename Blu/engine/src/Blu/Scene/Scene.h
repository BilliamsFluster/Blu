#pragma once
#include "entt.hpp"
#include "Component.h"
#include "Blu/Core/Timestep.h"
#include "Blu/Rendering/Renderer3D.h"
#include "Blu/Rendering/TimeOfDay.h"
#include <filesystem>
#include <string>

class b2World;
namespace Blu
{
	class Entity;
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
		uint32_t SpringArmCount = 0;
		uint32_t NativeScriptCount = 0;
		bool PlayerInputEnabled = false;
		bool EjectCameraActive = false;
		std::string PrimaryCameraName;
		std::string PossessedPawnName;
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

		// Eject mode: game logic keeps running but viewport renders from the editor camera.
		void BeginEject(EditorCamera* cam) { m_EjectCamera = cam; }
		void EndEject()                    { m_EjectCamera = nullptr; }

	private:
		entt::registry m_Registry; // container for all of our entt components
		float m_ViewportWidth = 0.0f, m_ViewportHeight = 0.0f;
		std::unordered_map<UUID, entt::entity> m_EntityMap;
		std::filesystem::path m_SceneFilePath; 
		Shared<LightManager> m_LightManager;

		b2World* m_PhysicsWorld = nullptr;
		Physics3DWorld* m_Physics3DWorld = nullptr;
		Shared<PostProcess> m_PostProcess;
		Shared<Skybox>      m_Skybox;
		friend class Entity;
		friend class SceneSerializer;
		friend class SceneHierarchyPanel;
	private:
		void Render2DPass(class EditorCamera& camera, Timestep deltaTime);
		void Render2DPass(Camera& camera, const glm::mat4& transform, Timestep deltaTime, bool updateParticles);
		void Render3DPass(class EditorCamera& camera);
		void Render3DPass(Camera& camera, const glm::mat4& transform);
		void ShadowPass(const std::vector<DirLightData>& dirLights,
		                const glm::mat4& cameraVP, float cameraNear, float cameraFar);
		void UpdateSpringArmCameras(float deltaTime);

		class EditorCamera* m_EjectCamera = nullptr;
		bool        m_ScenePaused    = false;
		bool        m_PlayerInputEnabled = true;
		int         m_StepFrames    = 0;
		bool        m_UseShadows    = false;
		bool        m_UsePostProcess= false;
		bool        m_UseSkybox     = false;
		bool        m_UseTimeOfDay  = false;
		FogSettings             m_Fog;
		TimeOfDayController     m_TimeOfDay;
		float       m_ElapsedTime   = 0.0f;
	};
}
