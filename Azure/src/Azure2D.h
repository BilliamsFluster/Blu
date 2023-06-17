#include <Blu.h>
#pragma once

class Azure2D: public Blu::Layers::Layer
{
public:
	Azure2D();
	virtual ~Azure2D() = default;
	virtual void OnAttach() override;
	virtual void OnDetach() override;
	void OnUpdate(Blu::Timestep deltaTime) override;
	void OnEvent(Blu::Events::Event& event) override;
private:
	Blu::OrthographicCameraController m_CameraController;
	Blu::Shared<Blu::Texture2D> m_Texture;
	Blu::Shared<Blu::VertexArray> m_VertexArray;
	Blu::Shared< Blu::IndexBuffer> m_IndexBuffer;
	Blu::Shared< Blu::VertexBuffer> m_VertexBuffer;
	Blu::Shared<Blu::OpenGLShader> m_FlatColorShader, m_TextureShader;



};

