#pragma once
#include <DirectXMath.h>

#include "GPUMeshBase.h"

class GPUMeshSkeletal final : public GPUMeshBase
{
public:
    void Init(ID3D11Device* dev, const CpuMeshBase* cpu) override;
    void Refresh(SceneProxyBase* procxy) override;
    virtual bool IsSkinned() { return true; }
    
    std::vector<DirectX::XMFLOAT4X4> BonePalette;
};
