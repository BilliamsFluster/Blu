#include "Blupch.h"
#include "OpenGLMaterial.h"
#include "OpenGLShader.h"
#include <glad/glad.h> 
#include <glm/glm.hpp>

namespace Blu
{
	OpenGLMaterial::OpenGLMaterial(Shader* shader)
		: m_Shader(shader)
	{
		// Initialize UBO
		glGenBuffers(1, &m_RendererID);
		glBindBuffer(GL_UNIFORM_BUFFER, m_RendererID);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(MaterialProperties), nullptr, GL_DYNAMIC_DRAW); // Use GL_DYNAMIC_DRAW for frequently updated data
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}
	OpenGLMaterial::OpenGLMaterial()
	{
		glDeleteBuffers(1, &m_RendererID);
	}
	
	
	void OpenGLMaterial::SetShaderData(Blu::MaterialProperties& properties)
	{
		// Update UBO with new material properties
		glBindBuffer(GL_UNIFORM_BUFFER, m_RendererID);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(MaterialProperties), &properties);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}
	void OpenGLMaterial::BindMaterialToShader(Material* material, Shader* shader)
	{
		GLuint blockIndex = glGetUniformBlockIndex(shader->GetProgramID(), "MaterialBlock");
		glUniformBlockBinding(shader->GetProgramID(), blockIndex, 0); // 0 is the binding point
		glBindBufferBase(GL_UNIFORM_BUFFER, 0, material->GetProgramID()); // Assuming you add a GetUBO method to access the UBO ID
	}
}