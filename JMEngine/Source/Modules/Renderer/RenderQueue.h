#pragma once

#include <DirectXMath.h>
#include <vector>
#include <cstdint>
#include <memory>

#include "RenderStruct.h"

class GPUMeshBase;
class MaterialInstance;
class Material;

struct RenderItem
{
    GPUMeshBase* mesh;
    std::vector<MaterialInstance*> materials;
    
    DirectX::XMMATRIX world = DirectX::XMMatrixIdentity();
    
    bool castShadow = true;
    bool receiveShadow = true;
    bool bSkinned = false;

    uint64_t sortKey = 0;
};

struct RenderQueue
{
    std::vector<RenderItem> cubeMap;
    std::vector<RenderItem> opaque;
    std::vector<RenderItem> transparent;
    std::vector<RenderItem> shadowCasters;

    void Clear()
    {
        opaque.clear();
        transparent.clear();
        shadowCasters.clear();
        cubeMap.clear();
    }
};