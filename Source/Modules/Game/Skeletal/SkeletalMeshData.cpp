#include "SkeletalMeshData.h"

BoundBox SkeletalMeshAsset::GetBoundBox() const
{
    std::vector<DirectX::XMFLOAT3> vertices;
    for (const SkinnedVertex& vertex : Vertices)
    {
        vertices.push_back(vertex.Pos);
    }

    return BoundBox::FromVertices(vertices);
}
