#pragma once
#include "CascadedShadowMap.h"
#include <glm/glm.hpp>
#include <vector>

namespace Blu
{
	struct DirLightData;
	struct PointLightData;
	struct SpotLightData;

	struct alignas(16) DirLightGPU
	{
		glm::vec3 Direction; float pad0;
		glm::vec3 Ambient;   float Intensity;
		glm::vec3 Diffuse;   float pad1;
		glm::vec3 Specular;  float pad2;
	};

	struct alignas(16) PointLightGPU
	{
		glm::vec3 Position; float Range;
		glm::vec3 Ambient;  float Intensity;
		glm::vec3 Diffuse;  float pad0;
		glm::vec3 Specular; float pad1;
		glm::vec3 Att;      float pad2;
	};

	struct alignas(16) SpotLightGPU
	{
		glm::vec3 Position;  float Range;
		glm::vec3 Direction; float Intensity;
		glm::vec3 Ambient;   float pad0;
		glm::vec3 Diffuse;   float pad1;
		glm::vec3 Specular;  float pad2;
		glm::vec3 Att;       float InnerCutoff;
		float OuterCutoff;   float pad3[3];
	};

	// kMaxPointLights: budget for the single-cbuffer light blob shared by the forward
	// mesh/instanced/skinned shaders and the deferred lighting pass. Raised from 8 to 32 so
	// scene lights + transient muzzle-flash/impact lights coexist. MUST stay in lockstep with
	// `u_PointLights[N]` in every HLSL that declares the LightData cbuffer (Deferred_Lighting,
	// PBR_Mesh, Renderer3D_Mesh, Skinned_Mesh, Foliage_Instanced) and with the static_assert below.
	static constexpr int kMaxPointLights = 32;

	struct alignas(16) LightDataGPU
	{
		DirLightGPU DirLights[4];
		PointLightGPU PointLights[kMaxPointLights];
		SpotLightGPU SpotLights[4];
		int NumDirLights;
		int NumPointLights;
		int NumSpotLights;
		float PadL;
	};
	// 256 (4 dir) + 2560 (32 point * 80) + 448 (4 spot * 112) + 16 (counts) = 3280.
	static_assert(sizeof(LightDataGPU) == 3280, "LightDataGPU layout must match the shared HLSL cbuffer");

	struct alignas(16) ShadowDataGPU
	{
		glm::mat4 LightVPs[CascadedShadowMap::NUM_CASCADES];
		glm::vec3 CascadeSplits;
		float ShadowMapSize;
	};
	static_assert(sizeof(ShadowDataGPU) == 208, "ShadowDataGPU layout must match the shared HLSL cbuffer");

	LightDataGPU BuildLightDataGPU(
		const std::vector<DirLightData>& dirLights,
		const std::vector<PointLightData>& pointLights,
		const std::vector<SpotLightData>& spotLights);
}
