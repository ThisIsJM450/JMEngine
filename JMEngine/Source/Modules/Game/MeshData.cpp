#include "MeshData.h"

BoundBox MeshAsset::GetBoundBox() const
{
    std::vector<DirectX::XMFLOAT3> vertices;
    for (const Vertex& vertex : Vertices)
    {
        vertices.push_back(vertex.Pos);
    }

    return BoundBox::FromVertices(vertices);
}
