#pragma once
#include <cstdint>
#include <d3d11.h>
#include <memory>
#include <unordered_map>

struct MeshAsset;
struct GPUMesh;

/**
 * CPUData를 GPUData로 변환 및 GPU Data 관리하는 역할
 */
class MeshManager
{
public:
    GPUMesh* GetOrCreate(ID3D11Device* dev, uint64_t meshId, const MeshAsset& cpu) const;

private:
    mutable std::unordered_map<uint64_t, std::unique_ptr<GPUMesh>> m_Cache;
};
