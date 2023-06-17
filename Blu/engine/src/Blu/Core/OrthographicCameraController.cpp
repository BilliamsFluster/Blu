#include "Blupch.h"
#include "OrthographicCameraController.h"
#include "Blu/Core/Input.h"
#include "Blu/Core/KeyCodes.h"

namespace Blu
{
	OrthographicCameraController::OrthographicCameraController(float aspectRatio, bool rotation)
		:m_AspectRatio(aspectRatio), m_Camera({ -m_AspectRatio * m_ZoomLevel, m_AspectRatio * m_ZoomLevel, -m_ZoomLevel, m_ZoomLevel }), m_Rotation(rotation)
	{

	}
	void OrthographicCameraController::OnUpdate(Timestep deltaTime)
	{
		if (WindowInput::Input::IsKeyPressed(BLU_KEY_W))
		{
			m_CameraPosition.y -= (m_CameraTranslationSpeed * deltaTime);
		}

		if (Blu::WindowInput::Input::IsKeyPressed(BLU_KEY_S))
		{
			m_CameraPosition.y += (m_CameraTranslationSpeed * deltaTime);
		}
		if (Blu::WindowInput::Input::IsKeyPressed(BLU_KEY_A))
		{
			m_CameraPosition.x += (m_CameraTranslationSpeed * deltaTime);

		}

		if (Blu::WindowInput::Input::IsKeyPressed(BLU_KEY_D))
		{
			m_CameraPosition.x -= (m_CameraTranslationSpeed * deltaTime);

		}
		if (m_Rotation)
		{
			if (Blu::WindowInput::Input::IsKeyPressed(BLU_KEY_LEFT))
			{
				m_CameraRotation -= (m_CameraRotationSpeed * deltaTime);

			}

			if (Blu::WindowInput::Input::IsKeyPressed(BLU_KEY_RIGHT))
			{
				m_CameraRotation += (m_CameraRotationSpeed * deltaTime);

			}
			m_Camera.SetRotation(m_CameraRotation);
		}
		m_Camera.SetPosition(m_CameraPosition);
		m_CameraTranslationSpeed = m_ZoomLevel;
		
	}
	void OrthographicCameraController::OnEvent(Events::Event& event)
	{
		switch (event.GetType())
		{
			case Events::Event::Type::MouseScrolled :
			{
				OnMouseScrolled(static_cast<Events::MouseScrolledEvent&>(event));
				break;

			}
			case Events::Event::Type::WindowResize :
			{
				OnWindowResize(static_cast<Events::WindowResizeEvent&>(event));
				break;

			}
		}
	}
	bool OrthographicCameraController::OnMouseScrolled(Events::MouseScrolledEvent& event)
	{
		m_ZoomLevel -= event.GetYOffset() * 0.5f;
		m_ZoomLevel = std::max(m_ZoomLevel, 0.25f);
		m_Camera.SetProjection({ -m_AspectRatio * m_ZoomLevel, m_AspectRatio * m_ZoomLevel, -m_ZoomLevel, m_ZoomLevel });
		return false;
	}
	bool OrthographicCameraController::OnWindowResize(Events::WindowResizeEvent& event)
	{
		m_ZoomLevel -= event.GetWidth() / event.GetHeight();
		m_Camera.SetProjection({ -m_AspectRatio * m_ZoomLevel, m_AspectRatio * m_ZoomLevel, -m_ZoomLevel, m_ZoomLevel });
		return false;

	}
}
