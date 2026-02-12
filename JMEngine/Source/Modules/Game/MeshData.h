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
    DirectX::XMFLOAT4 Tangent;
};

struct MeshSection
{
    uint32_t StartIndex;
    uint32_t IndexCount;
    uint32_t MaterialIndex; // 머티리얼 슬롯 번호
};

enum class ECpuMeshType : uint8_t
{
    Static,
    Skeletal
};

class CpuMeshBase
{
public:
    virtual ~CpuMeshBase() = default;

    virtual ECpuMeshType GetType() const = 0;

    virtual uint32_t GetStride() const = 0;

    virtual uint32_t GetVertexCount() const = 0;
    virtual const void* GetVertexData() const = 0;

    virtual const std::vector<uint32_t>& GetIndices() const = 0;
    virtual const std::vector<MeshSection>& GetSections() const = 0;
    virtual BoundBox GetBoundBox() const = 0;
    
};

class MeshAsset : public CpuMeshBase
{
public:
    ECpuMeshType GetType() const override { return ECpuMeshType::Static; }

    uint32_t GetStride() const override { return Stride; }

    uint32_t GetVertexCount() const override { return (uint32_t)Vertices.size(); }
    const void* GetVertexData() const override { return Vertices.data(); }

    const std::vector<uint32_t>& GetIndices() const override { return Indices; }
    const std::vector<MeshSection>& GetSections() const override { return Sections; }
    BoundBox GetBoundBox() const;

public:
    uint32_t Stride = sizeof(Vertex);
    std::vector<Vertex> Vertices;
    std::vector<uint32_t> Indices;
    std::vector<MeshSection> Sections;
};