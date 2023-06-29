#include "Blupch.h"
#include "Scene.h"
#include "Blu/Rendering/Renderer2D.h"
#include "Entity.h"

namespace Blu
{
	Scene::Scene()
	{
		

	}
	Scene::~Scene()
	{

	}
	Entity Scene::CreateEntity(const std::string& name)
	{
		Entity entity = { m_Registry.create(), this };
		entity.AddComponent<TransformComponent>();
		auto& tag = entity.AddComponent<TagComponent>();
		tag.Tag = name.empty() ? "Entity" : name;
		
		return entity;
	}
	void Scene::OnUpdate(Timestep deltaTime)
	{
		Camera* mainCamera = nullptr;
		glm::mat4* cameraTransform = nullptr;
		auto group = m_Registry.view<CameraComponent, TransformComponent>();
		for (auto& entity : group)
		{
			auto [camera, transform] = group.get<CameraComponent, TransformComponent>(entity);
			if (camera.Primary)
			{
				mainCamera = &camera.Camera;
				cameraTransform = &transform.Transform;
				break;
			}

		}
		if (mainCamera)
		{
			Renderer2D::BeginScene(mainCamera->GetProjectionMatrix(), *cameraTransform);

			auto group = m_Registry.group<TransformComponent>(entt::get<SpriteRendererComponent>);
			for (auto& entity : group)
			{
				auto [transform, sprite] = group.get<TransformComponent, SpriteRendererComponent>(entity);
				Renderer2D::DrawQuad(transform, sprite.Color);
			}
			Renderer2D::EndScene();
		}
	}
}