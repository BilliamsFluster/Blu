#pragma once
#include <string>
#include "Blu/Events/Event.h"



struct ImVec2;

namespace Blu
{
    class GuiManager
    {
    public:
        static void Initialize();
        static void Shutdown();

        static void BeginFrame();
        static void EndFrame();

        // Call ImGui::Begin with the given parameters
        static bool Begin(const std::string& name, bool* open = nullptr);

        // Call ImGui::End
        static void End();
        //Call ImGui::Text
        static void Text(const char* fmt, ...);

        // Call ImGui::Button with the given parameters
        static bool Button(const std::string& label, const  ImVec2& size);
        
        // Call ImGui::BeginMenu with the given parameters
        static bool BeginMenu(const std::string& label, bool enabled = true);

        // Call ImGui::EndMenu
        static void EndMenu();

        static bool OnMouseMovedEvent(class Events::MouseMovedEvent& event);
         
        static bool OnMouseButtonPressed(class Events::MouseButtonPressedEvent& event);
       
        static bool OnMouseButtonReleased(class Events::MouseButtonReleasedEvent& event);
        
        static bool OnMouseScrolledEvent(class Events::MouseScrolledEvent& event);
        
        static bool OnKeyPressedEvent(class Events::KeyPressedEvent& event);
        
        
        
    };
}
