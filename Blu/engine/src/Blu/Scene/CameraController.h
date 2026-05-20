#pragma once
#include "Blu/GameFramework/AActor.h"
#include "Blu/Core/Input.h"
#include "Blu/Core/KeyCodes.h"
#include "Blu/Core/MouseCodes.h"

namespace Blu
{
	class CameraController : public AActor
	{
	public:
		void BeginPlay() override;
		void EndPlay()   override;
		void Tick(float deltaTime) override;
		void ReceiveMouseScrolled(float xOffset, float yOffset);

		float GetCameraSpeed() const  { return m_CameraSpeed; }
		void  SetCameraSpeed(float s) { m_CameraSpeed = s; }

	private:
		float m_CameraSpeed = 1.0f;
	};
}
