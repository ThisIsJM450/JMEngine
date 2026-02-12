#pragma once
#include <string>
#include <unordered_map>
#include <assimp/matrix4x4.h>

#include "../MeshData.h"

struct SkinnedVertex
{
    DirectX::XMFLOAT3 Pos;
    DirectX::XMFLOAT3 Normal;
    DirectX::XMFLOAT2 UV;
    DirectX::XMFLOAT4 Tangent;
    
    uint32_t  BoneIndex[4] = {0,0,0,0};
    float    BoneWeight[4] = {0,0,0,0};
};

struct BoneInfo
{
    std::string Name;
    int ParentIndex = -1;

    DirectX::XMFLOAT4X4 Offset;      // aiBone::mOffsetMatrix (to bone space)
    aiMatrix4x4 OffsetA;
};

struct NodeInfo
{
    std::string Name;
    int32_t Parent = -1;
    std::vector<int32_t> Children;

    DirectX::XMFLOAT4X4 RefLocal;  // aiNode::mTransformation (bind/local)
};

struct Skeleton
{
    std::vector<BoneInfo> Bones;
    std::unordered_map<std::string, uint32_t> BoneNameToIndex;
    DirectX::XMFLOAT4X4 GlobalInverse;
    
    std::vector<DirectX::XMFLOAT4X4> RefLocalPose; // 기본 로컬 포즈
    
    // node 트리
    std::vector<NodeInfo> Nodes;
    std::unordered_map<std::string, int32_t> NodeNameToIndex;
    std::vector<int32_t> BoneToNode; // bone(64) -> node index 매핑
};

class SkeletalMeshAsset : public CpuMeshBase
{
public:
    ECpuMeshType GetType() const override { return ECpuMeshType::Skeletal; }

    uint32_t GetStride() const override { return Stride; }

    uint32_t GetVertexCount() const override { return (uint32_t)Vertices.size(); }
    const void* GetVertexData() const override { return Vertices.data(); }

    const std::vector<uint32_t>& GetIndices() const override { return Indices; }
    const std::vector<MeshSection>& GetSections() const override { return Sections; }
    BoundBox GetBoundBox() const override;

public:
    uint32_t Stride = sizeof(SkinnedVertex);
    std::vector<SkinnedVertex> Vertices;
    std::vector<uint32_t> Indices;
    std::vector<MeshSection> Sections;

    Skeleton Skeleton;
    std::vector<DirectX::XMFLOAT4X4> BonePalette; // 본 팔레트(최대 N개)용: 런타임 계산 결과를 여기 담아 ConstantBuffer로 보냄
};