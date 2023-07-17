#include "Blupch.h"
#include "Scene.h"
#include "Blu/Rendering/Renderer2D.h"
#include "Blu/Scene/ScriptableEntity.h"
#include "Entity.h"
#include "Blu/Rendering/EditorCamera.h"
#include "Blu/Rendering/Shader.h"
#include "box2d/b2_world.h"
#include "box2d/b2_body.h"
#include "box2d/b2_fixture.h"
#include "box2d/b2_polygon_shape.h"
#include "box2d/b2_circle_shape.h"

namespace Blu
{
	static b2BodyType BluRigidbody2DTypeToBox2D(Rigidbody2DComponent::BodyType bodyType)
	{
		switch (bodyType)
		{
			case Rigidbody2DComponent::BodyType::Static:
			{
				return b2BodyType::b2_staticBody;
			}
			case Rigidbody2DComponent::BodyType::Dynamic:
			{
				return b2BodyType::b2_dynamicBody;
			}
			case Rigidbody2DComponent::BodyType::Kinematic:
			{
				return b2BodyType::b2_kinematicBody;
			}
		}

		BLU_CORE_ASSERT(false, "Unknown body type")
		return b2BodyType::b2_staticBody;
	}

	Scene::Scene()
	{
		

	}
	Scene::~Scene()
	{

	}
	template<typename Component>
	static void CopyComponent(entt::registry& dst, entt::registry& src, std::unordered_map<UUID, entt::entity> map)
	{
		auto view = src.view<Component>();

		for (auto e : view)
		{
			UUID uuid = src.get<IDComponent>(e).ID;
			entt::entity enttID = map.at(uuid);
			auto& component = src.get<Component>(e);

			dst.emplace_or_replace<Component>(enttID, component);

		}
	}

	
	Shared<Scene> Scene::Copy(Shared<Scene> scene)
	{
		Shared<Scene> newScene = std::make_shared<Scene>();

		newScene->m_ViewportHeight = scene->m_ViewportHeight;
		newScene->m_ViewportWidth = scene->m_ViewportWidth;

		std::unordered_map<UUID, entt::entity> enttMap;

		auto& srcSceneRegistry = scene->m_Registry;
		auto& dstSceneRegistry = newScene->m_Registry;
		auto idView = srcSceneRegistry.view<IDComponent>();

		for (auto e : idView)
		{ 
			UUID uuid = srcSceneRegistry.get<IDComponent>(e).ID;
			const auto& name = srcSceneRegistry.get<TagComponent>(e).Tag;
			Entity entity = newScene->CreateEntityWithUUID(uuid, name);
			enttMap[uuid] = (entt::entity)entity;

		}
		// need to make sure when you create a component you add this function to it 
		CopyComponent<TransformComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<ParticleSystemComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		
		CopyComponent<SpriteRendererComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<CircleRendererComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		
		CopyComponent<Rigidbody2DComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		
		CopyComponent<BoxCollider2DComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<CircleCollider2DComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		
		CopyComponent<CameraComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<NativeScriptComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);

		return newScene;
	}
	Entity Scene::CreateEntity(const std::string& name)
	{
		return CreateEntityWithUUID(UUID(), name);
	}
	Entity Scene::CreateEntityWithUUID(UUID uuid, const std::string& name)
	{
		Entity entity = { m_Registry.create(), this };
		entity.AddComponent<IDComponent>();
		entity.AddComponent<TransformComponent>();
		auto& tag = entity.AddComponent<TagComponent>();
		tag.Tag = name.empty() ? "Entity" : name;

		return entity;
	}
	Entity Scene::GetPrimaryCameraEntity()
	{
		auto view = m_Registry.view<CameraComponent>();
		for (auto entity : view)
		{
			const auto& camera = view.get<CameraComponent>(entity);
			if (camera.Primary)
			{
				return Entity{ entity, this };
			}
			return {};
		}
	}
	Entity Scene::DuplicateEntity(Entity& targetEntity)
	{
		if (true)
		{
			Entity duplicatedEntity = CreateEntity("Entity");
			auto& tc = targetEntity.GetComponent<TransformComponent>();

			auto& detc = duplicatedEntity.GetComponent<TransformComponent>();

			detc.Translation = tc.Translation;
			detc.Rotation = tc.Rotation;
			detc.Scale = tc.Scale;

			if (targetEntity.HasComponent<SpriteRendererComponent>())
			{
				m_Registry.emplace<SpriteRendererComponent>((entt::entity)duplicatedEntity, targetEntity.GetComponent<SpriteRendererComponent>());
			}
			if (targetEntity.HasComponent<ParticleSystemComponent>())
			{
				m_Registry.emplace<ParticleSystemComponent>((entt::entity)duplicatedEntity, targetEntity.GetComponent<ParticleSystemComponent>());
			}
			if (targetEntity.HasComponent<BoxCollider2DComponent>())
			{
				m_Registry.emplace<BoxCollider2DComponent>((entt::entity)duplicatedEntity, targetEntity.GetComponent<BoxCollider2DComponent>());
			}
			if (targetEntity.HasComponent<Rigidbody2DComponent>())
			{
				m_Registry.emplace<Rigidbody2DComponent>((entt::entity)duplicatedEntity, targetEntity.GetComponent<Rigidbody2DComponent>());
			}
			if (targetEntity.HasComponent<CircleRendererComponent>())
			{
				m_Registry.emplace<CircleRendererComponent>((entt::entity)duplicatedEntity, targetEntity.GetComponent<CircleRendererComponent>());
			}

			return duplicatedEntity;
		}
		return Entity{};
	}
	void Scene::OnRuntimeStart()
	{
		m_PhysicsWorld = new b2World({ 0.0f, -0.0981f }); // gravity

		auto view = m_Registry.view<Rigidbody2DComponent>();

		for (auto e : view)
		{
			Entity entity = { e, this };
			auto& transform = entity.GetComponent<TransformComponent>();
			auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();

			b2BodyDef bodyDef;
			bodyDef.type = BluRigidbody2DTypeToBox2D(rb2d.Type);
			bodyDef.position.Set(transform.Translation.x, transform.Translation.y);
			bodyDef.angle = transform.Rotation.z;

			b2Body* body = m_PhysicsWorld->CreateBody(&bodyDef);
			body->SetFixedRotation(rb2d.FixedRotation);
			rb2d.RuntimeBody = body;

			if (entity.HasComponent<BoxCollider2DComponent>())
			{
				auto& bc = entity.GetComponent<BoxCollider2DComponent>();

				b2PolygonShape boxShape;
				float pixelToMetersScale = 0.5f;  
				boxShape.SetAsBox(bc.Size.x * transform.Scale.x * pixelToMetersScale, bc.Size.y * transform.Scale.y * pixelToMetersScale);


				b2FixtureDef fixtureDef;
				fixtureDef.shape = &boxShape;
				fixtureDef.density = bc.Density;
				fixtureDef.friction = bc.Friction;
				fixtureDef.restitution = bc.Restitution;
				fixtureDef.restitutionThreshold = bc.RestitutionThreshold;
				
				body->CreateFixture(&fixtureDef);

			}
			else if (entity.HasComponent<CircleCollider2DComponent>())
			{
				auto& cc = entity.GetComponent<CircleCollider2DComponent>();

				b2CircleShape circleShape;
				float pixelToMetersScale = 0.5f;
				circleShape.m_radius = cc.Radius * transform.Scale.x * pixelToMetersScale; // Assuming uniform scaling
				
				b2FixtureDef fixtureDef;
				fixtureDef.shape = &circleShape;
				fixtureDef.density = cc.Density;
				fixtureDef.friction = cc.Friction;
				fixtureDef.restitution = cc.Restitution;
				fixtureDef.restitutionThreshold = cc.RestitutionThreshold;

				body->CreateFixture(&fixtureDef);
			}

		}
	}
	void Scene::OnRuntimeStop()
	{
		delete m_PhysicsWorld;
		m_PhysicsWorld = nullptr;
	}
	void Scene::DestroyEntity(Entity entity)
	{
		m_Registry.destroy(entity);
	}
	void Scene::OnUpdateEditor(Timestep deltaTime, EditorCamera& camera)
	{
		Renderer2D::BeginScene(camera);
		Renderer2D::GetRendererData()->QuadShader->Bind();
		{
			auto group = m_Registry.group<TransformComponent>(entt::get<SpriteRendererComponent>);
			for (auto& entity : group)
			{
				auto [transform, sprite] = group.get<TransformComponent, SpriteRendererComponent>(entity);
				Renderer2D::DrawSprite(transform.GetTransform(), sprite, (int)entity);
			}
		}
		{
			auto view = m_Registry.view<TransformComponent, CircleRendererComponent>();
			for (auto& entity : view)
			{
				auto [transform, circle] = view.get<TransformComponent, CircleRendererComponent>(entity);
				Renderer2D::DrawCircle(transform.GetTransform(), circle.Color, circle.Thickness, circle.Fade, (int)entity);
			}
		}
		{
			auto particleView = m_Registry.view<ParticleSystemComponent>();
			for (auto& entity : particleView)
			{
				auto& particleSystem = particleView.get<ParticleSystemComponent>(entity);
				particleSystem.Update(deltaTime);


			}
		}
		Renderer2D::EndScene();

	}
	void Scene::OnUpdateRuntime(Timestep deltaTime)
	{
		{
			m_Registry.view<NativeScriptComponent>().each([=](auto entity, auto& nsc)
				{
					
					if (!nsc.Instance)
					{
						nsc.Instance = nsc.InstantiateScript();
						nsc.Instance->m_Entity = Entity{ entity, this };
						nsc.Instance->OnCreate();
					}
					if (nsc.Instance)
					{
						nsc.Instance->OnUpdate(deltaTime);	

					}
				});
		}

		{
			const int32_t velocityIterations = 6;
			const int32_t positionIterations = 2;

			m_PhysicsWorld->Step(deltaTime, velocityIterations, positionIterations);

			//retrieve transform form box 2d, the goal is to update the transform to the rigidbody component's transform
			auto view = m_Registry.view<Rigidbody2DComponent>();
			for (auto e : view)
			{
				Entity entity = { e, this };
				auto& transform = entity.GetComponent<TransformComponent>();
				auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();

				b2Body* body = (b2Body*)rb2d.RuntimeBody;
				const auto& position = body->GetPosition();
				transform.Translation.x = position.x;
				transform.Translation.y = position.y;

				transform.Rotation.z = body->GetAngle();
			}
		}
		Camera* mainCamera = nullptr;
		glm::mat4 cameraTransform;
		auto view = m_Registry.view<CameraComponent, TransformComponent>();
		for (auto& entity : view)
		{
			auto [camera, transform] = view.get<CameraComponent, TransformComponent>(entity);
			if (camera.Primary)
			{
				mainCamera = &camera.Camera;
				cameraTransform = transform.GetTransform();
				break;
			}

		}
		if (mainCamera)
		{
			

			Renderer2D::BeginScene(mainCamera->GetProjectionMatrix(), cameraTransform);
			// Update particle systems
			{
				auto particleView = m_Registry.view<ParticleSystemComponent>();
				for (auto& entity : particleView)
				{
					auto& particleSystem = particleView.get<ParticleSystemComponent>(entity);
					particleSystem.Update(deltaTime);


				}
			}
			{
				auto group = m_Registry.group<TransformComponent>(entt::get<SpriteRendererComponent>);
				for (auto& entity : group)
				{
					auto [transform, sprite] = group.get<TransformComponent, SpriteRendererComponent>(entity);
					Renderer2D::DrawSprite(transform.GetTransform(), sprite, (int)entity);
				}
			}
			{
				auto view = m_Registry.view<TransformComponent, CircleRendererComponent>();
				for (auto& entity : view)
				{
					auto [transform, circle] = view.get<TransformComponent, CircleRendererComponent>(entity);
					Renderer2D::DrawCircle(transform.GetTransform(), circle.Color, circle.Thickness, circle.Fade, (int)entity);
				}
			}

			

			Renderer2D::EndScene();
		}

	}
	void Scene::OnViewportResize(float width, float height)
	{
		m_ViewportWidth = width;
		m_ViewportHeight = height;

		auto view = m_Registry.view<CameraComponent>();
		for (auto entity : view)
		{
			auto& cameraComponent = view.get<CameraComponent>(entity);
			if (!cameraComponent.FixedAspectRatio)
			{
				cameraComponent.Camera.SetViewportSize(width, height);
			}
		}

	}
}