#pragma once
#include <DirectXMath.h>

struct Rotation
{
    float Pitch;
    float Yaw;
    float Roll;

    DirectX::XMVECTOR ToVector() const 
    {
        return DirectX::XMVectorSet(Pitch,Yaw,Roll,0);
    }
};

struct Transform
{
public:
    void SetPosition(float x, float y, float z) { m_Pos = { x, y, z }; }
    void SetRotationEuler(float pitch, float yaw, float roll) { m_Rot = { pitch, yaw, roll }; }
    void SetScale(float x, float y, float z) { m_Scale = { x, y, z }; }

    DirectX::XMMATRIX ToMatrix() const
    {
        using namespace DirectX;
        XMMATRIX S = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
        XMMATRIX R = XMMatrixRotationRollPitchYaw(m_Rot.Pitch, m_Rot.Yaw, m_Rot.Roll);
        XMMATRIX T = XMMatrixTranslation(m_Pos.x, m_Pos.y, m_Pos.z);
        return S * R * T;
    }

    DirectX::XMVECTOR GetForwardVector() const
    {
        using namespace DirectX;

        const XMVECTOR q = XMQuaternionNormalize(
            XMQuaternionRotationRollPitchYaw(m_Rot.Pitch, m_Rot.Yaw, m_Rot.Roll));

        const XMVECTOR f = XMVectorSet(0, 0, 1, 0);
        return XMVector3Rotate(f, q); // 단위 벡터 보장
    }

    DirectX::XMVECTOR GetRightVector() const
    {
        using namespace DirectX;

        const XMVECTOR q = XMQuaternionNormalize(
            XMQuaternionRotationRollPitchYaw(m_Rot.Pitch, m_Rot.Yaw, m_Rot.Roll));

        const XMVECTOR r = XMVectorSet(1, 0, 0, 0);
        return XMVector3Rotate(r, q);
    }

    DirectX::XMVECTOR GetUpVector() const
    {
        using namespace DirectX;

        const XMVECTOR q = XMQuaternionNormalize(
            XMQuaternionRotationRollPitchYaw(m_Rot.Pitch, m_Rot.Yaw, m_Rot.Roll));

        const XMVECTOR u = XMVectorSet(0, 1, 0, 0);
        return XMVector3Rotate(u, q);
    }
    
    DirectX::XMFLOAT3 m_Pos{ 0,0,0 };
    Rotation m_Rot{ 0,0,0 };
    DirectX::XMFLOAT3 m_Scale{ 1,1,1 };
};
