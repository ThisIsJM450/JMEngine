#pragma once
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <wrl/client.h>

#include <assimp/scene.h>
#include <d3d11.h>

#include "FbxImportUtils.h"

struct MaterialInstance;

namespace FbxMaterialBuilder
{
    using Microsoft::WRL::ComPtr;

    struct FMetalRoughSRVs
    {
        ComPtr<ID3D11ShaderResourceView> MetallicSRV;
        ComPtr<ID3D11ShaderResourceView> RoughnessSRV;
        bool bPackedMR_GB = false;
    };

    std::vector<std::shared_ptr<MaterialInstance>> BuildMaterialsByMatIndex(
        ID3D11Device* device,
        const aiScene* scene,
        const std::wstring& baseDir,
        const std::vector<uint8_t>& usedMaterials);

    // (내부적으로 쓰는 텍스처 로더/유틸)
}
