#include <Blu.h>
#include "Panels/SceneHierarchyPanel.h"
#include "Panels/ContentBrowserPanel.h"
#include "Panels/MaterialGraphPanel.h"
#include "Blu/Rendering/EditorCamera.h"
#include "Blu/Rendering/Terrain.h"
#include "EditorLog.h"
#include <chrono>


#pragma once

namespace Blu
{
	class BluEditorLayer : public Blu::Layers::Layer
	{
	public:
		BluEditorLayer();
		virtual ~BluEditorLayer() = default;
		virtual void OnAttach() override;
		virtual void OnDetach() override;
		void OnUpdate(Blu::Timestep deltaTime) override;
		void OnEvent(Blu::Events::Event& event) override;
		void OnOverlayRender();
		virtual void OnGuiDraw() override;
		bool OnKeyPressedEvent(Events::KeyPressedEvent& event);
		bool OnKeyReleasedEvent(Events::KeyReleasedEvent& event);
		bool OnKeyTypedEvent(Events::KeyTypedEvent& event);
		bool OnMouseButtonPressed(Events::MouseButtonPressedEvent& event);
		bool OnMouseButtonReleased(Events::MouseButtonReleasedEvent& event);
		bool OnMouseScrolledEvent(Events::MouseScrolledEvent& event);
		bool OnMouseMovedEvent(Events::MouseMovedEvent& event);
		bool OnWindowResizedEvent(Events::WindowResizeEvent& event);
		void GizmosTransform(glm::mat4& view, const glm::mat4& projection, glm::mat4& transform);
		void NewScene();
		void CreatePhysicsDemoScene();
		void Toolbar();
		void OpenScene();
		void OpenScene(const std::filesystem::path& path);
		void SaveSceneAs();
		void SaveCurrentScene();
		void UIDrawTitlebar(float& outTitlebarHeight);
		void ResetEditorLayout();
		void LoadEditorSettings();
		void SaveEditorSettings();
		void DrawActorEditor();
		void RenderActorPreview();
		void DrawPlaytestHUD();
		void SaveSelectedAsPrefab();
		Entity InstantiatePrefabAsset(const std::filesystem::path& path);

		void OnScenePlay();
		void OnScenePause();
		void OnSceneResume();
		void OnSceneStop();
		void OnSceneEject();
		void OnSceneRepossess();

		void OnScenePlayNewWindow();
		void OnSceneSimulate();

		void DisplayMissingSceneWarning();
	private:
		void QueueStaticCollisionPrompt(Entity entity);
		void DrawStaticCollisionImportPrompt();
		void ProcessPendingSceneLoad(); // honour SceneManager scene transitions during Play/Eject
		void QueueEntityDeleteConfirmation(Entity entity); // ask before destroying (Delete/Backspace + panel)
		void DrawDeleteEntityConfirmation();

		Blu::OrthographicCameraController m_CameraController;
		Blu::EditorCamera m_EditorCamera;
		Blu::ParticleSystem m_ParticleSystem;
		Blu::ParticleProps m_ParticleProps;
		float m_MousePosX, m_MousePosY;
		Blu::Shared<Blu::Texture2D> m_Texture, m_AppHeaderIcon, m_PlayIcon, m_PauseIcon,
			m_StopIcon, m_ExpandPlayOptionsIcon, m_StepIcon;
		Blu::Shared<Blu::Texture2D> m_TranslationIcon, m_RotationIcon, m_ScaleIcon, 
			m_WorldSpaceIcon, m_LocalSpaceIcon, m_CameraIcon, m_SelectIcon, m_SnappingIcon;

		Blu::Shared<Blu::VertexArray> m_VertexArray;
		Blu::Shared< Blu::IndexBuffer> m_IndexBuffer;
		Blu::Shared< Blu::VertexBuffer> m_VertexBuffer;
		Blu::Shared<Blu::OpenGLShader> m_FlatColorShader, m_QuadShader;
		Blu::Shared<Blu::FrameBuffer> m_FrameBuffer;
		Blu::Shared<Blu::FrameBuffer> m_CameraViewFrameBuffer;
		Blu::Shared<Blu::FrameBuffer> m_ActorPreviewFrameBuffer;
		glm::vec2 m_ViewportSize = { 0.0f, 0.0f };
		glm::vec2 m_ActorPreviewSize = { 512.0f, 512.0f };
		Entity m_CameraEntity;
		bool m_ViewPortFocused  = false;
		bool m_ViewPortHovered  = false;
		Blu::Shared<Scene> m_ActiveScene;
		Blu::Shared<Scene> m_EditorScene;
		std::vector<Entity> Entities;
		
		int m_ImGuizmoType = 0;
		int m_OperationMode = 0;
		bool enableTranslationSnap = false;
		bool enableRotationSnap = false;
		bool enableScaleSnap = false;
		bool m_TitleBarHovered = false;
		bool m_SceneMissing = false;
		bool m_PlayButtonHit = false;
		bool m_PendingEntityPick = false;
		bool m_F8Prev = false;
		bool m_ShowStaticCollisionImportPrompt = false;
		bool m_ResetEditorLayout = false;
		bool m_ShowActorEditor = false;
		bool m_ActorPreviewHovered = false;
		bool m_ActorPreviewFocused = false;
		bool m_ResetActorPreviewCamera = true;
		bool m_ShowSelectedColliderDebug = true;
		bool m_ShowCharacterDebug = true;
		bool m_ShowMeshColliderDebug = true;
		bool m_ShowCameraDebug = true;
		Entity m_PendingStaticCollisionEntity;
		std::string m_PendingStaticCollisionModelName;

		// Delete-entity confirmation modal (Delete/Backspace key + hierarchy context menu)
		bool m_ShowDeleteEntityConfirmation = false;
		Entity m_PendingDeleteEntity;
		std::string m_PendingDeleteEntityName;


		float translationSnapValue = 0.5f;
		float rotationSnapValue = 10.0f;
		float scaleSnapValue = 0.5f;
		/*Scene Panels */

		glm::vec2 m_ViewportBounds[2];
		Shared<SceneHierarchyPanel> m_SceneHierarchyPanel;
		Shared<ContentBrowserPanel> m_ContentBrowserPanel;
		Shared<MaterialGraphPanel> m_MaterialGraphPanel;
		EditorCamera m_ActorPreviewCamera;
		glm::vec2 m_ViewportOffset;
		int m_DrawnEntityID;
		Entity m_ActorEditorEntity;
		UUID m_LastActorPreviewEntityID = 0;
		std::string m_ImGuiIniPath;
		std::filesystem::path m_EditorSettingsPath;

		enum class SceneState
		{
			Edit = 0,
			Play = 1,
			Pause = 2,
			Simulate = 3,
			Eject = 4    // game keeps running, editor camera is active
		};

		enum class ViewMode { Lit = 0, Unlit, Wireframe };
		ViewMode m_ViewMode = ViewMode::Lit;

		SceneState m_SceneState = SceneState::Edit;

		// ---- Performance stats ----------------------------------------
		float m_FPS         = 0.0f;
		float m_FrameTimeMs = 0.0f;
		float m_CpuTimeMs   = 0.0f;
		float m_GpuTimeMs   = 0.0f;

		static constexpr int kPerfSamples = 128;
		float m_FrameTimePlot[kPerfSamples]{};
		float m_FpsPlot[kPerfSamples]{};
		int   m_PerfPlotOffset = 0;

		std::chrono::high_resolution_clock::time_point m_CpuTimerStart;
		float m_PerfPlotAccumMs = 0.0f;          // time since last graph sample
		static constexpr float kPlotIntervalMs = 50.0f; // sample every ~50 ms → ~20 Hz scroll

		// DX11 GPU timestamp queries (double-buffered).
		// Stored as void* so we don't need to pull d3d11.h into this header.
		void* m_GPUDisjointQuery[2]      = {};
		void* m_GPUTimestampBegin[2]     = {};
		void* m_GPUTimestampEnd[2]       = {};
		int   m_GPUQueryFrame            = 0;

		// OpenGL timer queries (double-buffered)
		uint32_t m_GLTimeQuery[2]  = {};

		// ---- Panel visibility (toggled via Window menu) --------------------
		bool m_ShowOutliner       = true;
		bool m_ShowDetails        = true;
		bool m_ShowContentBrowser = true;
		bool m_ShowOutputLog      = true;
		bool m_ShowRendering      = true;
		bool m_ShowRenderPath     = true;
		bool m_ShowDiagnostics    = true;
		bool m_ShowInputMap       = false;
		bool m_ShowMaterialGraph  = false;
		bool m_ShowSettings       = false;

		// ---- Terrain editor ------------------------------------------------
		bool        m_ShowTerrainPanel = false;
		TerrainSpec m_TerrainSpec;

		// ---- Output Log panel state ----------------------------------------
		bool m_LogShowTrace = true;
		bool m_LogShowInfo  = true;
		bool m_LogShowWarn  = true;
		bool m_LogShowError = true;
	};
}


