#include "MeshManager.h"

#include <iostream>
#include <ostream>

#include "../../Game/MeshData.h"
#include "../../Renderer/RenderStruct.h"

static void CheckHR(HRESULT hr, const wchar_t* msg)
{
    if (FAILED(hr))
    {
        std::cout << msg << std::endl;
        assert(false); abort();

    }
}


GPUMesh* MeshManager::GetOrCreate(ID3D11Device* dev, uint64_t meshId, const MeshAsset& cpu) const 
{
    auto it = m_Cache.find(meshId);
    if (it != m_Cache.end())
    {
        return it->second.get();
    }
    

    auto gpu = std::make_unique<GPUMesh>();
    gpu->Stride = cpu.Stride;
    gpu->IndexCount = (uint32_t)cpu.Indices.size();
    gpu->IndexFormat = DXGI_FORMAT_R32_UINT;
    
    // Section
    gpu->Sections.clear();
    gpu->Sections.reserve(cpu.Sections.size());
    for (const auto& s : cpu.Sections)
    {
        gpu->Sections.push_back({ s.StartIndex, s.IndexCount, s.MaterialIndex });
    }

    // VB
    D3D11_BUFFER_DESC vbDesc{};
    vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vbDesc.ByteWidth = (UINT)(cpu.Vertices.size() * cpu.Stride);
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbInit{};
    vbInit.pSysMem = cpu.Vertices.data();

    CheckHR(dev->CreateBuffer(&vbDesc, &vbInit, gpu->VB.GetAddressOf()), L"Create VB failed");

    // IB
    D3D11_BUFFER_DESC ibDesc{};
    ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
    ibDesc.ByteWidth = (UINT)(cpu.Indices.size() * sizeof(uint32_t));
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA ibInit{};
    ibInit.pSysMem = cpu.Indices.data();

    CheckHR(dev->CreateBuffer(&ibDesc, &ibInit, gpu->IB.GetAddressOf()), L"Create IB failed");

    auto* ret = gpu.get();
    m_Cache.emplace(meshId, std::move(gpu));
    return ret;
}
