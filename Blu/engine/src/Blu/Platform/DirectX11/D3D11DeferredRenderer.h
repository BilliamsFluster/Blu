#pragma once
#include "Blu/Rendering/DeferredRenderer.h"
#include "Blu/Rendering/FrameBuffer.h"
#include "Blu/Rendering/VertexArray.h"
#include <d3d11.h>
#include <wrl/client.h>

namespace Blu
{
	class D3D11DeferredRenderer final : public DeferredRenderer
	{
	public:
		D3D11DeferredRenderer();

		bool BeginGeometryPass() override;
		Shared<Shader> GetGeometryShader() const override { return m_GeometryShader; }
		void SubmitLightingPass(const DeferredLightingData& data) override;

	private:
		void CaptureOutput();
		void EnsureGBuffer(uint32_t width, uint32_t height);
		void CreateFullscreenQuad();
		void CopyDepthToOutput();
		void RestoreOutput();

		Shared<FrameBuffer> m_GBuffer;
		Shared<Shader> m_GeometryShader;
		Shared<Shader> m_LightingShader;
		Shared<VertexArray> m_FullscreenQuadVAO;
		uint32_t m_FullscreenIndexCount = 0;

		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_SavedOutputRTVs[2];
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_SavedOutputDSV;
		D3D11_VIEWPORT m_SavedViewport = {};
	};
}
