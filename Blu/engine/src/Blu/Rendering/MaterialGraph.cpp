#include "Blupch.h"
#include "MaterialGraph.h"
#include <algorithm>

namespace Blu
{
	MaterialGraphNodeID MaterialGraph::AddScalarParameter(std::string name, float value)
	{
		return AddNode(std::move(name), MaterialGraphValueType::Scalar, value);
	}

	MaterialGraphNodeID MaterialGraph::AddVector3Parameter(std::string name, const glm::vec3& value)
	{
		return AddNode(std::move(name), MaterialGraphValueType::Vector3, value);
	}

	MaterialGraphNodeID MaterialGraph::AddVector4Parameter(std::string name, const glm::vec4& value)
	{
		return AddNode(std::move(name), MaterialGraphValueType::Vector4, value);
	}

	MaterialGraphNodeID MaterialGraph::AddTextureParameter(std::string name, AssetHandle value)
	{
		return AddNode(std::move(name), MaterialGraphValueType::Texture, value);
	}

	void MaterialGraph::Connect(MaterialGraphInput input, MaterialGraphNodeID nodeID)
	{
		m_Connections[input] = nodeID;
	}

	void MaterialGraph::Disconnect(MaterialGraphInput input)
	{
		m_Connections.erase(input);
	}

	bool MaterialGraph::IsConnected(MaterialGraphInput input) const
	{
		return m_Connections.contains(input);
	}

	MaterialGraphNodeID MaterialGraph::AddNode(std::string name, MaterialGraphValueType type, MaterialGraphValue value)
	{
		const MaterialGraphNodeID id = m_NextNodeID++;
		m_Nodes.push_back({ id, std::move(name), type, std::move(value) });
		return id;
	}

	MaterialGraphCompileResult MaterialGraphCompiler::Compile(const MaterialGraph& graph, AssetHandle templateHandle)
	{
		MaterialGraphCompileResult result;
		auto materialTemplate = std::make_shared<MaterialTemplate>();
		materialTemplate->Handle = templateHandle;
		materialTemplate->Domain = graph.Domain;
		materialTemplate->Blend = graph.Blend;
		materialTemplate->Shading = graph.Shading;
		materialTemplate->TwoSided = graph.TwoSided;

		auto findNode = [&](MaterialGraphInput input) -> const MaterialGraphNode*
		{
			auto connection = graph.GetConnections().find(input);
			if (connection == graph.GetConnections().end())
				return nullptr;
			auto node = std::find_if(graph.GetNodes().begin(), graph.GetNodes().end(),
				[&](const MaterialGraphNode& candidate) { return candidate.ID == connection->second; });
			if (node == graph.GetNodes().end())
			{
				result.Diagnostics.emplace_back("Material graph connection references a missing node.");
				return nullptr;
			}
			return &*node;
		};

		auto assignScalar = [&](MaterialGraphInput input, float& target, const char* slot)
		{
			if (const auto* node = findNode(input))
			{
				if (node->Type != MaterialGraphValueType::Scalar)
					result.Diagnostics.emplace_back(std::string(slot) + " expects a scalar parameter.");
				else
					target = std::get<float>(node->Value);
			}
		};
		auto assignVector3 = [&](MaterialGraphInput input, glm::vec3& target, const char* slot)
		{
			if (const auto* node = findNode(input))
			{
				if (node->Type != MaterialGraphValueType::Vector3)
					result.Diagnostics.emplace_back(std::string(slot) + " expects a vector3 parameter.");
				else
					target = std::get<glm::vec3>(node->Value);
			}
		};
		auto assignVector4 = [&](MaterialGraphInput input, glm::vec4& target, const char* slot)
		{
			if (const auto* node = findNode(input))
			{
				if (node->Type != MaterialGraphValueType::Vector4)
					result.Diagnostics.emplace_back(std::string(slot) + " expects a vector4 parameter.");
				else
					target = std::get<glm::vec4>(node->Value);
			}
		};
		auto assignTexture = [&](MaterialGraphInput input, AssetHandle& target, MaterialFeature feature, const char* slot)
		{
			if (const auto* node = findNode(input))
			{
				if (node->Type != MaterialGraphValueType::Texture)
				{
					result.Diagnostics.emplace_back(std::string(slot) + " expects a texture parameter.");
					return;
				}
				target = std::get<AssetHandle>(node->Value);
				if (feature != MaterialFeature::None && (uint64_t)target != 0)
					materialTemplate->Features |= (uint32_t)feature;
			}
		};

		assignVector4(MaterialGraphInput::AlbedoColor, materialTemplate->Defaults.AlbedoColor, "AlbedoColor");
		assignScalar(MaterialGraphInput::Metallic, materialTemplate->Defaults.Metallic, "Metallic");
		assignScalar(MaterialGraphInput::Roughness, materialTemplate->Defaults.Roughness, "Roughness");
		assignScalar(MaterialGraphInput::AO, materialTemplate->Defaults.AO, "AO");
		assignVector3(MaterialGraphInput::EmissiveColor, materialTemplate->Defaults.EmissiveColor, "EmissiveColor");
		assignScalar(MaterialGraphInput::EmissiveStrength, materialTemplate->Defaults.EmissiveStrength, "EmissiveStrength");
		assignScalar(MaterialGraphInput::AlphaCutoff, materialTemplate->Defaults.AlphaCutoff, "AlphaCutoff");
		assignTexture(MaterialGraphInput::AlbedoTexture, materialTemplate->Textures.Albedo, MaterialFeature::None, "AlbedoTexture");
		assignTexture(MaterialGraphInput::NormalTexture, materialTemplate->Textures.Normal, MaterialFeature::NormalMap, "NormalTexture");
		assignTexture(MaterialGraphInput::MetallicRoughnessTexture, materialTemplate->Textures.MetallicRoughness, MaterialFeature::MetallicRoughnessMap, "MetallicRoughnessTexture");
		assignTexture(MaterialGraphInput::AOTexture, materialTemplate->Textures.AO, MaterialFeature::AOMap, "AOTexture");
		assignTexture(MaterialGraphInput::EmissiveTexture, materialTemplate->Textures.Emissive, MaterialFeature::EmissiveMap, "EmissiveTexture");

		if (result.Diagnostics.empty())
			result.Template = std::move(materialTemplate);
		return result;
	}
}
