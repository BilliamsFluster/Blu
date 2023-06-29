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
		auto& transform = GetComponent<TransformComponent>().Transform;
		if (Input::IsMouseButtonPressed(BLU_MOUSE_BUTTON_RIGHT))
		{
			if (Blu::Input::IsKeyPressed(BLU_KEY_W))
			{
				transform[3][1] -= 5 * deltaTime;

			}

			if (Blu::Input::IsKeyPressed(BLU_KEY_S))
			{
				transform[3][1] += 5 * deltaTime;

			}
			if (Blu::Input::IsKeyPressed(BLU_KEY_A))
			{
				transform[3][0] += 5 * deltaTime;

			}

			if (Blu::Input::IsKeyPressed(BLU_KEY_D))
			{
				transform[3][0] -= 5 * deltaTime;

			}


		}
	}
}