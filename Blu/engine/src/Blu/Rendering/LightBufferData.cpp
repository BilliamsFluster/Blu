#include "Blupch.h"
#include "LightBufferData.h"
#include "Renderer3D.h"
#include <algorithm>

namespace Blu
{
	LightDataGPU BuildLightDataGPU(
		const std::vector<DirLightData>& dirLights,
		const std::vector<PointLightData>& pointLights,
		const std::vector<SpotLightData>& spotLights)
	{
		const int dirCount = std::min(static_cast<int>(dirLights.size()), 4);
		const int pointCount = std::min(static_cast<int>(pointLights.size()), 8);
		const int spotCount = std::min(static_cast<int>(spotLights.size()), 4);

		LightDataGPU gpu = {};
		gpu.NumDirLights = dirCount;
		gpu.NumPointLights = pointCount;
		gpu.NumSpotLights = spotCount;

		for (int i = 0; i < dirCount; ++i)
		{
			const auto& light = dirLights[i];
			gpu.DirLights[i].Direction = glm::normalize(light.Direction);
			gpu.DirLights[i].Ambient = light.Ambient;
			gpu.DirLights[i].Diffuse = light.Diffuse;
			gpu.DirLights[i].Specular = light.Specular;
			gpu.DirLights[i].Intensity = light.Intensity;
		}

		for (int i = 0; i < pointCount; ++i)
		{
			const auto& light = pointLights[i];
			gpu.PointLights[i].Position = light.Position;
			gpu.PointLights[i].Ambient = light.Ambient;
			gpu.PointLights[i].Diffuse = light.Diffuse;
			gpu.PointLights[i].Specular = light.Specular;
			gpu.PointLights[i].Intensity = light.Intensity;
			gpu.PointLights[i].Range = light.Range;
			gpu.PointLights[i].Att = glm::vec3(light.AttConstant, light.AttLinear, light.AttQuadratic);
		}

		for (int i = 0; i < spotCount; ++i)
		{
			const auto& light = spotLights[i];
			gpu.SpotLights[i].Position = light.Position;
			gpu.SpotLights[i].Direction = glm::normalize(light.Direction);
			gpu.SpotLights[i].Ambient = light.Ambient;
			gpu.SpotLights[i].Diffuse = light.Diffuse;
			gpu.SpotLights[i].Specular = light.Specular;
			gpu.SpotLights[i].Intensity = light.Intensity;
			gpu.SpotLights[i].Range = light.Range;
			gpu.SpotLights[i].InnerCutoff = light.InnerCutoffCos;
			gpu.SpotLights[i].OuterCutoff = light.OuterCutoffCos;
			gpu.SpotLights[i].Att = glm::vec3(light.AttConstant, light.AttLinear, light.AttQuadratic);
		}

		return gpu;
	}
}
