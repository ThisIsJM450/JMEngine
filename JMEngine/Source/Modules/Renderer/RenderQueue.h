#pragma once

#include <DirectXMath.h>
#include <vector>
#include <cstdint>
#include <memory>

#include "RenderStruct.h"
#include "../Game/MeshData.h"

class MaterialInstance;
class Material;
//struct MeshData;

struct RenderItem
{
    GPUMesh* mesh;
    std::vector<MaterialInstance*> materials;
    //MaterialInstance* materialInstance = nullptr;
    
    DirectX::XMMATRIX world = DirectX::XMMatrixIdentity();



    bool castShadow = true;
    bool receiveShadow = true;

    uint64_t sortKey = 0;
};

struct RenderQueue
{
    std::vector<RenderItem> opaque;
    std::vector<RenderItem> transparent;
    std::vector<RenderItem> shadowCasters;

    void Clear()
    {
        opaque.clear();
        transparent.clear();
        shadowCasters.clear();
    }
};