#pragma once
#include <DirectXMath.h>
#include <cmath>

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
    void AddPosition(float x, float y, float z) { m_Pos = { m_Pos.x + x, m_Pos.y + y, m_Pos.z + z }; }

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
    void SetRotationQuat(DirectX::FXMVECTOR q)
    {
        using namespace DirectX;
        const XMVECTOR n = NormalizeQuatSafe(q);
        XMStoreFloat4(&m_RotQ, n);
        m_Rot = EulerFromQuat(n);
        m_Dirty = true;
    }
    
    void SetRotationQuat(const DirectX::XMFLOAT4& q)
    {
        using namespace DirectX;
        const XMVECTOR Q = XMLoadFloat4(&q);
        const XMVECTOR n = NormalizeQuatSafe(Q);
        XMStoreFloat4(&m_RotQ, n);
        m_Rot = EulerFromQuat(n);
        m_Dirty = true;
    }

    void AddRotationQuat(DirectX::FXMVECTOR deltaQ)
    {
        using namespace DirectX;
        const XMVECTOR curQ = XMLoadFloat4(&m_RotQ);
        const XMVECTOR nextQ = XMQuaternionMultiply(NormalizeQuatSafe(curQ), NormalizeQuatSafe(deltaQ));
        const XMVECTOR n = NormalizeQuatSafe(nextQ);
        XMStoreFloat4(&m_RotQ, n);
        m_Rot = EulerFromQuat(n);
        m_Dirty = true;
    }

    void AddRotationQuat(const DirectX::XMFLOAT4& deltaQ)
    {
        using namespace DirectX;
        AddRotationQuat(XMLoadFloat4(&deltaQ));
    }

    void NormalizeRotationQuat()
    {
        using namespace DirectX;
        const XMVECTOR n = NormalizeQuatSafe(XMLoadFloat4(&m_RotQ));
        XMStoreFloat4(&m_RotQ, n);
        m_Rot = EulerFromQuat(n);
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
    
    void AddRotationEuler(float dPitch, float dYaw, float dRoll)
    {
        using namespace DirectX;
        const XMVECTOR deltaQ = XMQuaternionRotationRollPitchYaw(dPitch, dYaw, dRoll);
        AddRotationQuat(deltaQ);
        m_Rot.Pitch += dPitch;
        m_Rot.Yaw   += dYaw;
        m_Rot.Roll  += dRoll;
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
        const XMVECTOR n = XMQuaternionNormalize(q);
        XMStoreFloat4(&o.m_RotQ, n);
        o.m_Rot = EulerFromQuat(n);
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

    static DirectX::XMVECTOR NormalizeQuatSafe(DirectX::FXMVECTOR q)
    {
        using namespace DirectX;
        const float lenSq = XMVectorGetX(XMVector4LengthSq(q));
        if (lenSq <= 1e-12f)
        {
            return XMQuaternionIdentity();
        }
        return XMQuaternionNormalize(q);
    }

    static Rotation EulerFromQuat(DirectX::FXMVECTOR q)
    {
        using namespace DirectX;

        const XMVECTOR n = NormalizeQuatSafe(q);
        XMFLOAT4 v{};
        XMStoreFloat4(&v, n);

        // Inverse of XMQuaternionRotationRollPitchYaw(Pitch, Yaw, Roll)
        const float sinPitch = 2.0f * (v.w * v.x - v.y * v.z);
        const float clampedSinPitch = (sinPitch < -1.0f) ? -1.0f : ((sinPitch > 1.0f) ? 1.0f : sinPitch);

        Rotation out{};
        out.Pitch = std::asin(clampedSinPitch);
        out.Yaw = std::atan2(2.0f * (v.w * v.y + v.x * v.z), 1.0f - 2.0f * (v.x * v.x + v.y * v.y));
        out.Roll = std::atan2(2.0f * (v.w * v.z + v.x * v.y), 1.0f - 2.0f * (v.x * v.x + v.z * v.z));
        return out;
    }
};
