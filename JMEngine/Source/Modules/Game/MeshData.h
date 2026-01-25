#pragma once
#include <cstdint>
#include <DirectXMath.h>
#include <vector>

#include "../Core/CoreStruct.h"

struct Vertex
{
    DirectX::XMFLOAT3 Pos;
    DirectX::XMFLOAT3 Normal;
    DirectX::XMFLOAT2 UV;
};

struct MeshSection
{
    uint32_t StartIndex;
    uint32_t IndexCount;
    uint32_t MaterialIndex; // 머티리얼 슬롯 번호
};

struct MeshAsset
{
    std::vector<Vertex> Vertices;
    std::vector<uint32_t> Indices; // 16/32는 렌더에서 결정
    uint32_t Stride = sizeof(Vertex);
    std::vector<MeshSection> Sections;
    
    BoundBox GetBoundBox() const;
};