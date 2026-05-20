#pragma once
#include "Camera.h"
#include "Blu/Core/Timestep.h"
#include "Blu/Events/Event.h"

#include <glm/glm.hpp>

namespace Blu
{
	class EditorCamera : public Camera
	{
	public:
		EditorCamera() = default;
		EditorCamera(float fov, float aspectRatio, float nearClip, float farClip);

		void OnUpdate(Timestep deltaTime);
		void OnEvent(Events::Event& event);

		inline float GetDistance() const { return m_Distance; }
		inline void SetDistance(float distance) { m_Distance = distance; }

		// Fly the camera to look at a world-space point from the current distance.
		// Position is repositioned so the point is m_Distance units in front of us.
		inline void SetFocalPoint(const glm::vec3& point)
		{
			m_FocalPoint = point;
			m_Position   = point - GetForwardDirection() * m_Distance;
			UpdateView();
		}

		inline void SetViewportSize(float width, float height) { m_ViewportWidth = width; m_ViewportHeight = height; UpdateProjectionMatrix(); }

		const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
		glm::mat4 GetViewProjectionMatrix() const { return m_ProjectionMatrix * m_ViewMatrix; }

		glm::vec3 GetUpDirection() const;
		glm::vec3 GetRightDirection() const;
		glm::vec3 GetForwardDirection() const;
		const glm::vec3& GetPosition() const {return m_Position; }
		glm::quat GetOrientation() const;
		float& GetCameraSpeed()  { return m_CameraSpeed; }
		void SetCameraSpeed(float speed) { m_CameraSpeed = speed; }
		float GetPitch() const { return m_Pitch; }
		float GetYaw() const { return m_Yaw; }
		float GetFOV() const { return m_FOV; }
		float GetNearClip() const { return m_NearClip; }
		float GetFarClip()  const { return m_FarClip; }

		// Orthographic / perspective toggle
		bool IsOrthographic() const { return m_IsOrthographic; }
		void SetOrthographic(bool ortho) { m_IsOrthographic = ortho; UpdateProjectionMatrix(); }

		// Set camera orientation directly (for cardinal ortho views).
		void SetPitchYaw(float pitch, float yaw) { m_Pitch = pitch; m_Yaw = yaw; UpdateView(); }
		float GetNormalizedDepthAtScreenCoordinate(float screenY, float viewportHeight) const;
	private:
		void UpdateProjectionMatrix();
		void UpdateView();

		bool OnMouseScroll(class Events::MouseScrolledEvent& event);

		void MousePan(const glm::vec2& delta);
		void MouseRotate(const glm::vec2& delta);
		void MouseZoom(float delta);

		glm::vec3 CalculatePosition() const;

		std::pair<float, float> PanSpeed() const;

		float RotationSpeed() const;
		float ZoomSpeed() const;

	private:
		float m_FOV = 45.0f, m_AspectRatio = 1.778f, m_NearClip = 0.1f, m_FarClip = 1000.0f;
		bool  m_IsOrthographic = false;
		float m_CameraSpeed = 2.0f;
		glm::mat4 m_ViewMatrix;
		glm::vec3 m_Position = { 0.0f, 0.0f, 0.0f };
		glm::vec3 m_FocalPoint = { 0.0f, 0.0f, 0.0f };
		glm::vec3 m_Velocity  = { 0.0f, 0.0f, 0.0f }; // for smooth start/stop easing

		glm::vec2 m_InitialMousePosition;

		float m_Distance = 10.0f;
		float m_Pitch = 0.0f, m_Yaw = 0.0f;

		float m_ViewportWidth = 1280.0f, m_ViewportHeight = 720.0f;

	};

}

