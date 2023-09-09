#include "Blupch.h"
#include "EditorCamera.h"
#include "Blu/Core/Input.h"
#include "Blu/Core/KeyCodes.h"
#include "Blu/Core/MouseCodes.h"
#include "Blu/Events/MouseEvent.h"

#include <GLFW/glfw3.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace Blu
{
	EditorCamera::EditorCamera(float fov, float aspectRatio, float nearClip, float farClip)
		:m_FOV(fov), m_AspectRatio(aspectRatio), m_NearClip(nearClip), m_FarClip(farClip), Camera(glm::perspective(glm::radians(fov), aspectRatio, nearClip, farClip))
	{
		UpdateView();
	}
	void EditorCamera::OnUpdate(Timestep deltaTime)
	{
		
		
		const glm::vec2& mouse{ Input::GetMouseX(), Input::GetMouseY() };
		glm::vec2 delta = (mouse - m_InitialMousePosition) * 0.003f;
		m_InitialMousePosition = mouse;
		
		if (Input::IsMouseButtonPressed(BLU_MOUSE_BUTTON_MIDDLE))
		{
			MousePan(delta);
		}
		
		
		if (Input::IsMouseButtonPressed(BLU_MOUSE_BUTTON_RIGHT))
		{
			MouseRotate(delta);
		}
		
		if (Input::IsMouseButtonPressed(BLU_MOUSE_BUTTON_RIGHT))
		{
			if (Blu::Input::IsKeyPressed(BLU_KEY_W))
			{
				m_FocalPoint += GetForwardDirection() * m_CameraSpeed * deltaTime.GetSeconds();
			}

			if (Blu::Input::IsKeyPressed(BLU_KEY_S))
			{
				m_FocalPoint -= GetForwardDirection() * m_CameraSpeed * deltaTime.GetSeconds();
			}

			if (Blu::Input::IsKeyPressed(BLU_KEY_A))
			{
				m_FocalPoint -= GetRightDirection() * m_CameraSpeed * deltaTime.GetSeconds();
			}

			if (Blu::Input::IsKeyPressed(BLU_KEY_D))
			{
				m_FocalPoint += GetRightDirection() * m_CameraSpeed * deltaTime.GetSeconds();
			}
			if (Blu::Input::IsKeyPressed(BLU_KEY_E))
			{
				m_FocalPoint += GetUpDirection() * m_CameraSpeed * deltaTime.GetSeconds();
			}
			if (Blu::Input::IsKeyPressed(BLU_KEY_Q))
			{
				m_FocalPoint -= GetUpDirection() * m_CameraSpeed * deltaTime.GetSeconds();
			}
		}
		


		UpdateView();
	}
	void EditorCamera::OnEvent(Events::Event& event)
	{
		switch (event.GetType())
		{
			case Events::Event::Type::MouseScrolled:
			{
				Events::MouseScrolledEvent& e = static_cast<Events::MouseScrolledEvent&>(event);
				OnMouseScroll(e);
				event.Handled = true;
				break;
			}
		}
	}
	glm::vec3 EditorCamera::GetUpDirection() const
	{
		return glm::rotate(GetOrientation(), glm::vec3(0.0f, 1.0f, 0.0f));
	}
	glm::vec3 EditorCamera::GetRightDirection() const
	{
		return glm::rotate(GetOrientation(), glm::vec3(1.0f, 0.0f, 0.0f));
	}
	glm::vec3 EditorCamera::GetForwardDirection() const
	{
		return glm::rotate(GetOrientation(), glm::vec3(0.0f, 0.0f, -1.0f));
	}
	glm::quat EditorCamera::GetOrientation() const
	{
		return glm::quat(glm::vec3(-m_Pitch, -m_Yaw, 0.0f));
	}
	void EditorCamera::UpdateProjectionMatrix()
	{
		m_AspectRatio = m_ViewportWidth / m_ViewportHeight;
		m_ProjectionMatrix = glm::perspective(glm::radians(m_FOV), m_AspectRatio, m_NearClip, m_FarClip);
	}
	void EditorCamera::UpdateView()
	{
		m_Position = CalculatePosition();

		glm::quat orientation = GetOrientation();
		m_ViewMatrix = glm::translate(glm::mat4(1.0), m_Position) * glm::toMat4(orientation);
		m_ViewMatrix = glm::inverse(m_ViewMatrix);

	}
	float EditorCamera::GetNormalizedDepthAtScreenCoordinate(float screenY, float viewportHeight) const
	{
		float normalizedDepth = screenY / viewportHeight; // Invert Y-axis
		normalizedDepth = glm::clamp(normalizedDepth, 0.0f, 1.0f); // Ensure it's within [0, 1]

		// Adjust the normalized depth based on the camera's zoom level
		normalizedDepth = (1.0f - normalizedDepth) * m_Distance;

		return normalizedDepth;
	}
	bool EditorCamera::OnMouseScroll(Events::MouseScrolledEvent& event)
	{
		float delta = event.GetYOffset() * 0.1f;
		MouseZoom(delta);
		UpdateView();
		return false;
	}
	void EditorCamera::MousePan(const glm::vec2& delta)
	{
		auto [xSpeed, ySpeed] = PanSpeed();
		m_FocalPoint -= GetRightDirection() * delta.x * xSpeed * m_Distance;
		m_FocalPoint += GetUpDirection() * delta.y * ySpeed * m_Distance;
	}
	void EditorCamera::MouseRotate(const glm::vec2& delta)
	{
		float yawSign = GetUpDirection().y < 0 ? -1.0f : 1.0f;
		m_Yaw += yawSign * delta.x * RotationSpeed();
		m_Pitch += delta.y * RotationSpeed();

	}
	void EditorCamera::MouseZoom(float delta)
	{
		m_Distance -= delta * ZoomSpeed();
		if (m_Distance < 1.0f)
		{
			m_FocalPoint += GetForwardDirection();
			m_Distance = 1.0f;
		}
	}
	glm::vec3 EditorCamera::CalculatePosition() const
	{
		return m_FocalPoint - GetForwardDirection() * m_Distance;
	}
	std::pair<float, float> EditorCamera::PanSpeed() const
	{
		float x = std::min(m_ViewportWidth / 1000.0f, 2.4f);
		float xFactor = 0.0366f * (x * x) - 0.1778f * x + 0.3021f;

		float y = std::min(m_ViewportHeight / 1000.0f, 2.4f);
		float yFactor = 0.0366f * (y * y) - 0.1778f * y + 0.3021f;

		return { xFactor, yFactor };

	}
	float EditorCamera::RotationSpeed() const
	{
		return 0.8f;
	}
	float EditorCamera::ZoomSpeed() const
	{
		float distance = m_Distance * 0.2f;
		distance = std::max(distance, 0.0f);
		float speed = distance * distance;
		speed = std::min(speed, 100.0f);
		return speed;
	}
}
