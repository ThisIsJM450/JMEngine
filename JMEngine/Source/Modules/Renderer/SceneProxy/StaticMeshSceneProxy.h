#pragma once
#include <DirectXMath.h>

#include "../../Core/CoreStruct.h"
#include "../../Game/MeshData.h"


class MaterialInstance;

class StaticMeshSceneProxy
{
public:
    const MeshAsset* Mesh = nullptr;
    //Material* Mat = nullptr;
    // MaterialInstance* materialInstance = nullptr;
    std::vector<MaterialInstance*> materialInstances;
    bool CastShadow = true;
    bool ReceiveShadow = true;
    int64_t MeshID = -1;
    DirectX::XMMATRIX WorldMatrix = DirectX::XMMatrixIdentity();
    BoundBox Bounds;

    // tODO: in material inst
    // DirectX::XMFLOAT4 color = {1, 1, 1, 1};
};
