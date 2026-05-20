#pragma once
#include "Blu/GameFramework/AActor.h"
#include "Blu/Core/Input.h"
#include "Blu/Core/KeyCodes.h"
#include "Blu/Scene/Component.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Blu
{
    // Hold right mouse button to look around.  WASD moves horizontally, Q/E up/down.
    class FreeFlyCamera : public AActor
    {
    public:
        float Speed       = 10.0f;
        float Sensitivity = 0.15f;

    private:
        float     m_Yaw      = 0.0f;
        float     m_Pitch    = 0.0f;
        float     m_PrevX    = 0.0f;
        float     m_PrevY    = 0.0f;
        bool      m_FirstMove = true;
        glm::vec3 m_Velocity  = {0.0f, 0.0f, 0.0f};

    protected:
        void BeginPlay() override
        {
            auto& tc = GetComponent<TransformComponent>();
            m_Yaw   = glm::degrees(tc.Rotation.y);
            m_Pitch = glm::degrees(tc.Rotation.x);
        }

        void Tick(float dt) override
        {
            auto& tc = GetComponent<TransformComponent>();

            if (Input::IsMouseButtonPressed(1))
            {
                auto [mx, my] = Input::GetMousePosition();
                if (m_FirstMove)
                {
                    m_PrevX = mx;
                    m_PrevY = my;
                    m_FirstMove = false;
                }
                float dx = mx - m_PrevX;
                float dy = my - m_PrevY;
                m_PrevX = mx;
                m_PrevY = my;

                m_Yaw   -= dx * Sensitivity;
                m_Pitch -= dy * Sensitivity;
                m_Pitch  = glm::clamp(m_Pitch, -89.0f, 89.0f);

                tc.Rotation.y = glm::radians(m_Yaw);
                tc.Rotation.x = glm::radians(m_Pitch);
            }
            else
            {
                m_FirstMove = true;
            }

            glm::mat4 rotMat  = glm::toMat4(glm::quat(tc.Rotation));
            glm::vec3 forward = -glm::vec3(rotMat[2]);
            glm::vec3 right   =  glm::vec3(rotMat[0]);
            (void)forward; (void)right;

            glm::vec3 hForward(-sinf(glm::radians(m_Yaw)), 0.0f, -cosf(glm::radians(m_Yaw)));
            glm::vec3 hRight  ( cosf(glm::radians(m_Yaw)), 0.0f, -sinf(glm::radians(m_Yaw)));

            glm::vec3 moveDir(0.0f);
            if (Input::IsKeyPressed(BLU_KEY_W)) moveDir += hForward;
            if (Input::IsKeyPressed(BLU_KEY_S)) moveDir -= hForward;
            if (Input::IsKeyPressed(BLU_KEY_A)) moveDir -= hRight;
            if (Input::IsKeyPressed(BLU_KEY_D)) moveDir += hRight;
            if (Input::IsKeyPressed(BLU_KEY_E)) moveDir.y += 1.0f;
            if (Input::IsKeyPressed(BLU_KEY_Q)) moveDir.y -= 1.0f;

            float len = glm::length(moveDir);
            glm::vec3 targetVel = (len > 0.001f) ? (moveDir / len) * Speed : glm::vec3(0.0f);

            float alpha = (len > 0.001f)
                ? 1.0f - glm::exp(-12.0f * dt)
                : 1.0f - glm::exp(-9.0f  * dt);
            m_Velocity = glm::mix(m_Velocity, targetVel, alpha);
            tc.Translation += m_Velocity * dt;
        }
    };
}
