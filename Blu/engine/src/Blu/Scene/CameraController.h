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
		
	};

}

