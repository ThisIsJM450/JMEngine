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
    BuildTangents(mesh->Vertices, mesh->Indices);
    
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
    BuildTangents(mesh->Vertices, mesh->Indices);

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

    stacks = (stacks < 3) ? 3 : stacks;
    slices = (slices < 3) ? 3 : slices;

    vertices.clear();
    indices.clear();

    const uint32_t ringVerts = slices + 1;

    // (stacks+1) * (slices+1)
    vertices.reserve((stacks + 1) * ringVerts);
    indices.reserve(stacks * slices * 6);

    // reference 코드 흐름:
    // j=0..stacks (bottom -> top)
    // i=0..slices (seam duplicate)
    // u = i/slices
    // v = 1 - j/stacks
    for (uint32_t j = 0; j <= stacks; ++j)
    {
        const float v = 1.0f - (float(j) / float(stacks)); // bottom=1, top=0
        const float phi = XM_PI * (float(j) / float(stacks)); // 0..PI

        // bottom(-Y) -> top(+Y)
        const float y = -radius * cosf(phi);
        const float r =  radius * sinf(phi);

        for (uint32_t i = 0; i <= slices; ++i)
        {
            const float u = float(i) / float(slices);
            const float theta = -XM_2PI * (float(i) / float(slices)); // sample처럼 음수 회전

            const float x = r * cosf(theta);
            const float z = r * sinf(theta);

            Vertex vert{};
            vert.Pos    = DirectX::XMFLOAT3(x, y, z);

            // normal = position normalized
            {
                DirectX::XMVECTOR n = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&vert.Pos));
                DirectX::XMStoreFloat3(&vert.Normal, n);
            }

            vert.UV = DirectX::XMFLOAT2(u, v);

            // tangent (sample 의도대로): Up(0,1,0)과 normal로 cross 해서 경도 방향 탄젠트 생성
            // (폴에서 cross가 0이 되므로 fallback)
            {
                const DirectX::XMFLOAT3 upF(0, 1, 0);

                DirectX::XMVECTOR N = DirectX::XMLoadFloat3(&vert.Normal);
                DirectX::XMVECTOR Up = DirectX::XMLoadFloat3(&upF);

                DirectX::XMVECTOR T = DirectX::XMVector3Cross(Up, N);
                float len2 = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(T));
                if (len2 < 1e-8f)
                {
                    // 폴(Up과 N이 평행) 근처 fallback
                    T = DirectX::XMVectorSet(1, 0, 0, 0);
                }
                else
                {
                    T = DirectX::XMVector3Normalize(T);
                }

                DirectX::XMFLOAT3 t3;
                DirectX::XMStoreFloat3(&t3, T);

                // w는 handedness(sign). 여기서는 기본 +1로 둠.
                // (B는 셰이더에서 cross(N,T)*w로 재구성하는 형태면 충분)
                vert.Tangent = DirectX::XMFLOAT4(t3.x, t3.y, t3.z, 1.0f);
            }

            vertices.push_back(vert);
        }
    }

    // indices: reference 코드와 동일한 패턴
    // offset = (slices+1)*j
    // (offset+i, offset+i+ringVerts, offset+i+1+ringVerts)
    // (offset+i, offset+i+1+ringVerts, offset+i+1)
    for (uint32_t j = 0; j < stacks; ++j)
    {
        const uint32_t offset = ringVerts * j;
        for (uint32_t i = 0; i < slices; ++i)
        {
            indices.push_back(offset + i);
            indices.push_back(offset + i + 1 + ringVerts);
            indices.push_back(offset + i + ringVerts);

            indices.push_back(offset + i);
            indices.push_back(offset + i + 1);
            indices.push_back(offset + i + 1 + ringVerts);
        }
    }

    // 여기서는 tangent를 이미 넣었으니 BuildTangents로 덮어쓰면 오히려 망가질 수 있음.
    // mesh->Vertices, mesh->Indices 기반으로 sign까지 재계산하려면 별도 BuildTangents(덮어쓰기/혼합 여부 선택)

    MeshSection section;
    section.MaterialIndex = 0;
    section.StartIndex = 0;
    section.IndexCount = (uint32_t)indices.size();
    mesh->Sections.push_back(section);

    return mesh;
}

void MeshFactory::BuildTangents(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
{
    if (vertices.empty() || indices.size() < 3) return;

    // UV 없는 경우 방어(여기선 UV가 있다고 가정하지만, 혹시 모를 방어)
    for (const auto& v : vertices)
    {
        // UV가 전부 0이면 탄젠트 의미 없음(원하면 조건 강화 가능)
        // if (v.UV.x == 0 && v.UV.y == 0) ...
    }

    std::vector<XMFLOAT3> tanSum(vertices.size(), XMFLOAT3(0, 0, 0));
    std::vector<XMFLOAT3> bitanSum(vertices.size(), XMFLOAT3(0, 0, 0));

    auto Add3 = [](XMFLOAT3& a, const XMFLOAT3& b)
    {
        a.x += b.x; a.y += b.y; a.z += b.z;
    };

    const size_t triCount = indices.size() / 3;
    for (size_t t = 0; t < triCount; ++t)
    {
        const uint32_t i0 = indices[t * 3 + 0];
        const uint32_t i1 = indices[t * 3 + 1];
        const uint32_t i2 = indices[t * 3 + 2];

        const Vertex& v0 = vertices[i0];
        const Vertex& v1 = vertices[i1];
        const Vertex& v2 = vertices[i2];

        // edge vectors in object space
        const float x1 = v1.Pos.x - v0.Pos.x;
        const float y1 = v1.Pos.y - v0.Pos.y;
        const float z1 = v1.Pos.z - v0.Pos.z;

        const float x2 = v2.Pos.x - v0.Pos.x;
        const float y2 = v2.Pos.y - v0.Pos.y;
        const float z2 = v2.Pos.z - v0.Pos.z;

        // UV delta
        const float s1 = v1.UV.x - v0.UV.x;
        const float t1 = v1.UV.y - v0.UV.y;
        const float s2 = v2.UV.x - v0.UV.x;
        const float t2 = v2.UV.y - v0.UV.y;

        const float denom = (s1 * t2 - s2 * t1);
        if (fabs(denom) < 1e-8f)
            continue;

        const float r = 1.0f / denom;

        XMFLOAT3 T{
            (t2 * x1 - t1 * x2) * r,
            (t2 * y1 - t1 * y2) * r,
            (t2 * z1 - t1 * z2) * r
        };

        XMFLOAT3 B{
            (s1 * x2 - s2 * x1) * r,
            (s1 * y2 - s2 * y1) * r,
            (s1 * z2 - s2 * z1) * r
        };

        Add3(tanSum[i0], T); Add3(tanSum[i1], T); Add3(tanSum[i2], T);
        Add3(bitanSum[i0], B); Add3(bitanSum[i1], B); Add3(bitanSum[i2], B);
    }

    // 정규화 + 직교화 + handedness(w)
    for (size_t i = 0; i < vertices.size(); ++i)
    {
        Vertex& v = vertices[i];

        XMVECTOR N = XMLoadFloat3(&v.Normal);
        XMVECTOR T = XMLoadFloat3(&tanSum[i]);
        XMVECTOR B = XMLoadFloat3(&bitanSum[i]);

        // 기본값(탄젠트 계산 실패한 경우 대비)
        if (XMVector3Equal(T, XMVectorZero()))
        {
            v.Tangent = XMFLOAT4(1, 0, 0, 1);
            continue;
        }

        N = XMVector3Normalize(N);

        // Gram-Schmidt로 T를 N에 직교화
        T = XMVector3Normalize(T - N * XMVector3Dot(N, T));

        // handedness = sign( dot( cross(N,T), B ) )
        float handed = (XMVectorGetX(XMVector3Dot(XMVector3Cross(N, T), B)) < 0.0f) ? -1.0f : 1.0f;

        XMFLOAT3 t3;
        XMStoreFloat3(&t3, T);

        v.Tangent = XMFLOAT4(t3.x, t3.y, t3.z, handed);
    }
}

