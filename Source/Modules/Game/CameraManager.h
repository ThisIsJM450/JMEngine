#pragma once

#include "../Renderer/SceneView.h"

class CameraActor;

class CameraManager
{
public:
    void SetActiveCamera(CameraActor* camera);
    CameraActor* GetActiveCamera() const { return m_ActiveCamera; }

    ViewInfo BuildViewInfo(int width, int height) const;
    bool HasActiveCamera() const { return m_ActiveCamera != nullptr; }

private:
    CameraActor* m_ActiveCamera = nullptr;
};
