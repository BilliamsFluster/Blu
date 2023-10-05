#pragma once
#include <glm/glm.hpp>
#include "Blu/Scene/SceneCamera.h"
#include "Blu/Core/Timestep.h"
#include "Blu/Core/Core.h"
#include "glm/gtc/matrix_transform.hpp"
#include "Blu/Rendering/ParticleSystem.h"
#include "Blu/Core/UUID.h"
#include "Blu/Rendering/Material.h"

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

	struct PointLightComponent
	{
		glm::vec3 Position{ 0.0f, 0.0f, 0.0f };
		glm::vec4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
		float Intensity = 1.0f;
		float Radius = 1.0f;

		// Additional properties for Phong reflection model
		glm::vec3 AmbientColor{ 0.1f, 0.1f, 0.1f }; // Ambient light color
		glm::vec3 DiffuseColor{ 0.8f, 0.8f, 0.8f }; // Diffuse light color
		glm::vec3 SpecularColor{ 1.0f, 1.0f, 1.0f }; // Specular light color
		float Shininess = 32.0f; // Shininess factor for specular reflection

		PointLightComponent() = default;
		PointLightComponent(const glm::vec3& position, const glm::vec4& color, float intensity, float radius)
			: Position(position), Color(color), Intensity(intensity), Radius(radius) {}
	};

	struct DirectionalLightComponent
	{
		glm::vec3 Direction = glm::vec3(0.0f, -1.0f, 0.0f); // Default direction is downward
		glm::vec4 Color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f); // Default color is white
		float Intensity = 1.0f; // Default intensity is 1.0

		DirectionalLightComponent() = default;

		DirectionalLightComponent(const glm::vec3& direction, const glm::vec4& color, float intensity)
			: Direction(direction), Color(color), Intensity(intensity) {}
	};
	struct SpotlightComponent
	{

	};

	template<typename... Component>
	struct Components
	{

	};
	using AllComponents =
		Components<TransformComponent, ParticleSystemComponent, SpriteRendererComponent, CircleRendererComponent,
		CircleCollider2DComponent, BoxCollider2DComponent, CameraComponent,
		ScriptComponent, NativeScriptComponent, Rigidbody2DComponent, PointLightComponent>;




}