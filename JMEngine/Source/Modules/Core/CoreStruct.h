#pragma once
#include <DirectXMath.h>
#include <vector>

struct BoundBox
{
    // UE 스타일로 "Center + Extents(half size)"
    DirectX::XMFLOAT3 Center {0,0,0};
    DirectX::XMFLOAT3 Extents{0,0,0}; // half sizes

    BoundBox ToWorldBound(const DirectX::XMMATRIX& world) const
    {
        using namespace DirectX;

        // local center -> world center
        XMVECTOR c = XMLoadFloat3(&Center);
        c = XMVector3TransformCoord(c, world);
        XMFLOAT3 wc;
        XMStoreFloat3(&wc, c);

        // local extents -> world extents (AABB 보수적 변환)
        // world의 3x3 절대값 행렬 * local extents
        XMFLOAT3 e = Extents;

        XMFLOAT3X3 A;
        A._11 = XMVectorGetX(world.r[0]); A._12 = XMVectorGetY(world.r[0]); A._13 = XMVectorGetZ(world.r[0]);
        A._21 = XMVectorGetX(world.r[1]); A._22 = XMVectorGetY(world.r[1]); A._23 = XMVectorGetZ(world.r[1]);
        A._31 = XMVectorGetX(world.r[2]); A._32 = XMVectorGetY(world.r[2]); A._33 = XMVectorGetZ(world.r[2]);

        auto absf = [](float v){ return v < 0 ? -v : v; };

        DirectX::XMFLOAT3 we;
        we.x = absf(A._11) * e.x + absf(A._21) * e.y + absf(A._31) * e.z;
        we.y = absf(A._12) * e.x + absf(A._22) * e.y + absf(A._32) * e.z;
        we.z = absf(A._13) * e.x + absf(A._23) * e.y + absf(A._33) * e.z;

        return BoundBox{ wc, we };
    }

    static BoundBox FromVertices(const std::vector<DirectX::XMFLOAT3>& verts)
    {
        BoundBox b{};
        if (verts.empty()) return b;

        DirectX::XMFLOAT3 mn = verts[0];
        DirectX::XMFLOAT3 mx = verts[0];

        for (const DirectX::XMFLOAT3& v : verts)
        {
            mn.x = (v.x < mn.x) ? v.x : mn.x;
            mn.y = (v.y < mn.y) ? v.y : mn.y;
            mn.z = (v.z < mn.z) ? v.z : mn.z;

            mx.x = (v.x > mx.x) ? v.x : mx.x;
            mx.y = (v.y > mx.y) ? v.y : mx.y;
            mx.z = (v.z > mx.z) ? v.z : mx.z;
        }

        DirectX::XMFLOAT3 center { (mn.x + mx.x)*0.5f, (mn.y + mx.y)*0.5f, (mn.z + mx.z)*0.5f };
        DirectX::XMFLOAT3 ext    { (mx.x - mn.x)*0.5f, (mx.y - mn.y)*0.5f, (mx.z - mn.z)*0.5f };

        b.Center = center;
        b.Extents = ext;
        return b;
    }
};
