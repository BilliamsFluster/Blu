#include <Blu.h>
#include "Panels/SceneHierarchyPanel.h"


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
		virtual void OnGuiDraw() override;
		bool OnKeyPressedEvent(Events::KeyPressedEvent& event);
		bool OnKeyReleasedEvent(Events::KeyReleasedEvent& event);
		bool OnKeyTypedEvent(Events::KeyTypedEvent& event);
		bool OnMouseButtonPressed(Events::MouseButtonPressedEvent& event);
		bool OnMouseButtonReleased(Events::MouseButtonReleasedEvent& event);
		bool OnMouseScrolledEvent(Events::MouseScrolledEvent& event);
		bool OnMouseMovedEvent(Events::MouseMovedEvent& event);
		bool OnWindowResizedEvent(Events::WindowResizeEvent& event);

		void NewScene();
		void OpenScene();
		void SaveSceneAs();

	private:
		Blu::OrthographicCameraController m_CameraController;

		Blu::ParticleSystem m_ParticleSystem;
		Blu::ParticleProps m_ParticleProps;
		float m_MousePosX, m_MousePosY;
		Blu::Shared<Blu::Texture2D> m_Texture;
		Blu::Shared<Blu::Texture2D> m_WallpaperTexture;
		Blu::Shared<Blu::VertexArray> m_VertexArray;
		Blu::Shared< Blu::IndexBuffer> m_IndexBuffer;
		Blu::Shared< Blu::VertexBuffer> m_VertexBuffer;
		Blu::Shared<Blu::OpenGLShader> m_FlatColorShader, m_TextureShader;
		Blu::Shared<Blu::FrameBuffer> m_FrameBuffer;
		glm::vec2 m_ViewportSize = { 0.0f, 0.0f };
		Entity m_CameraEntity;
		bool m_ViewPortFocused = false;
		Blu::Shared<Scene> m_ActiveScene;
		std::vector<Entity> Entities;
		
		int m_ImGuizmoType = -1;
		bool enableTranslationSnap = false;
		bool enableRotationSnap = false;
		bool enableScaleSnap = false;

		float translationSnapValue = 0.5f;
		float rotationSnapValue = 10.0f;
		float scaleSnapValue = 0.5f;
		/*Scene Panels */

		Shared<SceneHierarchyPanel> m_SceneHierarchyPanel;
	};
}


