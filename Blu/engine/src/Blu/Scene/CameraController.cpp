#include "Blupch.h"
#include "CameraController.h"

namespace Blu
{
	void CameraController::OnCreate()
	{

	}
	void CameraController::OnDestroy()
	{

	}
	void CameraController::OnUpdate(Timestep deltaTime)
	{
		auto& transform = GetComponent<TransformComponent>();
		
		if (Blu::Input::IsKeyPressed(BLU_KEY_W))
		{
			transform.Translation.y -= m_CameraSpeed * deltaTime;

		}

		if (Blu::Input::IsKeyPressed(BLU_KEY_S))
		{
			transform.Translation.y += m_CameraSpeed * deltaTime;

		}
		if (Blu::Input::IsKeyPressed(BLU_KEY_A))
		{
			transform.Translation.x += m_CameraSpeed * deltaTime;

		}

		if (Blu::Input::IsKeyPressed(BLU_KEY_D))
		{
			transform.Translation.x -= m_CameraSpeed * deltaTime;

		}


		
	}
	void CameraController::ReceiveMouseScrolled(float xOffset, float yOffset)
	{
		auto& transform = GetComponent<TransformComponent>();
		if (Input::IsMouseButtonPressed(BLU_MOUSE_BUTTON_RIGHT))
		{
			transform.Translation.z += xOffset;
		}
	}
}