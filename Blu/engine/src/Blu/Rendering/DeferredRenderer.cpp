#include "Blupch.h"
#include "DeferredRenderer.h"
#include "RendererAPI.h"
#include "Blu/Platform/DirectX11/D3D11DeferredRenderer.h"

namespace Blu
{
	Unique<DeferredRenderer> DeferredRenderer::Create()
	{
		if (RendererAPI::GetAPI() == RendererAPI::API::Direct3D)
			return std::make_unique<D3D11DeferredRenderer>();
		return nullptr;
	}
}
