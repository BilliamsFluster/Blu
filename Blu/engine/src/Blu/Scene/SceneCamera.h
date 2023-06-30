#pragma once
#include "Blu/Rendering/Camera.h"

namespace Blu
{
	class SceneCamera: public Camera
	{
	public:
		enum class ProjectionType { Perspective = 0, Orthographic = 1 };
	public:
		SceneCamera();
		virtual ~SceneCamera() = default;

		void SetOrthographic(float size, float nearClip, float farClip);
		void SetPerspective(float fov, float nearClip, float farClip);
		void SetViewportSize(uint32_t width, uint32_t height);
		float GetOrthographicSize() const { return m_OrthographicSize; }
		void SetOrthographicSize(float size) { m_OrthographicSize = size; RecalculateProjection(); }
		ProjectionType GetProjectionType() const { return m_ProjectionType; }
		void SetProjectionType(ProjectionType type) { m_ProjectionType = type; }
		
		/*Orthographic Getters and Setters*/
		float GetOrthographicNearClip() const { return m_OrthographicNear; }
		float GetOrthographicFarClip() const { return m_OrthographicFar; }

		void SetOrthographicNearClip(float clip) { m_OrthographicNear = clip; RecalculateProjection();}
		void SetOrthographicFarClip(float clip) { m_OrthographicFar = clip; RecalculateProjection();}

		/*Perspective Getters and Setters*/
		float GetPerspectiveFOV() const { return m_PerspectiveFOV; }
		float GetPerspectiveNear() const { return m_PerspectiveNear; }
		float GetPerspectiveFar() const { return m_PerspectiveFar; }

		void SetPerspectiveFOV(float fov) { m_PerspectiveFOV = fov; RecalculateProjection();}
		void SetPerspectiveNear(float pNear) { m_PerspectiveNear = pNear; RecalculateProjection();}
		void SetPerspectiveFar(float pFar) { m_PerspectiveFar = pFar; RecalculateProjection();}

	private:
		void RecalculateProjection();
	private:
		ProjectionType m_ProjectionType = ProjectionType::Orthographic;
		float m_OrthographicSize = 10.0f;
		float m_OrthographicNear = -1.0f;
		float m_OrthographicFar = 1.0f;

		float m_PerspectiveFOV = glm::radians(45.0f);
		float m_PerspectiveNear = 0.01f, m_PerspectiveFar = 1000.0f;
		float m_AspectRatio = 1.0f;


	};

}

