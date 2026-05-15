// GPUMeshBase.h
#pragma once
#include <cstdint>
#include <vector>
#include <wrl/client.h>
#include <d3d11.h>


class SceneProxyBase;
class CpuMeshBase;

struct GPUMeshSection
{
    uint32_t StartIndex = 0;
    uint32_t IndexCount = 0;
    uint32_t MaterialIndex = 0;
};

class GPUMeshBase
{
public:
    virtual ~GPUMeshBase() = default;


    uint32_t Stride = 0;
    uint32_t IndexCount = 0;
    DXGI_FORMAT IndexFormat = DXGI_FORMAT_R32_UINT;

    std::vector<GPUMeshSection> Sections;

    Microsoft::WRL::ComPtr<ID3D11Buffer> VB;
    Microsoft::WRL::ComPtr<ID3D11Buffer> IB;
    UINT Offset = 0;

    virtual void Init(ID3D11Device* dev, const CpuMeshBase* cpu) = 0;
    virtual void Refresh(SceneProxyBase* procxy) {}
    virtual bool IsSkinned() { return false; }
};
