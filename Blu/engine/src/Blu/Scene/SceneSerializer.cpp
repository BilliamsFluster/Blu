#include "Blupch.h"
#include "SceneSerializer.h"
#include "Entity.h"
#include "Component.h"
#include <algorithm>
#include <fstream>
#include <filesystem>
#include "yaml-cpp/yaml.h"
#include "Blu/Rendering/Texture.h"
#include "Blu/Rendering/ModelLoader.h"
#include "Blu/Rendering/Skybox.h"
#include "Blu/Rendering/TimeOfDay.h"
#include "Blu/Rendering/Renderer3D.h"
#include "Blu/Rendering/PostProcess.h"
#include "Blu/Utils/Helpers.h"
#include "Blu/Utils/AssetPath.h"
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

	template<>
	struct convert<glm::mat4>
	{
		static Node encode(const glm::mat4& rhs)
		{
			Node node;
			for (int col = 0; col < 4; ++col)
				for (int row = 0; row < 4; ++row)
					node.push_back(rhs[col][row]);
			return node;
		}

		static bool decode(const Node& node, glm::mat4& rhs)
		{
			if (!node.IsSequence() || node.size() != 16)
				return false;

			for (int col = 0; col < 4; ++col)
				for (int row = 0; row < 4; ++row)
					rhs[col][row] = node[col * 4 + row].as<float>();
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
	YAML::Emitter& operator <<(YAML::Emitter& out, const glm::mat4& m)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq;
		for (int col = 0; col < 4; ++col)
			for (int row = 0; row < 4; ++row)
				out << m[col][row];
		out << YAML::EndSeq;
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

	static std::string SerializeAssetPath(const std::string& path)
	{
		return AssetPath::ToProjectRelative(path);
	}

	static std::filesystem::path ResolveAssetPathForLoad(
		const std::string& rawPath,
		const std::filesystem::path& scenePath,
		const char* sourceComponent)
	{
		auto resolved = AssetPath::ResolvePath(rawPath, scenePath);
		if (!rawPath.empty() && !std::filesystem::exists(resolved))
			BLU_CORE_WARN("SceneSerializer: missing asset for {0}: {1}", sourceComponent, rawPath);
		return resolved;
	}

	static std::string NormalizeLoadedAssetPath(
		const std::string& rawPath,
		const std::filesystem::path& scenePath,
		const char* sourceComponent)
	{
		if (rawPath.empty())
			return {};

		auto resolved = ResolveAssetPathForLoad(rawPath, scenePath, sourceComponent);
		if (std::filesystem::exists(resolved))
			return AssetPath::ToProjectRelative(resolved);

		return AssetPath::ToProjectRelative(rawPath);
	}

	static Shared<Texture2D> LoadSceneTexture(
		const std::string& rawPath,
		const std::filesystem::path& scenePath,
		const char* sourceComponent)
	{
		if (rawPath.empty())
			return nullptr;

		auto resolved = ResolveAssetPathForLoad(rawPath, scenePath, sourceComponent);
		return Texture2D::Create(resolved.string());
	}

	static void SerializeSceneAssetManifest(Scene& scene, const std::string& sceneFilePath)
	{
		std::filesystem::path manifestPath = sceneFilePath;
		manifestPath.replace_extension(".assets.yaml");

		YAML::Emitter out;
		auto manifest = scene.CollectAssetManifest();

		out << YAML::BeginMap;
		out << YAML::Key << "Scene" << YAML::Value << AssetPath::ToProjectRelative(sceneFilePath);
		out << YAML::Key << "Summary" << YAML::BeginMap;
		out << YAML::Key << "Referenced" << YAML::Value << manifest.ReferencedCount;
		out << YAML::Key << "Missing" << YAML::Value << manifest.MissingCount;
		out << YAML::Key << "External" << YAML::Value << manifest.ExternalCount;
		out << YAML::Key << "Imported" << YAML::Value << manifest.ImportedCount;
		out << YAML::EndMap;

		out << YAML::Key << "Assets" << YAML::Value << YAML::BeginSeq;
		for (const auto& dependency : manifest.Dependencies)
		{
			out << YAML::BeginMap;
			out << YAML::Key << "Type" << YAML::Value << dependency.Type;
			out << YAML::Key << "Path" << YAML::Value << dependency.Path;
			out << YAML::Key << "ResolvedPath" << YAML::Value << dependency.ResolvedPath;
			out << YAML::Key << "SourceEntity" << YAML::Value << dependency.SourceEntity;
			out << YAML::Key << "SourceTag" << YAML::Value << dependency.SourceTag;
			out << YAML::Key << "SourceComponent" << YAML::Value << dependency.SourceComponent;
			out << YAML::Key << "Exists" << YAML::Value << dependency.Exists;
			out << YAML::Key << "External" << YAML::Value << dependency.External;
			out << YAML::Key << "Imported" << YAML::Value << dependency.Imported;
			out << YAML::EndMap;
		}
		out << YAML::EndSeq;
		out << YAML::EndMap;

		std::ofstream fout(manifestPath);
		if (!fout)
		{
			BLU_CORE_WARN("SceneSerializer: failed to write asset manifest: {0}", manifestPath.string());
			return;
		}

		fout << out.c_str();
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
		if (entity.HasComponent<VisualOffsetComponent>())
		{
			out << YAML::Key << "VisualOffsetComponent";
			out << YAML::BeginMap;

			auto& voc = entity.GetComponent<VisualOffsetComponent>();
			out << YAML::Key << "Translation" << YAML::Value << voc.Translation;
			out << YAML::Key << "Rotation" << YAML::Value << voc.Rotation;
			out << YAML::Key << "Scale" << YAML::Value << voc.Scale;
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
						out << YAML::Key << yamlKey << YAML::Value << SerializeAssetPath(texture->GetTexturePath());
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
			out << YAML::Key << "FilePath" << YAML::Value << SerializeAssetPath(mc.FilePath);
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
					if (tex) out << YAML::Key << key << YAML::Value << SerializeAssetPath(tex->GetTexturePath());
				};
				serializeTex("Tex_Albedo",  mc.MaterialInstance->AlbedoMap);
				serializeTex("Tex_Normal",  mc.MaterialInstance->NormalMap);
				serializeTex("Tex_MetallicRoughness", mc.MaterialInstance->MetallicRoughnessMap);
				serializeTex("Tex_AO",      mc.MaterialInstance->AOMap);
				serializeTex("Tex_Emissive", mc.MaterialInstance->EmissiveMap);
			}

			out << YAML::EndMap;
		}
		if (entity.HasComponent<MeshLODComponent>())
		{
			auto& lod = entity.GetComponent<MeshLODComponent>();
			out << YAML::Key << "MeshLODComponent" << YAML::BeginMap;
			out << YAML::Key << "Active" << YAML::Value << lod.Active;
			out << YAML::Key << "Levels" << YAML::Value << YAML::BeginSeq;
			for (const auto& level : lod.Levels)
			{
				out << YAML::BeginMap;
				out << YAML::Key << "FilePath" << YAML::Value << SerializeAssetPath(level.FilePath);
				out << YAML::Key << "MaxDistance" << YAML::Value << level.MaxDistance;
				out << YAML::EndMap;
			}
			out << YAML::EndSeq;
			out << YAML::EndMap;
		}
		if (entity.HasComponent<FoliageComponent>())
		{
			auto& foliage = entity.GetComponent<FoliageComponent>();
			out << YAML::Key << "FoliageComponent" << YAML::BeginMap;
			out << YAML::Key << "FilePath" << YAML::Value << SerializeAssetPath(foliage.FilePath);
			out << YAML::Key << "WindEnabled" << YAML::Value << foliage.WindEnabled;
			out << YAML::Key << "WindStrength" << YAML::Value << foliage.WindStrength;
			out << YAML::Key << "WindFrequency" << YAML::Value << foliage.WindFrequency;
			out << YAML::Key << "WindDirection" << YAML::Value << foliage.WindDirection;
			out << YAML::Key << "Transforms" << YAML::Value << YAML::BeginSeq;
			for (const auto& transform : foliage.Transforms)
				out << transform;
			out << YAML::EndSeq;
			out << YAML::EndMap;
		}
		if (entity.HasComponent<AudioSourceComponent>())
		{
			auto& audio = entity.GetComponent<AudioSourceComponent>();
			out << YAML::Key << "AudioSourceComponent" << YAML::BeginMap;
			out << YAML::Key << "FilePath" << YAML::Value << SerializeAssetPath(audio.FilePath);
			out << YAML::Key << "Volume" << YAML::Value << audio.Volume;
			out << YAML::Key << "Pitch" << YAML::Value << audio.Pitch;
			out << YAML::Key << "Loop" << YAML::Value << audio.Loop;
			out << YAML::Key << "PlayOnStart" << YAML::Value << audio.PlayOnStart;
			out << YAML::Key << "Spatial" << YAML::Value << audio.Spatial;
			out << YAML::Key << "MinDistance" << YAML::Value << audio.MinDistance;
			out << YAML::Key << "MaxDistance" << YAML::Value << audio.MaxDistance;
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
		if (entity.HasComponent<MeshCollider3DComponent>())
		{
			auto& mc = entity.GetComponent<MeshCollider3DComponent>();
			out << YAML::Key << "MeshCollider3DComponent" << YAML::BeginMap;
			out << YAML::Key << "Enabled"     << YAML::Value << mc.Enabled;
			out << YAML::Key << "DoubleSided" << YAML::Value << mc.DoubleSided;
			out << YAML::Key << "Friction"    << YAML::Value << mc.Friction;
			out << YAML::Key << "Restitution" << YAML::Value << mc.Restitution;
			out << YAML::EndMap;
		}
		if (entity.HasComponent<CharacterControllerComponent>())
		{
			auto& cc = entity.GetComponent<CharacterControllerComponent>();
			out << YAML::Key << "CharacterControllerComponent" << YAML::BeginMap;
			out << YAML::Key << "MoveSpeed"   << YAML::Value << cc.MoveSpeed;
			out << YAML::Key << "JumpImpulse" << YAML::Value << cc.JumpImpulse;
			out << YAML::Key << "StepHeight"  << YAML::Value << cc.StepHeight;
			out << YAML::Key << "SlopeLimit"  << YAML::Value << cc.SlopeLimit;
			out << YAML::EndMap;
		}
		if (entity.HasComponent<InteractableComponent>())
		{
			auto& interactable = entity.GetComponent<InteractableComponent>();
			out << YAML::Key << "InteractableComponent" << YAML::BeginMap;
			out << YAML::Key << "Enabled" << YAML::Value << interactable.Enabled;
			out << YAML::Key << "DisplayName" << YAML::Value << interactable.DisplayName;
			out << YAML::Key << "InteractionRadius" << YAML::Value << interactable.InteractionRadius;
			out << YAML::Key << "InteractionType" << YAML::Value << static_cast<int>(interactable.Type);
			out << YAML::EndMap;
		}
		if (entity.HasComponent<PickupComponent>())
		{
			auto& pickup = entity.GetComponent<PickupComponent>();
			out << YAML::Key << "PickupComponent" << YAML::BeginMap;
			out << YAML::Key << "PickupType" << YAML::Value << static_cast<int>(pickup.Type);
			out << YAML::Key << "Amount" << YAML::Value << pickup.Amount;
			out << YAML::Key << "Count" << YAML::Value << pickup.Count;
			out << YAML::Key << "ConsumeOnPickup" << YAML::Value << pickup.ConsumeOnPickup;
			out << YAML::EndMap;
		}
		if (entity.HasComponent<PlayerStatsComponent>())
		{
			auto& stats = entity.GetComponent<PlayerStatsComponent>();
			out << YAML::Key << "PlayerStatsComponent" << YAML::BeginMap;
			out << YAML::Key << "Health" << YAML::Value << stats.Health;
			out << YAML::Key << "MaxHealth" << YAML::Value << stats.MaxHealth;
			out << YAML::Key << "Stamina" << YAML::Value << stats.Stamina;
			out << YAML::Key << "MaxStamina" << YAML::Value << stats.MaxStamina;
			out << YAML::Key << "StaminaRegenRate" << YAML::Value << stats.StaminaRegenRate;
			out << YAML::Key << "SprintStaminaDrain" << YAML::Value << stats.SprintStaminaDrain;
			out << YAML::EndMap;
		}
		if (entity.HasComponent<UIRootComponent>())
		{
			auto& ui = entity.GetComponent<UIRootComponent>();
			out << YAML::Key << "UIRootComponent" << YAML::BeginMap;
			out << YAML::Key << "DocumentPath" << YAML::Value << SerializeAssetPath(ui.DocumentPath);
			out << YAML::Key << "Visible" << YAML::Value << ui.Visible;
			out << YAML::Key << "Scale" << YAML::Value << ui.Scale;
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
		std::filesystem::path sceneFilePath = filepath;
		m_Scene->SetSceneFilePath(sceneFilePath);
		std::filesystem::create_directories(std::filesystem::path(filepath).remove_filename());

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Scene" << YAML::Value << AssetPath::ToProjectRelative(filepath);

		// ── Scene rendering settings ────────────────────────────────────────
		out << YAML::Key << "RenderSettings" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "UseShadows"   << YAML::Value << m_Scene->GetUseShadows();
		out << YAML::Key << "UsePostProcess" << YAML::Value << m_Scene->GetUsePostProcess();
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
			out << YAML::Key << "Sky_CloudSoftness"   << YAML::Value << sky.CloudSoftness;
			out << YAML::Key << "Sky_CloudHeight"     << YAML::Value << sky.CloudHeight;
			out << YAML::Key << "Sky_CloudScale"      << YAML::Value << sky.CloudScale;
			out << YAML::Key << "Sky_CloudWindDirection" << YAML::Value << sky.CloudWindDirection;
			out << YAML::Key << "Sky_CloudScrollSpeed"<< YAML::Value << sky.CloudScrollSpeed;
			out << YAML::Key << "Sky_CloudShadowing"  << YAML::Value << sky.CloudShadowing;
			out << YAML::Key << "Sky_CloudHorizonFade"<< YAML::Value << sky.CloudHorizonFade;
		}
		if (auto pp = m_Scene->GetPostProcess())
		{
			m_Scene->m_PostProcessSettings.EnableBloom    = pp->EnableBloom;
			m_Scene->m_PostProcessSettings.BloomThreshold = pp->BloomThreshold;
			m_Scene->m_PostProcessSettings.BloomStrength  = pp->BloomStrength;
			m_Scene->m_PostProcessSettings.EnableFXAA     = pp->EnableFXAA;
			m_Scene->m_PostProcessSettings.Preview        = (int)pp->Preview;
			m_Scene->m_PostProcessSettings.EnableSSAO     = pp->EnableSSAO;
			m_Scene->m_PostProcessSettings.SSAORadius     = pp->SSAORadius;
			m_Scene->m_PostProcessSettings.SSAOBias       = pp->SSAOBias;
			m_Scene->m_PostProcessSettings.SSAOPower      = pp->SSAOPower;
			m_Scene->m_PostProcessSettings.SSAOSamples    = pp->SSAOSamples;
			m_Scene->m_PostProcessSettings.SSAOStrength   = pp->SSAOStrength;
			m_Scene->m_HasPostProcessSettings = true;
		}
		if (m_Scene->m_HasPostProcessSettings)
		{
			const auto& pp = m_Scene->m_PostProcessSettings;
			out << YAML::Key << "PP_EnableBloom"    << YAML::Value << pp.EnableBloom;
			out << YAML::Key << "PP_BloomThreshold" << YAML::Value << pp.BloomThreshold;
			out << YAML::Key << "PP_BloomStrength"  << YAML::Value << pp.BloomStrength;
			out << YAML::Key << "PP_EnableFXAA"     << YAML::Value << pp.EnableFXAA;
			out << YAML::Key << "PP_PreviewMode"    << YAML::Value << pp.Preview;
			out << YAML::Key << "PP_EnableSSAO"     << YAML::Value << pp.EnableSSAO;
			out << YAML::Key << "PP_SSAORadius"     << YAML::Value << pp.SSAORadius;
			out << YAML::Key << "PP_SSAOBias"       << YAML::Value << pp.SSAOBias;
			out << YAML::Key << "PP_SSAOPower"      << YAML::Value << pp.SSAOPower;
			out << YAML::Key << "PP_SSAOSamples"    << YAML::Value << pp.SSAOSamples;
			out << YAML::Key << "PP_SSAOStrength"   << YAML::Value << pp.SSAOStrength;
		}
		{
			auto& tod = m_Scene->GetTimeOfDay();
			out << YAML::Key << "ToD_NormalizedTime"  << YAML::Value << tod.NormalizedTime;
			out << YAML::Key << "ToD_AutoAdvance"     << YAML::Value << tod.AutoAdvance;
			out << YAML::Key << "ToD_DayDuration"     << YAML::Value << tod.DayDurationSecs;
			out << YAML::Key << "ToD_SunAzimuth"     << YAML::Value << tod.SunAzimuthDeg;
			out << YAML::Key << "ToD_SunMaxStrength" << YAML::Value << tod.SunMaxStrength;
			out << YAML::Key << "ToD_SunNoonTurbidity" << YAML::Value << tod.SunNoonTurbidity;
			out << YAML::Key << "ToD_SunHazeTurbidity" << YAML::Value << tod.SunHazeTurbidity;
		}
		{
			auto& fog = m_Scene->GetFog();
			out << YAML::Key << "Fog_Enabled"         << YAML::Value << fog.Enabled;
			out << YAML::Key << "Fog_Color"           << YAML::Value << fog.Color;
			out << YAML::Key << "Fog_Density"         << YAML::Value << fog.Density;
			out << YAML::Key << "Fog_HeightStart"     << YAML::Value << fog.HeightStart;
			out << YAML::Key << "Fog_HeightDensity"   << YAML::Value << fog.HeightDensity;
			out << YAML::Key << "Fog_AerialColor"     << YAML::Value << fog.AerialColor;
			out << YAML::Key << "Fog_AerialStrength"  << YAML::Value << fog.AerialStrength;
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
		SerializeSceneAssetManifest(*m_Scene, filepath);

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

	bool SceneSerializer::SerializePrefab(Entity entity, const std::string& filepath)
	{
		if (!entity || !entity.HasComponent<IDComponent>())
			return false;

		std::filesystem::create_directories(std::filesystem::path(filepath).parent_path());

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Scene" << YAML::Value << AssetPath::ToProjectRelative(filepath);
		out << YAML::Key << "Prefab" << YAML::Value << true;
		out << YAML::Key << "PrefabVersion" << YAML::Value << 1;
		out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;
		SerializeEntity(out, entity);
		out << YAML::EndSeq;
		out << YAML::EndMap;

		std::ofstream fout(filepath);
		if (!fout)
			return false;
		fout << out.c_str();
		return true;
	}

	bool SceneSerializer::DeserializePrefab(const std::string& filepath, Entity* outEntity)
	{
		if (!std::filesystem::exists(filepath))
			return false;

		Shared<Scene> prefabScene = std::make_shared<Scene>();
		SceneSerializer prefabSerializer(prefabScene);
		if (!prefabSerializer.Deserialize(filepath))
			return false;

		auto view = prefabScene->m_Registry.view<IDComponent>();
		if (view.begin() == view.end())
			return false;

		Entity source{ *view.begin(), prefabScene.get() };
		Entity instance = m_Scene->CloneEntityFrom(source);
		if (!instance)
			return false;

		if (outEntity)
			*outEntity = instance;
		return true;
	}

	bool SceneSerializer::Deserialize(const std::string& filepath)
	{
		std::filesystem::path sceneFilePath = filepath;
		m_Scene->SetSceneFilePath(sceneFilePath);

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
			if (rs["UseShadows"])   m_Scene->SetUseShadows(rs["UseShadows"].as<bool>());
			if (rs["UsePostProcess"]) m_Scene->SetUsePostProcess(rs["UsePostProcess"].as<bool>());
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
				if (rs["Sky_CloudSoftness"])    sky.CloudSoftness    = rs["Sky_CloudSoftness"].as<float>();
				if (rs["Sky_CloudHeight"])      sky.CloudHeight      = rs["Sky_CloudHeight"].as<float>();
				if (rs["Sky_CloudScale"])       sky.CloudScale       = rs["Sky_CloudScale"].as<float>();
				if (rs["Sky_CloudWindDirection"]) sky.CloudWindDirection = rs["Sky_CloudWindDirection"].as<glm::vec2>();
				if (rs["Sky_CloudScrollSpeed"]) sky.CloudScrollSpeed = rs["Sky_CloudScrollSpeed"].as<float>();
				if (rs["Sky_CloudShadowing"])   sky.CloudShadowing   = rs["Sky_CloudShadowing"].as<float>();
				if (rs["Sky_CloudHorizonFade"]) sky.CloudHorizonFade = rs["Sky_CloudHorizonFade"].as<float>();
			}
			if (rs["PP_EnableBloom"] || rs["PP_EnableFXAA"] || rs["PP_EnableSSAO"] || rs["PP_PreviewMode"])
			{
				auto& pp = m_Scene->m_PostProcessSettings;
				if (rs["PP_EnableBloom"])    pp.EnableBloom    = rs["PP_EnableBloom"].as<bool>();
				if (rs["PP_BloomThreshold"]) pp.BloomThreshold = rs["PP_BloomThreshold"].as<float>();
				if (rs["PP_BloomStrength"])  pp.BloomStrength  = rs["PP_BloomStrength"].as<float>();
				if (rs["PP_EnableFXAA"])     pp.EnableFXAA     = rs["PP_EnableFXAA"].as<bool>();
				if (rs["PP_PreviewMode"])    pp.Preview        = rs["PP_PreviewMode"].as<int>();
				if (rs["PP_EnableSSAO"])     pp.EnableSSAO     = rs["PP_EnableSSAO"].as<bool>();
				if (rs["PP_SSAORadius"])     pp.SSAORadius     = rs["PP_SSAORadius"].as<float>();
				if (rs["PP_SSAOBias"])       pp.SSAOBias       = rs["PP_SSAOBias"].as<float>();
				if (rs["PP_SSAOPower"])      pp.SSAOPower      = rs["PP_SSAOPower"].as<float>();
				if (rs["PP_SSAOSamples"])    pp.SSAOSamples    = rs["PP_SSAOSamples"].as<int>();
				if (rs["PP_SSAOStrength"])   pp.SSAOStrength   = rs["PP_SSAOStrength"].as<float>();
				pp.Preview = std::clamp(pp.Preview, 0, 5);
				m_Scene->m_HasPostProcessSettings = true;
				if (auto runtimePP = m_Scene->GetPostProcess())
				{
					runtimePP->EnableBloom    = pp.EnableBloom;
					runtimePP->BloomThreshold = pp.BloomThreshold;
					runtimePP->BloomStrength  = pp.BloomStrength;
					runtimePP->EnableFXAA     = pp.EnableFXAA;
					runtimePP->Preview        = (PostProcess::PreviewMode)pp.Preview;
					runtimePP->EnableSSAO     = pp.EnableSSAO;
					runtimePP->SSAORadius     = pp.SSAORadius;
					runtimePP->SSAOBias       = pp.SSAOBias;
					runtimePP->SSAOPower      = pp.SSAOPower;
					runtimePP->SSAOSamples    = pp.SSAOSamples;
					runtimePP->SSAOStrength   = pp.SSAOStrength;
				}
			}
			{
				auto& tod = m_Scene->GetTimeOfDay();
				if (rs["ToD_NormalizedTime"])  tod.NormalizedTime  = rs["ToD_NormalizedTime"].as<float>();
				if (rs["ToD_AutoAdvance"])     tod.AutoAdvance     = rs["ToD_AutoAdvance"].as<bool>();
				if (rs["ToD_DayDuration"])     tod.DayDurationSecs = rs["ToD_DayDuration"].as<float>();
				if (rs["ToD_SunAzimuth"])      tod.SunAzimuthDeg   = rs["ToD_SunAzimuth"].as<float>();
				if (rs["ToD_SunMaxStrength"])  tod.SunMaxStrength  = rs["ToD_SunMaxStrength"].as<float>();
				if (rs["ToD_SunNoonTurbidity"]) tod.SunNoonTurbidity = rs["ToD_SunNoonTurbidity"].as<float>();
				if (rs["ToD_SunHazeTurbidity"]) tod.SunHazeTurbidity = rs["ToD_SunHazeTurbidity"].as<float>();
			}
			{
				auto& fog = m_Scene->GetFog();
				if (rs["Fog_Enabled"])      fog.Enabled      = rs["Fog_Enabled"].as<bool>();
				if (rs["Fog_Color"])        fog.Color        = rs["Fog_Color"].as<glm::vec3>();
				if (rs["Fog_Density"])      fog.Density      = rs["Fog_Density"].as<float>();
				if (rs["Fog_HeightStart"])  fog.HeightStart  = rs["Fog_HeightStart"].as<float>();
				if (rs["Fog_HeightDensity"])fog.HeightDensity= rs["Fog_HeightDensity"].as<float>();
				if (rs["Fog_AerialColor"])  fog.AerialColor  = rs["Fog_AerialColor"].as<glm::vec3>();
				if (rs["Fog_AerialStrength"]) fog.AerialStrength = rs["Fog_AerialStrength"].as<float>();
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

				auto visualOffsetComponent = entity["VisualOffsetComponent"];
				if (visualOffsetComponent)
				{
					auto& voc = deserializedEntity.AddComponent<VisualOffsetComponent>();
					if (visualOffsetComponent["Translation"]) voc.Translation = visualOffsetComponent["Translation"].as<glm::vec3>();
					if (visualOffsetComponent["Rotation"])    voc.Rotation    = visualOffsetComponent["Rotation"].as<glm::vec3>();
					if (visualOffsetComponent["Scale"])       voc.Scale       = visualOffsetComponent["Scale"].as<glm::vec3>();
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
								texture = LoadSceneTexture(texturePath, sceneFilePath, yamlKey);
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

				auto meshCollider3DComponent = entity["MeshCollider3DComponent"];
				if (meshCollider3DComponent)
				{
					auto& mc = deserializedEntity.AddComponent<MeshCollider3DComponent>();
					if (meshCollider3DComponent["Enabled"])     mc.Enabled     = meshCollider3DComponent["Enabled"].as<bool>();
					if (meshCollider3DComponent["DoubleSided"]) mc.DoubleSided = meshCollider3DComponent["DoubleSided"].as<bool>();
					if (meshCollider3DComponent["Friction"])    mc.Friction    = meshCollider3DComponent["Friction"].as<float>();
					if (meshCollider3DComponent["Restitution"]) mc.Restitution = meshCollider3DComponent["Restitution"].as<float>();
				}

				auto characterControllerComponent = entity["CharacterControllerComponent"];
				if (characterControllerComponent)
				{
					auto& cc = deserializedEntity.AddComponent<CharacterControllerComponent>();
					if (characterControllerComponent["MoveSpeed"])   cc.MoveSpeed   = characterControllerComponent["MoveSpeed"].as<float>();
					if (characterControllerComponent["JumpImpulse"]) cc.JumpImpulse = characterControllerComponent["JumpImpulse"].as<float>();
					if (characterControllerComponent["StepHeight"])  cc.StepHeight  = characterControllerComponent["StepHeight"].as<float>();
					if (characterControllerComponent["SlopeLimit"])  cc.SlopeLimit  = characterControllerComponent["SlopeLimit"].as<float>();
				}

				auto interactableComponent = entity["InteractableComponent"];
				if (interactableComponent)
				{
					auto& interactable = deserializedEntity.AddComponent<InteractableComponent>();
					if (interactableComponent["Enabled"]) interactable.Enabled = interactableComponent["Enabled"].as<bool>();
					if (interactableComponent["DisplayName"]) interactable.DisplayName = interactableComponent["DisplayName"].as<std::string>();
					if (interactableComponent["InteractionRadius"]) interactable.InteractionRadius = interactableComponent["InteractionRadius"].as<float>();
					if (interactableComponent["InteractionType"]) interactable.Type = static_cast<InteractableComponent::InteractionType>(interactableComponent["InteractionType"].as<int>());
				}

				auto pickupComponent = entity["PickupComponent"];
				if (pickupComponent)
				{
					auto& pickup = deserializedEntity.AddComponent<PickupComponent>();
					if (pickupComponent["PickupType"]) pickup.Type = static_cast<PickupComponent::PickupType>(pickupComponent["PickupType"].as<int>());
					if (pickupComponent["Amount"]) pickup.Amount = pickupComponent["Amount"].as<float>();
					if (pickupComponent["Count"]) pickup.Count = pickupComponent["Count"].as<int>();
					if (pickupComponent["ConsumeOnPickup"]) pickup.ConsumeOnPickup = pickupComponent["ConsumeOnPickup"].as<bool>();
				}

				auto playerStatsComponent = entity["PlayerStatsComponent"];
				if (playerStatsComponent)
				{
					auto& stats = deserializedEntity.AddComponent<PlayerStatsComponent>();
					if (playerStatsComponent["Health"]) stats.Health = playerStatsComponent["Health"].as<float>();
					if (playerStatsComponent["MaxHealth"]) stats.MaxHealth = playerStatsComponent["MaxHealth"].as<float>();
					if (playerStatsComponent["Stamina"]) stats.Stamina = playerStatsComponent["Stamina"].as<float>();
					if (playerStatsComponent["MaxStamina"]) stats.MaxStamina = playerStatsComponent["MaxStamina"].as<float>();
					if (playerStatsComponent["StaminaRegenRate"]) stats.StaminaRegenRate = playerStatsComponent["StaminaRegenRate"].as<float>();
					if (playerStatsComponent["SprintStaminaDrain"]) stats.SprintStaminaDrain = playerStatsComponent["SprintStaminaDrain"].as<float>();
					stats.MaxHealth = std::max(stats.MaxHealth, 1.0f);
					stats.MaxStamina = std::max(stats.MaxStamina, 1.0f);
					stats.Health = std::clamp(stats.Health, 0.0f, stats.MaxHealth);
					stats.Stamina = std::clamp(stats.Stamina, 0.0f, stats.MaxStamina);
				}

				auto uiRootComponent = entity["UIRootComponent"];
				if (uiRootComponent)
				{
					auto& ui = deserializedEntity.AddComponent<UIRootComponent>();
					if (uiRootComponent["DocumentPath"])
						ui.DocumentPath = NormalizeLoadedAssetPath(uiRootComponent["DocumentPath"].as<std::string>(), sceneFilePath, "UIRootComponent.DocumentPath");
					if (uiRootComponent["Visible"]) ui.Visible = uiRootComponent["Visible"].as<bool>();
					if (uiRootComponent["Scale"]) ui.Scale = std::max(0.1f, uiRootComponent["Scale"].as<float>());
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
						std::string rawPath = meshComponent["FilePath"].as<std::string>();
						mc.FilePath = NormalizeLoadedAssetPath(rawPath, sceneFilePath, "MeshComponent.FilePath");
						if (!mc.FilePath.empty())
							mc.ModelAsset = ModelLoader::Load(ResolveAssetPathForLoad(mc.FilePath, sceneFilePath, "MeshComponent.FilePath").string());
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
							tex = LoadSceneTexture(meshComponent[key].as<std::string>(), sceneFilePath, key);
					};
					loadTex("Tex_Albedo",  mc.MaterialInstance->AlbedoMap);
					loadTex("Tex_Normal",  mc.MaterialInstance->NormalMap);
					loadTex("Tex_MetallicRoughness", mc.MaterialInstance->MetallicRoughnessMap);
					loadTex("Tex_AO",      mc.MaterialInstance->AOMap);
					loadTex("Tex_Emissive", mc.MaterialInstance->EmissiveMap);
				}

				auto meshLODComponent = entity["MeshLODComponent"];
				if (meshLODComponent)
				{
					auto& lod = deserializedEntity.AddComponent<MeshLODComponent>();
					if (meshLODComponent["Active"])
						lod.Active = meshLODComponent["Active"].as<bool>();
					if (meshLODComponent["Levels"])
					{
						for (auto levelNode : meshLODComponent["Levels"])
						{
							LODEntry level;
							if (levelNode["FilePath"])
							{
								std::string rawPath = levelNode["FilePath"].as<std::string>();
								level.FilePath = NormalizeLoadedAssetPath(rawPath, sceneFilePath, "MeshLODComponent.FilePath");
								if (!level.FilePath.empty())
									level.ModelAsset = ModelLoader::Load(ResolveAssetPathForLoad(level.FilePath, sceneFilePath, "MeshLODComponent.FilePath").string());
							}
							if (levelNode["MaxDistance"])
								level.MaxDistance = levelNode["MaxDistance"].as<float>();
							lod.Levels.push_back(level);
						}
					}
				}

				auto foliageComponent = entity["FoliageComponent"];
				if (foliageComponent)
				{
					auto& foliage = deserializedEntity.AddComponent<FoliageComponent>();
					if (foliageComponent["FilePath"])
					{
						std::string rawPath = foliageComponent["FilePath"].as<std::string>();
						foliage.FilePath = NormalizeLoadedAssetPath(rawPath, sceneFilePath, "FoliageComponent.FilePath");
						if (!foliage.FilePath.empty())
							foliage.ModelAsset = ModelLoader::Load(ResolveAssetPathForLoad(foliage.FilePath, sceneFilePath, "FoliageComponent.FilePath").string());
					}
					if (foliageComponent["WindEnabled"])   foliage.WindEnabled   = foliageComponent["WindEnabled"].as<bool>();
					if (foliageComponent["WindStrength"])  foliage.WindStrength  = foliageComponent["WindStrength"].as<float>();
					if (foliageComponent["WindFrequency"]) foliage.WindFrequency = foliageComponent["WindFrequency"].as<float>();
					if (foliageComponent["WindDirection"]) foliage.WindDirection = foliageComponent["WindDirection"].as<glm::vec3>();
					if (foliageComponent["Transforms"])
					{
						for (auto transformNode : foliageComponent["Transforms"])
							foliage.Transforms.push_back(transformNode.as<glm::mat4>());
					}
				}

				auto audioSourceComponent = entity["AudioSourceComponent"];
				if (audioSourceComponent)
				{
					auto& audio = deserializedEntity.AddComponent<AudioSourceComponent>();
					if (audioSourceComponent["FilePath"])
						audio.FilePath = NormalizeLoadedAssetPath(audioSourceComponent["FilePath"].as<std::string>(), sceneFilePath, "AudioSourceComponent.FilePath");
					if (audioSourceComponent["Volume"])      audio.Volume      = audioSourceComponent["Volume"].as<float>();
					if (audioSourceComponent["Pitch"])       audio.Pitch       = audioSourceComponent["Pitch"].as<float>();
					if (audioSourceComponent["Loop"])        audio.Loop        = audioSourceComponent["Loop"].as<bool>();
					if (audioSourceComponent["PlayOnStart"]) audio.PlayOnStart = audioSourceComponent["PlayOnStart"].as<bool>();
					if (audioSourceComponent["Spatial"])     audio.Spatial     = audioSourceComponent["Spatial"].as<bool>();
					if (audioSourceComponent["MinDistance"]) audio.MinDistance = audioSourceComponent["MinDistance"].as<float>();
					if (audioSourceComponent["MaxDistance"]) audio.MaxDistance = audioSourceComponent["MaxDistance"].as<float>();
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
