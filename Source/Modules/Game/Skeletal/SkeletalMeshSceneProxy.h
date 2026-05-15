#pragma once

#include <cstdint>
#include <vector>
#include <wrl/client.h>

#include <d3d11.h>
#include <DirectXMath.h>

#include "SkeletalMeshData.h"
#include "../../Graphics/Material/MaterialInstance.h"
#include "../../Game/Components/MeshComponent.h" 

class SkeletalMeshAsset;
class SkeletalMeshComponent;

class SkeletalMeshSceneProxy : public SceneProxyBase
{
public:
    SkeletalMeshSceneProxy() = default;
    ~SkeletalMeshSceneProxy();

public:
    SkeletalMeshComponent* Ownner = nullptr;
    const SkeletalMeshAsset* Mesh = nullptr;

    bool CastShadow = true;
    bool ReceiveShadow = true;

    uint64_t MeshID = 0;
    std::vector<MaterialInstance*> materialInstances;

    std::vector<DirectX::XMFLOAT4X4> BonePalette;
    bool bBoneDirty = false;

public:
    // ===== SceneProxyBase required overrides =====
    int64_t GetMeshID() const override { return (int64_t)MeshID; }

    const CpuMeshBase* GetMesh() const override
    {
        return Mesh;
    }

    const std::vector<MaterialInstance*>& GetMaterialInstances() const override
    {
        return materialInstances;
    }

    DirectX::XMMATRIX GetWorldMatrix() const override;
    bool GetCastShadow() const override { return CastShadow; }
    bool GetReceiveShadow() const override { return ReceiveShadow; }

};
