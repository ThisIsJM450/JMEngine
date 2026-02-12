#pragma once
#include <DirectXMath.h>

#include "../../Core/CoreStruct.h"
#include "../../Game/MeshData.h"
#include "../../Game/Components/MeshComponent.h"
#include "../../Graphics/Material/MaterialInstance.h"


class StaticMeshComponent;
class MaterialInstance;

class StaticMeshSceneProxy : public SceneProxyBase
{
public:
    StaticMeshSceneProxy() = default;

    const MeshAsset* Mesh = nullptr;
    std::vector<MaterialInstance*> materialInstances;
    bool CastShadow = true;
    bool ReceiveShadow = true;
    int64_t MeshID = -1;

    StaticMeshComponent* Ownner = nullptr;

    // 기존 구현(Owner에서 트랜스폼 가져오는 방식)
    DirectX::XMMATRIX GetWorldMatrix() const override;

    // ===== SceneProxyBase required overrides =====
    int64_t GetMeshID() const override { return MeshID; }

    const CpuMeshBase* GetMesh() const override { return Mesh; }

    const std::vector<MaterialInstance*>& GetMaterialInstances() const override
    {
        return materialInstances;
    }

    bool GetCastShadow() const override { return CastShadow; }
    bool GetReceiveShadow() const override { return ReceiveShadow; }
    // ============================================

    bool IsCubeMap() const override
    {
        if (!materialInstances.empty() && materialInstances[0])
            return materialInstances[0]->IsCubeMap();
        return false;
    }
};

