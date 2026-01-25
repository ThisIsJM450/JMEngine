#include "MeshFactory.h"

#include <vector>
#include <cmath>
#include "../../Graphics/Dx11Context.h"

using namespace DirectX;

/*
MeshEntity MeshFactory::CreatePlane(Dx11Context& gfx, float halfSize)
{
    std::vector<Vertex> vertices =
    {
        { {-halfSize, 0, -halfSize}, {0,1,0}, {0, 1} },
        { {-halfSize, 0, halfSize}, {0,1,0}, {0, 0} },
        { {halfSize, 0, halfSize}, {0,1,0}, {1, 0} },
        { {halfSize, 0, -halfSize}, {0,1,0}, {1, 1} },
    };
    std::vector<uint16_t> indexs = { 0,1,2, 0,2,3 };

    
    return CreateMeshEntity(gfx, vertices, indexs);;
}

MeshEntity MeshFactory::CreateCube(Dx11Context& gfx, float half)
{
    std::vector<Vertex> vertices;
    vertices.reserve(24);

    auto pushFace = [&](XMFLOAT3 n, XMFLOAT3 a, XMFLOAT3 b, XMFLOAT3 c, XMFLOAT3 d)
    {
        vertices.push_back({ a, n, {0,1} });
        vertices.push_back({ b, n, {0,0} });
        vertices.push_back({ c, n, {1,0} });
        vertices.push_back({ d, n, {1,1} });
    };

    pushFace({0,0,1},  {-half,-half, half}, {-half, half, half}, { half, half, half}, { half,-half, half});
    pushFace({0,0,-1}, { half,-half,-half}, { half, half,-half}, {-half, half,-half}, {-half,-half,-half});
    pushFace({1,0,0},  { half,-half, half}, { half, half, half}, { half, half,-half}, { half,-half,-half});
    pushFace({-1,0,0}, {-half,-half,-half}, {-half, half,-half}, {-half, half, half}, {-half,-half, half});
    pushFace({0,1,0},  {-half, half, half}, {-half, half,-half}, { half, half,-half}, { half, half, half});
    pushFace({0,-1,0}, {-half,-half,-half}, {-half,-half, half}, { half,-half, half}, { half,-half,-half});

    std::vector<uint16_t> indexs;
    indexs.reserve(36);
    for (uint16_t f = 0; f < 6; ++f)
    {
        uint16_t base = f * 4;
        indexs.push_back(base + 0); indexs.push_back(base + 1); indexs.push_back(base + 2);
        indexs.push_back(base + 0); indexs.push_back(base + 2); indexs.push_back(base + 3);
    }

   
    return CreateMeshEntity(gfx, vertices, indexs);;
}

MeshEntity MeshFactory::CreateSphere(Dx11Context& gfx, float radius, uint32_t stacks, uint32_t slices)
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indexs_32;

    vertices.push_back({ {0, radius, 0}, {0,1,0}, {0,0} }); // top

    for (uint32_t s = 1; s < stacks; ++s)
    {
        float phi = XM_PI * (float(s) / float(stacks));
        float y = radius * std::cos(phi);
        float r = radius * std::sin(phi);

        for (uint32_t t = 0; t <= slices; ++t)
        {
            float theta = XM_2PI * (float(t) / float(slices));
            float x = r * std::cos(theta);
            float z = r * std::sin(theta);

            XMFLOAT3 pos{ x, y, z };
            XMVECTOR n = XMVector3Normalize(XMLoadFloat3(&pos));
            XMFLOAT3 normal{};
            XMStoreFloat3(&normal, n);

            float u = float(t) / float(slices);
            float vv = float(s) / float(stacks);
            vertices.push_back({ pos, normal, {u, vv} });
        }
    }

    vertices.push_back({ {0, -radius, 0}, {0,-1,0}, {0,1} }); // bottom

    const uint32_t top = 0;
    const uint32_t bottom = (uint32_t)vertices.size() - 1;
    const uint32_t ringVerts = slices + 1;

    // top cap
    {
        uint32_t ringStart = 1;
        for (uint32_t t = 0; t < slices; ++t)
        {
            indexs_32.push_back(top);
            indexs_32.push_back(ringStart + t);
            indexs_32.push_back(ringStart + t + 1);
        }
    }

    // middle
    for (uint32_t s = 0; s < stacks - 2; ++s)
    {
        uint32_t ringA = 1 + s * ringVerts;
        uint32_t ringB = ringA + ringVerts;

        for (uint32_t t = 0; t < slices; ++t)
        {
            indexs_32.push_back(ringA + t);
            indexs_32.push_back(ringB + t);
            indexs_32.push_back(ringB + t + 1);

            indexs_32.push_back(ringA + t);
            indexs_32.push_back(ringB + t + 1);
            indexs_32.push_back(ringA + t + 1);
        }
    }

    // bottom cap
    {
        uint32_t lastRingStart = bottom - ringVerts;
        for (uint32_t t = 0; t < slices; ++t)
        {
            indexs_32.push_back(bottom);
            indexs_32.push_back(lastRingStart + t + 1);
            indexs_32.push_back(lastRingStart + t);
        }
    }

    assert(vertices.size() < 65535);
    std::vector<uint16_t> indexs;
    indexs.reserve(indexs_32.size());
    for (uint32_t x : indexs_32)
    {
        indexs.push_back((uint16_t)x);
    }

    
    return CreateMeshEntity(gfx, vertices, indexs);;
}

MeshEntity MeshFactory::CreateMeshEntity(Dx11Context& gfx, const std::vector<Vertex> vertices, std::vector<uint16_t> indexs)
{
    MeshEntity entity;
    // entity.meshData = CreateMeshData(gfx, vertices, indexs);
    return entity;
}

MeshAsset MeshFactory::CreateMeshData(Dx11Context& gfx, const std::vector<Vertex> vertices, std::vector<uint16_t> indexs)
{
    MeshAsset meshData;
    //gfx.CreateImmutableBuffer(vertices.data(), (UINT)(vertices.size() * sizeof(Vertex)), D3D11_BIND_VERTEX_BUFFER, meshData.VB);
    //gfx.CreateImmutableBuffer(indexs.data(), (UINT)(indexs.size() * sizeof(uint16_t)), D3D11_BIND_INDEX_BUFFER, meshData.IB);
    meshData.IndexCount = (UINT)indexs.size();

    return meshData;
}
*/
std::shared_ptr<MeshAsset> MeshFactory::CreatePlane(float halfSize)
{
    auto mesh = std::make_shared<MeshAsset>();

    mesh->Vertices =
    {
        { {-halfSize, 0, -halfSize}, {0,1,0}, {0, 1} },
        { {-halfSize, 0,  halfSize}, {0,1,0}, {0, 0} },
        { { halfSize, 0,  halfSize}, {0,1,0}, {1, 0} },
        { { halfSize, 0, -halfSize}, {0,1,0}, {1, 1} },
    };

    mesh->Indices = { 0,1,2, 0,2,3 };
    
    MeshSection section;
    section.MaterialIndex = 0;
    section.StartIndex = 0;
    section.IndexCount = (uint32_t)mesh->Vertices.size();
    mesh->Sections.push_back(section);
    return mesh;
}

std::shared_ptr<MeshAsset> MeshFactory::CreateCube(float half)
{
    auto mesh = std::make_shared<MeshAsset>();
    mesh->Vertices.reserve(24);

    auto pushFace = [&](XMFLOAT3 n, XMFLOAT3 a, XMFLOAT3 b, XMFLOAT3 c, XMFLOAT3 d)
    {
        mesh->Vertices.push_back({ a, n, {0,1} });
        mesh->Vertices.push_back({ b, n, {0,0} });
        mesh->Vertices.push_back({ c, n, {1,0} });
        mesh->Vertices.push_back({ d, n, {1,1} });
    };

    pushFace({0,0, 1}, {-half,-half, half}, {-half, half, half}, { half, half, half}, { half,-half, half});
    pushFace({0,0,-1}, { half,-half,-half}, { half, half,-half}, {-half, half,-half}, {-half,-half,-half});
    pushFace({1,0, 0}, { half,-half, half}, { half, half, half}, { half, half,-half}, { half,-half,-half});
    pushFace({-1,0,0}, {-half,-half,-half}, {-half, half,-half}, {-half, half, half}, {-half,-half, half});
    pushFace({0,1, 0}, {-half, half, half}, {-half, half,-half}, { half, half,-half}, { half, half, half});
    pushFace({0,-1,0}, {-half,-half,-half}, {-half,-half, half}, { half,-half, half}, { half,-half,-half});

    mesh->Indices.reserve(36);
    for (uint32_t f = 0; f < 6; ++f)
    {
        uint32_t base = f * 4;
        mesh->Indices.push_back(base + 0); mesh->Indices.push_back(base + 2); mesh->Indices.push_back(base + 1);
        mesh->Indices.push_back(base + 0); mesh->Indices.push_back(base + 3); mesh->Indices.push_back(base + 2);
    }

    MeshSection section;
    section.MaterialIndex = 0;
    section.StartIndex = 0;
    section.IndexCount = (uint32_t)mesh->Indices.size();
    mesh->Sections.push_back(section);
    return mesh;
}

std::shared_ptr<MeshAsset> MeshFactory::CreateSphere(float radius, uint32_t stacks, uint32_t slices)
{
    auto mesh = std::make_shared<MeshAsset>();

    std::vector<Vertex>& vertices = mesh->Vertices;
    std::vector<uint32_t>& indices = mesh->Indices;

    // 최소값 방어
    stacks = (stacks < 3) ? 3 : stacks;
    slices = (slices < 3) ? 3 : slices;

    vertices.clear();
    indices.clear();

    vertices.push_back({ {0, radius, 0}, {0,1,0}, {0,0} }); // top

    for (uint32_t s = 1; s < stacks; ++s)
    {
        float phi = XM_PI * (float(s) / float(stacks));
        float y = radius * std::cos(phi);
        float r = radius * std::sin(phi);

        for (uint32_t t = 0; t <= slices; ++t)
        {
            float theta = XM_2PI * (float(t) / float(slices));
            float x = r * std::cos(theta);
            float z = r * std::sin(theta);

            XMFLOAT3 pos{ x, y, z };

            XMVECTOR n = XMVector3Normalize(XMLoadFloat3(&pos));
            XMFLOAT3 normal{};
            XMStoreFloat3(&normal, n);

            float u = float(t) / float(slices);
            float v = float(s) / float(stacks);
            vertices.push_back({ pos, normal, {u, v} });
        }
    }

    vertices.push_back({ {0, -radius, 0}, {0,-1,0}, {0,1} }); // bottom

    const uint32_t top = 0;
    const uint32_t bottom = (uint32_t)vertices.size() - 1;
    const uint32_t ringVerts = slices + 1;

    // top cap 
    {
        uint32_t ringStart = 1;
        for (uint32_t t = 0; t < slices; ++t)
        {
            indices.push_back(top);
            indices.push_back(ringStart + t + 1);
            indices.push_back(ringStart + t);

        }
    }

    // middle
    for (uint32_t s = 0; s < stacks - 2; ++s)
    {
        uint32_t ringA = 1 + s * ringVerts;
        uint32_t ringB = ringA + ringVerts;

        for (uint32_t t = 0; t < slices; ++t)
        {
            indices.push_back(ringA + t);
            indices.push_back(ringB + t + 1);
            indices.push_back(ringB + t);


            indices.push_back(ringA + t);
            indices.push_back(ringA + t + 1);
            indices.push_back(ringB + t + 1);

        }
    }

    // bottom cap 
    {
        uint32_t lastRingStart = bottom - ringVerts;
        for (uint32_t t = 0; t < slices; ++t)
        {
            indices.push_back(bottom);
            indices.push_back(lastRingStart + t);
            indices.push_back(lastRingStart + t + 1);
        }
    }
    
    MeshSection section;
    section.MaterialIndex = 0;
    section.StartIndex = 0;
    section.IndexCount = (uint32_t)mesh->Indices.size();
    mesh->Sections.push_back(section);

    return mesh;
}
