#include "StaticMeshSceneProxy.h"

#include "../../Game/Components/StaticMeshComponent.h"

DirectX::XMMATRIX StaticMeshSceneProxy::GetWorldMatrix() const
{
    if (Ownner)
    {
        return Ownner->GetWorldMatrix();
        
    }
    return DirectX::XMMatrixIdentity();
}
