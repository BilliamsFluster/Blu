#include "Blupch.h"
#include "EditorCamera.h"
#include "Blu/Core/Input.h"
#include "Blu/Core/KeyCodes.h"
#include "Blu/Core/MouseCodes.h"
#include "Blu/Events/MouseEvent.h"
#include "Blu/Rendering/RendererAPI.h"

#include <GLFW/glfw3.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/ext/matrix_clip_space.hpp>

namespace Blu
{
	static glm::mat4 MakePerspective(float fovRad, float aspect, float nearZ, float farZ)
	{
		if (RendererAPI::GetAPI() == RendererAPI::API::Direct3D)
			return glm::perspectiveRH_ZO(fovRad, aspect, nearZ, farZ);
		return glm::perspective(fovRad, aspect, nearZ, farZ);
	}

	EditorCamera::EditorCamera(float fov, float aspectRatio, float nearClip, float farClip)
		:m_FOV(fov), m_AspectRatio(aspectRatio), m_NearClip(nearClip), m_FarClip(farClip),
		 Camera(MakePerspective(glm::radians(fov), aspectRatio, nearClip, farClip))
	{
		// Place the camera m_Distance units behind the focal point along the initial
		// forward direction (0,0,-1) at zero pitch/yaw → position = (0, 0, +distance).
		m_Position = m_FocalPoint - GetForwardDirection() * m_Distance;
		UpdateView();
	}

	void EditorCamera::OnUpdate(Timestep deltaTime)
	{
		const float dt = deltaTime.GetSeconds();
		const glm::vec2& mouse{ Input::GetMouseX(), Input::GetMouseY() };
		glm::vec2 delta = (mouse - m_InitialMousePosition) * 0.003f;
		m_InitialMousePosition = mouse;

		// Middle mouse — pan: slide the camera position laterally / vertically.
		if (Input::IsMouseButtonPressed(BLU_MOUSE_BUTTON_MIDDLE))
			MousePan(delta);

		// Right mouse — look around and fly with velocity-based easing.
		if (Input::IsMouseButtonPressed(BLU_MOUSE_BUTTON_RIGHT))
		{
			MouseRotate(delta);

			// Build desired move direction from WASD/QE.
			glm::vec3 moveDir(0.0f);
			if (Input::IsKeyPressed(BLU_KEY_W)) moveDir += GetForwardDirection();
			if (Input::IsKeyPressed(BLU_KEY_S)) moveDir -= GetForwardDirection();
			if (Input::IsKeyPressed(BLU_KEY_A)) moveDir -= GetRightDirection();
			if (Input::IsKeyPressed(BLU_KEY_D)) moveDir += GetRightDirection();
			if (Input::IsKeyPressed(BLU_KEY_Q)) moveDir -= GetUpDirection();
			if (Input::IsKeyPressed(BLU_KEY_E)) moveDir += GetUpDirection();

			float len = glm::length(moveDir);
			glm::vec3 targetVel = (len > 0.001f)
			    ? (moveDir / len) * m_CameraSpeed
			    : glm::vec3(0.0f);

			// Exponential smoothing: ramp up quickly, coast to zero smoothly.
			float alpha = 1.0f - glm::exp(-12.0f * dt);
			m_Velocity = glm::mix(m_Velocity, targetVel, alpha);
		}
		else
		{
			// Brake: exponential decay so the camera glides to a stop.
			float brake = 1.0f - glm::exp(-9.0f * dt);
			m_Velocity = glm::mix(m_Velocity, glm::vec3(0.0f), brake);
		}

		m_Position += m_Velocity * dt;
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

		if (m_IsOrthographic)
		{
			// Scale orthographic size with distance so scroll-to-zoom still works.
			float halfH = m_Distance * 0.5f;
			float halfW = halfH * m_AspectRatio;
			if (RendererAPI::GetAPI() == RendererAPI::API::Direct3D)
				m_ProjectionMatrix = glm::orthoRH_ZO(-halfW, halfW, -halfH, halfH, m_NearClip, m_FarClip);
			else
				m_ProjectionMatrix = glm::ortho(-halfW, halfW, -halfH, halfH, m_NearClip, m_FarClip);
		}
		else
		{
			m_ProjectionMatrix = MakePerspective(glm::radians(m_FOV), m_AspectRatio, m_NearClip, m_FarClip);
		}
	}

	void EditorCamera::UpdateView()
	{
		// m_Position is now the authoritative camera position — maintained directly
		// by WASD, pan, and zoom. No focal-point orbit computation here.
		glm::quat orientation = GetOrientation();
		m_ViewMatrix = glm::translate(glm::mat4(1.0f), m_Position) * glm::toMat4(orientation);
		m_ViewMatrix = glm::inverse(m_ViewMatrix);
	}

	float EditorCamera::GetNormalizedDepthAtScreenCoordinate(float screenY, float viewportHeight) const
	{
		float normalizedDepth = screenY / viewportHeight;
		normalizedDepth = glm::clamp(normalizedDepth, 0.0f, 1.0f);
		normalizedDepth = (1.0f - normalizedDepth) * m_Distance;
		return normalizedDepth;
	}

	bool EditorCamera::OnMouseScroll(Events::MouseScrolledEvent& event)
	{
		float delta = event.GetYOffset();

		if (Input::IsMouseButtonPressed(BLU_MOUSE_BUTTON_RIGHT))
		{
			// Unreal-style: scroll while right-mouse held adjusts fly speed.
			// Each notch multiplies/divides speed by a fixed factor so it feels
			// logarithmic — same as Epic's viewport camera speed wheel.
			constexpr float kSpeedScale = 1.15f;
			if (delta > 0.0f)
				m_CameraSpeed *= kSpeedScale;
			else if (delta < 0.0f)
				m_CameraSpeed /= kSpeedScale;

			// Clamp to a sane range so the camera never becomes unusably
			// slow or impossibly fast.
			m_CameraSpeed = glm::clamp(m_CameraSpeed, 0.05f, 500.0f);
		}
		else
		{
			// Normal scroll: dolly the camera forward/backward.
			float scaledDelta = delta * 0.1f;
			float speed = ZoomSpeed();
			m_Position += GetForwardDirection() * scaledDelta * speed;
			m_Distance -= scaledDelta * speed;
			m_Distance = std::max(m_Distance, 0.5f);
		}

		UpdateView();
		return false;
	}

	void EditorCamera::MousePan(const glm::vec2& delta)
	{
		// Slide the camera position laterally and vertically.
		auto [xSpeed, ySpeed] = PanSpeed();
		m_Position += GetRightDirection() * delta.x * xSpeed * m_Distance;
		m_Position -= GetUpDirection()    * delta.y * ySpeed * m_Distance;
	}

	void EditorCamera::MouseRotate(const glm::vec2& delta)
	{
		// Rotate the camera in place — position does not change.
		float yawSign = GetUpDirection().y < 0 ? -1.0f : 1.0f;
		m_Yaw   += yawSign * delta.x * RotationSpeed();
		m_Pitch += delta.y * RotationSpeed();
	}

	glm::vec3 EditorCamera::CalculatePosition() const
	{
		// Kept for reference; no longer called by UpdateView().
		return m_FocalPoint - GetForwardDirection() * m_Distance;
	}

	std::pair<float, float> EditorCamera::PanSpeed() const
	{
		float x = std::min(m_ViewportWidth  / 1000.0f, 2.4f);
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
		float speed   = distance * distance;
		speed = std::min(speed, 100.0f);
		return speed;
	}
}
