#pragma once
#include "RenderSettings.h"
#include "RendererAPI.h"
#include <vector>

namespace Blu
{
	enum class SceneRenderStage : uint8_t
	{
		ForwardOpaque,
		GBufferGeometry,
		DeferredLighting,
		ForwardTransparent,
		Skybox,
		PostProcessComposition
	};

	struct SceneRenderPipelinePlan
	{
		RenderPath RequestedPath = RenderPath::Forward;
		RenderPath EffectivePath = RenderPath::Forward;
		std::vector<SceneRenderStage> Stages;

		bool UsesDeferred() const { return EffectivePath == RenderPath::Deferred; }
	};

	SceneRenderPipelinePlan BuildSceneRenderPipelinePlan(RenderPath requestedPath, RendererAPI::API api);
}
