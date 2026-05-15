#pragma once

#include "InputRouter.h"
#include "Input.h"
#include "../Editor/EditorContext.h"
#include "../Game/Actors/CameraActor.h"
#include "../Game/World.h"

class EditorViewportInputContext final : public IInputContext
{
public:
    bool HandleInput(float deltaTime, const InputSystem& inputSystem, World& world) override
    {
        if (!GEditor.viewportInput.IsCameraInputEnabled())
        {
            return false;
        }

        CameraActor* camera = world.GetCameraManager().GetActiveCamera();
        if (!camera)
        {
            return false;
        }

        if (inputSystem.RMBDown())
        {
            constexpr float lookSpeed = 0.3f;
            camera->AddYawPitch(inputSystem.MouseDeltaX(), -inputSystem.MouseDeltaY(), lookSpeed);
        }

        constexpr float moveSpeed = 10.0f;
        const float speedScale = inputSystem.ShiftDown() ? 3.0f : 1.0f;
        if (inputSystem.KeyDown('W')) camera->MoveLocal(moveSpeed, 0.0f, 0.0f, speedScale, deltaTime);
        if (inputSystem.KeyDown('S')) camera->MoveLocal(-moveSpeed, 0.0f, 0.0f, speedScale, deltaTime);
        if (inputSystem.KeyDown('A')) camera->MoveLocal(0.0f, -moveSpeed, 0.0f, speedScale, deltaTime);
        if (inputSystem.KeyDown('D')) camera->MoveLocal(0.0f, moveSpeed, 0.0f, speedScale, deltaTime);
        if (inputSystem.KeyDown('Q')) camera->MoveLocal(0.0f, 0.0f, -moveSpeed, speedScale, deltaTime);
        if (inputSystem.KeyDown('E')) camera->MoveLocal(0.0f, 0.0f, moveSpeed, speedScale, deltaTime);
        return true;
    }
};
