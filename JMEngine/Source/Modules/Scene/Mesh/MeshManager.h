#pragma once
#include <cstdint>
#include <d3d11.h>
#include <memory>
#include <unordered_map>
#include "../../Renderer/RenderData/GPUMesh/GPUMeshBase.h"

class SceneProxyBase;
class CpuMeshBase;

/**
 * CPUData를 GPUData로 변환 및 GPU Data 관리하는 역할
 */
class MeshManager
{
public:
    
    GPUMeshBase* GetOrCreate(ID3D11Device* dev, SceneProxyBase* procxy) const;

private:
    std::unique_ptr<GPUMeshBase> CreateGpuMeshByType(const CpuMeshBase* cpu) const;

private:
    mutable std::unordered_map<uint64_t, std::unique_ptr<GPUMeshBase>> m_Cache;
};
