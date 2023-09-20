#include "Blupch.h"
#include "SceneSerializer.h"
#include "Entity.h"
#include "Component.h"
#include <fstream>
#include <filesystem>
#include "yaml-cpp/yaml.h"
#include "Blu/Scripting/ScriptEngine.h"
#include "Blu/Rendering/Texture.h"

namespace YAML
{
	template<>
	struct YAML::convert<glm::vec2>
	{
		static Node encode(const glm::vec2& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			return node;
		}
		static bool decode(const Node& node, glm::vec2& rhs)
		{
			if (!node.IsSequence() || node.size() != 2)
			{
				return false;
			}
			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			return true;
		}
	};
	template<>
	struct convert<glm::vec3>
	{
		static Node encode(const glm::vec3& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			return node;
		}
		static bool decode(const Node& node, glm::vec3& rhs)
		{
			if (!node.IsSequence() || node.size() != 3)
			{
				return false;
			}
			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			rhs.z = node[2].as<float>();
			return true;
			
		}
	};

	template<>
	struct convert<glm::vec4>
	{
		static Node encode(const glm::vec4& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			node.push_back(rhs.w);
			return node;
		}
		static bool decode(const Node& node, glm::vec4& rhs)
		{
			if (!node.IsSequence() || node.size() != 4)
			{
				return false;
			}
			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			rhs.z = node[2].as<float>();
			rhs.w = node[3].as<float>();
			return true;

		}
	};
}



namespace Blu
{
	YAML::Emitter& operator <<(YAML::Emitter& out, const glm::vec2& v)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << YAML::EndSeq;
		return out;
	}

	YAML::Emitter& operator <<(YAML::Emitter& out, const glm::vec3& v)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
		return out;
	}
	YAML::Emitter& operator <<(YAML::Emitter& out, const glm::vec4& v)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
		return out; 
	}
	YAML::Emitter& operator <<(YAML::Emitter& out, const ParticleProps& pProps)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq <<  pProps.Position << pProps.Velocity << pProps.LifeTime << pProps.ColorBegin << 
			pProps.ColorEnd << pProps.SizeBegin << pProps.SizeEnd << pProps.SizeVariation << YAML::EndSeq;
		return out;
	}
	SceneSerializer::SceneSerializer(const Shared<Scene>& scene)
		:m_Scene(scene)
	{
	}
	static  void SerializeEntity(YAML::Emitter& out, Entity entity)
	{ 
		BLU_CORE_ASSERT("", entity.HasComponent<IDComponent>());
		out << YAML::BeginMap;
		out << YAML::Key << "Entity" << YAML::Value << entity.GetUUID();

		if (entity.HasComponent<TagComponent>())
		{
			out << YAML::Key << "TagComponent";
			out << YAML::BeginMap;

			auto& tag = entity.GetComponent<TagComponent>().Tag;
			out << YAML::Key << "Tag" << YAML::Value << tag;
			out << YAML::EndMap;

		}
		/*if (entity.HasComponent<NativeScriptComponent>())
		{
			out << YAML::Key << "NativeScriptComponent";
			out << YAML::BeginMap;

			auto& instance = entity.GetComponent<NativeScriptComponent>().Instance;
			out << YAML::Key << "Tag" << YAML::Value << instance;
			out << YAML::EndMap;

		}*/

		if (entity.HasComponent<ScriptComponent>())
		{
			out << YAML::Key << "ScriptComponent";
			out << YAML::BeginMap;
			auto& sc = entity.GetComponent<ScriptComponent>();
			out << YAML::Key << "Name" << YAML::Value << sc.Name;
			

			// we need to serialize the fields 
			Shared<ScriptClass> scriptClass = ScriptEngine::GetEntityScriptClass(sc.Name);


			Shared<ScriptInstance> scriptInstance = ScriptEngine::GetEntityScriptInstance(entity.GetUUID());

			if (scriptClass)
			{
				if (scriptInstance)
				{
					out << YAML::Key << "ScriptFields" << YAML::Value;
					out << YAML::BeginMap; // script fields
					auto& fields = scriptClass->GetScriptFields();
					for (const auto& [name, field] : fields)
					{
						if (field.Type == ScriptFieldType::Float)
						{
							float data = scriptInstance->GetFieldValue<float>(name);
							out << YAML::Key << name << YAML::Value << data;

						}
					}
					out << YAML::EndMap;
				}
			}
			out << YAML::EndMap; // script component

		}
		if (entity.HasComponent<CameraComponent>())
		{
			out << YAML::Key << "CameraComponent";
			out << YAML::BeginMap;

			auto& cc = entity.GetComponent<CameraComponent>();
			auto& camera = cc.Camera;
			out << YAML::Key << "Camera" << YAML::Value;
			out << YAML::BeginMap;
			out << YAML::Key << "ProjectionType" << YAML::Value << (int)camera.GetProjectionType();
			out << YAML::Key << "OrthographicFar" << YAML::Value << camera.GetOrthographicFarClip();
			out << YAML::Key << "OrthographicNear" << YAML::Value << camera.GetOrthographicNearClip();
			out << YAML::Key << "OrthographicSize"  << YAML::Value << camera.GetOrthographicSize();
			out << YAML::Key << "PerspectiveFar" << YAML::Value << camera.GetPerspectiveFar();
			out << YAML::Key << "PerspectiveNear" << YAML::Value << camera.GetPerspectiveNear();
			out << YAML::Key << "PerspectiveFOV" << YAML::Value << camera.GetPerspectiveFOV();
			out << YAML::EndMap;
			out << YAML::Key << "Primary" << YAML::Value << cc.Primary;
			out << YAML::Key << "FixedAspectRatio" << YAML::Value << cc.FixedAspectRatio;
			out << YAML::EndMap;

		}
		if (entity.HasComponent<TransformComponent>())
		{
			out << YAML::Key << "TransformComponent";
			out << YAML::BeginMap;

			auto& tc = entity.GetComponent<TransformComponent>();
			out << YAML::Key << "Translation" << YAML::Value << tc.Translation;
			out << YAML::Key << "Rotation" << YAML::Value << tc.Rotation;
			out << YAML::Key << "Scale" << YAML::Value << tc.Scale;
			out << YAML::EndMap;

		}
		if (entity.HasComponent<SpriteRendererComponent>())
		{
			out << YAML::Key << "SpriteRendererComponent";
			out << YAML::BeginMap;

			auto& src = entity.GetComponent<SpriteRendererComponent>();
			out << YAML::Key << "Color" << YAML::Value << src.Color;
			if(src.Texture)
				out << YAML::Key << "TexturePath" << YAML::Value << src.Texture->GetTexturePath();

			out << YAML::EndMap;

		}
		if (entity.HasComponent<CircleRendererComponent>())
		{
			out << YAML::Key << "CircleRendererComponent";
			out << YAML::BeginMap;

			auto& crc = entity.GetComponent<CircleRendererComponent>();
			out << YAML::Key << "Color" << YAML::Value << crc.Color;
			out << YAML::Key << "Radius" << YAML::Value << crc.Radius;
			out << YAML::Key << "Thickness" << YAML::Value << crc.Thickness;
			out << YAML::Key << "Fade" << YAML::Value << crc.Fade;

			out << YAML::EndMap;
		}

		if (entity.HasComponent<Rigidbody2DComponent>())
		{
			out << YAML::Key << "Rigidbody2DComponent";
			out << YAML::BeginMap;

			auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
			out << YAML::Key << "BodyType" << YAML::Value << static_cast<int>(rb2d.Type);
			out << YAML::Key << "FixedRotation" << YAML::Value << rb2d.FixedRotation;

			out << YAML::EndMap;
		}

		if (entity.HasComponent<BoxCollider2DComponent>())
		{
			out << YAML::Key << "BoxCollider2DComponent";
			out << YAML::BeginMap;

			auto& bc2d = entity.GetComponent<BoxCollider2DComponent>();
			out << YAML::Key << "Offset" << YAML::Value << bc2d.Offset;
			out << YAML::Key << "Size" << YAML::Value << bc2d.Size;

			out << YAML::Key << "Density" << YAML::Value << bc2d.Density;
			out << YAML::Key << "Friction" << YAML::Value << bc2d.Friction;
			out << YAML::Key << "Restitution" << YAML::Value << bc2d.Restitution;
			out << YAML::Key << "RestitutionThreshold" << YAML::Value << bc2d.RestitutionThreshold;

			out << YAML::EndMap;
		}
		if (entity.HasComponent<CircleCollider2DComponent>())
		{
			out << YAML::Key << "CircleCollider2DComponent";
			out << YAML::BeginMap;

			auto& cc = entity.GetComponent<CircleCollider2DComponent>();
			out << YAML::Key << "Offset" << YAML::Value << cc.Offset;
			out << YAML::Key << "Radius" << YAML::Value << cc.Radius;
			out << YAML::Key << "Density" << YAML::Value << cc.Density;
			out << YAML::Key << "Friction" << YAML::Value << cc.Friction;
			out << YAML::Key << "Restitution" << YAML::Value << cc.Restitution;
			out << YAML::Key << "RestitutionThreshold" << YAML::Value << cc.RestitutionThreshold;

			out << YAML::EndMap;
		}
		//if (entity.HasComponent<ParticleSystemComponent>())
		//{
		//	out << YAML::Key << "ParticleSystemComponent";
		//	out << YAML::BeginMap;

		//	auto& psc = entity.GetComponent<ParticleSystemComponent>();
		//	//out << YAML::Key << "Particle System" << YAML::Value << psc.PSystem;
		//	out << YAML::Key << "Particle System Props" << YAML::Value << psc.ParticleSystemProps;
		//	out << YAML::Key << "Attached Entity" << YAML::Value << psc.AttachedEntity;
		//	out << YAML::EndMap;

		//}
		out << YAML::EndMap;
	}
	void SceneSerializer::Serialize(const std::string& filepath)
	{
		std::filesystem::create_directories(std::filesystem::path(filepath).remove_filename());

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Scene" << YAML::Value << filepath;
		out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;
		m_Scene->m_Registry.each([&](auto entityID)
			{
				Entity entity = { entityID, m_Scene.get() };
				if (!entity)
					return;
				SerializeEntity(out, entity);
			});
		out << YAML::EndSeq;
		out << YAML::EndMap;

		std::ofstream fout(filepath);
		if (!fout)
		{
			std::cerr << "Failed to create the file at: " << filepath << '\n';
			return;
		}
		fout << out.c_str();

	}
	void SceneSerializer::SerializeBinary(const std::string & filepath)
	{
		
		
	}

	void SceneSerializer::SerializeLoadedScene(const std::string& filepath)
	{
		std::string saveScene = "LoadedScenes\\LoadScene.blu";
		bool createdDirectory = std::filesystem::create_directories(std::filesystem::path(saveScene).remove_filename());
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "LastLoadedScene" << YAML::Value << filepath;
		out << YAML::EndMap;

		std::ofstream fout(saveScene);
		if (!fout)
		{
			std::cerr << "Failed to create the file at: " << filepath << '\n';
			return;
		}
		fout << out.c_str();

	}
	std::string SceneSerializer::DeserializeLoadedScene()
	{
		std::string filepath = "LoadedScenes\\LoadScene.blu";
		std::ifstream stream(filepath);
		std::stringstream strStream;
		strStream << stream.rdbuf();

		YAML::Node data = YAML::Load(strStream.str());
		if (!data["LastLoadedScene"])
			return "noScene";

		std::string sceneName = data["LastLoadedScene"].as<std::string>();
		return sceneName;
		
	}
	bool SceneSerializer::Deserialize(const std::string& filepath)
	{
		std::ifstream stream(filepath);
		std::stringstream strStream;
		strStream << stream.rdbuf();

		YAML::Node data = YAML::Load(strStream.str());
		if (!data["Scene"])
			return false;
		
		std::string sceneName = data["Scene"].as<std::string>();

		auto entities = data["Entities"];
		if (entities)
		{
			for (auto entity : entities)
			{
				uint64_t uuid = entity["Entity"].as<uint64_t>();
				std::string name;

				auto tagComponent = entity["TagComponent"];
				if (tagComponent)
				{
					name = tagComponent["Tag"].as<std::string>();
				}

				Entity deserializedEntity = m_Scene->CreateEntityWithUUID(uuid, name);
				

				auto transformComponent = entity["TransformComponent"];

				if (transformComponent)
				{
					auto& tc = deserializedEntity.GetComponent<TransformComponent>();
					tc.Translation = transformComponent["Translation"].as<glm::vec3>();
					tc.Rotation = transformComponent["Rotation"].as<glm::vec3>();
					tc.Scale = transformComponent["Scale"].as<glm::vec3>();
				}

				auto scriptComponent = entity["ScriptComponent"];
				if (scriptComponent)
				{
					auto& sc = deserializedEntity.AddComponent<ScriptComponent>();
					sc.Name = scriptComponent["Name"].as<std::string>();

				}

				auto cameraComponent = entity["CameraComponent"];
				if (cameraComponent)
				{
					auto& cc = deserializedEntity.AddComponent<CameraComponent>();
		 			auto cameraProps = cameraComponent["Camera"];

					cc.Camera.SetProjectionType((SceneCamera::ProjectionType)cameraProps["ProjectionType"].as<int>());
					cc.Camera.SetOrthographicFarClip(cameraProps["OrthographicFar"].as<float>());
					cc.Camera.SetOrthographicNearClip(cameraProps["OrthographicNear"].as<float>());
					cc.Camera.SetOrthographicSize(cameraProps["OrthographicSize"].as<float>());

					cc.Camera.SetPerspectiveFar(cameraProps["PerspectiveFar"].as<float>());
					cc.Camera.SetPerspectiveNear(cameraProps["PerspectiveNear"].as<float>());
					cc.Camera.SetPerspectiveFOV(cameraProps["PerspectiveFOV"].as<float>());

					
					cc.Primary = cameraComponent["Primary"].as<bool>();
					cc.FixedAspectRatio = cameraComponent["FixedAspectRatio"].as<bool>();




				}

				auto spriteRendererComponent = entity["SpriteRendererComponent"];

				if (spriteRendererComponent)
				{
					auto& src = deserializedEntity.AddComponent<SpriteRendererComponent>();
					src.Color = spriteRendererComponent["Color"].as<glm::vec4>();
					
					// Check if the "TexturePath" key exists in the YAML node
					if (spriteRendererComponent["TexturePath"])
					{
						// Create a new Texture2D instance with the provided path
						std::string texturePath = spriteRendererComponent["TexturePath"].as<std::string>();
						src.Texture = Texture2D::Create(texturePath);
						src.Texture->ConfigureTexture();
					}
					

				}
				
				auto circleRendererComponent = entity["CircleRendererComponent"];

				if (circleRendererComponent)
				{
					auto& crc = deserializedEntity.AddComponent<CircleRendererComponent>();
					crc.Color = circleRendererComponent["Color"].as<glm::vec4>();
					crc.Radius = circleRendererComponent["Radius"].as<float>();
					crc.Thickness = circleRendererComponent["Thickness"].as<float>();
					crc.Fade = circleRendererComponent["Fade"].as<float>();
				}

				
				auto rigidbody2DComponent = entity["Rigidbody2DComponent"];

				if (rigidbody2DComponent)
				{
					auto& rb2d = deserializedEntity.AddComponent<Rigidbody2DComponent>();
					rb2d.Type = static_cast<Rigidbody2DComponent::BodyType>(rigidbody2DComponent["BodyType"].as<int>());
					rb2d.FixedRotation = rigidbody2DComponent["FixedRotation"].as<bool>();
				}

				auto boxCollider2DComponent = entity["BoxCollider2DComponent"];

				if (boxCollider2DComponent)
				{
					auto& bc2d = deserializedEntity.AddComponent<BoxCollider2DComponent>();
					bc2d.Offset = boxCollider2DComponent["Offset"].as<glm::vec2>();
					bc2d.Size = boxCollider2DComponent["Size"].as<glm::vec2>();

					bc2d.Density = boxCollider2DComponent["Density"].as<float>();
					bc2d.Friction = boxCollider2DComponent["Friction"].as<float>();
					bc2d.Restitution = boxCollider2DComponent["Restitution"].as<float>();
					bc2d.RestitutionThreshold = boxCollider2DComponent["RestitutionThreshold"].as<float>();
				}
				auto circleColliderComponent = entity["CircleCollider2DComponent"];

				if (circleColliderComponent)
				{
					auto& cc = deserializedEntity.AddComponent<CircleCollider2DComponent>();
					cc.Offset = circleColliderComponent["Offset"].as<glm::vec2>();
					cc.Radius = circleColliderComponent["Radius"].as<float>();
					cc.Density = circleColliderComponent["Density"].as<float>();
					cc.Friction = circleColliderComponent["Friction"].as<float>();
					cc.Restitution = circleColliderComponent["Restitution"].as<float>();
					cc.RestitutionThreshold = circleColliderComponent["RestitutionThreshold"].as<float>();
				}

			}
		}

		return true;
	}
	
	bool SceneSerializer::DeserializeEntityScriptInstances(const std::string& filepath)
	{
		std::ifstream stream(filepath);
		std::stringstream strStream;
		strStream << stream.rdbuf();

		YAML::Node data = YAML::Load(strStream.str());
		if (!data["Scene"])
			return false;

		std::string sceneName = data["Scene"].as<std::string>();
		auto entities = data["Entities"];
		if (entities)
		{
			for (auto entity : entities)
			{
				uint64_t uuid = entity["Entity"].as<uint64_t>();
				std::string name;



				auto scriptComponent = entity["ScriptComponent"];
				if (scriptComponent)
				{


					Entity entity = m_Scene->GetEntityByUUID(uuid);
					auto& sc = entity.GetComponent<ScriptComponent>();
					Shared<ScriptInstance> scriptInstance = ScriptEngine::GetEntityScriptInstance(uuid);
					sc.Name = scriptComponent["Name"].as<std::string>();
					Shared<ScriptClass> scriptClass = ScriptEngine::GetEntityScriptClass(sc.Name);

					auto scriptFields = scriptComponent["ScriptFields"];
					for (auto it = scriptFields.begin(); it != scriptFields.end(); ++it)
					{
						const std::string& fieldName = it->first.as<std::string>();
						auto fieldValue = it->second; // YAML will convert to appropriate C++ type
						if (scriptInstance)
						{
							auto& fields = scriptClass->GetScriptFields();
							auto fieldIt = fields.find(fieldName);
							if (fieldIt != fields.end() && fieldIt->second.Type == ScriptFieldType::Float)
							{
								float data = fieldValue.as<float>();
								scriptInstance->SetFieldValue<float>(fieldName, data);
							}
						}

						// Handle other field types...
					}


				}
			}
		}
	}
	bool SceneSerializer::DeserializeBinary(const std::string& filepath)
	{
		return false;
	}
}