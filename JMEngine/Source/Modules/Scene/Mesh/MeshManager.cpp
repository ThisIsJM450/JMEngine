#include "MeshManager.h"

#include "../../Game/MeshData.h"
#include "../../Game/Components/MeshComponent.h"
#include "../../Game/Skeletal/SkeletalMeshData.h"
#include "../../Renderer/RenderData/GPUMesh/GPUMeshSkeletal.h"
#include "../../Renderer/RenderData/GPUMesh/GPUMeshStatic.h"


class SkeletalMeshAsset;

std::unique_ptr<GPUMeshBase> MeshManager::CreateGpuMeshByType(const CpuMeshBase* cpu) const
{
    switch (cpu->GetType())
    {
    case ECpuMeshType::Static:   return std::make_unique<GPUMeshStatic>();
    case ECpuMeshType::Skeletal: return std::make_unique<GPUMeshSkeletal>();
    default:                     return nullptr;
    }
    return nullptr;
}


GPUMeshBase* MeshManager::GetOrCreate(ID3D11Device* dev, SceneProxyBase* procxy) const
{
    if (!procxy)
    {
        return nullptr;
    }
    auto it = m_Cache.find(procxy->GetMeshID());
    if (it != m_Cache.end())
    {
        it->second->Refresh(procxy);
        return it->second.get();
    }
        

    auto gpu = CreateGpuMeshByType(procxy->GetMesh());
    if (!gpu)
    {
        return nullptr;
    }
    
    gpu->Init(dev, procxy->GetMesh());

    auto* ret = gpu.get();
    m_Cache.emplace(procxy->GetMeshID(), std::move(gpu));
    return ret;
}
