#include "Blupch.h"
#include "Scene.h"
#include "Blu/Rendering/Renderer2D.h"

namespace Blu
{
	Scene::Scene()
	{
		

	}
	Scene::~Scene()
	{

	}
	entt::entity Scene::CreateEntity()
	{
		return m_Registry.create();
	}
	void Scene::OnUpdate(Timestep deltaTime)
	{
		auto group = m_Registry.group<TransformComponent>(entt::get<SpriteRendererComponent>);
		for (auto entity : group)
		{
			auto [transform, sprite] = group.get<TransformComponent, SpriteRendererComponent>(entity);
			Renderer2D::DrawQuad(transform, sprite.Color);
		}
	}
}