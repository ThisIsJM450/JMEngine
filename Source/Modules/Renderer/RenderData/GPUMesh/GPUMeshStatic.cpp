#include "GPUMeshStatic.h"
#include "GPUMeshStatic.h"

#include <cassert>
#include <iostream>
#include <ostream>

#include "../../../Game/MeshData.h"

static void CheckHR(HRESULT hr, const wchar_t* msg)
{
    if (FAILED(hr))
    {
        std::cout << msg << std::endl;
        assert(false); abort();

    }
}

void GPUMeshStatic::Init(ID3D11Device* dev, const CpuMeshBase* cpu)
{
    if (!dev || !cpu)
        return; 

    Stride = cpu->GetStride();
    IndexCount = (uint32_t)cpu->GetIndices().size();
    IndexFormat = DXGI_FORMAT_R32_UINT;

    // Sections
    Sections.clear();
    Sections.reserve(cpu->GetSections().size());
    for (const auto& s : cpu->GetSections())
    {
        Sections.push_back({ s.StartIndex, s.IndexCount, s.MaterialIndex });
    }
        

    // VB
    D3D11_BUFFER_DESC vbDesc{};
    vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vbDesc.ByteWidth = (UINT)(cpu->GetVertexCount() * Stride);
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbInit{};
    vbInit.pSysMem = cpu->GetVertexData();

    CheckHR(dev->CreateBuffer(&vbDesc, &vbInit, VB.GetAddressOf()), L"Create VB failed");

    // IB
    const auto& indices = cpu->GetIndices();

    D3D11_BUFFER_DESC ibDesc{};
    ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
    ibDesc.ByteWidth = (UINT)(indices.size() * sizeof(uint32_t));
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA ibInit{};
    ibInit.pSysMem = indices.data();

    CheckHR(dev->CreateBuffer(&ibDesc, &ibInit, IB.GetAddressOf()), L"Create IB failed");
}
