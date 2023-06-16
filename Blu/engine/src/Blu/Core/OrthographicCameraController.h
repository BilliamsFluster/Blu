#pragma once
#include "Blu/Rendering/OrthographicCamera.h"
#include "Blu/Core/Timestep.h"

#include "Blu/Events/WindowEvent.h"
#include "Blu/Events/MouseEvent.h"
#include "Blu/Events/EventHandler.h"
namespace Blu
{
	class OrthographicCameraController
	{
	public:
		OrthographicCameraController(float aspectRatio, bool rotation = false);

		void OnUpdate(Timestep deltaTime);
		void OnEvent(Events::EventHandler& handler, Events::Event& event);
		OrthographicCamera& GetCamera() { return m_Camera; }
		const OrthographicCamera& GetCamera() const  { return m_Camera; }
	private:
		bool OnMouseScrolled(Events::MouseScrolledEvent& event);
		bool OnWindowResize(Events::WindowResizeEvent& event);
	private:
		float m_AspectRatio = 1.0f;
		float m_ZoomLevel = 1.0f;
		bool m_Rotation = 0.0f;
		float m_CameraRotation = 0.0f;
		float m_CameraTranslationSpeed = 1.0f;
		float m_CameraRotationSpeed = 1.0f;
		glm::vec3 m_CameraPosition = { 0.0f, 0.0f, 0.0f };
		OrthographicCamera m_Camera;
	};

}

