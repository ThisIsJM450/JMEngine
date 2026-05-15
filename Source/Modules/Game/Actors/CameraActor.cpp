#include "CameraActor.h"

#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace
{
    XMVECTOR SafeNormalize3(FXMVECTOR v, FXMVECTOR fallback)
    {
        const float lenSq = XMVectorGetX(XMVector3LengthSq(v));
        if (lenSq <= 1e-8f)
        {
            return fallback;
        }
        return XMVector3Normalize(v);
    }
}

CameraActor::CameraActor()
{
    Root = CreateComponent<SceneComponent>();
    SetRootComponent(Root);
}

void CameraActor::SetPerspective(float fovDegree, float aspect, float nearZ, float farZ)
{
    m_FovDegree = fovDegree;
    m_Aspect = aspect;
    m_NearZ = nearZ;
    m_FarZ = farZ;
}

void CameraActor::SetPerspectiveByViewport(float fovDegree, int width, int height, float nearZ, float farZ)
{
    const float aspect = (height > 0) ? (static_cast<float>(width) / static_cast<float>(height)) : (16.0f / 9.0f);
    SetPerspective(fovDegree, aspect, nearZ, farZ);
}

void CameraActor::LookAt(const XMFLOAT3& eye, const XMFLOAT3& at, const XMFLOAT3& up)
{
    (void)up;
    Root->GetRelativeTransform().SetPosition(eye.x, eye.y, eye.z);

    const XMVECTOR eyeV = XMLoadFloat3(&eye);
    const XMVECTOR atV = XMLoadFloat3(&at);
    const XMVECTOR dir = SafeNormalize3(XMVectorSubtract(atV, eyeV), XMVectorSet(0, 0, 1, 0));

    XMFLOAT3 d{};
    XMStoreFloat3(&d, dir);
    m_PitchRad = std::asin((std::max)(-1.0f, (std::min)(1.0f, d.y)));
    m_YawRad = std::atan2(d.x, d.z);
    Root->GetRelativeTransform().SetRotationEuler(m_PitchRad, m_YawRad, 0.0f);
}

void CameraActor::AddYawPitch(float deltaX, float deltaY, float degPerPixel)
{
    const float radPerPixel = XMConvertToRadians(degPerPixel);
    m_YawRad += deltaX * radPerPixel;
    m_PitchRad += deltaY * radPerPixel;
    const float kPitchLimit = XMConvertToRadians(89.0f);
    m_PitchRad = (std::max)(-kPitchLimit, (std::min)(kPitchLimit, m_PitchRad));
    Root->GetRelativeTransform().SetRotationEuler(m_PitchRad, m_YawRad, 0.0f);
}

void CameraActor::MoveLocal(float forward, float strafe, float up, float speed, float dt)
{
    const XMVECTOR fwd = SafeNormalize3(Root->GetWorldForward(), XMVectorSet(0, 0, 1, 0));
    const XMVECTOR worldUp = XMVectorSet(0, 1, 0, 0);
    const XMVECTOR right = SafeNormalize3(XMVector3Cross(worldUp, fwd), XMVectorSet(1, 0, 0, 0));

    XMVECTOR delta = XMVectorZero();
    delta = XMVectorAdd(delta, XMVectorScale(fwd, forward));
    delta = XMVectorAdd(delta, XMVectorScale(right, strafe));
    delta = XMVectorAdd(delta, XMVectorScale(worldUp, up));
    delta = SafeNormalize3(delta, XMVectorZero());

    XMFLOAT3 d{};
    XMStoreFloat3(&d, XMVectorScale(delta, speed * dt));
    Root->GetRelativeTransform().AddPosition(d.x, d.y, d.z);
}

ViewInfo CameraActor::BuildViewInfo(int width, int height) const
{
    ViewInfo info{};

    const float aspect = (height > 0) ? (static_cast<float>(width) / static_cast<float>(height)) : m_Aspect;
    const XMFLOAT3 pos = Root->GetWorldLocation();
    const XMVECTOR eye = XMLoadFloat3(&pos);
    const XMVECTOR forward = SafeNormalize3(Root->GetWorldForward(), XMVectorSet(0, 0, 1, 0));
    const XMVECTOR up = SafeNormalize3(Root->GetWorldUp(), XMVectorSet(0, 1, 0, 0));

    info.view = XMMatrixLookAtLH(eye, XMVectorAdd(eye, forward), up);
    info.proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(m_FovDegree), aspect, m_NearZ, m_FarZ);
    info.viewProj = info.view * info.proj;
    info.cameraPosition = pos;
    return info;
}
