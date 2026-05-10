#include "Blupch.h"
#include "OrthographicCamera.h"
#include<glm/gtc/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include "Blu/Rendering/RendererAPI.h"

namespace Blu
{
	static glm::mat4 MakeOrtho(float left, float right, float bottom, float top, float n, float f)
	{
		if (RendererAPI::GetAPI() == RendererAPI::API::Direct3D)
			return glm::orthoRH_ZO(left, right, bottom, top, n, f);
		return glm::ortho(left, right, bottom, top, n, f);
	}

	void OrthographicCamera::SetBounds(const OrthographicCameraBounds& bounds)
	{
		m_Bounds = bounds;
		m_ProjectionMatrix = MakeOrtho(bounds.Left, bounds.Right, bounds.Bottom, bounds.Top, -1.0f, 1.0f);
		RecalculateViewMatrix();
	}
	OrthographicCamera::OrthographicCamera(const OrthographicCameraBounds& bounds)
		:m_Bounds(bounds), m_ProjectionMatrix(MakeOrtho(bounds.Left, bounds.Right, bounds.Bottom, bounds.Top,-1.0f,1.0f)),
		m_ViewMatrix(1.0f)
	{
		m_ViewProjectionMatrix = m_ProjectionMatrix * m_ViewMatrix;
	}
	void OrthographicCamera::SetProjection(const OrthographicCameraBounds& bounds)
	{
		BLU_PROFILE_FUNCTION();

		m_ProjectionMatrix = MakeOrtho(bounds.Left, bounds.Right, bounds.Bottom, bounds.Top, -1.0f, 1.0f);
		m_ViewProjectionMatrix = m_ProjectionMatrix * m_ViewMatrix;

	}

	
	
	void OrthographicCamera::RecalculateViewMatrix()
	{
		BLU_PROFILE_FUNCTION();

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), m_Position) *
			glm::rotate(glm::mat4(1.0f), glm::radians(m_Rotation), glm::vec3(0, 0, 1)); // transform matrix

		m_ViewMatrix = glm::inverse(transform);
		m_ViewProjectionMatrix = m_ProjectionMatrix * m_ViewMatrix; // needs to be in this order for it to work
	}
}