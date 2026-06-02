#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include "MaterialSystem.h"

namespace Blu
{
	using MaterialGraphNodeID = uint32_t;

	enum class MaterialGraphValueType : uint8_t
	{
		Scalar,
		Vector3,
		Vector4,
		Texture
	};

	enum class MaterialGraphInput : uint8_t
	{
		AlbedoColor,
		Metallic,
		Roughness,
		AO,
		EmissiveColor,
		EmissiveStrength,
		AlphaCutoff,
		AlbedoTexture,
		NormalTexture,
		MetallicRoughnessTexture,
		AOTexture,
		EmissiveTexture
	};

	using MaterialGraphValue = std::variant<float, glm::vec3, glm::vec4, AssetHandle>;

	struct MaterialGraphNode
	{
		MaterialGraphNodeID ID = 0;
		std::string Name;
		MaterialGraphValueType Type = MaterialGraphValueType::Scalar;
		MaterialGraphValue Value = 0.0f;
	};

	class MaterialGraph
	{
	public:
		ShaderDomain Domain = ShaderDomain::Surface;
		BlendMode Blend = BlendMode::Opaque;
		ShadingModel Shading = ShadingModel::PBR;
		bool TwoSided = false;

		MaterialGraphNodeID AddScalarParameter(std::string name, float value);
		MaterialGraphNodeID AddVector3Parameter(std::string name, const glm::vec3& value);
		MaterialGraphNodeID AddVector4Parameter(std::string name, const glm::vec4& value);
		MaterialGraphNodeID AddTextureParameter(std::string name, AssetHandle value);
		void Connect(MaterialGraphInput input, MaterialGraphNodeID nodeID);
		void Disconnect(MaterialGraphInput input);
		bool IsConnected(MaterialGraphInput input) const;

		std::vector<MaterialGraphNode>& GetNodes() { return m_Nodes; }
		const std::vector<MaterialGraphNode>& GetNodes() const { return m_Nodes; }
		const std::unordered_map<MaterialGraphInput, MaterialGraphNodeID>& GetConnections() const { return m_Connections; }

	private:
		MaterialGraphNodeID AddNode(std::string name, MaterialGraphValueType type, MaterialGraphValue value);

		MaterialGraphNodeID m_NextNodeID = 1;
		std::vector<MaterialGraphNode> m_Nodes;
		std::unordered_map<MaterialGraphInput, MaterialGraphNodeID> m_Connections;
	};

	struct MaterialGraphCompileResult
	{
		Shared<MaterialTemplate> Template;
		std::vector<std::string> Diagnostics;

		bool Succeeded() const { return Template != nullptr && Diagnostics.empty(); }
	};

	class MaterialGraphCompiler
	{
	public:
		static MaterialGraphCompileResult Compile(const MaterialGraph& graph, AssetHandle templateHandle);
	};
}
