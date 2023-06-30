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
		if (Input::IsMouseButtonPressed(BLU_MOUSE_BUTTON_RIGHT))
		{
			if (Blu::Input::IsKeyPressed(BLU_KEY_W))
			{
				transform.Translation.y += 5 * deltaTime;

			}

			if (Blu::Input::IsKeyPressed(BLU_KEY_S))
			{
				transform.Translation.y -= 5 * deltaTime;

			}
			if (Blu::Input::IsKeyPressed(BLU_KEY_A))
			{
				transform.Translation.x -= 5 * deltaTime;

			}

			if (Blu::Input::IsKeyPressed(BLU_KEY_D))
			{
				transform.Translation.x += 5 * deltaTime;

			}


		}
	}
}