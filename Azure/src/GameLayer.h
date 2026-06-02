#pragma once
#include <Blu.h>

namespace Azure
{
	class GameLayer : public Blu::Layers::Layer
	{
	public:
		GameLayer() : Blu::Layers::Layer("GameLayer") {}

		void OnAttach() override;
		void OnDetach() override;
		void OnUpdate(Blu::Timestep ts) override;
		void OnEvent(Blu::Events::Event& event) override;
		void OnGuiDraw() override;

	private:
		std::shared_ptr<Blu::Scene> m_Scene;
	};
}
