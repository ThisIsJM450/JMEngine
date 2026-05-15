#include "CameraManager.h"

#include "Actors/CameraActor.h"

void CameraManager::SetActiveCamera(CameraActor* camera)
{
    m_ActiveCamera = camera;
}

ViewInfo CameraManager::BuildViewInfo(int width, int height) const
{
    if (m_ActiveCamera)
    {
        return m_ActiveCamera->BuildViewInfo(width, height);
    }

    ViewInfo fallback{};
    fallback.view = DirectX::XMMatrixIdentity();
    fallback.proj = DirectX::XMMatrixIdentity();
    fallback.viewProj = fallback.view * fallback.proj;
    fallback.cameraPosition = DirectX::XMFLOAT3(0.f, 0.f, 0.f);
    return fallback;
}
