#pragma once
#include <glm/glm.hpp>
#include "Blu/Scene/SceneCamera.h"
#include "Blu/Core/Timestep.h"
#include "Blu/Core/Core.h"
#include "glm/gtc/matrix_transform.hpp"
#include "Blu/Rendering/ParticleSystem.h"
#include "Blu/Core/UUID.h"
#include "Blu/Rendering/Material.h"
#include "Blu/Rendering/Mesh.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace Blu
{
	class ScriptableEntity;
	class Entity;
	
	struct IDComponent
	{
		UUID ID;

		IDComponent() = default;
		IDComponent(const IDComponent&) = default;
		IDComponent(UUID& uuid) : ID(uuid) {}

	};
	struct TransformComponent
	{
		glm::vec3 Translation = { 0.0f, 0.0f, 0.0f };
		glm::vec3 Rotation = { 0.0f, 0.0f, 0.0f };
		glm::vec3 Scale = { 1.0f, 1.0f, 1.0f };
		TransformComponent() = default;
		TransformComponent(const TransformComponent&) = default;
		TransformComponent(const glm::vec3& translation)
			:Translation(translation) {}

		glm::mat4 GetTransform() const 
		{
			glm::mat4 rotation = glm::toMat4(glm::quat(Rotation));
			
			return glm::translate(glm::mat4(1.0f), Translation)
				* rotation
				* glm::scale(glm::mat4(1.0f), Scale);
		}
		
	};

	struct ParticleSystemComponent
	{
		std::function<void()> CurrentParticleSystem; // function will call the correct particle system template
		Particle ParticleAttributes;
		Entity* AttachedEntity = nullptr;
		ParticleProps ParticleSystemProps;
		ParticleSystem PSystem;
		ParticleSystemComponent() = default;
		ParticleSystemComponent(const ParticleSystemComponent&) = default;
		ParticleSystemComponent(const glm::mat4& attachTransform) {}
		bool operator==(const ParticleSystemComponent& other) const {
			
			return ParticleAttributes.ColorBegin == other.ParticleAttributes.ColorBegin;
		}
		void Update(float deltaTime)
		{
			if (CurrentParticleSystem)
			{
				CurrentParticleSystem();
			}


			PSystem.OnUpdate(deltaTime);
			PSystem.OnRender();
		}
			
	};

	struct SpriteRendererComponent 
	{
		glm::vec4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };

		
		Shared<class Material> MaterialInstance;  

		SpriteRendererComponent()
		{
			MaterialInstance = Material::Create();

		}
		SpriteRendererComponent(const SpriteRendererComponent&) = default;
		SpriteRendererComponent(const glm::vec4& color)
			:Color(color) 
		{
			MaterialInstance = Material::Create();

		}
		
	};

	struct CircleRendererComponent
	{
		glm::vec4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
		float Radius = 0.5f;
		float Thickness = 1.0f;
		float Fade = 0.005f;

		CircleRendererComponent() = default;
		CircleRendererComponent(const CircleRendererComponent&) = default;
		CircleRendererComponent(const glm::vec4& color)
			:Color(color) {}

	};

	struct MeshComponent
	{
		Shared<class Mesh> MeshData;
		Shared<class Material> MaterialInstance;
		Shared<Model> ModelAsset;
		std::string FilePath;

		MeshComponent() = default;
		MeshComponent(const MeshComponent&) = default;
	};

	struct TagComponent 
	{
		std::string Tag;
		TagComponent() = default;
		TagComponent(const TagComponent&) = default;
		TagComponent(const std::string& tag)
			:Tag(tag) {}

	};

	struct CameraComponent 
	{
		Blu::SceneCamera Camera;
		bool Primary = false;
		bool FixedAspectRatio = false;

		CameraComponent() = default;
		CameraComponent(const CameraComponent&) = default;
		
	};
	struct ScriptComponent
	{
		std::string Name;

		ScriptComponent() = default;
		ScriptComponent(const ScriptComponent&) = default;
		
	};
	struct NativeScriptComponent 
	{

		ScriptableEntity* Instance = nullptr;
		
		ScriptableEntity*(*InstantiateScript)();
		void (*DestroyScript)(NativeScriptComponent*);

		
		template<typename T>
		void Bind()
		{
			InstantiateScript = []() {return static_cast<ScriptableEntity*>(new T()); };
			DestroyScript = [](NativeScriptComponent* nsc) {delete nsc->Instance; nsc->Instance = nullptr; };
		}
	};

	struct Rigidbody2DComponent
	{
		enum class BodyType {Static = 0, Dynamic, Kinematic};

		BodyType Type = BodyType::Static;
		bool FixedRotation = false;

		void* RuntimeBody = nullptr;

		Rigidbody2DComponent() = default;
		Rigidbody2DComponent(const Rigidbody2DComponent& other) = default;
	};

	struct BoxCollider2DComponent
	{
		glm::vec2 Offset = { 0.0f, 0.0f };
		glm::vec2 Size = { 1.0f, 1.0f };

		float Density = 1.0f;
		float Friction = 0.5f;
		float Restitution = 0.1f;
		float RestitutionThreshold = 0.5f;
		bool ShowCollision = false;
		void* RuntimeFixture = nullptr;

		BoxCollider2DComponent() = default;
		BoxCollider2DComponent(const BoxCollider2DComponent& other) = default;
		
	};

	struct CircleCollider2DComponent
	{
		glm::vec2 Offset = { 0.0f, 0.0f };
		float Radius = 1.0f;

		float Density = 1.0f;
		float Friction = 0.5f;
		float Restitution = 0.1f;
		float RestitutionThreshold = 0.5f;
		bool ShowCollision = false;

		void* RuntimeFixture = nullptr;

		CircleCollider2DComponent() = default;
		CircleCollider2DComponent(const CircleCollider2DComponent& other) = default;
	};

	// ─── Point Light ──────────────────────────────────────────────────────────
	// Position is taken from the entity's TransformComponent::Translation.
	// Attenuation formula: att = 1 / (Constant + Linear*d + Quadratic*d²)
	// Typical presets (d = range in world units):
	//   Range 7    → Constant 1.0 / Linear 0.7  / Quadratic 1.8
	//   Range 50   → Constant 1.0 / Linear 0.09 / Quadratic 0.032
	//   Range 200  → Constant 1.0 / Linear 0.022/ Quadratic 0.0019
	struct PointLightComponent
	{
		glm::vec3 Ambient   = glm::vec3(0.05f, 0.05f, 0.05f);
		glm::vec3 Diffuse   = glm::vec3(1.00f, 1.00f, 1.00f);
		glm::vec3 Specular  = glm::vec3(1.00f, 1.00f, 1.00f);
		float     Intensity    = 1.0f;
		float     Range        = 20.0f;
		// Attenuation coefficients
		float     AttConstant  = 1.0f;
		float     AttLinear    = 0.09f;
		float     AttQuadratic = 0.032f;

		PointLightComponent() = default;
		PointLightComponent(const PointLightComponent&) = default;
	};

	// ─── Directional Light ────────────────────────────────────────────────────
	// Infinite-distance source (sun / sky).  No position, no attenuation.
	struct DirectionalLightComponent
	{
		glm::vec3 Direction = glm::normalize(glm::vec3(-0.2f, -1.0f, -0.3f));
		glm::vec3 Ambient   = glm::vec3(0.10f, 0.10f, 0.10f);
		glm::vec3 Diffuse   = glm::vec3(0.80f, 0.80f, 0.80f);
		glm::vec3 Specular  = glm::vec3(0.50f, 0.50f, 0.50f);
		float     Intensity = 1.0f;

		DirectionalLightComponent() = default;
		DirectionalLightComponent(const DirectionalLightComponent&) = default;
	};

	// ─── Spot Light ───────────────────────────────────────────────────────────
	// Position comes from TransformComponent::Translation.
	// InnerConeAngle / OuterConeAngle in degrees; shader pre-computes cos values.
	// Smooth falloff between inner (full intensity) and outer (zero) edges.
	struct SpotLightComponent
	{
		glm::vec3 Ambient        = glm::vec3(0.00f, 0.00f, 0.00f);
		glm::vec3 Diffuse        = glm::vec3(1.00f, 1.00f, 1.00f);
		glm::vec3 Specular       = glm::vec3(1.00f, 1.00f, 1.00f);
		glm::vec3 Direction      = glm::vec3(0.0f, -1.0f, 0.0f);
		float     Intensity      = 1.0f;
		float     Range          = 30.0f;
		float     InnerConeAngle = 12.5f;   // degrees
		float     OuterConeAngle = 17.5f;   // degrees
		float     AttConstant    = 1.0f;
		float     AttLinear      = 0.09f;
		float     AttQuadratic   = 0.032f;

		SpotLightComponent() = default;
		SpotLightComponent(const SpotLightComponent&) = default;
	};

	template<typename... Component>
	struct Components
	{

	};
	using AllComponents =
		Components<TransformComponent, ParticleSystemComponent, SpriteRendererComponent, CircleRendererComponent,
		CircleCollider2DComponent, BoxCollider2DComponent, CameraComponent,
		ScriptComponent, NativeScriptComponent, Rigidbody2DComponent,
		PointLightComponent, DirectionalLightComponent, SpotLightComponent, MeshComponent>;




}