#include "Blupch.h"
#include "Scene.h"
#include "Blu/Physics/Physics3D.h"
#include "Blu/Rendering/Renderer2D.h"
#include "Blu/Rendering/Renderer3D.h"
#include "Blu/Rendering/GpuParticleSystem.h"
#include "Blu/Rendering/Material.h"
#include "Blu/Rendering/CascadedShadowMap.h"
#include "Blu/Rendering/PostProcess.h"
#include "Blu/UI/RuntimeUI.h"
#include "Blu/Rendering/Skybox.h"
#include "Blu/Rendering/Texture.h"
#include "Blu/Rendering/TimeOfDay.h"
#include "Blu/GameFramework/GameFramework.h"
#include "Entity.h"
#include "Blu/Rendering/EditorCamera.h"
#include "Blu/Rendering/Shader.h"
#include "Blu/Rendering/ModelLoader.h"
#include "Blu/Utils/AssetPath.h"
#include "box2d/b2_world.h"
#include "box2d/b2_body.h"
#include "box2d/b2_fixture.h"
#include "box2d/b2_polygon_shape.h"
#include "box2d/b2_circle_shape.h"
#include "Blu/LightSystem/LightManager.h"
#include "Blu/Audio/AudioEngine.h"
#include "Blu/Rendering/Animator.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/norm.hpp>
#include <cfloat>
#include <algorithm>
#include <limits>

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

	static uint32_t CountModelCollisionTriangles(const Shared<Model>& model)
	{
		if (!model)
			return 0;

		uint32_t triangleCount = 0;
		for (const auto& submesh : model->Meshes)
			triangleCount += (uint32_t)(submesh.Indices.size() / 3);
		return triangleCount;
	}

	static bool HasEnabled3DCollider(Entity entity)
	{
		if (entity.HasComponent<BoxCollider3DComponent>() ||
			entity.HasComponent<SphereCollider3DComponent>() ||
			entity.HasComponent<CapsuleCollider3DComponent>())
			return true;

		if (entity.HasComponent<MeshCollider3DComponent>())
			return entity.GetComponent<MeshCollider3DComponent>().Enabled;

		return false;
	}

	static glm::mat4 GetRenderTransform(const entt::registry& registry, entt::entity entity, const TransformComponent& transform)
	{
		glm::mat4 renderTransform = transform.GetTransform();
		if (registry.any_of<VisualOffsetComponent>(entity))
			renderTransform *= registry.get<VisualOffsetComponent>(entity).GetTransform();

		return renderTransform;
	}

	static void ReadCharacterCapsule(Entity entity, float& outRadius, float& outHalfHeight, glm::vec3& outOffset)
	{
		outRadius = 0.3f;
		outHalfHeight = 0.55f;
		outOffset = { 0.0f, 0.0f, 0.0f };

		if (!entity.HasComponent<CapsuleCollider3DComponent>())
			return;

		auto& cap = entity.GetComponent<CapsuleCollider3DComponent>();
		outRadius = std::max(0.01f, cap.Radius);
		outHalfHeight = std::max(0.01f, cap.HalfHeight);
		outOffset = cap.Offset;
	}

	static bool IsCharacterColliderValid(Entity entity)
	{
		if (!entity.HasComponent<CapsuleCollider3DComponent>())
			return false;

		auto& cap = entity.GetComponent<CapsuleCollider3DComponent>();
		return cap.Radius > 0.0f && cap.HalfHeight > 0.0f;
	}

	static JPH::CharacterVirtual* CreateRuntimeCharacter(
		Entity entity,
		const TransformComponent& tc,
		CharacterControllerComponent& ccc,
		Physics3DWorld* physicsWorld,
		std::string& outStatus)
	{
		if (!physicsWorld || !physicsWorld->IsValid())
		{
			outStatus = "No active Physics3D world";
			return nullptr;
		}

		if (!entity.HasComponent<CapsuleCollider3DComponent>())
		{
			outStatus = "Character Controller requires Capsule Collider 3D";
			return nullptr;
		}

		float radius = 0.0f;
		float halfHeight = 0.0f;
		glm::vec3 offset;
		ReadCharacterCapsule(entity, radius, halfHeight, offset);

		if (radius <= 0.0f || halfHeight <= 0.0f)
		{
			outStatus = "Character capsule radius/height must be greater than zero";
			return nullptr;
		}

		JPH::CapsuleShapeSettings capsuleSettings(halfHeight, radius);
		auto capsuleResult = capsuleSettings.Create();
		if (capsuleResult.HasError())
		{
			outStatus = capsuleResult.GetError().c_str();
			return nullptr;
		}

		JPH::RotatedTranslatedShapeSettings offsetSettings(
			JPH::Vec3(offset.x, offset.y + halfHeight + radius, offset.z),
			JPH::Quat::sIdentity(),
			capsuleResult.Get());
		auto shapeResult = offsetSettings.Create();
		if (shapeResult.HasError())
		{
			outStatus = shapeResult.GetError().c_str();
			return nullptr;
		}

		JPH::CharacterVirtualSettings settings;
		settings.mUp = JPH::Vec3::sAxisY();
		settings.mMaxSlopeAngle = JPH::DegreesToRadians(ccc.SlopeLimit);
		settings.mShape = shapeResult.Get();

		auto* character = new JPH::CharacterVirtual(
			&settings,
			JPH::RVec3(tc.Translation.x, tc.Translation.y, tc.Translation.z),
			JPH::Quat::sIdentity(),
			physicsWorld->GetPhysicsSystem());

		outStatus = "Runtime character active";
		return character;
	}

	static bool BuildMeshColliderSpec(
		Entity entity,
		TransformComponent& tc,
		MeshComponent& mesh,
		MeshCollider3DComponent& collider,
		const std::filesystem::path& sceneFilePath,
		Physics3DBodySpec& spec,
		std::string& outStatus)
	{
		if (!collider.Enabled)
		{
			outStatus = "Mesh collider is disabled";
			return false;
		}

		if (!mesh.ModelAsset && !mesh.FilePath.empty())
			mesh.ModelAsset = ModelLoader::Load(AssetPath::ResolvePath(mesh.FilePath, sceneFilePath).string());

		if (!mesh.ModelAsset)
		{
			outStatus = "Mesh collider has no loaded model";
			return false;
		}

		if (CountModelCollisionTriangles(mesh.ModelAsset) == 0 && !mesh.FilePath.empty())
			mesh.ModelAsset = ModelLoader::Load(AssetPath::ResolvePath(mesh.FilePath, sceneFilePath).string());

		if (CountModelCollisionTriangles(mesh.ModelAsset) == 0)
		{
			outStatus = "Mesh collider has no CPU triangle data";
			return false;
		}

		glm::mat4 scaleTransform = glm::scale(glm::mat4(1.0f), tc.Scale);
		spec.ShapeType = Physics3DShapeType::Mesh;
		spec.MeshDoubleSided = collider.DoubleSided;
		spec.Friction = collider.Friction;
		spec.Restitution = collider.Restitution;
		spec.MeshTriangleVertices.clear();

		for (const auto& submesh : mesh.ModelAsset->Meshes)
		{
			if (submesh.Vertices.empty() || submesh.Indices.empty())
				continue;

			glm::mat4 localTransform = scaleTransform * submesh.LocalTransform;
			for (size_t i = 0; i + 2 < submesh.Indices.size(); i += 3)
			{
				uint32_t i0 = submesh.Indices[i + 0];
				uint32_t i1 = submesh.Indices[i + 1];
				uint32_t i2 = submesh.Indices[i + 2];
				if (i0 >= submesh.Vertices.size() || i1 >= submesh.Vertices.size() || i2 >= submesh.Vertices.size())
					continue;

				glm::vec3 v0 = glm::vec3(localTransform * glm::vec4(submesh.Vertices[i0].Position, 1.0f));
				glm::vec3 v1 = glm::vec3(localTransform * glm::vec4(submesh.Vertices[i1].Position, 1.0f));
				glm::vec3 v2 = glm::vec3(localTransform * glm::vec4(submesh.Vertices[i2].Position, 1.0f));

				glm::vec3 edge0 = v1 - v0;
				glm::vec3 edge1 = v2 - v0;
				if (glm::dot(glm::cross(edge0, edge1), glm::cross(edge0, edge1)) < 0.00000001f)
					continue;

				spec.MeshTriangleVertices.push_back(v0);
				spec.MeshTriangleVertices.push_back(v1);
				spec.MeshTriangleVertices.push_back(v2);
			}
		}

		collider.RuntimeTriangleCount = (uint32_t)(spec.MeshTriangleVertices.size() / 3);
		if (collider.RuntimeTriangleCount == 0)
		{
			outStatus = "Mesh collider cooked zero valid triangles";
			return false;
		}

		outStatus = "Ready (" + std::to_string(collider.RuntimeTriangleCount) + " triangles)";
		return true;
	}

	Scene::Scene()
	{
		m_LightManager = std::make_shared<LightManager>();
		m_ActorSystem = std::make_unique<ActorSystem>(*this);
		OnRuntimeStop(); // make sure there are not any instances that may create compounding for gravity and etc
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
				cameraComponent.Camera.SetViewportSize((uint32_t)width, (uint32_t)height);
			}
		}

		if (!m_PostProcess && width > 0 && height > 0)
		{
			m_PostProcess = PostProcess::Create((uint32_t)width, (uint32_t)height);
			if (m_HasPostProcessSettings && m_PostProcess)
			{
				m_PostProcess->EnableBloom    = m_PostProcessSettings.EnableBloom;
				m_PostProcess->BloomThreshold = m_PostProcessSettings.BloomThreshold;
				m_PostProcess->BloomStrength  = m_PostProcessSettings.BloomStrength;
				m_PostProcess->EnableFXAA     = m_PostProcessSettings.EnableFXAA;
				m_PostProcess->Preview        = (PostProcess::PreviewMode)m_PostProcessSettings.Preview;
				m_PostProcess->EnableSSAO     = m_PostProcessSettings.EnableSSAO;
				m_PostProcess->SSAORadius     = m_PostProcessSettings.SSAORadius;
				m_PostProcess->SSAOBias       = m_PostProcessSettings.SSAOBias;
				m_PostProcess->SSAOPower      = m_PostProcessSettings.SSAOPower;
				m_PostProcess->SSAOSamples    = m_PostProcessSettings.SSAOSamples;
				m_PostProcess->SSAOStrength   = m_PostProcessSettings.SSAOStrength;
			}
		}
		else if (m_PostProcess)
			m_PostProcess->Resize((uint32_t)width, (uint32_t)height);

		if (!m_Skybox)
			m_Skybox = std::make_shared<Skybox>();
	}
	Scene::~Scene()
	{
		m_LightManager = nullptr;
		OnRuntimeStop();
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
		newScene->m_SceneFilePath = scene->m_SceneFilePath;
		newScene->m_GameModeClassID = scene->m_GameModeClassID;
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
		CopyComponent<ActorComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<PointLightComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<DirectionalLightComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<SpotLightComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<MeshComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<VisualOffsetComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<TerrainComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);

		CopyComponent<Rigidbody3DComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<BoxCollider3DComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<SphereCollider3DComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<CapsuleCollider3DComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<MeshCollider3DComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<CharacterControllerComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<InteractableComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<PickupComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<PlayerStatsComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<UIRootComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<SpringArmComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<AnimatorComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<MeshLODComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<AudioSourceComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<FoliageComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);

		newScene->m_UseShadows     = scene->m_UseShadows;
		newScene->m_UsePostProcess = scene->m_UsePostProcess;
		newScene->m_UseSkybox      = scene->m_UseSkybox;
		newScene->m_Fog            = scene->m_Fog;
		newScene->m_HasPostProcessSettings = scene->m_HasPostProcessSettings;
		newScene->m_PostProcessSettings = scene->m_PostProcessSettings;
		if (scene->m_UseSkybox && scene->m_Skybox)
		{
			newScene->m_Skybox = std::make_shared<Skybox>();
			auto& src = *scene->m_Skybox;
			auto& dst = *newScene->m_Skybox;
			dst.GroundColor      = src.GroundColor;
			dst.Turbidity        = src.Turbidity;
			dst.SkyExposure      = src.SkyExposure;
			dst.SunColor         = src.SunColor;
			dst.SunSize          = src.SunSize;
			dst.SunStrength      = src.SunStrength;
			dst.CloudColor       = src.CloudColor;
			dst.CloudCoverage    = src.CloudCoverage;
			dst.CloudDensity     = src.CloudDensity;
			dst.CloudSoftness    = src.CloudSoftness;
			dst.CloudHeight      = src.CloudHeight;
			dst.CloudScale       = src.CloudScale;
			dst.CloudWindDirection = src.CloudWindDirection;
			dst.CloudScrollSpeed = src.CloudScrollSpeed;
			dst.CloudShadowing   = src.CloudShadowing;
			dst.CloudHorizonFade = src.CloudHorizonFade;
		}
		if (scene->m_UsePostProcess && scene->m_PostProcess &&
		    newScene->m_ViewportWidth > 0 && newScene->m_ViewportHeight > 0)
		{
			newScene->m_PostProcess = PostProcess::Create(
			    (uint32_t)newScene->m_ViewportWidth, (uint32_t)newScene->m_ViewportHeight);
			newScene->m_PostProcess->EnableBloom    = scene->m_PostProcess->EnableBloom;
			newScene->m_PostProcess->BloomThreshold = scene->m_PostProcess->BloomThreshold;
			newScene->m_PostProcess->BloomStrength  = scene->m_PostProcess->BloomStrength;
			newScene->m_PostProcess->EnableFXAA     = scene->m_PostProcess->EnableFXAA;
			newScene->m_PostProcess->Preview        = scene->m_PostProcess->Preview;
			newScene->m_PostProcess->EnableSSAO     = scene->m_PostProcess->EnableSSAO;
			newScene->m_PostProcess->SSAORadius     = scene->m_PostProcess->SSAORadius;
			newScene->m_PostProcess->SSAOBias       = scene->m_PostProcess->SSAOBias;
			newScene->m_PostProcess->SSAOPower      = scene->m_PostProcess->SSAOPower;
			newScene->m_PostProcess->SSAOSamples    = scene->m_PostProcess->SSAOSamples;
			newScene->m_PostProcess->SSAOStrength   = scene->m_PostProcess->SSAOStrength;
		}

		return newScene;
	}
	Entity Scene::CreateEntity(const std::string& name)
	{
		return CreateEntityWithUUID(UUID(), name);
	}
	Entity Scene::CreateEntityWithUUID(UUID uuid, const std::string& name)
	{
		
		Entity entity = { m_Registry.create(), this };		
		entity.AddComponent<IDComponent>(uuid);
		entity.AddComponent<TransformComponent>();
		auto& tag = entity.AddComponent<TagComponent>();
		tag.Tag = name.empty() ? "Entity" : name;
		m_EntityMap[uuid] = entity;
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
		}
		return {};
	}
	Entity Scene::EnsurePrimaryCamera()
	{
		{
			auto springView = m_Registry.view<SpringArmComponent>();
			const bool hasSpringArm = springView.begin() != springView.end();

			if (!hasSpringArm)
			{
				Entity selected;
				auto camView = m_Registry.view<CameraComponent>();
				for (auto ce : camView)
				{
					if (!selected)
						selected = Entity{ ce, this };
					if (camView.get<CameraComponent>(ce).Primary)
					{
						selected = Entity{ ce, this };
						break;
					}
				}

				if (!selected)
				{
					selected = CreateEntity("Camera");
					selected.AddComponent<CameraComponent>();
				}

				for (auto ce : camView)
					camView.get<CameraComponent>(ce).Primary = (Entity{ ce, this } == selected);

				auto& camera = selected.GetComponent<CameraComponent>();
				camera.Primary = true;
				if (m_ViewportWidth > 0.0f && m_ViewportHeight > 0.0f && !camera.FixedAspectRatio)
					camera.Camera.SetViewportSize((uint32_t)m_ViewportWidth, (uint32_t)m_ViewportHeight);
				return selected;
			}

			Entity playerCamera;
			for (auto e : springView)
			{
				auto& arm = springView.get<SpringArmComponent>(e);
				if (arm.TargetCameraUUID == 0)
					continue;

				Entity linkedCamera = GetEntityByUUID(arm.TargetCameraUUID);
				if (linkedCamera && linkedCamera.HasComponent<CameraComponent>())
				{
					playerCamera = linkedCamera;
					break;
				}
			}

			if (!playerCamera)
			{
				Entity namedCamera = FindEntityByName("PlayerCamera");
				if (namedCamera && namedCamera.HasComponent<CameraComponent>())
					playerCamera = namedCamera;
			}

			if (!playerCamera)
			{
				playerCamera = CreateEntity("PlayerCamera");
				playerCamera.AddComponent<CameraComponent>();
			}

			auto& playerCameraComponent = playerCamera.GetComponent<CameraComponent>();
			playerCameraComponent.Primary = true;
			if (m_ViewportWidth > 0.0f && m_ViewportHeight > 0.0f && !playerCameraComponent.FixedAspectRatio)
				playerCameraComponent.Camera.SetViewportSize((uint32_t)m_ViewportWidth, (uint32_t)m_ViewportHeight);

			auto camView = m_Registry.view<CameraComponent>();
			for (auto ce : camView)
			{
				Entity cameraEntity{ ce, this };
				auto& camera = camView.get<CameraComponent>(ce);
				camera.Primary = (cameraEntity == playerCamera);

				if (cameraEntity == playerCamera || !m_Registry.any_of<ActorComponent>(ce))
					continue;

				auto& actorComponent = m_Registry.get<ActorComponent>(ce);
				m_ActorSystem->QueueDestroy(cameraEntity.GetUUID());
				actorComponent.ClassID.clear();
				actorComponent.Overrides.clear();
			}

			UUID playerCameraUUID = playerCamera.GetUUID();
			for (auto e : springView)
			{
				auto& arm = springView.get<SpringArmComponent>(e);
				Entity linkedCamera = arm.TargetCameraUUID != 0 ? GetEntityByUUID(arm.TargetCameraUUID) : Entity{};
				if (!linkedCamera || !linkedCamera.HasComponent<CameraComponent>())
					arm.TargetCameraUUID = playerCameraUUID;
			}

			return playerCamera;
		}
	}

	Entity Scene::FindEntityByName(std::string_view name)
	{
		auto view = m_Registry.view<TagComponent>();
		for (auto entity : view)
		{
			const TagComponent& tc = view.get<TagComponent>(entity);
			if (tc.Tag == name)
				return Entity{ entity, this };
		}
		return {};
	}
	Entity Scene::GetEntityByUUID(UUID id)
	{
		if (m_EntityMap.find(id) != m_EntityMap.end())
		{
			return { m_EntityMap.at(id), this };
		}
		return {};
	}
	AActor* Scene::FindActor(UUID id) const
	{
		return m_ActorSystem ? m_ActorSystem->FindActor(id) : nullptr;
	}
	void Scene::SetPlayerInputEnabled(bool enabled)
	{
		m_PlayerInputEnabled = enabled;

		auto view = m_Registry.view<ActorComponent, IDComponent>();
		for (auto e : view)
		{
			auto& actorComponent = view.get<ActorComponent>(e);
			AActor* actor = FindActor(view.get<IDComponent>(e).ID);
			if (!actor)
				continue;

			const NativeClassID resolvedClassID = NativeClassRegistry::Get().ResolveClassID(actorComponent.ClassID);
			bool playerCandidate = resolvedClassID == "Azure::PlayerCharacter" || actorComponent.ClassID == "PlayerCharacter";
			if (!playerCandidate && m_Registry.any_of<TagComponent>(e))
			{
				const auto& tag = m_Registry.get<TagComponent>(e).Tag;
				playerCandidate = tag == "Player" || tag == "PlayerCharacter";
			}

			if (!playerCandidate)
				continue;

			if (auto* pawn = dynamic_cast<APawn*>(actor))
				pawn->SetPlayerControlled(enabled);
		}
	}

	SceneDiagnostics Scene::GetDiagnostics()
	{
		SceneDiagnostics diagnostics;
		diagnostics.PlayerInputEnabled = m_PlayerInputEnabled;
		diagnostics.EjectCameraActive = m_EjectCamera != nullptr;
		diagnostics.UIRootCount = m_LastRuntimeUIRootCount;
		diagnostics.RuntimeUIWidgetCount = m_LastRuntimeUIWidgetCount;
		diagnostics.RuntimeUIRendered = m_LastRuntimeUIRendered;
		diagnostics.RuntimeUIMissingDocument = m_LastRuntimeUIMissingDocument;
		diagnostics.RuntimeUIViewportWidth = m_LastRuntimeUIViewportWidth;
		diagnostics.RuntimeUIViewportHeight = m_LastRuntimeUIViewportHeight;
		diagnostics.RuntimeUIDocumentPath = m_LastRuntimeUIDocumentPath;
		Entity possessedEntity;

		auto entityView = m_Registry.view<IDComponent>();
		for (auto e : entityView)
			diagnostics.EntityCount++;

		auto cameraView = m_Registry.view<CameraComponent>();
		for (auto e : cameraView)
		{
			diagnostics.CameraCount++;
			auto& camera = cameraView.get<CameraComponent>(e);
			if (!camera.Primary)
				continue;

			diagnostics.PrimaryCameraCount++;
			if (m_Registry.any_of<TagComponent>(e))
				diagnostics.PrimaryCameraName = m_Registry.get<TagComponent>(e).Tag;
		}

		auto meshView = m_Registry.view<MeshComponent>();
		for (auto e : meshView)
		{
			diagnostics.MeshEntityCount++;
			auto& mesh = meshView.get<MeshComponent>(e);
			if (mesh.MeshData || mesh.ModelAsset)
				diagnostics.DrawableMeshCount++;
		}

		for (auto e : m_Registry.view<VisualOffsetComponent>())
			diagnostics.VisualOffsetCount++;

		auto rb3dView = m_Registry.view<Rigidbody3DComponent>();
		for (auto e : rb3dView)
		{
			Entity entity = { e, this };
			auto& rb = rb3dView.get<Rigidbody3DComponent>(e);

			diagnostics.Rigidbody3DCount++;
			if (rb.RuntimeBodyID != UINT32_MAX)
				diagnostics.RuntimePhysicsBody3DCount++;

			switch (rb.Type)
			{
				case Rigidbody3DComponent::BodyType::Static:    diagnostics.StaticBody3DCount++; break;
				case Rigidbody3DComponent::BodyType::Dynamic:   diagnostics.DynamicBody3DCount++; break;
				case Rigidbody3DComponent::BodyType::Kinematic: diagnostics.KinematicBody3DCount++; break;
			}

			if (!HasEnabled3DCollider(entity))
				diagnostics.MissingCollider3DCount++;
		}

		for (auto e : m_Registry.view<BoxCollider3DComponent>())
			diagnostics.BoxCollider3DCount++;
		for (auto e : m_Registry.view<SphereCollider3DComponent>())
			diagnostics.SphereCollider3DCount++;
		for (auto e : m_Registry.view<CapsuleCollider3DComponent>())
			diagnostics.CapsuleCollider3DCount++;

		auto meshColliderView = m_Registry.view<MeshCollider3DComponent>();
		for (auto e : meshColliderView)
		{
			Entity entity = { e, this };
			auto& collider = meshColliderView.get<MeshCollider3DComponent>(e);
			diagnostics.MeshCollider3DCount++;

			if (!collider.Enabled)
				continue;

			uint32_t triangleCount = collider.RuntimeTriangleCount;
			if (entity.HasComponent<MeshComponent>())
				triangleCount = std::max(triangleCount, CountModelCollisionTriangles(entity.GetComponent<MeshComponent>().ModelAsset));
			diagnostics.MeshColliderTriangleCount += triangleCount;

			bool valid = entity.HasComponent<MeshComponent>() &&
			             entity.GetComponent<MeshComponent>().ModelAsset &&
			             triangleCount > 0 &&
			             entity.HasComponent<Rigidbody3DComponent>() &&
			             entity.GetComponent<Rigidbody3DComponent>().Type == Rigidbody3DComponent::BodyType::Static;
			if (!valid)
				diagnostics.InvalidMeshCollider3DCount++;
		}
		diagnostics.PhysicsBodyCreationFailureCount = m_Physics3DBodyCreationFailureCount;

		auto characterView = m_Registry.view<CharacterControllerComponent>();
		for (auto e : characterView)
		{
			Entity entity = { e, this };
			auto& ccc = characterView.get<CharacterControllerComponent>(e);
			diagnostics.CharacterControllerCount++;
			if (ccc._RuntimeCharacter)
				diagnostics.RuntimeCharacterControllerCount++;
			if (!IsCharacterColliderValid(entity))
				diagnostics.InvalidCharacterColliderCount++;
		}

		auto springArmView = m_Registry.view<SpringArmComponent>();
		for (auto e : springArmView)
			diagnostics.SpringArmCount++;

		auto scriptView = m_Registry.view<ActorComponent, IDComponent>();
		for (auto e : scriptView)
		{
			auto& actorComponent = scriptView.get<ActorComponent>(e);
			AActor* actor = FindActor(scriptView.get<IDComponent>(e).ID);
			if (!actorComponent.ClassID.empty())
				diagnostics.NativeScriptCount++;
			const NativeClassID resolvedClassID = NativeClassRegistry::Get().ResolveClassID(actorComponent.ClassID);
			if (actor && (resolvedClassID == "Azure::ZombieTestActor" || resolvedClassID == "Azure::ZombieCharacter"))
				diagnostics.ActiveZombieCount++;

			if (actor)
			{
				if (auto* pawn = dynamic_cast<APawn*>(actor); pawn && pawn->IsPlayerControlled())
				{
					if (m_Registry.any_of<TagComponent>(e))
						diagnostics.PossessedPawnName = m_Registry.get<TagComponent>(e).Tag;

					Entity pawnEntity = { e, this };
					possessedEntity = pawnEntity;
					diagnostics.PossessedPawnHasVisualOffset = pawnEntity.HasComponent<VisualOffsetComponent>();
					if (pawnEntity.HasComponent<TransformComponent>())
						diagnostics.PossessedPawnFeetPosition = pawnEntity.GetComponent<TransformComponent>().Translation;
					if (pawnEntity.HasComponent<PlayerStatsComponent>())
					{
						auto& stats = pawnEntity.GetComponent<PlayerStatsComponent>();
						diagnostics.PossessedPawnHasStats = true;
						diagnostics.PossessedPawnHealth = stats.Health;
						diagnostics.PossessedPawnMaxHealth = stats.MaxHealth;
						diagnostics.PossessedPawnStamina = stats.Stamina;
						diagnostics.PossessedPawnMaxStamina = stats.MaxStamina;
					}
					if (pawnEntity.HasComponent<AmmoComponent>())
						{
							auto& ammo = pawnEntity.GetComponent<AmmoComponent>();
							diagnostics.PossessedPawnHasAmmo     = true;
							diagnostics.PossessedPawnAmmoInMag   = ammo.InMag;
							diagnostics.PossessedPawnAmmoReserve = ammo.Reserve;
							diagnostics.PossessedPawnReloading   = ammo.Reloading;
							diagnostics.HitmarkerTimer           = ammo.HitFlash;
						}
						if (pawnEntity.HasComponent<CharacterControllerComponent>())
					{
						auto& ccc = pawnEntity.GetComponent<CharacterControllerComponent>();
						diagnostics.PossessedPawnGrounded = ccc.IsGrounded;
						diagnostics.PossessedPawnVelocity = ccc.Velocity;
					}
					if (pawnEntity.HasComponent<CapsuleCollider3DComponent>())
					{
						auto& cap = pawnEntity.GetComponent<CapsuleCollider3DComponent>();
						diagnostics.PossessedPawnCapsuleRadius = cap.Radius;
						diagnostics.PossessedPawnCapsuleHalfHeight = cap.HalfHeight;
					}
				}
			}
		}

		for (auto e : m_Registry.view<InteractableComponent>())
			diagnostics.InteractableCount++;
		for (auto e : m_Registry.view<PickupComponent>())
			diagnostics.PickupCount++;

		if (possessedEntity && possessedEntity.HasComponent<TransformComponent>())
		{
			const glm::vec3 playerPos = possessedEntity.GetComponent<TransformComponent>().Translation;
			float bestDistSq = std::numeric_limits<float>::max();
			auto interactableView = m_Registry.view<TransformComponent, InteractableComponent>();
			for (auto e : interactableView)
			{
				auto&& [tc, interactable] = interactableView.get<TransformComponent, InteractableComponent>(e);
				if (!interactable.Enabled)
					continue;

				float distSq = glm::length2(tc.Translation - playerPos);
				float radiusSq = interactable.InteractionRadius * interactable.InteractionRadius;
				if (distSq <= radiusSq && distSq < bestDistSq)
				{
					bestDistSq = distSq;
					if (!interactable.DisplayName.empty())
						diagnostics.NearbyInteractableName = interactable.DisplayName;
					else if (m_Registry.any_of<TagComponent>(e))
						diagnostics.NearbyInteractableName = m_Registry.get<TagComponent>(e).Tag;
					else
						diagnostics.NearbyInteractableName = "<unnamed>";
				}
			}
		}

		auto manifest = CollectAssetManifest();
		diagnostics.AssetReferenceCount = manifest.ReferencedCount;
		diagnostics.MissingAssetCount = manifest.MissingCount;
		diagnostics.ExternalAssetCount = manifest.ExternalCount;
		diagnostics.ImportedAssetCount = manifest.ImportedCount;

		return diagnostics;
	}

	SceneAssetManifest Scene::CollectAssetManifest()
	{
		SceneAssetManifest manifest;

		auto addDependency = [&](entt::entity entity, const std::string& type, const std::string& rawPath, const std::string& component)
		{
			if (rawPath.empty())
				return;

			AssetDependency dependency;
			dependency.Type = type;
			dependency.Path = AssetPath::ToProjectRelative(rawPath);

			auto resolved = AssetPath::ResolvePath(rawPath, m_SceneFilePath);
			dependency.ResolvedPath = AssetPath::NormalizePath(resolved);
			dependency.Exists = std::filesystem::exists(resolved);
			dependency.External = AssetPath::IsExternal(rawPath);
			dependency.Imported = AssetPath::IsImported(dependency.Path);
			dependency.SourceComponent = component;

			if (m_Registry.any_of<IDComponent>(entity))
				dependency.SourceEntity = (uint64_t)m_Registry.get<IDComponent>(entity).ID;
			if (m_Registry.any_of<TagComponent>(entity))
				dependency.SourceTag = m_Registry.get<TagComponent>(entity).Tag;

			manifest.ReferencedCount++;
			if (!dependency.Exists)
				manifest.MissingCount++;
			if (dependency.External)
				manifest.ExternalCount++;
			if (dependency.Imported)
				manifest.ImportedCount++;

			manifest.Dependencies.push_back(std::move(dependency));
		};

		auto addMaterialDependencies = [&](entt::entity entity, const Shared<Material>& material, const std::string& componentPrefix)
		{
			if (!material)
				return;

			auto addTexture = [&](const std::string& slot, Shared<Texture2D>& texture)
			{
				if (texture)
					addDependency(entity, "Texture", texture->GetTexturePath(), componentPrefix + "." + slot);
			};

			addTexture("Albedo", material->AlbedoMap);
			addTexture("Normal", material->NormalMap);
			addTexture("MetallicRoughness", material->MetallicRoughnessMap);
			addTexture("AO", material->AOMap);
			addTexture("Emissive", material->EmissiveMap);
		};

		auto meshView = m_Registry.view<MeshComponent>();
		for (auto entity : meshView)
		{
			auto& mesh = meshView.get<MeshComponent>(entity);
			addDependency(entity, "Model", mesh.FilePath, "MeshComponent");
			addMaterialDependencies(entity, mesh.MaterialInstance, "MeshComponent.Material");

			if (mesh.ModelAsset)
			{
				int materialIndex = 0;
				for (auto& material : mesh.ModelAsset->Materials)
					addMaterialDependencies(entity, material, "MeshComponent.ModelMaterial" + std::to_string(materialIndex++));
			}
		}

		auto terrainView = m_Registry.view<TerrainComponent>();
		for (auto entity : terrainView)
		{
			auto& terrain = terrainView.get<TerrainComponent>(entity);
			addDependency(entity, "Texture", terrain.Spec.HeightmapPath, "TerrainComponent.Heightmap");
		}

		auto spriteView = m_Registry.view<SpriteRendererComponent>();
		for (auto entity : spriteView)
		{
			auto& sprite = spriteView.get<SpriteRendererComponent>(entity);
			addMaterialDependencies(entity, sprite.MaterialInstance, "SpriteRendererComponent.Material");
		}

		auto foliageView = m_Registry.view<FoliageComponent>();
		for (auto entity : foliageView)
		{
			auto& foliage = foliageView.get<FoliageComponent>(entity);
			addDependency(entity, "Model", foliage.FilePath, "FoliageComponent");
			if (foliage.ModelAsset)
			{
				int materialIndex = 0;
				for (auto& material : foliage.ModelAsset->Materials)
					addMaterialDependencies(entity, material, "FoliageComponent.ModelMaterial" + std::to_string(materialIndex++));
			}
		}

		auto lodView = m_Registry.view<MeshLODComponent>();
		for (auto entity : lodView)
		{
			auto& lod = lodView.get<MeshLODComponent>(entity);
			for (size_t i = 0; i < lod.Levels.size(); ++i)
			{
				auto& level = lod.Levels[i];
				addDependency(entity, "Model", level.FilePath, "MeshLODComponent.Level" + std::to_string(i));
			}
		}

		auto audioView = m_Registry.view<AudioSourceComponent>();
		for (auto entity : audioView)
		{
			auto& audio = audioView.get<AudioSourceComponent>(entity);
			addDependency(entity, "Audio", audio.FilePath, "AudioSourceComponent");
		}

		auto uiView = m_Registry.view<UIRootComponent>();
		for (auto entity : uiView)
		{
			auto& ui = uiView.get<UIRootComponent>(entity);
			addDependency(entity, "UI", ui.DocumentPath, "UIRootComponent");
		}

		return manifest;
	}

	bool Scene::GenerateStaticMeshCollision(Entity entity, std::string* outMessage)
	{
		auto setMessage = [&](const std::string& message)
		{
			if (outMessage)
				*outMessage = message;
		};

		if (!entity)
		{
			setMessage("No entity selected");
			return false;
		}

		if (!entity.HasComponent<MeshComponent>())
		{
			setMessage("Selected entity has no Mesh Renderer");
			return false;
		}

		auto& mesh = entity.GetComponent<MeshComponent>();
		if (!mesh.ModelAsset && !mesh.FilePath.empty())
			mesh.ModelAsset = ModelLoader::Load(AssetPath::ResolvePath(mesh.FilePath, m_SceneFilePath).string());

		if (!mesh.ModelAsset)
		{
			setMessage("Mesh Renderer has no loaded model");
			return false;
		}

		if (CountModelCollisionTriangles(mesh.ModelAsset) == 0 && !mesh.FilePath.empty())
			mesh.ModelAsset = ModelLoader::Load(AssetPath::ResolvePath(mesh.FilePath, m_SceneFilePath).string());

		const uint32_t triangleCount = CountModelCollisionTriangles(mesh.ModelAsset);
		if (triangleCount == 0)
		{
			setMessage("Model has no CPU triangle data; reload or reimport the model");
			return false;
		}

		Rigidbody3DComponent* rb = nullptr;
		if (entity.HasComponent<Rigidbody3DComponent>())
			rb = &entity.GetComponent<Rigidbody3DComponent>();
		else
			rb = &entity.AddComponent<Rigidbody3DComponent>();

		rb->Type = Rigidbody3DComponent::BodyType::Static;
		rb->RuntimeBodyID = UINT32_MAX;

		MeshCollider3DComponent* collider = nullptr;
		if (entity.HasComponent<MeshCollider3DComponent>())
			collider = &entity.GetComponent<MeshCollider3DComponent>();
		else
			collider = &entity.AddComponent<MeshCollider3DComponent>();

		collider->Enabled = true;
		collider->RuntimeTriangleCount = triangleCount;
		collider->RuntimeBodyCreated = false;
		collider->RuntimeStatus = "Ready (" + std::to_string(triangleCount) + " triangles)";

		setMessage("Static mesh collision ready: " + std::to_string(triangleCount) + " triangles");
		return true;
	}

	bool Scene::RebuildTerrain(Entity entity, std::string* outMessage)
	{
		auto setMessage = [&](const std::string& message)
		{
			if (outMessage)
				*outMessage = message;
		};

		if (!entity || !entity.HasComponent<TerrainComponent>())
		{
			setMessage("TerrainComponent is required.");
			return false;
		}

		auto& terrain = entity.GetComponent<TerrainComponent>();
		terrain.Spec = SanitizeTerrainSpec(terrain.Spec);

		TerrainSpec generationSpec = terrain.Spec;
		if (!generationSpec.HeightmapPath.empty())
			generationSpec.HeightmapPath = AssetPath::ResolvePath(generationSpec.HeightmapPath, m_SceneFilePath).string();

		auto terrainMesh = GenerateTerrain(generationSpec);
		if (!terrainMesh)
		{
			setMessage("Terrain mesh generation failed.");
			return false;
		}

		auto& mesh = entity.HasComponent<MeshComponent>()
			? entity.GetComponent<MeshComponent>()
			: entity.AddComponent<MeshComponent>();
		mesh.MeshData = std::move(terrainMesh);
		mesh.Primitive = MeshComponent::PrimitiveType::None;
		if (!mesh.MaterialInstance)
			mesh.MaterialInstance = Material::Create();

		setMessage("Terrain mesh rebuilt.");
		return true;
	}

	bool Scene::FitCharacterVisualToCapsule(Entity entity, std::string* outMessage)
	{
		auto setMessage = [&](const std::string& message)
		{
			if (outMessage)
				*outMessage = message;
		};

		if (!entity)
		{
			setMessage("No entity selected");
			return false;
		}

		if (!entity.HasComponent<CapsuleCollider3DComponent>())
		{
			setMessage("Selected entity has no Capsule Collider 3D");
			return false;
		}

		auto& capsule = entity.GetComponent<CapsuleCollider3DComponent>();
		if (capsule.Radius <= 0.0f || capsule.HalfHeight <= 0.0f)
		{
			setMessage("Capsule radius and half height must be greater than zero");
			return false;
		}

		VisualOffsetComponent* visual = nullptr;
		if (entity.HasComponent<VisualOffsetComponent>())
			visual = &entity.GetComponent<VisualOffsetComponent>();
		else
			visual = &entity.AddComponent<VisualOffsetComponent>();

		const float diameter = capsule.Radius * 2.0f;
		const float fullHeight = (capsule.HalfHeight + capsule.Radius) * 2.0f;
		visual->Translation = capsule.Offset + glm::vec3(0.0f, capsule.HalfHeight + capsule.Radius, 0.0f);

		if (entity.HasComponent<MeshComponent>())
		{
			auto& mesh = entity.GetComponent<MeshComponent>();
			if (mesh.Primitive == MeshComponent::PrimitiveType::Cube && mesh.FilePath.empty() && !mesh.ModelAsset)
				visual->Scale = { diameter, fullHeight, diameter };
		}

		setMessage("Visual offset fitted to capsule");
		return true;
	}

	bool Scene::ResetVisualOffset(Entity entity, std::string* outMessage)
	{
		if (!entity)
		{
			if (outMessage)
				*outMessage = "No entity selected";
			return false;
		}

		VisualOffsetComponent* visual = nullptr;
		if (entity.HasComponent<VisualOffsetComponent>())
			visual = &entity.GetComponent<VisualOffsetComponent>();
		else
			visual = &entity.AddComponent<VisualOffsetComponent>();

		visual->Translation = { 0.0f, 0.0f, 0.0f };
		visual->Rotation = { 0.0f, 0.0f, 0.0f };
		visual->Scale = { 1.0f, 1.0f, 1.0f };

		if (outMessage)
			*outMessage = "Visual offset reset";
		return true;
	}

	bool Scene::SnapCharacterFeetToGround(Entity entity, std::string* outMessage)
	{
		auto setMessage = [&](const std::string& message)
		{
			if (outMessage)
				*outMessage = message;
		};

		if (!entity || !entity.HasComponent<TransformComponent>())
		{
			setMessage("Selected entity has no Transform");
			return false;
		}

		if (!m_Physics3DWorld || !m_Physics3DWorld->IsValid())
		{
			setMessage("Snap Feet To Ground requires active Play/Simulate physics");
			return false;
		}

		auto& tc = entity.GetComponent<TransformComponent>();
		glm::vec3 hitPosition;
		const glm::vec3 rayOrigin = tc.Translation + glm::vec3(0.0f, 5.0f, 0.0f);
		if (!m_Physics3DWorld->CastRay(rayOrigin, glm::vec3(0.0f, -200.0f, 0.0f), hitPosition))
		{
			setMessage("No ground found below selected entity");
			return false;
		}

		tc.Translation.y = hitPosition.y;
		if (entity.HasComponent<CharacterControllerComponent>())
		{
			auto& ccc = entity.GetComponent<CharacterControllerComponent>();
			if (ccc._RuntimeCharacter)
			{
				auto* character = static_cast<JPH::CharacterVirtual*>(ccc._RuntimeCharacter);
				character->SetPosition(JPH::RVec3(tc.Translation.x, tc.Translation.y, tc.Translation.z));
			}
		}

		setMessage("Character feet snapped to ground");
		return true;
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

	Entity Scene::CloneEntityFrom(Entity source, const std::string& nameOverride)
	{
		if (!source || !source.HasComponent<TagComponent>() || !source.HasComponent<TransformComponent>())
			return {};

		const std::string name = nameOverride.empty() ? source.GetComponent<TagComponent>().Tag : nameOverride;
		Entity destination = CreateEntity(name);
		destination.GetComponent<TransformComponent>() = source.GetComponent<TransformComponent>();

		auto copy = [&]<typename T>()
		{
			if (source.HasComponent<T>() && !destination.HasComponent<T>())
				m_Registry.emplace<T>((entt::entity)destination, source.GetComponent<T>());
		};

		copy.template operator()<ParticleSystemComponent>();
		copy.template operator()<SpriteRendererComponent>();
		copy.template operator()<CircleRendererComponent>();
		copy.template operator()<Rigidbody2DComponent>();
		copy.template operator()<BoxCollider2DComponent>();
		copy.template operator()<CircleCollider2DComponent>();
		copy.template operator()<CameraComponent>();
		copy.template operator()<ActorComponent>();
		copy.template operator()<PointLightComponent>();
		copy.template operator()<DirectionalLightComponent>();
		copy.template operator()<SpotLightComponent>();
		copy.template operator()<MeshComponent>();
		copy.template operator()<VisualOffsetComponent>();
		copy.template operator()<TerrainComponent>();
		copy.template operator()<Rigidbody3DComponent>();
		copy.template operator()<BoxCollider3DComponent>();
		copy.template operator()<SphereCollider3DComponent>();
		copy.template operator()<CapsuleCollider3DComponent>();
		copy.template operator()<MeshCollider3DComponent>();
		copy.template operator()<CharacterControllerComponent>();
		copy.template operator()<InteractableComponent>();
		copy.template operator()<PickupComponent>();
		copy.template operator()<PlayerStatsComponent>();
		copy.template operator()<UIRootComponent>();
		copy.template operator()<SpringArmComponent>();
		copy.template operator()<AnimatorComponent>();
		copy.template operator()<MeshLODComponent>();
		copy.template operator()<AudioSourceComponent>();
		copy.template operator()<FoliageComponent>();

		if (destination.HasComponent<Rigidbody3DComponent>())
			destination.GetComponent<Rigidbody3DComponent>().RuntimeBodyID = UINT32_MAX;
		if (destination.HasComponent<CharacterControllerComponent>())
		{
			auto& controller = destination.GetComponent<CharacterControllerComponent>();
			controller._RuntimeCharacter = nullptr;
			controller._PendingMoveInput = glm::vec3(0.0f);
			controller._PendingJump = false;
			controller.IsGrounded = false;
			controller.Velocity = glm::vec3(0.0f);
		}
		if (destination.HasComponent<MeshCollider3DComponent>())
		{
			auto& collider = destination.GetComponent<MeshCollider3DComponent>();
			collider.RuntimeBodyCreated = false;
			collider.RuntimeTriangleCount = 0;
			collider.RuntimeStatus.clear();
		}
		if (destination.HasComponent<AudioSourceComponent>())
			destination.GetComponent<AudioSourceComponent>()._RuntimeHandle = kInvalidSound;

		return destination;
	}

	void Scene::OnRuntimeStart()
	{
		OnPhysics2DStart();
		OnPhysics3DStart();
		if (!m_GameModeClassID.empty())
		{
			m_GameMode = NativeClassRegistry::Get().Create<AGameMode>(m_GameModeClassID);
			if (m_GameMode)
			{
				m_GameMode->m_Scene = this; // grant scene access before lifecycle hooks
				m_GameMode->BeginPlay();
				m_GameMode->OnGameStart();
			}
			else
			{
				BLU_CORE_WARN("Scene: native game mode class '{0}' is not registered", m_GameModeClassID);
			}
		}
		m_ActorSystem->Start();

		// --- Audio ---
		AudioEngine::Get().Initialize();
		m_Registry.view<AudioSourceComponent, TransformComponent>().each(
			[&](auto entity, AudioSourceComponent& asc, const TransformComponent& tc)
			{
				if (asc.FilePath.empty()) return;
				asc._RuntimeHandle = AudioEngine::Get().LoadSound(AssetPath::ResolvePath(asc.FilePath, m_SceneFilePath).string());
				if (asc._RuntimeHandle == kInvalidSound) return;

				AudioEngine::Get().SetVolume (asc._RuntimeHandle, asc.Volume);
				AudioEngine::Get().SetPitch  (asc._RuntimeHandle, asc.Pitch);
				AudioEngine::Get().SetLooping(asc._RuntimeHandle, asc.Loop);
				AudioEngine::Get().SetSpatial(asc._RuntimeHandle, asc.Spatial);
				if (asc.Spatial)
				{
					AudioEngine::Get().SetSoundPosition(asc._RuntimeHandle, tc.Translation);
					AudioEngine::Get().SetAttenuation  (asc._RuntimeHandle, asc.MinDistance, asc.MaxDistance);
				}
				if (asc.PlayOnStart)
					AudioEngine::Get().Play(asc._RuntimeHandle);
			});
	}
	void Scene::OnPhysics2DStart()
	{
		m_PhysicsWorld = new b2World({ 0.0f, -0.0981f }); // gravity
		m_PhysicsWorld->SetGravity(b2Vec2(0, - 0.0981f));

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
				circleShape.m_radius = cc.Radius * transform.Scale.x * pixelToMetersScale;

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
	void Scene::OnPhysics3DStart()
	{
		m_Physics3DWorld = new Physics3DWorld();
		m_Physics3DWorld->Init({ 0.0f, -9.81f, 0.0f });
		m_Physics3DBodyCreationFailureCount = 0;
		for (auto e : m_Registry.view<MeshCollider3DComponent>())
		{
			auto& collider = m_Registry.get<MeshCollider3DComponent>(e);
			collider.RuntimeBodyCreated = false;
			collider.RuntimeStatus.clear();
		}
		for (auto e : m_Registry.view<CharacterControllerComponent>())
		{
			auto& ccc = m_Registry.get<CharacterControllerComponent>(e);
			ccc.IsGrounded = false;
			ccc.Velocity = { 0.0f, 0.0f, 0.0f };
			ccc._PendingMoveInput = { 0.0f, 0.0f, 0.0f };
			ccc._PendingJump = false;
		}

		auto view = m_Registry.view<Rigidbody3DComponent>();
		for (auto e : view)
		{
			Entity entity = { e, this };
			auto& tc = entity.GetComponent<TransformComponent>();
			auto& rb = entity.GetComponent<Rigidbody3DComponent>();
			rb.RuntimeBodyID = UINT32_MAX;

			// Build body spec from whichever collider component is present
			Physics3DBodySpec spec;
			MeshCollider3DComponent* meshCollider = entity.HasComponent<MeshCollider3DComponent>()
				? &entity.GetComponent<MeshCollider3DComponent>() : nullptr;

			if (entity.HasComponent<BoxCollider3DComponent>())
			{
				auto& bc       = entity.GetComponent<BoxCollider3DComponent>();
				spec.ShapeType = Physics3DShapeType::Box;
				spec.HalfExtents = bc.HalfExtents * tc.Scale; // scale into world space
				spec.Offset    = bc.Offset;
				spec.Friction  = bc.Friction;
				spec.Restitution = bc.Restitution;
				spec.Density   = bc.Density;
			}
			else if (entity.HasComponent<SphereCollider3DComponent>())
			{
				auto& sc       = entity.GetComponent<SphereCollider3DComponent>();
				spec.ShapeType = Physics3DShapeType::Sphere;
				// Use the maximum scale component to uniformly scale the sphere
				float maxScale = glm::max(glm::max(tc.Scale.x, tc.Scale.y), tc.Scale.z);
				spec.Radius    = sc.Radius * maxScale;
				spec.Offset    = sc.Offset;
				spec.Friction  = sc.Friction;
				spec.Restitution = sc.Restitution;
				spec.Density   = sc.Density;
			}
			else if (entity.HasComponent<CapsuleCollider3DComponent>())
			{
				auto& cc       = entity.GetComponent<CapsuleCollider3DComponent>();
				spec.ShapeType = Physics3DShapeType::Capsule;
				spec.Radius    = cc.Radius * glm::max(tc.Scale.x, tc.Scale.z);
				spec.HalfHeight = cc.HalfHeight * tc.Scale.y;
				spec.Offset    = cc.Offset;
				spec.Friction  = cc.Friction;
				spec.Restitution = cc.Restitution;
				spec.Density   = cc.Density;
			}
			else if (meshCollider && meshCollider->Enabled)
			{
				meshCollider->RuntimeBodyCreated = false;
				meshCollider->RuntimeTriangleCount = 0;
				meshCollider->RuntimeStatus.clear();

				if (rb.Type != Rigidbody3DComponent::BodyType::Static)
				{
					meshCollider->RuntimeStatus = "Mesh Collider supports Static Rigidbody3D only";
					m_Physics3DBodyCreationFailureCount++;
					BLU_CORE_WARN("Physics3D: entity has MeshCollider3D but Rigidbody3D is not Static");
					continue;
				}

				if (!entity.HasComponent<MeshComponent>())
				{
					meshCollider->RuntimeStatus = "Mesh Collider requires a Mesh Renderer";
					m_Physics3DBodyCreationFailureCount++;
					continue;
				}

				std::string status;
				if (!BuildMeshColliderSpec(entity, tc, entity.GetComponent<MeshComponent>(), *meshCollider, m_SceneFilePath, spec, status))
				{
					meshCollider->RuntimeStatus = status;
					m_Physics3DBodyCreationFailureCount++;
					BLU_CORE_WARN("Physics3D: mesh collider skipped: {0}", status);
					continue;
				}

				meshCollider->RuntimeStatus = status;
			}
			else
			{
				m_Physics3DBodyCreationFailureCount++;
				BLU_CORE_WARN("Physics3D: Rigidbody3D has no enabled 3D collider; skipping body");
				continue;
			}

			// Derive world-space rotation from euler angles stored in TransformComponent
			glm::quat worldRotation = glm::quat(tc.Rotation);

			Physics3DBodyType bodyType;
			switch (rb.Type)
			{
				case Rigidbody3DComponent::BodyType::Static:    bodyType = Physics3DBodyType::Static;    break;
				case Rigidbody3DComponent::BodyType::Dynamic:   bodyType = Physics3DBodyType::Dynamic;   break;
				case Rigidbody3DComponent::BodyType::Kinematic: bodyType = Physics3DBodyType::Kinematic; break;
				default:                                        bodyType = Physics3DBodyType::Static;    break;
			}

			rb.RuntimeBodyID = m_Physics3DWorld->AddBody(
				tc.Translation, worldRotation, bodyType, spec);
			if (rb.RuntimeBodyID == UINT32_MAX)
			{
				m_Physics3DBodyCreationFailureCount++;
				if (meshCollider)
					meshCollider->RuntimeStatus = "Jolt failed to create runtime body";
			}
			else if (meshCollider && spec.ShapeType == Physics3DShapeType::Mesh)
			{
				meshCollider->RuntimeBodyCreated = true;
				meshCollider->RuntimeStatus = "Runtime body active (" + std::to_string(meshCollider->RuntimeTriangleCount) + " triangles)";
			}
		}

		// Create JPH::CharacterVirtual for each entity with CharacterControllerComponent
		{
			auto charView = m_Registry.view<CharacterControllerComponent, TransformComponent>();
			for (auto e : charView)
			{
				Entity entity = { e, this };
				auto& tc  = entity.GetComponent<TransformComponent>();
				auto& ccc = entity.GetComponent<CharacterControllerComponent>();

				std::string status;
				ccc._RuntimeCharacter = CreateRuntimeCharacter(entity, tc, ccc, m_Physics3DWorld, status);
				if (!ccc._RuntimeCharacter)
				{
					m_Physics3DBodyCreationFailureCount++;
					BLU_CORE_ERROR("CharacterVirtual: {0}", status);
					continue;
				}
				BLU_CORE_INFO("CharacterVirtual created for entity at ({0:.1f},{1:.1f},{2:.1f})",
					tc.Translation.x, tc.Translation.y, tc.Translation.z);
			}
		}
	}

	void Scene::OnPhysics3DStop()
	{
		if (m_Physics3DWorld)
		{
			// Clear stored body IDs so we don't reference stale data after restart
			auto view = m_Registry.view<Rigidbody3DComponent>();
			for (auto e : view)
			{
				auto& rb = view.get<Rigidbody3DComponent>(e);
				rb.RuntimeBodyID = UINT32_MAX;
			}

			auto meshColliderView = m_Registry.view<MeshCollider3DComponent>();
			for (auto e : meshColliderView)
			{
				auto& collider = meshColliderView.get<MeshCollider3DComponent>(e);
				collider.RuntimeBodyCreated = false;
				collider.RuntimeStatus.clear();
			}

			// Destroy CharacterVirtual instances
			auto charView = m_Registry.view<CharacterControllerComponent>();
			for (auto e : charView)
			{
				auto& ccc = charView.get<CharacterControllerComponent>(e);
				if (ccc._RuntimeCharacter)
				{
					delete static_cast<JPH::CharacterVirtual*>(ccc._RuntimeCharacter);
					ccc._RuntimeCharacter = nullptr;
				}
				ccc.IsGrounded       = false;
				ccc.Velocity         = { 0.0f, 0.0f, 0.0f };
				ccc._PendingMoveInput = { 0.0f, 0.0f, 0.0f };
				ccc._PendingJump     = false;
			}

			delete m_Physics3DWorld;
			m_Physics3DWorld = nullptr;
			m_Physics3DBodyCreationFailureCount = 0;
		}
	}

	void Scene::OnRuntimeStop()
	{
		if (m_ActorSystem)
			m_ActorSystem->Stop();
		if (m_GameMode)
		{
			m_GameMode->EndPlay();
			m_GameMode.reset();
		}

		if (m_PhysicsWorld)
		{
			delete m_PhysicsWorld;
			m_PhysicsWorld = nullptr;
		}

		OnPhysics3DStop();

		// --- Audio: stop and release all runtime sounds ---
		m_Registry.view<AudioSourceComponent>().each([](auto /*entity*/, AudioSourceComponent& asc)
		{
			if (asc._RuntimeHandle != kInvalidSound)
			{
				AudioEngine::Get().Stop(asc._RuntimeHandle);
				AudioEngine::Get().UnloadSound(asc._RuntimeHandle);
				asc._RuntimeHandle = kInvalidSound;
			}
		});
		AudioEngine::Get().Shutdown();
	}
	void Scene::UpdateActiveCameraComponent(Timestep deltaTime)
	{
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
	void Scene::DestroyEntity(Entity entity)
	{
		const UUID id = entity.GetUUID();
		if (m_ActorSystem)
			m_ActorSystem->DestroyNow(id);
		m_EntityMap.erase(id);
		m_Registry.destroy(entity);
	}
    // ─── Helper: collect raw light data from the ECS ─────────────────────────--
    // Intensity is NOT baked into ambient/diffuse/specular here — that happens in
    // Renderer3D::PassLights during the single cbuffer upload.  Keeping this path
    // raw avoids double-multiplication and keeps concerns separated.
    static void GatherLights(entt::registry& reg,
        std::vector<DirLightData>&   outDir,
        std::vector<PointLightData>& outPoint,
        std::vector<SpotLightData>&  outSpot)
    {
        // Directional lights
        {
            auto view = reg.view<TransformComponent, DirectionalLightComponent>();
            for (auto e : view)
            {
                auto&& [tc, dlc] = view.get<TransformComponent, DirectionalLightComponent>(e);
                DirLightData d;
                glm::mat4 rotMat = glm::toMat4(glm::quat(tc.Rotation));
                d.Direction = glm::normalize(glm::vec3(rotMat * glm::vec4(dlc.Direction, 0.0f)));
                d.Ambient   = dlc.Ambient;
                d.Diffuse   = dlc.Diffuse;
                d.Specular  = dlc.Specular;
                d.Intensity = dlc.Intensity;
                outDir.push_back(d);
            }
        }
        // Point lights
        {
            auto view = reg.view<TransformComponent, PointLightComponent>();
            for (auto e : view)
            {
                auto&& [tc, plc] = view.get<TransformComponent, PointLightComponent>(e);
                PointLightData p;
                p.Position     = tc.Translation;
                p.Ambient      = plc.Ambient;
                p.Diffuse      = plc.Diffuse;
                p.Specular     = plc.Specular;
                p.Intensity    = plc.Intensity;
                p.Range        = plc.Range;
                p.AttConstant  = plc.AttConstant;
                p.AttLinear    = plc.AttLinear;
                p.AttQuadratic = plc.AttQuadratic;
                outPoint.push_back(p);
            }
        }
        // Spot lights
        {
            auto view = reg.view<TransformComponent, SpotLightComponent>();
            for (auto e : view)
            {
                auto&& [tc, slc] = view.get<TransformComponent, SpotLightComponent>(e);
                SpotLightData s;
                s.Position       = tc.Translation;
                s.Direction      = glm::normalize(slc.Direction);
                s.Ambient        = slc.Ambient;
                s.Diffuse        = slc.Diffuse;
                s.Specular       = slc.Specular;
                s.Intensity      = slc.Intensity;
                s.Range          = slc.Range;
                s.InnerCutoffCos = glm::cos(glm::radians(slc.InnerConeAngle));
                s.OuterCutoffCos = glm::cos(glm::radians(slc.OuterConeAngle));
                s.AttConstant    = slc.AttConstant;
                s.AttLinear      = slc.AttLinear;
                s.AttQuadratic   = slc.AttQuadratic;
                outSpot.push_back(s);
            }
        }
    }

	// ─── Shared render helpers ──────────────────────────────────────────────────

	void Scene::Render2DPass(EditorCamera& camera, Timestep deltaTime)
	{
		Renderer2D::BeginScene(camera);
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

	void Scene::Render2DPass(Camera& camera, const glm::mat4& transform, Timestep deltaTime, bool updateParticles)
	{
		Renderer2D::BeginScene(camera.GetProjectionMatrix(), transform);
		if (updateParticles)
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

	void Scene::RenderRuntimeUI()
	{
		auto result = RuntimeUI::RenderGameplayHUD(*this, m_ViewportWidth, m_ViewportHeight);
		m_LastRuntimeUIRootCount = result.UIRootCount;
		m_LastRuntimeUIWidgetCount = result.WidgetCount;
		m_LastRuntimeUIRendered = result.Rendered;
		m_LastRuntimeUIMissingDocument = result.MissingDocument;
		m_LastRuntimeUIViewportWidth = result.ViewportWidth;
		m_LastRuntimeUIViewportHeight = result.ViewportHeight;
		m_LastRuntimeUIDocumentPath = result.DocumentPath;
	}

	void Scene::Render3DPass(EditorCamera& camera)
	{
		std::vector<DirLightData>   dirLights;
		std::vector<PointLightData> pointLights;
		std::vector<SpotLightData>  spotLights;
		GatherLights(m_Registry, dirLights, pointLights, spotLights);

		ShadowPass(dirLights, camera.GetViewProjectionMatrix(), camera.GetNearClip(), camera.GetFarClip());

		Renderer3D::BeginScene(camera);
		Renderer3D::SetLights(dirLights, pointLights, spotLights);
		Renderer3D::SetFog(m_Fog);

		// God rays: project the sun (directional light) to screen for the post-process pass.
		if (m_PostProcess && !dirLights.empty())
		{
			glm::vec3 toSun = -glm::normalize(dirLights[0].Direction);
			glm::vec4 clip = Renderer3D::GetViewProjectionMatrix() * glm::vec4(toSun * 100000.0f, 1.0f);
			bool vis = clip.w > 0.0001f;
			glm::vec2 uv(0.5f);
			if (vis)
			{
				uv = glm::vec2(clip.x, clip.y) / clip.w * 0.5f + 0.5f;
				uv.y = 1.0f - uv.y;
				vis = uv.x > -0.25f && uv.x < 1.25f && uv.y > -0.25f && uv.y < 1.25f;
			}
			m_PostProcess->GodRaySunUV = uv;
			m_PostProcess->GodRaySunVisible = vis;
		}

		// Localized fog volumes — gather for the post-process composite (cheap when none exist).
		if (m_PostProcess)
		{
			m_PostProcess->FogVolumes.clear();
			auto fogView = m_Registry.view<TransformComponent, FogVolumeComponent>();
			for (auto e : fogView)
			{
				auto&& [tc, fv] = fogView.get<TransformComponent, FogVolumeComponent>(e);
				FogVolumeGPU g;
				g.Position = tc.Translation;
				g.Shape    = (int)fv.VolumeShape;
				g.Extents  = (fv.VolumeShape == FogVolumeComponent::Shape::Sphere)
				             ? glm::vec3(fv.Radius)
				             : fv.Extents;
				g.Density  = fv.Density;
				g.Color    = fv.Color;
				g.Falloff  = fv.Falloff;
				m_PostProcess->FogVolumes.push_back(g);
			}
			m_PostProcess->EnableFogVolumes = !m_PostProcess->FogVolumes.empty();
			m_PostProcess->FogInvViewProj   = glm::inverse(Renderer3D::GetViewProjectionMatrix());
			m_PostProcess->FogCameraPos     = camera.GetPosition();
		}
		{
			glm::vec3 camPos = camera.GetPosition();
			auto view = m_Registry.view<TransformComponent, MeshComponent>();
			for (auto& entity : view)
			{
				auto [transform, mesh] = view.get<TransformComponent, MeshComponent>(entity);
				glm::mat4 renderTransform = GetRenderTransform(m_Registry, entity, transform);

				// Skinned draw path
				if (mesh.ModelAsset && mesh.ModelAsset->HasSkeleton() &&
				    m_Registry.any_of<AnimatorComponent>(entity))
				{
					auto& anim = m_Registry.get<AnimatorComponent>(entity);
					Renderer3D::DrawSkinnedMesh(renderTransform, mesh,
					                            anim.FinalBoneMatrices, (int)entity);
					continue;
				}

				// LOD: if entity has a MeshLODComponent, pick the right model for this distance
				Shared<Model> savedModel = mesh.ModelAsset;
				if (m_Registry.any_of<MeshLODComponent>(entity))
				{
					auto& lod = m_Registry.get<MeshLODComponent>(entity);
					float dist = glm::length(glm::vec3(renderTransform[3]) - camPos);
					if (auto lodModel = lod.SelectLOD(dist)) mesh.ModelAsset = lodModel;
				}
				Renderer3D::DrawMesh(renderTransform, mesh, (int)entity);
				mesh.ModelAsset = savedModel;
			}
		}
		Renderer3D::FlushDrawCalls();

		// --- Foliage (GPU-instanced) ---
		{
			auto fview = m_Registry.view<FoliageComponent>();
			for (auto& entity : fview)
			{
				auto& fc = fview.get<FoliageComponent>(entity);
				if (fc.ModelAsset && !fc.Transforms.empty())
					Renderer3D::DrawMeshInstanced(fc.ModelAsset, fc.Transforms, nullptr,
						{ fc.WindEnabled, fc.WindDirection, fc.WindStrength, fc.WindFrequency, m_ElapsedTime });
			}
		}

		if (m_UseSkybox && m_Skybox)
		{
			// DirLight stores "toward-scene" convention; Skybox wants "toward-sun", so negate.
			glm::vec3 sunDir = dirLights.empty() ? glm::vec3(0.3f, 1.0f, 0.5f) : -dirLights[0].Direction;
			m_Skybox->Render(camera.GetViewMatrix(), camera.GetProjectionMatrix(), sunDir, m_ElapsedTime);
		}

		GpuParticleSystem::Get().Render();
		Renderer3D::EndScene();
	}

	void Scene::Render3DPass(Camera& camera, const glm::mat4& cameraTransform)
	{
		std::vector<DirLightData>   dirLights;
		std::vector<PointLightData> pointLights;
		std::vector<SpotLightData>  spotLights;
		GatherLights(m_Registry, dirLights, pointLights, spotLights);

		{
			glm::mat4 camView = glm::inverse(cameraTransform);
			glm::mat4 camVP   = camera.GetProjectionMatrix() * camView;
			ShadowPass(dirLights, camVP, 0.1f, 1000.0f);
		}

		Renderer3D::BeginScene(camera, cameraTransform);
		Renderer3D::SetLights(dirLights, pointLights, spotLights);
		Renderer3D::SetFog(m_Fog);

		// God rays: project the sun (directional light) to screen for the post-process pass.
		if (m_PostProcess && !dirLights.empty())
		{
			glm::vec3 toSun = -glm::normalize(dirLights[0].Direction);
			glm::vec4 clip = Renderer3D::GetViewProjectionMatrix() * glm::vec4(toSun * 100000.0f, 1.0f);
			bool vis = clip.w > 0.0001f;
			glm::vec2 uv(0.5f);
			if (vis)
			{
				uv = glm::vec2(clip.x, clip.y) / clip.w * 0.5f + 0.5f;
				uv.y = 1.0f - uv.y;
				vis = uv.x > -0.25f && uv.x < 1.25f && uv.y > -0.25f && uv.y < 1.25f;
			}
			m_PostProcess->GodRaySunUV = uv;
			m_PostProcess->GodRaySunVisible = vis;
		}

		// Localized fog volumes — gather for the post-process composite (cheap when none exist).
		if (m_PostProcess)
		{
			m_PostProcess->FogVolumes.clear();
			auto fogView = m_Registry.view<TransformComponent, FogVolumeComponent>();
			for (auto e : fogView)
			{
				auto&& [tc, fv] = fogView.get<TransformComponent, FogVolumeComponent>(e);
				FogVolumeGPU g;
				g.Position = tc.Translation;
				g.Shape    = (int)fv.VolumeShape;
				g.Extents  = (fv.VolumeShape == FogVolumeComponent::Shape::Sphere)
				             ? glm::vec3(fv.Radius)
				             : fv.Extents;
				g.Density  = fv.Density;
				g.Color    = fv.Color;
				g.Falloff  = fv.Falloff;
				m_PostProcess->FogVolumes.push_back(g);
			}
			m_PostProcess->EnableFogVolumes = !m_PostProcess->FogVolumes.empty();
			m_PostProcess->FogInvViewProj   = glm::inverse(Renderer3D::GetViewProjectionMatrix());
			m_PostProcess->FogCameraPos     = glm::vec3(cameraTransform[3]);
		}
		{
			glm::vec3 camPos = glm::vec3(cameraTransform[3]);
			auto view = m_Registry.view<TransformComponent, MeshComponent>();
			for (auto& entity : view)
			{
				auto [transform, mesh] = view.get<TransformComponent, MeshComponent>(entity);
				glm::mat4 renderTransform = GetRenderTransform(m_Registry, entity, transform);

				// Skinned draw path
				if (mesh.ModelAsset && mesh.ModelAsset->HasSkeleton() &&
				    m_Registry.any_of<AnimatorComponent>(entity))
				{
					auto& anim = m_Registry.get<AnimatorComponent>(entity);
					Renderer3D::DrawSkinnedMesh(renderTransform, mesh,
					                            anim.FinalBoneMatrices, (int)entity);
					continue;
				}

				Shared<Model> savedModel = mesh.ModelAsset;
				if (m_Registry.any_of<MeshLODComponent>(entity))
				{
					auto& lod = m_Registry.get<MeshLODComponent>(entity);
					float dist = glm::length(glm::vec3(renderTransform[3]) - camPos);
					if (auto lodModel = lod.SelectLOD(dist)) mesh.ModelAsset = lodModel;
				}
				Renderer3D::DrawMesh(renderTransform, mesh, (int)entity);
				mesh.ModelAsset = savedModel;
			}
		}
		Renderer3D::FlushDrawCalls();

		// --- Foliage (GPU-instanced) ---
		{
			auto fview = m_Registry.view<FoliageComponent>();
			for (auto& entity : fview)
			{
				auto& fc = fview.get<FoliageComponent>(entity);
				if (fc.ModelAsset && !fc.Transforms.empty())
					Renderer3D::DrawMeshInstanced(fc.ModelAsset, fc.Transforms, nullptr,
						{ fc.WindEnabled, fc.WindDirection, fc.WindStrength, fc.WindFrequency, m_ElapsedTime });
			}
		}

		if (m_UseSkybox && m_Skybox)
		{
			glm::mat4 camView = glm::inverse(cameraTransform);
			glm::vec3 sunDir  = dirLights.empty() ? glm::vec3(0.3f, 1.0f, 0.5f) : -dirLights[0].Direction;
			m_Skybox->Render(camView, camera.GetProjectionMatrix(), sunDir, m_ElapsedTime);
		}

		GpuParticleSystem::Get().Render();
		Renderer3D::EndScene();
	}

	// ─── CSM helper ──────────────────────────────────────────────────────────────

	static constexpr int   kNumCascades  = CascadedShadowMap::NUM_CASCADES;
	static constexpr float kShadowFar    = 200.0f; // world units of shadow coverage
	// World-distance far edge for each cascade (cumulative)
	static constexpr float kCascadeFar[kNumCascades] = { 15.0f, 60.0f, kShadowFar };
	static constexpr uint32_t kCSMSize   = 2048;

	// Compute the lightVP for one cascade slice [tNear, tFar] (fractions of [0,1]).
	static glm::mat4 FitCascade(
	    const glm::vec3 nearCorners[4],   // world-space corners of the camera near frustum face
	    const glm::vec3 farCorners[4],    // world-space corners of the camera far frustum face
	    float tNear, float tFar,          // fractions along the frustum edges
	    const glm::vec3& lightDir)        // normalized direction (toward the light)
	{
	    // 8 world-space corners of this cascade slice
	    glm::vec3 corners[8];
	    glm::vec3 center(0.0f);
	    for (int i = 0; i < 4; ++i)
	    {
	        corners[i]     = glm::mix(nearCorners[i], farCorners[i], tNear);
	        corners[i + 4] = glm::mix(nearCorners[i], farCorners[i], tFar);
	        center += corners[i] + corners[i + 4];
	    }
	    center /= 8.0f;

	    // Light-view matrix: look from behind the scene along lightDir
	    glm::vec3 up = glm::abs(lightDir.y) < 0.99f
	                       ? glm::vec3(0.0f, 1.0f, 0.0f)
	                       : glm::vec3(1.0f, 0.0f, 0.0f);
	    glm::mat4 lightView = glm::lookAt(center - lightDir * 100.0f, center, up);

	    // AABB of cascade corners in light space
	    glm::vec3 lsMin( FLT_MAX), lsMax(-FLT_MAX);
	    for (int i = 0; i < 8; ++i)
	    {
	        glm::vec3 lc = glm::vec3(lightView * glm::vec4(corners[i], 1.0f));
	        lsMin = glm::min(lsMin, lc);
	        lsMax = glm::max(lsMax, lc);
	    }

	    // Extra Z padding to capture shadow casters behind the visible slice
	    lsMin.z -= 50.0f;

	    // Snap XY to texel-sized increments to suppress shimmering as the camera moves
	    float worldUnitsPerTexel = (lsMax.x - lsMin.x) / static_cast<float>(kCSMSize);
	    if (worldUnitsPerTexel > 0.0f)
	    {
	        lsMin.x = std::floor(lsMin.x / worldUnitsPerTexel) * worldUnitsPerTexel;
	        lsMin.y = std::floor(lsMin.y / worldUnitsPerTexel) * worldUnitsPerTexel;
	        lsMax.x = std::ceil (lsMax.x / worldUnitsPerTexel) * worldUnitsPerTexel;
	        lsMax.y = std::ceil (lsMax.y / worldUnitsPerTexel) * worldUnitsPerTexel;
	    }

	    // Use RH_ZO so DX11 sees depth in [0,1] natively
	    // glm::orthoRH_ZO expects POSITIVE near/far DISTANCES. In RH light-view space the cascade
	    // corners sit in front of the eye at NEGATIVE z, so near = -lsMax.z, far = -lsMin.z. Passing
	    // raw signed lsMin.z/lsMax.z mapped casters to ndc_z far outside [0,1] → all shadow casters
	    // were depth-clipped and the shadow map came out empty (no shadows rendered at all).
	    glm::mat4 lightProj = glm::orthoRH_ZO(lsMin.x, lsMax.x, lsMin.y, lsMax.y, -lsMax.z, -lsMin.z);
	    return lightProj * lightView;
	}

	void Scene::ShadowPass(const std::vector<DirLightData>& dirLights,
	                        const glm::mat4& cameraVP, float cameraNear, float cameraFar)
	{
	    Renderer3D::SetShadowsEnabled(false);
	    if (!m_UseShadows || dirLights.empty()) return;

	    const glm::vec3 lightDir = glm::normalize(dirLights[0].Direction);

	    // Unproject camera frustum corners from NDC (RH_ZO: Z ∈ [0,1])
	    glm::mat4 invVP = glm::inverse(cameraVP);
	    glm::vec3 nearCorners[4], farCorners[4];
	    int ni = 0, fi = 0;
	    for (float x : {-1.0f, 1.0f})
	        for (float y : {-1.0f, 1.0f})
	        {
	            auto unproj = [&](float z) {
	                glm::vec4 p = invVP * glm::vec4(x, y, z, 1.0f);
	                return glm::vec3(p) / p.w;
	            };
	            nearCorners[ni++] = unproj(0.0f);
	            farCorners [fi++] = unproj(1.0f);
	        }

	    // For each cascade, compute what fraction of the frustum it covers
	    const float fullRange = cameraFar - cameraNear;
	    float prevFar = cameraNear;

	    glm::mat4  lightVPs[kNumCascades];
	    glm::vec3  splits;

	    for (int c = 0; c < kNumCascades; ++c)
	    {
	        float cascadeFarWorld  = std::min(kCascadeFar[c], cameraFar);
	        float tNear = (prevFar         - cameraNear) / fullRange;
	        float tFar  = (cascadeFarWorld - cameraNear) / fullRange;
	        tFar  = std::min(tFar,  1.0f);

	        lightVPs[c] = FitCascade(nearCorners, farCorners, tNear, tFar, lightDir);
	        splits[c]   = cascadeFarWorld; // world-space distance threshold

	        // Cull shadow casters against this cascade's light frustum so we don't rasterize
	        // every submesh of every model into all 3 cascades (the dominant import-FPS cost).
	        Frustum cascadeFrustum;
	        cascadeFrustum.ExtractFromVP(lightVPs[c]);

	        Renderer3D::BeginCSMPass(c, lightVPs[c]);
	        {
	            auto view = m_Registry.view<TransformComponent, MeshComponent>();
	            for (auto entity : view)
	            {
	                auto [transform, mesh] = view.get<TransformComponent, MeshComponent>(entity);
	                // Skinned models keep their geometry in SkinnedMeshes (Meshes is empty),
	                // so DrawMeshShadow draws nothing for them here — they're handled below.
	                Renderer3D::DrawMeshShadow(GetRenderTransform(m_Registry, entity, transform), mesh, cascadeFrustum);
	            }

	            // Skinned (animated) meshes: render bone-deformed depth so characters cast
	            // shadows. FinalBoneMatrices were filled by the per-frame animator tick.
	            auto skinnedView = m_Registry.view<TransformComponent, MeshComponent, AnimatorComponent>();
	            for (auto entity : skinnedView)
	            {
	                auto [transform, mesh, anim] =
	                    skinnedView.get<TransformComponent, MeshComponent, AnimatorComponent>(entity);
	                if (mesh.ModelAsset && mesh.ModelAsset->HasSkeleton())
	                    Renderer3D::DrawSkinnedMeshShadow(GetRenderTransform(m_Registry, entity, transform),
	                                                      mesh, anim.FinalBoneMatrices, lightVPs[c]);
	            }
	        }
	        Renderer3D::EndCSMPass();

	        prevFar = cascadeFarWorld;
	    }

	    // After all cascades, restore the main render target
	    Renderer3D::GetCSM()->UnbindForWriting();

	    Renderer3D::BindCSM(lightVPs, splits);
	}

	void Scene::UpdateSpringArmCameras(float deltaTime)
	{
		auto view = m_Registry.view<TransformComponent, SpringArmComponent>();
		for (auto e : view)
		{
			auto [pivotXform, arm] = view.get<TransformComponent, SpringArmComponent>(e);
			Entity camEntity;
			if (arm.TargetCameraUUID != 0)
				camEntity = GetEntityByUUID(arm.TargetCameraUUID);
			if (!camEntity)
				camEntity = EnsurePrimaryCamera();
			if (!camEntity || !camEntity.HasComponent<TransformComponent>())
				continue;

			glm::vec3 pivotWorld = glm::vec3(pivotXform.GetTransform() * glm::vec4(arm.PivotOffset, 1.0f));
			float targetYaw = arm.InheritYaw ? pivotXform.Rotation.y : 0.0f;
			float totalYaw = targetYaw + glm::radians(arm.Yaw);
			float pitchRad = glm::radians(arm.Pitch);

			glm::vec3 cameraForward = glm::normalize(glm::vec3(
				std::cos(pitchRad) * std::sin(totalYaw),
				std::sin(pitchRad),
				-std::cos(pitchRad) * std::cos(totalYaw)
			));
			glm::vec3 targetPos = pivotWorld - cameraForward * glm::max(0.0f, arm.ArmLength) + arm.SocketOffset;

			auto& camXform = camEntity.GetComponent<TransformComponent>();
			if (arm.EnableLag && deltaTime > 0.0f)
			{
				float alpha = 1.0f - std::exp(-arm.PositionLagSpeed * deltaTime);
				camXform.Translation = glm::mix(camXform.Translation, targetPos, alpha);
			}
			else
			{
				camXform.Translation = targetPos;
			}

			glm::vec3 toTarget = pivotWorld - camXform.Translation;
			if (glm::dot(toTarget, toTarget) > 0.000001f)
			{
				toTarget = glm::normalize(toTarget);
				glm::vec3 up = std::abs(glm::dot(toTarget, glm::vec3(0.0f, 1.0f, 0.0f))) > 0.98f
					? glm::vec3(0.0f, 0.0f, -1.0f)
					: glm::vec3(0.0f, 1.0f, 0.0f);
				camXform.Rotation = glm::eulerAngles(glm::quatLookAtRH(toTarget, up));
			}
		}
	}

	// ─── Update entry points ───────────────────────────────────────────────────

	void Scene::OnUpdateEditor(Timestep deltaTime, EditorCamera& camera)
	{
		m_ElapsedTime += (float)deltaTime;
		float dt = (float)deltaTime;

		UpdateSpringArmCameras(dt);

		// Animator: advance animation time and compute bone matrices
		{
			auto animView = m_Registry.view<AnimatorComponent, MeshComponent>();
			for (auto e : animView)
			{
				auto [anim, mesh] = animView.get<AnimatorComponent, MeshComponent>(e);
				if (!anim.SkelData && mesh.ModelAsset)
					anim.SkelData = mesh.ModelAsset->SkelData;
				if (!anim.SkelData || !anim.SkelData->Skel) continue;

				// No clip to play → show the rest/bind pose (not identity, which collapses
				// the mesh). Covers clip-less rigs and meshes before their clip is assigned.
				if (anim.SkelData->Clips.empty())
				{
					Animator::ComputeBindPose(*anim.SkelData->Skel, anim.FinalBoneMatrices);
					continue;
				}

				anim.CurrentClipIndex = std::clamp(anim.CurrentClipIndex, 0, (int)anim.SkelData->Clips.size() - 1);
				const AnimationClip& clip = anim.SkelData->Clips[anim.CurrentClipIndex];
				Animator::Update(anim.Playing ? dt : 0.0f, anim.CurrentTime, anim.Loop, anim.SpeedScale,
				                 clip, *anim.SkelData->Skel, anim.FinalBoneMatrices);
			}
		}

		// Time of Day: advance time and push sky + fog params before rendering
		if (m_UseTimeOfDay && m_UseSkybox && m_Skybox)
		{
			glm::vec3 todSunDir;
			float     todAmbient;
			m_TimeOfDay.Update((float)deltaTime, *m_Skybox, m_Fog, todSunDir, todAmbient);
			// Override the first directional light's direction and ambient from ECS
			auto view = m_Registry.view<DirectionalLightComponent>();
			for (auto e : view)
			{
				auto& dlc = view.get<DirectionalLightComponent>(e);
				dlc.Direction = -todSunDir; // TimeOfDay gives toward-sun; component stores toward-scene
				dlc.Ambient   = glm::vec3(todAmbient);
				break; // only drive the first dir light
			}
		}

		if (m_UsePostProcess && m_PostProcess)
			m_PostProcess->Begin();

		Render2DPass(camera, deltaTime);
		Render3DPass(camera);

		if (m_UsePostProcess && m_PostProcess)
		{
			m_PostProcess->SSAOProjection    = camera.GetProjectionMatrix();
			m_PostProcess->SSAOInvProjection = glm::inverse(camera.GetProjectionMatrix());
			m_PostProcess->Submit();
		}
	}
	void Scene::OnUpdateRuntime(Timestep deltaTime)
	{
		m_ElapsedTime += (float)deltaTime;
		float dt = (float)deltaTime;

		GpuParticleSystem::Get().OnUpdate(dt); // advance instanced particle sim once per frame
		Renderer3D::ClearDynamicLights();      // reset transient lights; actor ticks re-add this frame

		// Animate skeletal meshes
		{
			auto animView = m_Registry.view<AnimatorComponent, MeshComponent>();
			for (auto e : animView)
			{
				auto [anim, mesh] = animView.get<AnimatorComponent, MeshComponent>(e);
				if (!anim.SkelData && mesh.ModelAsset)
					anim.SkelData = mesh.ModelAsset->SkelData;
				if (!anim.SkelData || !anim.SkelData->Skel) continue;

				// No clip to play → show the rest/bind pose (not identity, which collapses
				// the mesh). Covers clip-less rigs and meshes before their clip is assigned.
				if (anim.SkelData->Clips.empty())
				{
					Animator::ComputeBindPose(*anim.SkelData->Skel, anim.FinalBoneMatrices);
					continue;
				}

				anim.CurrentClipIndex = std::clamp(anim.CurrentClipIndex, 0, (int)anim.SkelData->Clips.size() - 1);
				const AnimationClip& clip = anim.SkelData->Clips[anim.CurrentClipIndex];
				Animator::Update(anim.Playing ? dt : 0.0f, anim.CurrentTime, anim.Loop, anim.SpeedScale,
				                 clip, *anim.SkelData->Skel, anim.FinalBoneMatrices);
			}
		}

		m_ActorSystem->Tick(dt);
		if (m_GameMode)
			m_GameMode->Tick(dt);

		{
			const int32_t velocityIterations = 6;
			const int32_t positionIterations = 2;

			m_PhysicsWorld->Step(deltaTime, velocityIterations, positionIterations);
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

		// ── CharacterVirtual update (before rigid-body step) ─────────────────
		if (m_Physics3DWorld && m_Physics3DWorld->IsValid())
		{
			const JPH::Vec3 gravity(0.0f, -9.81f, 0.0f);
			auto bpFilter  = m_Physics3DWorld->GetPhysicsSystem()->GetDefaultBroadPhaseLayerFilter(Physics3DLayers::MOVING);
			auto layFilter = m_Physics3DWorld->GetPhysicsSystem()->GetDefaultLayerFilter(Physics3DLayers::MOVING);
			JPH::BodyFilter  bodyFilter;
			JPH::ShapeFilter shapeFilter;

			auto charView = m_Registry.view<TransformComponent, CharacterControllerComponent>();
			for (auto e : charView)
			{
				auto&& [tc, ccc] = charView.get<TransformComponent, CharacterControllerComponent>(e);
				// Lazy-create CharacterVirtual if BeginPlay added the component after OnPhysics3DStart
				if (!ccc._RuntimeCharacter)
				{
					Entity entity = { e, this };
					std::string status;
					ccc._RuntimeCharacter = CreateRuntimeCharacter(entity, tc, ccc, m_Physics3DWorld, status);
					if (!ccc._RuntimeCharacter)
					{
						m_Physics3DBodyCreationFailureCount++;
						BLU_CORE_WARN("CharacterVirtual lazy creation failed: {0}", status);
					}
				}
				if (!ccc._RuntimeCharacter) continue;

				auto* character = static_cast<JPH::CharacterVirtual*>(ccc._RuntimeCharacter);

				bool grounded = (character->GetGroundState() == JPH::CharacterBase::EGroundState::OnGround);
				ccc.IsGrounded = grounded;

				// Horizontal velocity: from pending move input (already speed-scaled)
				JPH::Vec3 currentVel = character->GetLinearVelocity();
				float     vertVel    = currentVel.GetY() + gravity.GetY() * dt;

				if (grounded)
				{
					vertVel = std::max(0.0f, vertVel);
					if (ccc._PendingJump)
					{
						vertVel          = ccc.JumpImpulse;
						ccc._PendingJump = false;
					}
				}
				else
				{
					ccc._PendingJump = false;
				}

				character->SetLinearVelocity(JPH::Vec3(
					ccc._PendingMoveInput.x,
					vertVel,
					ccc._PendingMoveInput.z));

				ccc._PendingMoveInput = { 0.0f, 0.0f, 0.0f };

				JPH::CharacterVirtual::ExtendedUpdateSettings updateSettings;
				updateSettings.mStickToFloorStepDown = JPH::Vec3(0.0f, -0.5f, 0.0f);
				updateSettings.mWalkStairsStepUp      = JPH::Vec3(0.0f, ccc.StepHeight, 0.0f);

				character->ExtendedUpdate(dt, gravity, updateSettings,
					bpFilter, layFilter, bodyFilter, shapeFilter,
					*m_Physics3DWorld->GetTempAllocator());

				// Sync position back to transform
				JPH::RVec3 pos = character->GetPosition();
				tc.Translation = glm::vec3(
					static_cast<float>(pos.GetX()),
					static_cast<float>(pos.GetY()),
					static_cast<float>(pos.GetZ()));

				// Cache velocity for ACharacter::GetVelocity()
				JPH::Vec3 vel = character->GetLinearVelocity();
				ccc.Velocity = glm::vec3(vel.GetX(), vel.GetY(), vel.GetZ());
			}
		}

		// ── 3D Physics step + transform sync ─────────────────────────────────
		if (m_Physics3DWorld && m_Physics3DWorld->IsValid())
		{
			// Kinematic bodies: NativeScript already updated TransformComponent, now tell Jolt
			{
				auto kinView = m_Registry.view<TransformComponent, Rigidbody3DComponent>();
				for (auto e : kinView)
				{
					auto&& [tc, rb] = kinView.get<TransformComponent, Rigidbody3DComponent>(e);
					if (rb.RuntimeBodyID != UINT32_MAX && rb.Type == Rigidbody3DComponent::BodyType::Kinematic)
						m_Physics3DWorld->MoveKinematic(rb.RuntimeBodyID, tc.Translation, glm::quat(tc.Rotation), (float)deltaTime);
				}
			}

			m_Physics3DWorld->Step(deltaTime);

			auto view3D = m_Registry.view<TransformComponent, Rigidbody3DComponent>();
			for (auto e : view3D)
			{
				auto&& [tc, rb] = view3D.get<TransformComponent, Rigidbody3DComponent>(e);
				if (rb.RuntimeBodyID != UINT32_MAX && rb.Type == Rigidbody3DComponent::BodyType::Dynamic)
				{
					glm::vec3 pos;
					glm::quat rot;
					m_Physics3DWorld->GetTransform(rb.RuntimeBodyID, pos, rot);
					tc.Translation = pos;
					tc.Rotation    = glm::eulerAngles(rot);
				}
			}
		}

		// ── Eject override: render from editor camera, skip game camera ─────────
		UpdateSpringArmCameras(dt);

		if (m_EjectCamera)
		{
			if (m_UsePostProcess && m_PostProcess)
				m_PostProcess->Begin();
			Render2DPass(*m_EjectCamera, deltaTime);
			Render3DPass(*m_EjectCamera);
			if (m_UsePostProcess && m_PostProcess)
				m_PostProcess->Submit();
			RenderRuntimeUI();
			AudioEngine::Get().OnUpdate();
			return;
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
			if (m_UsePostProcess && m_PostProcess)
				m_PostProcess->Begin();

			Render2DPass(*mainCamera, cameraTransform, deltaTime, true);
			Render3DPass(*mainCamera, cameraTransform);

			if (m_UsePostProcess && m_PostProcess)
				m_PostProcess->Submit();
			RenderRuntimeUI();

			// Update 3D audio listener to match the primary camera.
			glm::vec3 camPos = glm::vec3(cameraTransform[3]);
			glm::vec3 camFwd = -glm::vec3(cameraTransform[2]); // RH: -Z is forward
			glm::vec3 camUp  =  glm::vec3(cameraTransform[1]);
			AudioEngine::Get().SetListenerTransform(camPos, camFwd, camUp);
		}

		// Update spatial audio source positions.
		m_Registry.view<AudioSourceComponent, TransformComponent>().each(
			[](auto /*entity*/, AudioSourceComponent& asc, const TransformComponent& tc)
			{
				if (asc._RuntimeHandle != kInvalidSound && asc.Spatial)
					AudioEngine::Get().SetSoundPosition(asc._RuntimeHandle, tc.Translation);
			});
		AudioEngine::Get().OnUpdate();
	}
	void Scene::OnUpdatePaused(Timestep deltaTime)
	{
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
			if (m_UsePostProcess && m_PostProcess)
				m_PostProcess->Begin();

			Render2DPass(*mainCamera, cameraTransform, deltaTime, false);
			Render3DPass(*mainCamera, cameraTransform);

			if (m_UsePostProcess && m_PostProcess)
				m_PostProcess->Submit();
		}
	}
	void Scene::OnSceneStep(int frames)
	{
		m_StepFrames = frames;
		OnUpdateStep();
	}

	void Scene::OnUpdateStep()
	{
		// This function should be called when you want to advance the game by one step/frame.

		// You can implement logic to pause the game if you've reached the desired step.
		if (m_StepFrames <= 0) {
			// Pause the game or take any other action.
			return;
		}
		while (m_StepFrames > 0)
		{
			OnUpdateRuntime(1);
			m_StepFrames--;

		}
	}

}
