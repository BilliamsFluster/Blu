#include "Azure2D.h"
#include "Blu/Rendering/Renderer2D.h"
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>

//template<typename Fn>
//class Timer
//{
//public:
//	Timer(const char* name, Fn&& func)
//		:m_Name(name), m_Stopped(false), m_Func(func)
//	{
//		m_StartTimepoint = std::chrono::high_resolution_clock::now();
//
//	}
//	~Timer()
//	{
//		if (!m_Stopped)
//		{
//			Stop();
//		}
//	}
//	void Stop()
//	{
//		auto endTimepoint = std::chrono::high_resolution_clock::now();
//		long long start = std::chrono::time_point_cast<std::chrono::microseconds>(m_StartTimepoint).time_since_epoch().count();
//		long long end = std::chrono::time_point_cast<std::chrono::microseconds>(endTimepoint).time_since_epoch().count();
//
//		m_Stopped = true;
//		float duration = (end - start) * 0.001f;
//		std::cout << m_Name << duration << std::endl;
//		 
//		m_Func({ m_Name, duration });
//	}
//
//private:
//	const char* m_Name;
//	std::chrono::time_point<std::chrono::steady_clock> m_StartTimepoint;
//	bool m_Stopped;
//	Fn m_Func;
//};
//#define PROFILE_SCOPE(name) Timer timer##__LINE__(name, [&](ProfileResult profileResult) {m_ProfileResults.push_back(profileResult);})


Azure2D::Azure2D()
	:Layer("TestRenderingLayer"), m_CameraController(1280.0f / 720.0f, true)
{
}

void Azure2D::OnAttach()
{
	m_Texture = Blu::Texture2D::Create("assets/textures/Wallpaper.png");


}

void Azure2D::OnDetach()
{
}

void Azure2D::OnUpdate(Blu::Timestep deltaTime)
{
	BLU_PROFILE_FUNCTION();
	{

		BLU_PROFILE_SCOPE("Azure2D::OnUpdate: ")
		m_CameraController.OnUpdate(deltaTime);
	}
	Blu::RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
	Blu::RenderCommand::Clear();
	
	Blu::Renderer2D::BeginScene(m_CameraController.GetCamera());
	
	Blu::Renderer2D::DrawRotatedQuad({ 0, 0 }, { 1, 1 }, glm::radians(-45.0f), { 1.0f ,1.0f ,0.0f ,1.0f });

	Blu::Renderer2D::DrawQuad({ 1.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f ,1.0f ,1.0f ,1.0f });


	

	Blu::Renderer2D::EndScene();
}

void Azure2D::OnEvent(Blu::Events::Event& event)
{
	m_CameraController.OnEvent(event);
}
