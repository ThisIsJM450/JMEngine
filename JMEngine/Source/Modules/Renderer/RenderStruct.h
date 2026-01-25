#pragma once
#include <cstdint>
#include <d3d11.h>
#include <wrl/client.h>

struct GPUMeshSection
{
    uint32_t StartIndex = 0;
    uint32_t IndexCount = 0;
    uint32_t MaterialIndex = 0;
};

struct GPUMesh
{
    Microsoft::WRL::ComPtr<ID3D11Buffer> VB;
    Microsoft::WRL::ComPtr<ID3D11Buffer> IB;
    
    DXGI_FORMAT IndexFormat = DXGI_FORMAT_R32_UINT;
    uint32_t IndexCount = 0;
    
    uint32_t Stride = 0;
    uint32_t Offset = 0;
    
    std::vector<GPUMeshSection> Sections;
};
