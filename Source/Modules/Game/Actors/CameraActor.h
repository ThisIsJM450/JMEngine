#pragma once

#include "../Actor.h"
#include "../Components/SceneComponent.h"
#include "../../Renderer/SceneView.h"

class CameraActor : public Actor
{
public:
    CameraActor();

    SceneComponent* GetRootComponent() const { return Root; }

    void SetPerspective(float fovDegree, float aspect, float nearZ, float farZ);
    void SetPerspectiveByViewport(float fovDegree, int width, int height, float nearZ, float farZ);
    void LookAt(const DirectX::XMFLOAT3& eye, const DirectX::XMFLOAT3& at, const DirectX::XMFLOAT3& up);

    void AddYawPitch(float deltaX, float deltaY, float degPerPixel);
    void MoveLocal(float forward, float strafe, float up, float speed, float dt);

    float GetFovDegree() const { return m_FovDegree; }
    float GetNearZ() const { return m_NearZ; }
    float GetFarZ() const { return m_FarZ; }

    ViewInfo BuildViewInfo(int width, int height) const;

private:
    SceneComponent* Root = nullptr;
    float m_FovDegree = 60.0f;
    float m_Aspect = 16.0f / 9.0f;
    float m_NearZ = 0.1f;
    float m_FarZ = 1000.0f;

    float m_PitchRad = 0.0f;
    float m_YawRad = 0.0f;
};
