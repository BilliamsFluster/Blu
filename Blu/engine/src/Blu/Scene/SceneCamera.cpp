#include "Blupch.h"
#include "SceneCamera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include "Blu/Rendering/RendererAPI.h"

namespace Blu
{
	SceneCamera::SceneCamera()
	{
		m_ProjectionType = ProjectionType::Perspective;

		RecalculateProjection();
	}
	void SceneCamera::SetOrthographic(float size, float nearClip, float farClip)
	{
		m_ProjectionType = ProjectionType::Orthographic;
		m_OrthographicSize = size;
		m_OrthographicNear = nearClip;
		m_OrthographicFar = farClip;
		RecalculateProjection();
	}

	void SceneCamera::SetPerspective(float fov, float nearClip, float farClip)
	{
		m_ProjectionType = ProjectionType::Perspective;
		m_PerspectiveFOV = fov;
		m_PerspectiveNear = nearClip;
		m_PerspectiveFar = farClip;
	}

	void SceneCamera::SetViewportSize(uint32_t width, uint32_t height)
	{
		m_AspectRatio = (float)width / (float)height;
		RecalculateProjection();
	}

	void SceneCamera::RecalculateProjection()
	{
		if (m_ProjectionType == ProjectionType::Perspective)
		{
			if(m_AspectRatio > 0.0f)
			{
				// DX11 expects depth in [0,1]; OpenGL expects [-1,1]
				if (RendererAPI::GetAPI() == RendererAPI::API::Direct3D)
					m_ProjectionMatrix = glm::perspectiveRH_ZO(m_PerspectiveFOV, m_AspectRatio, m_PerspectiveNear, m_PerspectiveFar);
				else
					m_ProjectionMatrix = glm::perspective(m_PerspectiveFOV, m_AspectRatio, m_PerspectiveNear, m_PerspectiveFar);
			}
			else
			{
				m_AspectRatio = 1.0f;
			}
		}
		if (m_ProjectionType == ProjectionType::Orthographic)
		{
			float orthoLeft   = -m_OrthographicSize * m_AspectRatio * 0.5f;
			float orthoRight  =  m_OrthographicSize * m_AspectRatio * 0.5f;
			float orthoBottom = -m_OrthographicSize * 0.5f;
			float orthoTop    =  m_OrthographicSize * 0.5f;
			if (RendererAPI::GetAPI() == RendererAPI::API::Direct3D)
				m_ProjectionMatrix = glm::orthoRH_ZO(orthoLeft, orthoRight, orthoBottom, orthoTop,
					m_OrthographicNear, m_OrthographicFar);
			else
				m_ProjectionMatrix = glm::ortho(orthoLeft, orthoRight, orthoBottom, orthoTop,
					m_OrthographicNear, m_OrthographicFar);
		}
	}

}
