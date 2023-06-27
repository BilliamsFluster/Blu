#include <Blu.h>
#include <Blu/Core/EntryPoint.h>

#include "glm/glm.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include "BluEditorLayer.h"


namespace Blu
{
	class BluEditor : public Blu::Application
	{
	public:
		BluEditor()
			:Application("Blu Editor")
		{
			PushLayer(std::make_shared<BluEditorLayer>());




		}
		~BluEditor()
		{

		}
	};
		
	

	Blu::Application* Blu::CreateApplication()
	{

		return new BluEditor();
	}
}
