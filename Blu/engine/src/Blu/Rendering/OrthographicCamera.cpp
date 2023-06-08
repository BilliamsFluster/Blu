#include "Blupch.h"
#include "OrthographicCamera.h"
#include<glm/gtc/matrix_transform.hpp>

namespace Blu
{
	void OrthographicCamera::SetBounds(const OrthographicCameraBounds& bounds) {
		m_Bounds = bounds;
		m_ProjectionMatrix = glm::ortho(bounds.Left, bounds.Right, bounds.Bottom, bounds.Top, -1.0f, 1.0f);
		RecalculateViewMatrix();
	}
	OrthographicCamera::OrthographicCamera(const OrthographicCameraBounds& bounds)
		:m_Bounds(bounds), m_ProjectionMatrix(glm::ortho(bounds.Left, bounds.Right, bounds.Bottom, bounds.Top,-1.0f,1.0f)),
		m_ViewMatrix(1.0f)
	{
		m_ViewProjectionMatrix = m_ProjectionMatrix * m_ViewMatrix;
	}
	void OrthographicCamera::RecalculateViewMatrix()
	{
		glm::mat4 transform = glm::translate(glm::mat4(1.0f), m_Position) *
			glm::rotate(glm::mat4(1.0f), glm::radians(m_Rotation), glm::vec3(0, 0, 1)); // transform matrix

		m_ViewMatrix = glm::inverse(transform);
		m_ViewProjectionMatrix = m_ProjectionMatrix * m_ViewMatrix; // needs to be in this order for it to work
	}
}