#include "Blupch.h"
#include "SceneRenderPipeline.h"

namespace Blu
{
	bool SceneRenderUsesDeferred(RenderPath requestedPath, RendererAPI::API api)
	{
		return requestedPath == RenderPath::Deferred && api == RendererAPI::API::Direct3D;
	}

	SceneRenderPipelinePlan BuildSceneRenderPipelinePlan(RenderPath requestedPath, RendererAPI::API api)
	{
		SceneRenderPipelinePlan plan;
		plan.RequestedPath = requestedPath;
		plan.EffectivePath = SceneRenderUsesDeferred(requestedPath, api)
			? RenderPath::Deferred
			: RenderPath::Forward;

		if (plan.UsesDeferred())
		{
			plan.Stages = {
				SceneRenderStage::GBufferGeometry,
				SceneRenderStage::DeferredLighting,
				SceneRenderStage::ForwardTransparent,
				SceneRenderStage::Skybox,
				SceneRenderStage::PostProcessComposition
			};
		}
		else
		{
			plan.Stages = {
				SceneRenderStage::ForwardOpaque,
				SceneRenderStage::ForwardTransparent,
				SceneRenderStage::Skybox,
				SceneRenderStage::PostProcessComposition
			};
		}

		return plan;
	}
}
