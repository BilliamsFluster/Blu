#pragma once
#include "Asset.h"
#include "Material.h"
#include "RenderSettings.h"
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Blu
{
	enum class ShaderDomain : uint8_t
	{
		Surface,
		PostProcess,
		UI
	};

	enum class MaterialFeature : uint32_t
	{
		None = 0,
		NormalMap = BIT(0),
		MetallicRoughnessMap = BIT(1),
		AOMap = BIT(2),
		EmissiveMap = BIT(3)
	};

	enum class MaterialPermutation : uint32_t
	{
		Forward = BIT(0),
		Deferred = BIT(1),
		Static = BIT(2),
		Skinned = BIT(3),
		Foliage = BIT(4),
		Opaque = BIT(5),
		Masked = BIT(6),
		Transparent = BIT(7),
		Additive = BIT(8),
		TwoSided = BIT(9),
		NormalMap = BIT(10),
		MetallicRoughnessMap = BIT(11),
		AOMap = BIT(12),
		EmissiveMap = BIT(13)
	};

	struct MaterialPermutationKey
	{
		uint32_t Bits = 0;

		bool Has(MaterialPermutation permutation) const
		{
			return (Bits & (uint32_t)permutation) != 0;
		}

		void Set(MaterialPermutation permutation)
		{
			Bits |= (uint32_t)permutation;
		}

		bool operator==(const MaterialPermutationKey&) const = default;
	};

	struct MaterialParameters
	{
		glm::vec4 AlbedoColor = glm::vec4(1.0f);
		float Metallic = 0.0f;
		float Roughness = 0.5f;
		float AO = 1.0f;
		glm::vec3 EmissiveColor = glm::vec3(0.0f);
		float EmissiveStrength = 0.0f;
		float AlphaCutoff = 0.5f;
	};

	struct MaterialParameterOverrides
	{
		std::optional<glm::vec4> AlbedoColor;
		std::optional<float> Metallic;
		std::optional<float> Roughness;
		std::optional<float> AO;
		std::optional<glm::vec3> EmissiveColor;
		std::optional<float> EmissiveStrength;
		std::optional<float> AlphaCutoff;
	};

	struct MaterialTextureSlots
	{
		AssetHandle Albedo = AssetHandle(0);
		AssetHandle Normal = AssetHandle(0);
		AssetHandle MetallicRoughness = AssetHandle(0);
		AssetHandle AO = AssetHandle(0);
		AssetHandle Emissive = AssetHandle(0);
	};

	struct MaterialTextureOverrides
	{
		std::optional<AssetHandle> Albedo;
		std::optional<AssetHandle> Normal;
		std::optional<AssetHandle> MetallicRoughness;
		std::optional<AssetHandle> AO;
		std::optional<AssetHandle> Emissive;
	};

	class MaterialTemplate : public Asset
	{
	public:
		MaterialTemplate() { Type = AssetType::Material; }

		ShaderDomain Domain = ShaderDomain::Surface;
		BlendMode Blend = BlendMode::Opaque;
		ShadingModel Shading = ShadingModel::PBR;
		bool TwoSided = false;
		uint32_t Features = 0;
		MaterialParameters Defaults;
		MaterialTextureSlots Textures;
	};

	class MaterialInstance : public Asset
	{
	public:
		MaterialInstance() { Type = AssetType::Material; }

		AssetHandle TemplateHandle = AssetHandle(0);
		MaterialParameterOverrides Overrides;
		MaterialTextureOverrides Textures;
	};

	struct MaterialRenderContext
	{
		RenderPath Path = RenderPath::Forward;
		bool Skinned = false;
		bool Foliage = false;
	};

	struct ResolvedMaterial
	{
		ShaderDomain Domain = ShaderDomain::Surface;
		BlendMode Blend = BlendMode::Opaque;
		ShadingModel Shading = ShadingModel::PBR;
		bool TwoSided = false;
		uint32_t Features = 0;
		MaterialParameters Parameters;
		MaterialTextureSlots Textures;
		RenderPath EffectivePath = RenderPath::Forward;
		MaterialPermutationKey Permutation;
		std::vector<std::string> MissingTextureSlots;
		bool UsedFallbackTemplate = false;
		bool UsedFallbackInstance = false;
	};

	class MaterialResolver
	{
	public:
		static MaterialResolver& Get();

		void RegisterTemplate(const Shared<MaterialTemplate>& materialTemplate);
		void RegisterInstance(const Shared<MaterialInstance>& materialInstance);
		ResolvedMaterial Resolve(AssetHandle templateHandle, AssetHandle instanceHandle, const MaterialRenderContext& context) const;
		ResolvedMaterial ResolveLegacy(const Material& material, const MaterialRenderContext& context) const;
		void Clear();

	private:
		static MaterialPermutationKey BuildPermutation(const ResolvedMaterial& material, const MaterialRenderContext& context);
		static void ApplyOverrides(ResolvedMaterial& target, const MaterialInstance& source);
		static void CollectMissingTextures(ResolvedMaterial& material);

		std::unordered_map<AssetHandle, Shared<MaterialTemplate>> m_Templates;
		std::unordered_map<AssetHandle, Shared<MaterialInstance>> m_Instances;
	};
}
