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

	struct alignas(16) LightDataGPU
	{
		DirLightGPU DirLights[4];
		PointLightGPU PointLights[8];
		SpotLightGPU SpotLights[4];
		int NumDirLights;
		int NumPointLights;
		int NumSpotLights;
		float PadL;
	};
	static_assert(sizeof(LightDataGPU) == 1360, "LightDataGPU layout must match the shared HLSL cbuffer");

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
