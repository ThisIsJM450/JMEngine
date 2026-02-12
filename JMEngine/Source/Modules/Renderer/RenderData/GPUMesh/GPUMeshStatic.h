#pragma once
#include "GPUMeshBase.h"

class GPUMeshStatic final : public GPUMeshBase
{
public:
    void Init(ID3D11Device* dev, const CpuMeshBase* cpu) override;
};