#include "Blupch.h"
#include "MaterialSystem.h"
#include "AssetManager.h"

namespace Blu
{
	MaterialResolver& MaterialResolver::Get()
	{
		static MaterialResolver instance;
		return instance;
	}

	void MaterialResolver::RegisterTemplate(const Shared<MaterialTemplate>& materialTemplate)
	{
		if (materialTemplate && (uint64_t)materialTemplate->Handle != 0)
			m_Templates[materialTemplate->Handle] = materialTemplate;
	}

	void MaterialResolver::RegisterInstance(const Shared<MaterialInstance>& materialInstance)
	{
		if (materialInstance && (uint64_t)materialInstance->Handle != 0)
			m_Instances[materialInstance->Handle] = materialInstance;
	}

	ResolvedMaterial MaterialResolver::Resolve(
		AssetHandle templateHandle,
		AssetHandle instanceHandle,
		const MaterialRenderContext& context) const
	{
		ResolvedMaterial resolved;
		const MaterialInstance* materialInstance = nullptr;
		if ((uint64_t)instanceHandle != 0)
		{
			auto instance = m_Instances.find(instanceHandle);
			if (instance != m_Instances.end())
				materialInstance = instance->second.get();
			else
				resolved.UsedFallbackInstance = true;
		}

		const AssetHandle effectiveTemplateHandle = materialInstance && (uint64_t)materialInstance->TemplateHandle != 0
			? materialInstance->TemplateHandle
			: templateHandle;
		auto materialTemplate = m_Templates.find(effectiveTemplateHandle);
		if (materialTemplate != m_Templates.end())
		{
			const auto& source = *materialTemplate->second;
			resolved.Domain = source.Domain;
			resolved.Blend = source.Blend;
			resolved.Shading = source.Shading;
			resolved.TwoSided = source.TwoSided;
			resolved.Features = source.Features;
			resolved.Parameters = source.Defaults;
			resolved.Textures = source.Textures;
		}
		else
		{
			resolved.UsedFallbackTemplate = true;
		}

		if (materialInstance)
			ApplyOverrides(resolved, *materialInstance);

		resolved.EffectivePath = context.Path;
		if (resolved.Domain != ShaderDomain::Surface || resolved.Blend == BlendMode::Transparent || resolved.Blend == BlendMode::Additive)
			resolved.EffectivePath = RenderPath::Forward;
		CollectMissingTextures(resolved);
		resolved.Permutation = BuildPermutation(resolved, context);
		return resolved;
	}

	ResolvedMaterial MaterialResolver::ResolveLegacy(const Material& material, const MaterialRenderContext& context) const
	{
		ResolvedMaterial resolved;
		resolved.Blend = material.Blend;
		resolved.Shading = material.Shading;
		resolved.TwoSided = material.TwoSided;
		resolved.Parameters.AlbedoColor = material.AlbedoColor;
		resolved.Parameters.Metallic = material.Metallic;
		resolved.Parameters.Roughness = material.Roughness;
		resolved.Parameters.AO = material.AO;
		resolved.Parameters.EmissiveColor = material.EmissiveColor;
		resolved.Parameters.EmissiveStrength = material.EmissiveStrength;
		resolved.Parameters.AlphaCutoff = material.AlphaCutoff;
		if (material.NormalMap) resolved.Features |= (uint32_t)MaterialFeature::NormalMap;
		if (material.MetallicRoughnessMap) resolved.Features |= (uint32_t)MaterialFeature::MetallicRoughnessMap;
		if (material.AOMap) resolved.Features |= (uint32_t)MaterialFeature::AOMap;
		if (material.EmissiveMap) resolved.Features |= (uint32_t)MaterialFeature::EmissiveMap;
		resolved.EffectivePath = context.Path;
		if (resolved.Blend == BlendMode::Transparent || resolved.Blend == BlendMode::Additive)
			resolved.EffectivePath = RenderPath::Forward;
		resolved.Permutation = BuildPermutation(resolved, context);
		return resolved;
	}

	void MaterialResolver::Clear()
	{
		m_Templates.clear();
		m_Instances.clear();
	}

	MaterialPermutationKey MaterialResolver::BuildPermutation(const ResolvedMaterial& material, const MaterialRenderContext& context)
	{
		MaterialPermutationKey key;
		key.Set(material.EffectivePath == RenderPath::Deferred ? MaterialPermutation::Deferred : MaterialPermutation::Forward);
		if (context.Skinned) key.Set(MaterialPermutation::Skinned);
		else if (context.Foliage) key.Set(MaterialPermutation::Foliage);
		else key.Set(MaterialPermutation::Static);

		switch (material.Blend)
		{
			case BlendMode::Masked: key.Set(MaterialPermutation::Masked); break;
			case BlendMode::Transparent: key.Set(MaterialPermutation::Transparent); break;
			case BlendMode::Additive: key.Set(MaterialPermutation::Additive); break;
			default: key.Set(MaterialPermutation::Opaque); break;
		}

		if (material.TwoSided) key.Set(MaterialPermutation::TwoSided);
		if (material.Features & (uint32_t)MaterialFeature::NormalMap) key.Set(MaterialPermutation::NormalMap);
		if (material.Features & (uint32_t)MaterialFeature::MetallicRoughnessMap) key.Set(MaterialPermutation::MetallicRoughnessMap);
		if (material.Features & (uint32_t)MaterialFeature::AOMap) key.Set(MaterialPermutation::AOMap);
		if (material.Features & (uint32_t)MaterialFeature::EmissiveMap) key.Set(MaterialPermutation::EmissiveMap);
		return key;
	}

	void MaterialResolver::ApplyOverrides(ResolvedMaterial& target, const MaterialInstance& source)
	{
		if (source.Overrides.AlbedoColor) target.Parameters.AlbedoColor = *source.Overrides.AlbedoColor;
		if (source.Overrides.Metallic) target.Parameters.Metallic = *source.Overrides.Metallic;
		if (source.Overrides.Roughness) target.Parameters.Roughness = *source.Overrides.Roughness;
		if (source.Overrides.AO) target.Parameters.AO = *source.Overrides.AO;
		if (source.Overrides.EmissiveColor) target.Parameters.EmissiveColor = *source.Overrides.EmissiveColor;
		if (source.Overrides.EmissiveStrength) target.Parameters.EmissiveStrength = *source.Overrides.EmissiveStrength;
		if (source.Overrides.AlphaCutoff) target.Parameters.AlphaCutoff = *source.Overrides.AlphaCutoff;
		if (source.Textures.Albedo) target.Textures.Albedo = *source.Textures.Albedo;
		if (source.Textures.Normal) target.Textures.Normal = *source.Textures.Normal;
		if (source.Textures.MetallicRoughness) target.Textures.MetallicRoughness = *source.Textures.MetallicRoughness;
		if (source.Textures.AO) target.Textures.AO = *source.Textures.AO;
		if (source.Textures.Emissive) target.Textures.Emissive = *source.Textures.Emissive;
	}

	void MaterialResolver::CollectMissingTextures(ResolvedMaterial& material)
	{
		auto collect = [&material](AssetHandle handle, const char* slot)
		{
			if ((uint64_t)handle != 0 && !AssetManager::Get().FindMetadata(handle))
				material.MissingTextureSlots.emplace_back(slot);
		};
		collect(material.Textures.Albedo, "Albedo");
		collect(material.Textures.Normal, "Normal");
		collect(material.Textures.MetallicRoughness, "MetallicRoughness");
		collect(material.Textures.AO, "AO");
		collect(material.Textures.Emissive, "Emissive");
	}
}
