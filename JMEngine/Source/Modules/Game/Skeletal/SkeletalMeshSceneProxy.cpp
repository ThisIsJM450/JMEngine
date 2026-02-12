#include "SkeletalMeshSceneProxy.h"

#include <algorithm>
#include <cstring>

#include "SkeletalMeshComponent.h"
#include "SkeletalMeshData.h"

SkeletalMeshSceneProxy::~SkeletalMeshSceneProxy()
{

}

DirectX::XMMATRIX SkeletalMeshSceneProxy::GetWorldMatrix() const
{
    if (Ownner)
    {
        return Ownner->GetWorldMatrix();
        
    }
    return DirectX::XMMatrixIdentity();
}
