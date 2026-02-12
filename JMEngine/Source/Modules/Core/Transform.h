#pragma once
#include <DirectXMath.h>

struct Rotation
{
    float Pitch = 0.f; // X
    float Yaw   = 0.f; // Y
    float Roll  = 0.f; // Z

    DirectX::XMVECTOR ToVector() const 
    {
        return DirectX::XMVectorSet(Pitch,Yaw,Roll,0);
    }
};

struct Transform
{
public:
    void SetPosition(float x, float y, float z) { m_Pos = { x, y, z }; }
    void SetScale(float x, float y, float z)    { m_Scale = { x, y, z }; }

    // Euler 입력은 받되, 내부 저장은 Quaternion
    void SetRotationEuler(float pitch, float yaw, float roll)
    {
        m_Rot = { pitch, yaw, roll };
        using namespace DirectX;
        const XMVECTOR q = XMQuaternionRotationRollPitchYaw(pitch, yaw, roll);
        XMStoreFloat4(&m_RotQ, XMQuaternionNormalize(q));
        m_Dirty = true;
    }

    // Quaternion 직접 세팅도 가능 (애니메이션/보간에 유리)
    virtual void SetRotationQuat(DirectX::FXMVECTOR& q)
    {
        using namespace DirectX;
        XMStoreFloat4(&m_RotQ, XMQuaternionNormalize(q));
        m_Dirty = true;
    }
    
    virtual void SetRotationQuat(DirectX::XMFLOAT4& q)
    {
        using namespace DirectX;
        m_RotQ = q;
        m_Dirty = true;
    }

    DirectX::XMVECTOR GetRotationQuat() const
    {
        return DirectX::XMLoadFloat4(&m_RotQ);
    }

    // (선택) Euler 값이 필요하면 그대로 반환 (입력 캐시)
    Rotation GetRotationEulerCached() const { return m_Rot; }

    DirectX::XMMATRIX ToMatrix() const
    {
        using namespace DirectX;
        const XMMATRIX S = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
        const XMMATRIX R = XMMatrixRotationQuaternion(XMLoadFloat4(&m_RotQ));
        const XMMATRIX T = XMMatrixTranslation(m_Pos.x, m_Pos.y, m_Pos.z);
        return S * R * T;
    }
    
    DirectX::XMMATRIX ToMatrix_TRS() const
    {
        using namespace DirectX;
        const XMMATRIX S = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
        const XMMATRIX R = XMMatrixRotationQuaternion(XMLoadFloat4(&m_RotQ));
        const XMMATRIX T = XMMatrixTranslation(m_Pos.x, m_Pos.y, m_Pos.z);
        return T * S * R;
    }

    DirectX::XMVECTOR GetForwardVector() const
    {
        using namespace DirectX;
        const XMVECTOR q = XMLoadFloat4(&m_RotQ);
        return XMVector3Rotate(gForward, q);
    }

    DirectX::XMVECTOR GetRightVector() const
    {
        using namespace DirectX;
        const XMVECTOR q = XMLoadFloat4(&m_RotQ);
        return XMVector3Rotate(gRight, q);
    }

    DirectX::XMVECTOR GetUpVector() const
    {
        using namespace DirectX;
        const XMVECTOR q = XMLoadFloat4(&m_RotQ);
        return XMVector3Rotate(gUp, q);
    }

    // 필요하면: 회전 누적(언리얼 느낌)
    void AddRotationEuler(float dPitch, float dYaw, float dRoll)
    {
        SetRotationEuler(
            m_Rot.Pitch + dPitch,
            m_Rot.Yaw   + dYaw,
            m_Rot.Roll  + dRoll
        );
    }
    
    static Transform Lerp(const Transform& a, const Transform& b, float alpha)
    {
        using namespace DirectX;

        Transform o;
        o.m_Pos = {
            a.m_Pos.x + (b.m_Pos.x - a.m_Pos.x) * alpha,
            a.m_Pos.y + (b.m_Pos.y - a.m_Pos.y) * alpha,
            a.m_Pos.z + (b.m_Pos.z - a.m_Pos.z) * alpha
        };
        o.m_Scale = {
            a.m_Scale.x + (b.m_Scale.x - a.m_Scale.x) * alpha,
            a.m_Scale.y + (b.m_Scale.y - a.m_Scale.y) * alpha,
            a.m_Scale.z + (b.m_Scale.z - a.m_Scale.z) * alpha
        };

        XMVECTOR qa = XMLoadFloat4(&a.m_RotQ);
        XMVECTOR qb = XMLoadFloat4(&b.m_RotQ);
        XMVECTOR q  = XMQuaternionSlerp(qa, qb, alpha);
        XMStoreFloat4(&o.m_RotQ, XMQuaternionNormalize(q));
        return o;
    }


public:
    DirectX::XMFLOAT3 m_Pos{ 0,0,0 };
    DirectX::XMFLOAT3 m_Scale{ 1,1,1 };
    
    // 내부 저장은 quat (x,y,z,w)
    DirectX::XMFLOAT4 m_RotQ{ 0,0,0,1 };

    // UI/디버그/직렬화용 Euler 캐시(선택)
    Rotation m_Rot{ 0,0,0 };
private:
    // (선택) 나중에 캐시할 게 생기면 활용
    mutable bool m_Dirty = true;

    // 축 벡터는 static으로 재사용
    static const DirectX::XMVECTORF32 gForward; 
    static const DirectX::XMVECTORF32 gRight;
    static const DirectX::XMVECTORF32 gUp ; 
};
