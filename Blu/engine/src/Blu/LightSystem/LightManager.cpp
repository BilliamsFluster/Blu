#include "Blupch.h"
#include "LightManager.h"
#include "Blu/Scene/Component.h"
#include "Blu/Rendering/Renderer2D.h"
#include "Blu/Rendering/Shader.h"
#include "Blu/Scene/Entity.h"

Blu::LightManager::LightManager()
{
}

Blu::LightManager::~LightManager()
{
}

void Blu::LightManager::AddPointLight(Entity& light)
{
    // Add the provided point light to the vector
    m_PointLights.push_back(light);
 
}

void Blu::LightManager::AddDirectionalLight(Entity& light)
{
    
}

void Blu::LightManager::UpdateLights()
{
    
}

void Blu::LightManager::RenderLights()
{
    //// Use the quad shader for rendering lights
    //Shared<Shader> quadShader = Renderer2D::s_RendererData->QuadShader;
    //quadShader->Bind();

    //// Loop through and update the uniform array of PointLights
    //for (size_t i = 0; i < pointLights.size() && i < 32; ++i)
    //{
    //    // Set shader uniforms for the point light
    //    // Assuming SetUniformPointLight sets individual uniform properties
    //    // Update the u_PointLights array in the shader with data from pointLights
    //    quadShader->SetUniformPointLight("u_PointLights[" + std::to_string(i) + "]", pointLights[i]);
    //}

    //// Unbind the quad shader
    //quadShader->UnBind();
}
