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
	static int nextID = 0;
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
				boxShape.SetAsBox(bc.Size.x * transform.Scale.x, bc.Size.y * transform.Scale.y);

				b2FixtureDef fixtureDef;
				fixtureDef.shape = &boxShape;
				fixtureDef.density = bc.Density;
				fixtureDef.friction = bc.Friction;
				fixtureDef.restitution = bc.Restitution;
				fixtureDef.restitutionThreshold = bc.RestitutionThreshold;
				
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
		Renderer2D::GetRendererData()->TextureShader->Bind();
		auto group = m_Registry.group<TransformComponent>(entt::get<SpriteRendererComponent>);
		for (auto& entity : group)
		{
			auto [transform, sprite] = group.get<TransformComponent, SpriteRendererComponent>(entity);
			Renderer2D::DrawSprite(transform.GetTransform(), sprite, (int)entity);
		}

		auto particleView = m_Registry.view<ParticleSystemComponent>();
		for (auto& entity : particleView)
		{
			auto& particleSystem = particleView.get<ParticleSystemComponent>(entity);
			particleSystem.Update(deltaTime);


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
			auto particleView = m_Registry.view<ParticleSystemComponent>();
			for (auto& entity : particleView)
			{
				auto& particleSystem = particleView.get<ParticleSystemComponent>(entity);
				particleSystem.Update(deltaTime);


			}
			auto group = m_Registry.group<TransformComponent>(entt::get<SpriteRendererComponent>);
			for (auto& entity : group)
			{
				auto [transform, sprite] = group.get<TransformComponent, SpriteRendererComponent>(entity);
				Renderer2D::DrawSprite(transform.GetTransform(), sprite, (int)entity);
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