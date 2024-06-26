#pragma once
#include <glm/glm.hpp>
namespace Blu
{
	class Camera
	{
	public:
		Camera() = default;
		Camera(const glm::mat4& projection)
			:m_ProjectionMatrix(projection) {}
		
		virtual ~Camera() = default;

		const glm::mat4& GetProjectionMatrix() const { return m_ProjectionMatrix; }
	protected:
		glm::mat4 m_ProjectionMatrix = glm::mat4(1.0f);
	};

}

 