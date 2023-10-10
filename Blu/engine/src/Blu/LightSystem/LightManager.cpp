#include "Blupch.h"
#include "LightManager.h"
#include "Blu/Scene/Component.h"
#include "Blu/Rendering/Renderer2D.h"
#include "Blu/Rendering/Shader.h"
#include "Blu/Scene/Scene.h"
#include "Blu/Scene/Entity.h"

Blu::LightManager::LightManager()
{
}

Blu::LightManager::~LightManager()
{
}

void Blu::LightManager::AddPointLight(Shared<class Scene> scene,Entity& light)
{
    // Add the provided point light to the vector
    m_PointLights.push_back(light);

    //// Ensure that the vector does not exceed a maximum limit, e.g., 32 lights
    //if (pointLights.size() > 32)
    //{
    //    // Handle the case where you have more lights than supported
    //    // You can remove the oldest or least important light based on your game's logic.
    //    // For example, you can remove the first light:
    //   
    //    pointLights.erase(pointLights.begin());
    //}
    //// Calculate the index of the newly added light
    //int lightIndex = pointLights.size() - 1;
    // Use the quad shader for rendering lights
    // Use the quad shader for rendering lights
    Shared<Shader> quadShader = Renderer2D::s_RendererData->QuadShader;
    quadShader->Bind();
    // Update the shader uniform using the calculated index
    Renderer2D::s_RendererData->QuadShader->SetUniformFloat3("a_LightPosition", light.GetComponent<TransformComponent>().Translation);
    
    // Unbind the quad shader
    quadShader->UnBind();

    
}

void Blu::LightManager::AddDirectionalLight(Shared<class Scene> scene, Entity& light)
{
    
}

void Blu::LightManager::UpdateLights()
{
    if (m_PointLights.size() <= 0) return;

    Shared<Shader> quadShader = Renderer2D::s_RendererData->QuadShader;
    quadShader->Bind();
    

    // Unbind the quad shader
    quadShader->UnBind();
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
