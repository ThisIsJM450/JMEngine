#pragma once

#include "../../Core/Transform.h"
#include "ActorComponent.h"
#include <vector>

class SceneComponent : public ActorComponent
{
public:
    
    SceneComponent() 
    {
        TypeName = std::string("ActorComponent");
    }
    
    void AttachTo(SceneComponent* parent)
    {
        if (m_Parent == parent) return;
        m_Parent = parent;
        if (parent) parent->m_Children.push_back(this);
        MarkDirty();
    }

    Transform& GetRelativeTransform() { return m_Relative; }
    const Transform& GetRelativeTransform() const { return m_Relative; }

    DirectX::XMMATRIX GetWorldMatrix() const
    {
        using namespace DirectX;
        XMMATRIX local = m_Relative.ToMatrix();
        if (m_Parent)
        {
            m_CachedWorld = local * m_Parent->GetWorldMatrix();
        }
        else
        {
            m_CachedWorld = local;
        }
        
        m_Dirty = false;
        return m_CachedWorld;
    }

    DirectX::XMFLOAT3 GetWorldLocation() const
    {
        using namespace DirectX;
        XMMATRIX w = GetWorldMatrix();
        XMFLOAT3 p;
        XMStoreFloat3(&p, w.r[3]);
        return p;
    }

    DirectX::XMVECTOR GetWorldForward() const
    {
        using namespace DirectX;

        const XMVECTOR localF = m_Relative.GetForwardVector(); // 보통 w=0 의미(방향)

        if (!m_Parent)
        {
            return XMVector3Normalize(localF);
        }

        const XMMATRIX parentWorld = m_Parent->GetWorldMatrix();

        // parentWorld = S * R * T 형태로 분해 (scale/rotation/translation)
        XMVECTOR S, R, T;
        if (!XMMatrixDecompose(&S, &R, &T, parentWorld))
        {
            // 분해 실패 시: 기존 방식에 safe normalize라도 적용 (최후의 폴백)
            XMVECTOR v = XMVector3TransformNormal(localF, parentWorld);
            const float lenSq = XMVectorGetX(XMVector3LengthSq(v));
            if (lenSq < 1e-8f) return XMVectorSet(0, 0, 1, 0);
            return XMVector3Normalize(v);
        }

        // 회전만 적용 (스케일 영향 제거)
        XMVECTOR worldF = XMVector3Rotate(localF, R);

        // safe normalize
        const float lenSq = XMVectorGetX(XMVector3LengthSq(worldF));
        if (lenSq < 1e-8f)
            return XMVectorSet(0, 0, 1, 0);

        return XMVector3Normalize(worldF);
    }
    
    DirectX::XMVECTOR GetWorldUp() const
    {
        using namespace DirectX;

        const XMVECTOR localF = m_Relative.GetUpVector(); // 보통 w=0 의미(방향)

        if (!m_Parent)
        {
            return XMVector3Normalize(localF);
        }

        const XMMATRIX parentWorld = m_Parent->GetWorldMatrix();

        // parentWorld = S * R * T 형태로 분해 (scale/rotation/translation)
        XMVECTOR S, R, T;
        if (!XMMatrixDecompose(&S, &R, &T, parentWorld))
        {
            // 분해 실패 시: 기존 방식에 safe normalize라도 적용 (최후의 폴백)
            XMVECTOR v = XMVector3TransformNormal(localF, parentWorld);
            const float lenSq = XMVectorGetX(XMVector3LengthSq(v));
            if (lenSq < 1e-8f) return XMVectorSet(0, 0, 1, 0);
            return XMVector3Normalize(v);
        }

        // 회전만 적용 (스케일 영향 제거)
        XMVECTOR worldF = XMVector3Rotate(localF, R);

        // safe normalize
        const float lenSq = XMVectorGetX(XMVector3LengthSq(worldF));
        if (lenSq < 1e-8f)
            return XMVectorSet(0, 0, 1, 0);

        return XMVector3Normalize(worldF);
    }

    void MarkDirty()
    {
        m_Dirty = true;
        for (SceneComponent* c : m_Children) c->MarkDirty();
    }

private:
    Transform m_Relative;
    SceneComponent* m_Parent = nullptr;
    std::vector<SceneComponent*> m_Children;

    mutable bool m_Dirty = true;
    mutable DirectX::XMMATRIX m_CachedWorld = DirectX::XMMatrixIdentity();
};