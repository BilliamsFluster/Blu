#pragma once
#include "ScriptableEntity.h"
#include "Blu/Core/Input.h"
#include "Blu/Core/KeyCodes.h"
#include "Blu/Core/MouseCodes.h"

namespace Blu
{
	class CameraController: public ScriptableEntity
	{
	public:
	
		void OnCreate();

		void OnDestroy();

		void OnUpdate(Timestep deltaTime);
		void ReceiveMouseScrolled(float xOffset, float yOffset);

		float GetCameraSpeed() const { return m_CameraSpeed;}
		void SetCameraSpeed(float speed) { m_CameraSpeed = speed; }

		
	private:
		float m_CameraSpeed = 1.0f;
		
	};

}

