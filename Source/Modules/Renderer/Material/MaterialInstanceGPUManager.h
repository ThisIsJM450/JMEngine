#pragma once
#include <unordered_map>
#include <wrl/client.h>
#include <d3d11.h>
#include "../../Graphics/Material/MaterialInstance.h"

struct MaterialInstanceGPUData
{
    Microsoft::WRL::ComPtr<ID3D11Buffer> CBMaterial; // b4
    // uint64_t LastVersion = 0;
};

class MaterialInstanceGPUManager
{
public:
    void Bind(ID3D11Device* dev,
              ID3D11DeviceContext* ctx,
              const MaterialInstance& mi,
              PassType Pass);

private:
    MaterialInstanceGPUData& GetOrCreate(ID3D11Device* dev, const MaterialInstance& mi);
    // void UpdateIfDirty(ID3D11DeviceContext* ctx, MaterialInstanceGPU& gpu, const MaterialInstance& mi);
};