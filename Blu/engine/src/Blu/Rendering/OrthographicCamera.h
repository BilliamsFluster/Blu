#pragma once
#include <glm/glm.hpp>

namespace Blu
{
	struct OrthographicCameraBounds
	{
		float Left, Right, Bottom, Top;

		OrthographicCameraBounds(float left, float right, float bottom, float top)
			: Left(left), Right(right), Bottom(bottom), Top(top) {}
	};

	class OrthographicCamera
	{
	public:
		OrthographicCamera(const OrthographicCameraBounds& bounds);
			
		void SetProjection(const OrthographicCameraBounds& bounds);

		const glm::vec3& GetPosition() const { return m_Position; }
		float GetRotation() const { return m_Rotation; }

		void SetPosition(const glm::vec3 position) {
			m_Position = position; RecalculateViewMatrix();
		}
		void SetRotation(float rotation) {
			m_Rotation = rotation; RecalculateViewMatrix();
		}
		OrthographicCameraBounds& GetCameraBounds() { return m_Bounds; }
		void SetBounds(const OrthographicCameraBounds& bounds);

		const glm::mat4& GetProjectionMatrix() const { return m_ProjectionMatrix; }
		const glm::mat4& GetViewProjectionMatrix() const { return m_ViewProjectionMatrix; }
		const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
	private:
		void RecalculateViewMatrix();

	private:
		OrthographicCameraBounds m_Bounds;

		glm::mat4 m_ProjectionMatrix;
		glm::mat4 m_ViewMatrix;
		glm::mat4 m_ViewProjectionMatrix;

		glm::vec3 m_Position = { 0.0f, 0.0f, 0.0f };
		float m_Rotation = 0.0f;
	};
}


