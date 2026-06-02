#pragma once
#include "Blu/Core/Core.h"
#include "LightBufferData.h"
#include <glm/glm.hpp>

namespace Blu
{
	class Shader;

	struct DeferredLightingData
	{
		LightDataGPU Lights = {};
		ShadowDataGPU Shadows = {};
		glm::vec3 ViewPosition = glm::vec3(0.0f);
		glm::vec3 FogColor = glm::vec3(0.0f);
		glm::vec3 AerialColor = glm::vec3(0.0f);
		float FogDensity = 0.0f;
		float FogHeightStart = 0.0f;
		float FogHeightDensity = 0.0f;
		float AerialStrength = 0.0f;
		float IBLStrength = 1.0f;
		int FogEnabled = 0;
		int HasShadowMap = 0;
		int IBLEnabled = 0;
		int IBLMipLevels = 0;
	};

	class DeferredRenderer
	{
	public:
		virtual ~DeferredRenderer() = default;

		static Unique<DeferredRenderer> Create();

		virtual bool BeginGeometryPass() = 0;
		virtual Shared<Shader> GetGeometryShader() const = 0;
		virtual void SubmitLightingPass(const DeferredLightingData& data) = 0;
	};
}
