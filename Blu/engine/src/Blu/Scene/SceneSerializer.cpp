#include "Blupch.h"
#include "SceneSerializer.h"
#include "Entity.h"
#include "Component.h"
#include <fstream>
#include <filesystem>
#include "yaml-cpp/yaml.h"
#include "Blu/Rendering/Texture.h"
#include "Blu/Rendering/ModelLoader.h"
#include "Blu/Rendering/Skybox.h"
#include "Blu/Rendering/TimeOfDay.h"
#include "Blu/Rendering/Renderer3D.h"
#include "Blu/Utils/Helpers.h"
#include "Blu/LightSystem/LightManager.h"


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
		if (entity.HasComponent<NativeScriptComponent>())
		{
			auto& nsc = entity.GetComponent<NativeScriptComponent>();
			out << YAML::Key << "NativeScriptComponent";
			out << YAML::BeginMap;
			out << YAML::Key << "ClassName" << YAML::Value << nsc.ClassName;
			out << YAML::EndMap;
		}
		if (entity.HasComponent<PointLightComponent>())
		{
			out << YAML::Key << "PointLightComponent";
			out << YAML::BeginMap;
			auto& sc = entity.GetComponent<PointLightComponent>();
			out << YAML::Key << "Ambient"      << YAML::Value << sc.Ambient;
			out << YAML::Key << "Diffuse"      << YAML::Value << sc.Diffuse;
			out << YAML::Key << "Specular"     << YAML::Value << sc.Specular;
			out << YAML::Key << "Intensity"    << YAML::Value << sc.Intensity;
			out << YAML::Key << "Range"        << YAML::Value << sc.Range;
			out << YAML::Key << "AttConstant"  << YAML::Value << sc.AttConstant;
			out << YAML::Key << "AttLinear"    << YAML::Value << sc.AttLinear;
			out << YAML::Key << "AttQuadratic" << YAML::Value << sc.AttQuadratic;
			out << YAML::EndMap;
		}
		if (entity.HasComponent<DirectionalLightComponent>())
		{
			out << YAML::Key << "DirectionalLightComponent";
			out << YAML::BeginMap;
			auto& sc = entity.GetComponent<DirectionalLightComponent>();
			out << YAML::Key << "Direction" << YAML::Value << sc.Direction;
			out << YAML::Key << "Ambient"   << YAML::Value << sc.Ambient;
			out << YAML::Key << "Diffuse"   << YAML::Value << sc.Diffuse;
			out << YAML::Key << "Specular"  << YAML::Value << sc.Specular;
			out << YAML::Key << "Intensity" << YAML::Value << sc.Intensity;
			out << YAML::EndMap;
		}
		if (entity.HasComponent<SpotLightComponent>())
		{
			out << YAML::Key << "SpotLightComponent";
			out << YAML::BeginMap;
			auto& sc = entity.GetComponent<SpotLightComponent>();
			out << YAML::Key << "Direction"      << YAML::Value << sc.Direction;
			out << YAML::Key << "Ambient"        << YAML::Value << sc.Ambient;
			out << YAML::Key << "Diffuse"        << YAML::Value << sc.Diffuse;
			out << YAML::Key << "Specular"       << YAML::Value << sc.Specular;
			out << YAML::Key << "Intensity"      << YAML::Value << sc.Intensity;
			out << YAML::Key << "Range"          << YAML::Value << sc.Range;
			out << YAML::Key << "InnerConeAngle" << YAML::Value << sc.InnerConeAngle;
			out << YAML::Key << "OuterConeAngle" << YAML::Value << sc.OuterConeAngle;
			out << YAML::Key << "AttConstant"    << YAML::Value << sc.AttConstant;
			out << YAML::Key << "AttLinear"      << YAML::Value << sc.AttLinear;
			out << YAML::Key << "AttQuadratic"   << YAML::Value << sc.AttQuadratic;
			out << YAML::EndMap;
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
			if(!src.MaterialInstance)
				src.MaterialInstance = Material::Create();
			
			auto serializeTexture = [&](const std::string& yamlKey, Shared<Texture2D>& texture)
				{
					if (texture)
					{
						out << YAML::Key << yamlKey << YAML::Value << texture->GetTexturePath();
					}
				};
			// Check to see if texture paths are valid before we serialize them 
			serializeTexture("AlbedoPath",           src.MaterialInstance->AlbedoMap);
			serializeTexture("NormalPath",            src.MaterialInstance->NormalMap);
			serializeTexture("MetallicRoughnessPath", src.MaterialInstance->MetallicRoughnessMap);
			serializeTexture("AOPath",                src.MaterialInstance->AOMap);
			serializeTexture("EmissivePath",          src.MaterialInstance->EmissiveMap);

			out << YAML::EndMap;

		}
		if (entity.HasComponent<MeshComponent>())
		{
			out << YAML::Key << "MeshComponent";
			out << YAML::BeginMap;

			auto& mc = entity.GetComponent<MeshComponent>();
			out << YAML::Key << "FilePath" << YAML::Value << mc.FilePath;
			out << YAML::Key << "PrimitiveType" << YAML::Value << static_cast<int>(mc.Primitive);

			if (mc.MaterialInstance)
			{
				out << YAML::Key << "PBR_AlbedoColor" << YAML::Value << glm::vec3(mc.MaterialInstance->AlbedoColor);
				out << YAML::Key << "PBR_AlbedoAlpha" << YAML::Value << mc.MaterialInstance->AlbedoColor.a;
				out << YAML::Key << "PBR_Metallic"    << YAML::Value << mc.MaterialInstance->Metallic;
				out << YAML::Key << "PBR_Roughness"   << YAML::Value << mc.MaterialInstance->Roughness;
				out << YAML::Key << "PBR_AO"          << YAML::Value << mc.MaterialInstance->AO;
				out << YAML::Key << "PBR_EmissiveColor" << YAML::Value << mc.MaterialInstance->EmissiveColor;
				out << YAML::Key << "PBR_EmissiveStrength" << YAML::Value << mc.MaterialInstance->EmissiveStrength;
				out << YAML::Key << "Mat_BlendMode"   << YAML::Value << static_cast<int>(mc.MaterialInstance->Blend);
				out << YAML::Key << "Mat_ShadingModel" << YAML::Value << static_cast<int>(mc.MaterialInstance->Shading);
				out << YAML::Key << "Mat_TwoSided"    << YAML::Value << mc.MaterialInstance->TwoSided;
				out << YAML::Key << "Mat_AlphaCutoff" << YAML::Value << mc.MaterialInstance->AlphaCutoff;

				auto serializeTex = [&](const std::string& key, Shared<Texture2D>& tex)
				{
					if (tex) out << YAML::Key << key << YAML::Value << tex->GetTexturePath();
				};
				serializeTex("Tex_Albedo",  mc.MaterialInstance->AlbedoMap);
				serializeTex("Tex_Normal",  mc.MaterialInstance->NormalMap);
				serializeTex("Tex_MetallicRoughness", mc.MaterialInstance->MetallicRoughnessMap);
				serializeTex("Tex_AO",      mc.MaterialInstance->AOMap);
				serializeTex("Tex_Emissive", mc.MaterialInstance->EmissiveMap);
			}

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
		if (entity.HasComponent<Rigidbody3DComponent>())
		{
			auto& rb = entity.GetComponent<Rigidbody3DComponent>();
			out << YAML::Key << "Rigidbody3DComponent" << YAML::BeginMap;
			out << YAML::Key << "BodyType"        << YAML::Value << static_cast<int>(rb.Type);
			out << YAML::Key << "GravityScale"    << YAML::Value << rb.GravityScale;
			out << YAML::Key << "LinearDamping"   << YAML::Value << rb.LinearDamping;
			out << YAML::Key << "AngularDamping"  << YAML::Value << rb.AngularDamping;
			out << YAML::Key << "FixedRotationX"  << YAML::Value << rb.FixedRotationX;
			out << YAML::Key << "FixedRotationY"  << YAML::Value << rb.FixedRotationY;
			out << YAML::Key << "FixedRotationZ"  << YAML::Value << rb.FixedRotationZ;
			out << YAML::EndMap;
		}
		if (entity.HasComponent<BoxCollider3DComponent>())
		{
			auto& bc = entity.GetComponent<BoxCollider3DComponent>();
			out << YAML::Key << "BoxCollider3DComponent" << YAML::BeginMap;
			out << YAML::Key << "HalfExtents" << YAML::Value << bc.HalfExtents;
			out << YAML::Key << "Offset"      << YAML::Value << bc.Offset;
			out << YAML::Key << "Friction"    << YAML::Value << bc.Friction;
			out << YAML::Key << "Restitution" << YAML::Value << bc.Restitution;
			out << YAML::Key << "Density"     << YAML::Value << bc.Density;
			out << YAML::EndMap;
		}
		if (entity.HasComponent<SphereCollider3DComponent>())
		{
			auto& sc = entity.GetComponent<SphereCollider3DComponent>();
			out << YAML::Key << "SphereCollider3DComponent" << YAML::BeginMap;
			out << YAML::Key << "Radius"     << YAML::Value << sc.Radius;
			out << YAML::Key << "Offset"     << YAML::Value << sc.Offset;
			out << YAML::Key << "Friction"   << YAML::Value << sc.Friction;
			out << YAML::Key << "Restitution"<< YAML::Value << sc.Restitution;
			out << YAML::Key << "Density"    << YAML::Value << sc.Density;
			out << YAML::EndMap;
		}
		if (entity.HasComponent<CapsuleCollider3DComponent>())
		{
			auto& cc = entity.GetComponent<CapsuleCollider3DComponent>();
			out << YAML::Key << "CapsuleCollider3DComponent" << YAML::BeginMap;
			out << YAML::Key << "Radius"     << YAML::Value << cc.Radius;
			out << YAML::Key << "HalfHeight" << YAML::Value << cc.HalfHeight;
			out << YAML::Key << "Offset"     << YAML::Value << cc.Offset;
			out << YAML::Key << "Friction"   << YAML::Value << cc.Friction;
			out << YAML::Key << "Restitution"<< YAML::Value << cc.Restitution;
			out << YAML::Key << "Density"    << YAML::Value << cc.Density;
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
		if (entity.HasComponent<SpringArmComponent>())
		{
			auto& arm = entity.GetComponent<SpringArmComponent>();
			out << YAML::Key << "SpringArmComponent" << YAML::BeginMap;
			out << YAML::Key << "ArmLength"        << YAML::Value << arm.ArmLength;
			out << YAML::Key << "SocketOffset"     << YAML::Value << arm.SocketOffset;
			out << YAML::Key << "PivotOffset"      << YAML::Value << arm.PivotOffset;
			out << YAML::Key << "Pitch"            << YAML::Value << arm.Pitch;
			out << YAML::Key << "Yaw"              << YAML::Value << arm.Yaw;
			out << YAML::Key << "InheritYaw"       << YAML::Value << arm.InheritYaw;
			out << YAML::Key << "PositionLagSpeed" << YAML::Value << arm.PositionLagSpeed;
			out << YAML::Key << "EnableLag"        << YAML::Value << arm.EnableLag;
			out << YAML::Key << "TargetCameraUUID" << YAML::Value << (uint64_t)arm.TargetCameraUUID;
			out << YAML::EndMap;
		}
		out << YAML::EndMap;
	}
	void SceneSerializer::Serialize(const std::string& filepath)
	{
		std::filesystem::create_directories(std::filesystem::path(filepath).remove_filename());

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Scene" << YAML::Value << filepath;

		// ── Scene rendering settings ────────────────────────────────────────
		out << YAML::Key << "RenderSettings" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "UseSkybox"    << YAML::Value << m_Scene->GetUseSkybox();
		out << YAML::Key << "UseTimeOfDay" << YAML::Value << m_Scene->GetUseTimeOfDay();
		if (m_Scene->GetSkybox())
		{
			auto& sky = *m_Scene->GetSkybox();
			out << YAML::Key << "Sky_Turbidity"       << YAML::Value << sky.Turbidity;
			out << YAML::Key << "Sky_SkyExposure"     << YAML::Value << sky.SkyExposure;
			out << YAML::Key << "Sky_GroundColor"     << YAML::Value << sky.GroundColor;
			out << YAML::Key << "Sky_SunColor"        << YAML::Value << sky.SunColor;
			out << YAML::Key << "Sky_SunSize"         << YAML::Value << sky.SunSize;
			out << YAML::Key << "Sky_SunStrength"     << YAML::Value << sky.SunStrength;
			out << YAML::Key << "Sky_CloudColor"      << YAML::Value << sky.CloudColor;
			out << YAML::Key << "Sky_CloudCoverage"   << YAML::Value << sky.CloudCoverage;
			out << YAML::Key << "Sky_CloudDensity"    << YAML::Value << sky.CloudDensity;
			out << YAML::Key << "Sky_CloudHeight"     << YAML::Value << sky.CloudHeight;
			out << YAML::Key << "Sky_CloudScale"      << YAML::Value << sky.CloudScale;
			out << YAML::Key << "Sky_CloudScrollSpeed"<< YAML::Value << sky.CloudScrollSpeed;
		}
		{
			auto& tod = m_Scene->GetTimeOfDay();
			out << YAML::Key << "ToD_NormalizedTime"  << YAML::Value << tod.NormalizedTime;
			out << YAML::Key << "ToD_AutoAdvance"     << YAML::Value << tod.AutoAdvance;
			out << YAML::Key << "ToD_DayDuration"     << YAML::Value << tod.DayDurationSecs;
			out << YAML::Key << "ToD_SunAzimuth"     << YAML::Value << tod.SunAzimuthDeg;
		}
		{
			auto& fog = m_Scene->GetFog();
			out << YAML::Key << "Fog_Enabled"         << YAML::Value << fog.Enabled;
			out << YAML::Key << "Fog_Color"           << YAML::Value << fog.Color;
			out << YAML::Key << "Fog_Density"         << YAML::Value << fog.Density;
			out << YAML::Key << "Fog_HeightStart"     << YAML::Value << fog.HeightStart;
			out << YAML::Key << "Fog_HeightDensity"   << YAML::Value << fog.HeightDensity;
		}
		out << YAML::EndMap; // RenderSettings

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

		// ── Scene rendering settings ─────────────────────────────────────────
		auto rs = data["RenderSettings"];
		if (rs)
		{
			if (rs["UseSkybox"])    m_Scene->SetUseSkybox(rs["UseSkybox"].as<bool>());
			if (rs["UseTimeOfDay"]) m_Scene->SetUseTimeOfDay(rs["UseTimeOfDay"].as<bool>());
			if (!m_Scene->m_Skybox) m_Scene->m_Skybox = std::make_shared<Skybox>();
			if (m_Scene->GetSkybox() && rs["Sky_Turbidity"])
			{
				auto& sky = *m_Scene->GetSkybox();
				if (rs["Sky_Turbidity"])        sky.Turbidity        = rs["Sky_Turbidity"].as<float>();
				if (rs["Sky_SkyExposure"])      sky.SkyExposure      = rs["Sky_SkyExposure"].as<float>();
				if (rs["Sky_GroundColor"])      sky.GroundColor      = rs["Sky_GroundColor"].as<glm::vec3>();
				if (rs["Sky_SunColor"])         sky.SunColor         = rs["Sky_SunColor"].as<glm::vec3>();
				if (rs["Sky_SunSize"])          sky.SunSize          = rs["Sky_SunSize"].as<float>();
				if (rs["Sky_SunStrength"])      sky.SunStrength      = rs["Sky_SunStrength"].as<float>();
				if (rs["Sky_CloudColor"])       sky.CloudColor       = rs["Sky_CloudColor"].as<glm::vec3>();
				if (rs["Sky_CloudCoverage"])    sky.CloudCoverage    = rs["Sky_CloudCoverage"].as<float>();
				if (rs["Sky_CloudDensity"])     sky.CloudDensity     = rs["Sky_CloudDensity"].as<float>();
				if (rs["Sky_CloudHeight"])      sky.CloudHeight      = rs["Sky_CloudHeight"].as<float>();
				if (rs["Sky_CloudScale"])       sky.CloudScale       = rs["Sky_CloudScale"].as<float>();
				if (rs["Sky_CloudScrollSpeed"]) sky.CloudScrollSpeed = rs["Sky_CloudScrollSpeed"].as<float>();
			}
			{
				auto& tod = m_Scene->GetTimeOfDay();
				if (rs["ToD_NormalizedTime"])  tod.NormalizedTime  = rs["ToD_NormalizedTime"].as<float>();
				if (rs["ToD_AutoAdvance"])     tod.AutoAdvance     = rs["ToD_AutoAdvance"].as<bool>();
				if (rs["ToD_DayDuration"])     tod.DayDurationSecs = rs["ToD_DayDuration"].as<float>();
				if (rs["ToD_SunAzimuth"])      tod.SunAzimuthDeg   = rs["ToD_SunAzimuth"].as<float>();
			}
			{
				auto& fog = m_Scene->GetFog();
				if (rs["Fog_Enabled"])      fog.Enabled      = rs["Fog_Enabled"].as<bool>();
				if (rs["Fog_Color"])        fog.Color        = rs["Fog_Color"].as<glm::vec3>();
				if (rs["Fog_Density"])      fog.Density      = rs["Fog_Density"].as<float>();
				if (rs["Fog_HeightStart"])  fog.HeightStart  = rs["Fog_HeightStart"].as<float>();
				if (rs["Fog_HeightDensity"])fog.HeightDensity= rs["Fog_HeightDensity"].as<float>();
			}
		}

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
					
					if (!src.MaterialInstance)
						src.MaterialInstance = Material::Create();
					// Lambda to handle texture assignment
					auto assignTextureFromYAML = [&](const char* yamlKey, Shared<Texture2D>& texture)
						{
							if (spriteRendererComponent[yamlKey])
							{
								std::string texturePath = spriteRendererComponent[yamlKey].as<std::string>();
								
								texture = Texture2D::Create(texturePath);
							}
						};

					// Use the lambda for each texture type
					assignTextureFromYAML("AlbedoPath",           src.MaterialInstance->AlbedoMap);
					assignTextureFromYAML("NormalPath",            src.MaterialInstance->NormalMap);
					assignTextureFromYAML("MetallicRoughnessPath", src.MaterialInstance->MetallicRoughnessMap);
					assignTextureFromYAML("AOPath",                src.MaterialInstance->AOMap);
					assignTextureFromYAML("EmissivePath",          src.MaterialInstance->EmissiveMap);

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

				auto rigidbody3DComponent = entity["Rigidbody3DComponent"];
				if (rigidbody3DComponent)
				{
					auto& rb = deserializedEntity.AddComponent<Rigidbody3DComponent>();
					rb.Type           = static_cast<Rigidbody3DComponent::BodyType>(rigidbody3DComponent["BodyType"].as<int>());
					if (rigidbody3DComponent["GravityScale"])   rb.GravityScale   = rigidbody3DComponent["GravityScale"].as<float>();
					if (rigidbody3DComponent["LinearDamping"])  rb.LinearDamping  = rigidbody3DComponent["LinearDamping"].as<float>();
					if (rigidbody3DComponent["AngularDamping"]) rb.AngularDamping = rigidbody3DComponent["AngularDamping"].as<float>();
					if (rigidbody3DComponent["FixedRotationX"]) rb.FixedRotationX = rigidbody3DComponent["FixedRotationX"].as<bool>();
					if (rigidbody3DComponent["FixedRotationY"]) rb.FixedRotationY = rigidbody3DComponent["FixedRotationY"].as<bool>();
					if (rigidbody3DComponent["FixedRotationZ"]) rb.FixedRotationZ = rigidbody3DComponent["FixedRotationZ"].as<bool>();
				}

				auto boxCollider3DComponent = entity["BoxCollider3DComponent"];
				if (boxCollider3DComponent)
				{
					auto& bc = deserializedEntity.AddComponent<BoxCollider3DComponent>();
					if (boxCollider3DComponent["HalfExtents"])  bc.HalfExtents  = boxCollider3DComponent["HalfExtents"].as<glm::vec3>();
					if (boxCollider3DComponent["Offset"])       bc.Offset       = boxCollider3DComponent["Offset"].as<glm::vec3>();
					if (boxCollider3DComponent["Friction"])     bc.Friction     = boxCollider3DComponent["Friction"].as<float>();
					if (boxCollider3DComponent["Restitution"])  bc.Restitution  = boxCollider3DComponent["Restitution"].as<float>();
					if (boxCollider3DComponent["Density"])      bc.Density      = boxCollider3DComponent["Density"].as<float>();
				}

				auto sphereCollider3DComponent = entity["SphereCollider3DComponent"];
				if (sphereCollider3DComponent)
				{
					auto& sc = deserializedEntity.AddComponent<SphereCollider3DComponent>();
					if (sphereCollider3DComponent["Radius"])      sc.Radius      = sphereCollider3DComponent["Radius"].as<float>();
					if (sphereCollider3DComponent["Offset"])      sc.Offset      = sphereCollider3DComponent["Offset"].as<glm::vec3>();
					if (sphereCollider3DComponent["Friction"])    sc.Friction    = sphereCollider3DComponent["Friction"].as<float>();
					if (sphereCollider3DComponent["Restitution"]) sc.Restitution = sphereCollider3DComponent["Restitution"].as<float>();
					if (sphereCollider3DComponent["Density"])     sc.Density     = sphereCollider3DComponent["Density"].as<float>();
				}

				auto capsuleCollider3DComponent = entity["CapsuleCollider3DComponent"];
				if (capsuleCollider3DComponent)
				{
					auto& cc = deserializedEntity.AddComponent<CapsuleCollider3DComponent>();
					if (capsuleCollider3DComponent["Radius"])      cc.Radius      = capsuleCollider3DComponent["Radius"].as<float>();
					if (capsuleCollider3DComponent["HalfHeight"])  cc.HalfHeight  = capsuleCollider3DComponent["HalfHeight"].as<float>();
					if (capsuleCollider3DComponent["Offset"])      cc.Offset      = capsuleCollider3DComponent["Offset"].as<glm::vec3>();
					if (capsuleCollider3DComponent["Friction"])    cc.Friction    = capsuleCollider3DComponent["Friction"].as<float>();
					if (capsuleCollider3DComponent["Restitution"]) cc.Restitution = capsuleCollider3DComponent["Restitution"].as<float>();
					if (capsuleCollider3DComponent["Density"])     cc.Density     = capsuleCollider3DComponent["Density"].as<float>();
				}

				auto pointLightComponent = entity["PointLightComponent"];
				if (pointLightComponent)
				{
					auto& pl = deserializedEntity.AddComponent<PointLightComponent>();
					// New field names — fall back to defaults if loading an old scene
					if (pointLightComponent["Ambient"])      pl.Ambient      = pointLightComponent["Ambient"].as<glm::vec3>();
					if (pointLightComponent["Diffuse"])      pl.Diffuse      = pointLightComponent["Diffuse"].as<glm::vec3>();
					if (pointLightComponent["Specular"])     pl.Specular     = pointLightComponent["Specular"].as<glm::vec3>();
					if (pointLightComponent["Intensity"])    pl.Intensity    = pointLightComponent["Intensity"].as<float>();
					if (pointLightComponent["Range"])        pl.Range        = pointLightComponent["Range"].as<float>();
					if (pointLightComponent["AttConstant"])  pl.AttConstant  = pointLightComponent["AttConstant"].as<float>();
					if (pointLightComponent["AttLinear"])    pl.AttLinear    = pointLightComponent["AttLinear"].as<float>();
					if (pointLightComponent["AttQuadratic"]) pl.AttQuadratic = pointLightComponent["AttQuadratic"].as<float>();
				}
				auto dirLightComponent = entity["DirectionalLightComponent"];
				if (dirLightComponent)
				{
					auto& dl = deserializedEntity.AddComponent<DirectionalLightComponent>();
					if (dirLightComponent["Direction"]) dl.Direction = dirLightComponent["Direction"].as<glm::vec3>();
					if (dirLightComponent["Ambient"])   dl.Ambient   = dirLightComponent["Ambient"].as<glm::vec3>();
					if (dirLightComponent["Diffuse"])   dl.Diffuse   = dirLightComponent["Diffuse"].as<glm::vec3>();
					if (dirLightComponent["Specular"])  dl.Specular  = dirLightComponent["Specular"].as<glm::vec3>();
					if (dirLightComponent["Intensity"]) dl.Intensity = dirLightComponent["Intensity"].as<float>();
				}
				auto meshComponent = entity["MeshComponent"];
				if (meshComponent)
				{
					auto& mc = deserializedEntity.AddComponent<MeshComponent>();
					auto createPrimitiveMesh = [](MeshComponent::PrimitiveType primitive) -> Shared<Mesh>
					{
						switch (primitive)
						{
							case MeshComponent::PrimitiveType::Cube: return Mesh::CreateCube();
							case MeshComponent::PrimitiveType::Quad: return Mesh::CreateQuad();
							default: return nullptr;
						}
					};

					if (meshComponent["FilePath"])
					{
						mc.FilePath = meshComponent["FilePath"].as<std::string>();
						if (!mc.FilePath.empty())
							mc.ModelAsset = ModelLoader::Load(mc.FilePath);
					}
					if (meshComponent["PrimitiveType"])
						mc.Primitive = static_cast<MeshComponent::PrimitiveType>(meshComponent["PrimitiveType"].as<int>());

					if (!mc.ModelAsset)
					{
						if (mc.Primitive == MeshComponent::PrimitiveType::None && mc.FilePath.empty())
						{
							// Legacy primitive-only scenes had MeshData at edit time but saved no
							// mesh identity. Existing sample scenes used cube visuals for these.
							mc.Primitive = MeshComponent::PrimitiveType::Cube;
						}
						mc.MeshData = createPrimitiveMesh(mc.Primitive);
					}

					if (!mc.MaterialInstance)
						mc.MaterialInstance = Material::Create();

					// PBR properties
					if (meshComponent["PBR_AlbedoColor"])
					{
						glm::vec3 rgb = meshComponent["PBR_AlbedoColor"].as<glm::vec3>();
						float alpha = meshComponent["PBR_AlbedoAlpha"] ? meshComponent["PBR_AlbedoAlpha"].as<float>() : 1.0f;
						mc.MaterialInstance->AlbedoColor = glm::vec4(rgb, alpha);
					}
					if (meshComponent["PBR_Metallic"])         mc.MaterialInstance->Metallic         = meshComponent["PBR_Metallic"].as<float>();
					if (meshComponent["PBR_Roughness"])        mc.MaterialInstance->Roughness        = meshComponent["PBR_Roughness"].as<float>();
					if (meshComponent["PBR_AO"])               mc.MaterialInstance->AO               = meshComponent["PBR_AO"].as<float>();
					if (meshComponent["PBR_EmissiveColor"])    mc.MaterialInstance->EmissiveColor    = meshComponent["PBR_EmissiveColor"].as<glm::vec3>();
					if (meshComponent["PBR_EmissiveStrength"]) mc.MaterialInstance->EmissiveStrength = meshComponent["PBR_EmissiveStrength"].as<float>();
					if (meshComponent["Mat_BlendMode"])        mc.MaterialInstance->Blend            = static_cast<BlendMode>(meshComponent["Mat_BlendMode"].as<int>());
					if (meshComponent["Mat_ShadingModel"])     mc.MaterialInstance->Shading          = static_cast<ShadingModel>(meshComponent["Mat_ShadingModel"].as<int>());
					if (meshComponent["Mat_TwoSided"])         mc.MaterialInstance->TwoSided         = meshComponent["Mat_TwoSided"].as<bool>();
					if (meshComponent["Mat_AlphaCutoff"])      mc.MaterialInstance->AlphaCutoff      = meshComponent["Mat_AlphaCutoff"].as<float>();

					// Textures
					auto loadTex = [&](const char* key, Shared<Texture2D>& tex)
					{
						if (meshComponent[key])
							tex = Texture2D::Create(meshComponent[key].as<std::string>());
					};
					loadTex("Tex_Albedo",  mc.MaterialInstance->AlbedoMap);
					loadTex("Tex_Normal",  mc.MaterialInstance->NormalMap);
					loadTex("Tex_MetallicRoughness", mc.MaterialInstance->MetallicRoughnessMap);
					loadTex("Tex_AO",      mc.MaterialInstance->AOMap);
					loadTex("Tex_Emissive", mc.MaterialInstance->EmissiveMap);
				}

				auto spotLightComponent = entity["SpotLightComponent"];
				if (spotLightComponent)
				{
					auto& sl = deserializedEntity.AddComponent<SpotLightComponent>();
					if (spotLightComponent["Direction"])      sl.Direction      = spotLightComponent["Direction"].as<glm::vec3>();
					if (spotLightComponent["Ambient"])        sl.Ambient        = spotLightComponent["Ambient"].as<glm::vec3>();
					if (spotLightComponent["Diffuse"])        sl.Diffuse        = spotLightComponent["Diffuse"].as<glm::vec3>();
					if (spotLightComponent["Specular"])       sl.Specular       = spotLightComponent["Specular"].as<glm::vec3>();
					if (spotLightComponent["Intensity"])      sl.Intensity      = spotLightComponent["Intensity"].as<float>();
					if (spotLightComponent["Range"])          sl.Range          = spotLightComponent["Range"].as<float>();
					if (spotLightComponent["InnerConeAngle"]) sl.InnerConeAngle = spotLightComponent["InnerConeAngle"].as<float>();
					if (spotLightComponent["OuterConeAngle"]) sl.OuterConeAngle = spotLightComponent["OuterConeAngle"].as<float>();
					if (spotLightComponent["AttConstant"])    sl.AttConstant    = spotLightComponent["AttConstant"].as<float>();
					if (spotLightComponent["AttLinear"])      sl.AttLinear      = spotLightComponent["AttLinear"].as<float>();
					if (spotLightComponent["AttQuadratic"])   sl.AttQuadratic   = spotLightComponent["AttQuadratic"].as<float>();
				}

			auto nativeScriptComponent = entity["NativeScriptComponent"];
				if (nativeScriptComponent)
				{
					auto& nsc = deserializedEntity.AddComponent<NativeScriptComponent>();
					nsc.ClassName = nativeScriptComponent["ClassName"].as<std::string>("");
				}

				auto springArmNode = entity["SpringArmComponent"];
				if (springArmNode)
				{
					auto& arm = deserializedEntity.AddComponent<SpringArmComponent>();
					if (springArmNode["ArmLength"])        arm.ArmLength        = springArmNode["ArmLength"].as<float>();
					if (springArmNode["SocketOffset"])     arm.SocketOffset     = springArmNode["SocketOffset"].as<glm::vec3>();
					if (springArmNode["PivotOffset"])      arm.PivotOffset      = springArmNode["PivotOffset"].as<glm::vec3>();
					if (springArmNode["Pitch"])            arm.Pitch            = springArmNode["Pitch"].as<float>();
					if (springArmNode["Yaw"])              arm.Yaw              = springArmNode["Yaw"].as<float>();
					if (springArmNode["InheritYaw"])       arm.InheritYaw       = springArmNode["InheritYaw"].as<bool>();
					if (springArmNode["PositionLagSpeed"]) arm.PositionLagSpeed = springArmNode["PositionLagSpeed"].as<float>();
					if (springArmNode["EnableLag"])        arm.EnableLag        = springArmNode["EnableLag"].as<bool>();
					if (springArmNode["TargetCameraUUID"]) arm.TargetCameraUUID = springArmNode["TargetCameraUUID"].as<uint64_t>();
				}

			}
		}

		return true;
	}
	
	bool SceneSerializer::DeserializeBinary(const std::string& filepath)
	{
		return false;
	}
}
