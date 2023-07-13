#pragma once
#include <glm/glm.hpp>
#include "Blu/Scene/SceneCamera.h"
#include "Blu/Core/Timestep.h"
#include "Blu/Core/Core.h"
#include "glm/gtc/matrix_transform.hpp"
#include "Blu/Rendering/ParticleSystem.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace Blu
{
	class ScriptableEntity;
	class Entity;
	
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

		Shared<class Texture2D> Texture;
		SpriteRendererComponent() = default;
		SpriteRendererComponent(const SpriteRendererComponent&) = default;
		SpriteRendererComponent(const glm::vec4& color)
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
}