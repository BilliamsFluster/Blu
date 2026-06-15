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

	// The effective-path decision in isolation (deferred only on D3D11). Use this in hot paths
	// that just need the bool, instead of BuildSceneRenderPipelinePlan which allocates a Stages
	// vector. Single source of truth for the rule.
	bool SceneRenderUsesDeferred(RenderPath requestedPath, RendererAPI::API api);

	SceneRenderPipelinePlan BuildSceneRenderPipelinePlan(RenderPath requestedPath, RendererAPI::API api);
}
