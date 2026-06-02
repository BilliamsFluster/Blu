#include "Blupch.h"
#include "D3D11DeferredRenderer.h"
#include "D3D11Context.h"
#include "D3D11FrameBuffer.h"
#include "Blu/Rendering/Buffer.h"
#include "Blu/Rendering/IBLSystem.h"
#include "Blu/Rendering/PipelineState.h"
#include "Blu/Rendering/RenderCommand.h"
#include "Blu/Rendering/Renderer.h"
#include "Blu/Rendering/Shader.h"

namespace Blu
{
	D3D11DeferredRenderer::D3D11DeferredRenderer()
	{
		auto& library = *Renderer::GetShaderLibrary();
		library.Load("assets/shaders/DX11/Deferred_GBuffer.hlsl");
		library.Load("assets/shaders/DX11/Deferred_Lighting.hlsl");
		m_GeometryShader = library.Get("Deferred_GBuffer");
		m_LightingShader = library.Get("Deferred_Lighting");
		CreateFullscreenQuad();
	}

	void D3D11DeferredRenderer::CaptureOutput()
	{
		auto* dc = D3D11Context::Get()->GetDeviceContext();
		for (auto& rtv : m_SavedOutputRTVs)
			rtv.Reset();
		m_SavedOutputDSV.Reset();

		ID3D11RenderTargetView* rawRTVs[2] = {};
		ID3D11DepthStencilView* rawDSV = nullptr;
		dc->OMGetRenderTargets(2, rawRTVs, &rawDSV);
		for (uint32_t i = 0; i < 2; ++i)
			m_SavedOutputRTVs[i].Attach(rawRTVs[i]);
		m_SavedOutputDSV.Attach(rawDSV);

		UINT viewportCount = 1;
		dc->RSGetViewports(&viewportCount, &m_SavedViewport);
	}

	void D3D11DeferredRenderer::EnsureGBuffer(uint32_t width, uint32_t height)
	{
		if (m_GBuffer)
		{
			const auto& spec = m_GBuffer->GetSpecification();
			if (spec.Width == width && spec.Height == height)
				return;
		}

		FrameBufferSpecifications spec;
		spec.Width = std::max(width, 1u);
		spec.Height = std::max(height, 1u);
		spec.Attachments = {
			FrameBufferTextureFormat::RGBA16F,
			FrameBufferTextureFormat::RGBA16F,
			FrameBufferTextureFormat::RGBA8,
			FrameBufferTextureFormat::RGBA16F,
			FrameBufferTextureFormat::RGBA16F,
			FrameBufferTextureFormat::RED_INTEGER,
			FrameBufferTextureFormat::DEPTH24STENCIL8
		};
		m_GBuffer = FrameBuffer::Create(spec);
	}

	bool D3D11DeferredRenderer::BeginGeometryPass()
	{
		CaptureOutput();
		if (!m_SavedOutputRTVs[0])
			return false;

		Microsoft::WRL::ComPtr<ID3D11Resource> outputResource;
		m_SavedOutputRTVs[0]->GetResource(outputResource.GetAddressOf());
		Microsoft::WRL::ComPtr<ID3D11Texture2D> outputTexture;
		if (FAILED(outputResource.As(&outputTexture)))
			return false;

		D3D11_TEXTURE2D_DESC outputDesc = {};
		outputTexture->GetDesc(&outputDesc);
		EnsureGBuffer(outputDesc.Width, outputDesc.Height);

		auto* dc = D3D11Context::Get()->GetDeviceContext();
		auto* gBuffer = static_cast<D3D11FrameBuffer*>(m_GBuffer.get());
		const float clearColor[4] = {};
		for (uint32_t i = 0; i < 5; ++i)
			dc->ClearRenderTargetView(gBuffer->GetColorAttachmentRTV(i), clearColor);
		m_GBuffer->ClearAttachment(5, -1);
		m_GBuffer->Bind();
		dc->ClearDepthStencilView(gBuffer->GetDepthStencilView(),
			D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		return true;
	}

	void D3D11DeferredRenderer::CopyDepthToOutput()
	{
		if (!m_SavedOutputDSV || !m_GBuffer)
			return;

		auto* gBuffer = static_cast<D3D11FrameBuffer*>(m_GBuffer.get());
		Microsoft::WRL::ComPtr<ID3D11Resource> outputDepth;
		m_SavedOutputDSV->GetResource(outputDepth.GetAddressOf());
		D3D11Context::Get()->GetDeviceContext()->CopyResource(outputDepth.Get(), gBuffer->GetDepthTexture());
	}

	void D3D11DeferredRenderer::RestoreOutput()
	{
		auto* dc = D3D11Context::Get()->GetDeviceContext();
		ID3D11RenderTargetView* rtvs[2] = {
			m_SavedOutputRTVs[0].Get(),
			m_SavedOutputRTVs[1].Get()
		};
		const UINT rtvCount = m_SavedOutputRTVs[1] ? 2u : 1u;
		dc->OMSetRenderTargets(rtvCount, rtvs, m_SavedOutputDSV.Get());
		if (m_SavedViewport.Width > 0.0f && m_SavedViewport.Height > 0.0f)
			dc->RSSetViewports(1, &m_SavedViewport);
	}

	void D3D11DeferredRenderer::SubmitLightingPass(const DeferredLightingData& data)
	{
		if (!m_GBuffer || !m_LightingShader)
			return;

        auto* dc = D3D11Context::Get()->GetDeviceContext();
        auto* gBuffer = static_cast<D3D11FrameBuffer*>(m_GBuffer.get());
        dc->OMSetRenderTargets(0, nullptr, nullptr);
        CopyDepthToOutput();
        RestoreOutput();

		PipelineStateCache::GetNoDepth()->Bind();
		m_LightingShader->Bind();
		m_LightingShader->SetUniformBuffer("LightData", &data.Lights, sizeof(data.Lights));
		m_LightingShader->SetUniformBuffer("ShadowData", &data.Shadows, sizeof(data.Shadows));
		m_LightingShader->SetUniformFloat3("u_ViewPos", data.ViewPosition);
		m_LightingShader->SetUniformInt("u_HasShadowMap", data.HasShadowMap);
		m_LightingShader->SetUniformFloat3("u_FogColor", data.FogColor);
		m_LightingShader->SetUniformFloat("u_FogDensity", data.FogDensity);
		m_LightingShader->SetUniformFloat("u_FogHeightStart", data.FogHeightStart);
		m_LightingShader->SetUniformFloat("u_FogHeightDensity", data.FogHeightDensity);
		m_LightingShader->SetUniformInt("u_FogEnabled", data.FogEnabled);
		m_LightingShader->SetUniformFloat3("u_AerialColor", data.AerialColor);
		m_LightingShader->SetUniformFloat("u_AerialStrength", data.AerialStrength);
		m_LightingShader->SetUniformInt("u_IBLEnabled", data.IBLEnabled);
		m_LightingShader->SetUniformFloat("u_IBLStrength", data.IBLStrength);
		m_LightingShader->SetUniformInt("u_IBLMipLevels", data.IBLMipLevels);
		m_LightingShader->Flush();

		ID3D11ShaderResourceView* gBufferSRVs[5] = {
			gBuffer->GetColorAttachmentSRV(0),
			gBuffer->GetColorAttachmentSRV(1),
			gBuffer->GetColorAttachmentSRV(2),
			gBuffer->GetColorAttachmentSRV(3),
			gBuffer->GetColorAttachmentSRV(4)
		};
		dc->PSSetShaderResources(0, 5, gBufferSRVs);
		ID3D11ShaderResourceView* entityIDSRV = gBuffer->GetColorAttachmentSRV(5);
		dc->PSSetShaderResources(9, 1, &entityIDSRV);
		if (data.IBLEnabled)
			IBLSystem::BindIBL(6, 7, 8);

		m_FullscreenQuadVAO->Bind();
		RenderCommand::DrawIndexed(m_FullscreenQuadVAO, m_FullscreenIndexCount);
		m_FullscreenQuadVAO->UnBind();
		m_LightingShader->UnBind();

		ID3D11ShaderResourceView* nullGBufferSRVs[5] = {};
		dc->PSSetShaderResources(0, 5, nullGBufferSRVs);
		ID3D11ShaderResourceView* nullEntityIDSRV = nullptr;
		dc->PSSetShaderResources(9, 1, &nullEntityIDSRV);
		PipelineStateCache::GetOpaque()->Bind();

		for (auto& rtv : m_SavedOutputRTVs)
			rtv.Reset();
		m_SavedOutputDSV.Reset();
	}

	void D3D11DeferredRenderer::CreateFullscreenQuad()
	{
		float vertices[] = {
			-1.0f, -1.0f, 0.0f, 1.0f,
			 1.0f, -1.0f, 1.0f, 1.0f,
			 1.0f,  1.0f, 1.0f, 0.0f,
			-1.0f,  1.0f, 0.0f, 0.0f
		};
		uint32_t indices[] = { 0, 1, 2, 2, 3, 0 };
		m_FullscreenIndexCount = 6;

		m_FullscreenQuadVAO = VertexArray::Create();
		auto vertexBuffer = VertexBuffer::Create(sizeof(vertices));
		vertexBuffer->SetData(vertices, sizeof(vertices));
		vertexBuffer->SetLayout({
			{ ShaderDataType::Float2, "a_Position" },
			{ ShaderDataType::Float2, "a_TexCoord" }
		});
		m_FullscreenQuadVAO->AddVertexBuffer(vertexBuffer);
		m_FullscreenQuadVAO->AddIndexBuffer(IndexBuffer::Create(indices, m_FullscreenIndexCount));
	}
}
