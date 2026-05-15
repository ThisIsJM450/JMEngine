#include "Camera.h"

#include <valarray>

using namespace DirectX;

void Camera::SetPerspective(float fovDegree, float aspect, float nearZ, float fearZ)
{
    m_Proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(fovDegree), aspect, nearZ, fearZ);
}

void Camera::LookAt(const XMFLOAT3& eye, const XMFLOAT3& at, const XMFLOAT3& up)
{
    Position = eye;
    XMVECTOR eyeVector = XMLoadFloat3(&Position);
    XMVECTOR atVector = XMLoadFloat3(&at);
    XMVECTOR upVector = XMLoadFloat3(&up);

    m_View = XMMatrixLookAtLH(eyeVector, atVector, upVector);
}

DirectX::XMFLOAT3 Camera::GetPosition() const
{
    return Position;
}

void Camera::AddYawPitch(float deltaX, float deltaY, float degPerPixel)
{
    if (deltaX == 0.0f && deltaY == 0.0f)
    {
        return;
    }
    YawDeg   += deltaX * degPerPixel;
    PitchDeg += deltaY * degPerPixel;
    PitchDeg = std::max<float>(-89.0f, std::min<float>(PitchDeg, 89.0f));

    UpdateView();
}

void Camera::MoveLocal(float forward, float strafe, float up, float speed, float dt)
{
    XMVECTOR pos = XMLoadFloat3(&Position);

    XMVECTOR fwd = GetForward();                 // 카메라 forward
    XMVECTOR right = GetRight();                 // 카메라 right
    XMVECTOR worldUp = XMVectorSet(0, 1, 0, 0);  // 월드 up

    XMVECTOR delta = XMVectorZero();
    delta = XMVectorAdd(delta, XMVectorScale(fwd,   forward));
    delta = XMVectorAdd(delta, XMVectorScale(right, strafe));
    delta = XMVectorAdd(delta, XMVectorScale(worldUp, up));

    // 방향 입력이 크면 normalize (W+D 대각선 속도 보정)
    if (!XMVector3Equal(delta, XMVectorZero()))
        delta = XMVector3Normalize(delta);

    float step = speed * dt;
    pos = XMVectorAdd(pos, XMVectorScale(delta, step));
    XMStoreFloat3(&Position, pos);

    UpdateView();
}

DirectX::XMVECTOR Camera::GetForward() const
{
    float yaw   = XMConvertToRadians(YawDeg);
    float pitch = XMConvertToRadians(PitchDeg);

    float cy = std::cos(yaw);
    float sy = std::sin(yaw);
    float cp = std::cos(pitch);
    float sp = std::sin(pitch);

    XMVECTOR fwd = XMVectorSet(sy * cp, sp, cy * cp, 0.0f);
    return XMVector3Normalize(fwd);
}

DirectX::XMVECTOR Camera::GetRight() const
{
    XMVECTOR up = XMVectorSet(0, 1, 0, 0);
    XMVECTOR fwd = GetForward();

    // LH LookAt 기준: right = up x forward
    return XMVector3Normalize(XMVector3Cross(up, fwd));
}

void Camera::UpdateView()
{
    XMVECTOR eye = XMLoadFloat3(&Position);
    XMVECTOR fwd = GetForward();
    XMFLOAT3 at;
    XMStoreFloat3(&at, XMVectorAdd(eye, fwd));
    XMFLOAT3 up(0, 1, 0);
    LookAt(Position, at, up);
}
