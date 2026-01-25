#include "Frustum.h"

#include <cmath> // sqrtf, fabsf

#include "../../Core/CoreStruct.h"

using namespace DirectX;

void Frustum::NormalizePlane(XMFLOAT4& p)
{
    const float len = std::sqrt(p.x*p.x + p.y*p.y + p.z*p.z);
    if (len > 1e-6f)
    {
        p.x /= len; p.y /= len; p.z /= len; p.w /= len;
    }
}

XMFLOAT4 Frustum::PlaneFromPoints(XMVECTOR a, XMVECTOR b, XMVECTOR c, XMVECTOR insidePoint)
{
    // n = normalize( (b-a) x (c-a) )
    XMVECTOR n = XMVector3Cross(b - a, c - a);
    n = XMVector3Normalize(n);

    float d = -XMVectorGetX(XMVector3Dot(n, a));

    // insidePoint가 plane의 안쪽(>=0)이 되도록 방향 플립
    float side = XMVectorGetX(XMVector3Dot(n, insidePoint)) + d;
    if (side < 0.0f)
    {
        n = XMVectorNegate(n);
        d = -d;
    }

    XMFLOAT3 nf{};
    XMStoreFloat3(&nf, n);

    XMFLOAT4 p(nf.x, nf.y, nf.z, d);
    NormalizePlane(p);
    return p;
}

void Frustum::Build(const DirectX::XMMATRIX& viewProj)
{
    using namespace DirectX;

    static const XMVECTOR ndc[8] =
    {
        XMVectorSet(-1.f, -1.f, 0.f, 1.f),
        XMVectorSet(-1.f, +1.f, 0.f, 1.f),
        XMVectorSet(+1.f, -1.f, 0.f, 1.f),
        XMVectorSet(+1.f, +1.f, 0.f, 1.f),

        XMVectorSet(-1.f, -1.f, 1.f, 1.f),
        XMVectorSet(-1.f, +1.f, 1.f, 1.f),
        XMVectorSet(+1.f, -1.f, 1.f, 1.f),
        XMVectorSet(+1.f, +1.f, 1.f, 1.f),
    };

    // inverse 필수
    XMMATRIX invVP = XMMatrixInverse(nullptr, viewProj);
    XMFLOAT4X4 inv;
    XMStoreFloat4x4(&inv, invVP);

    for (int i = 0; i < 8; ++i)
    {
        float x = XMVectorGetX(ndc[i]);
        float y = XMVectorGetY(ndc[i]);
        float z = XMVectorGetZ(ndc[i]);
        float w = 1.0f;

        // row-vector: pWS4 = pNDC * invVP
        float rx = x*inv._11 + y*inv._21 + z*inv._31 + w*inv._41;
        float ry = x*inv._12 + y*inv._22 + z*inv._32 + w*inv._42;
        float rz = x*inv._13 + y*inv._23 + z*inv._33 + w*inv._43;
        float rw = x*inv._14 + y*inv._24 + z*inv._34 + w*inv._44;

        float invW = (std::fabs(rw) > 1e-6f) ? (1.0f / rw) : 1.0f;
        m_CornersWS[i] = XMVectorSet(rx*invW, ry*invW, rz*invW, 1.0f);
    }

    XMVECTOR center = GetCenter();

    // 코너
    m_Planes[4] = PlaneFromPoints(m_CornersWS[0], m_CornersWS[1], m_CornersWS[2], center); // Near
    m_Planes[5] = PlaneFromPoints(m_CornersWS[4], m_CornersWS[6], m_CornersWS[5], center); // Far

    m_Planes[0] = PlaneFromPoints(m_CornersWS[0], m_CornersWS[4], m_CornersWS[1], center); // Left
    m_Planes[1] = PlaneFromPoints(m_CornersWS[2], m_CornersWS[3], m_CornersWS[6], center); // Right
    m_Planes[2] = PlaneFromPoints(m_CornersWS[0], m_CornersWS[2], m_CornersWS[4], center); // Bottom
    m_Planes[3] = PlaneFromPoints(m_CornersWS[1], m_CornersWS[5], m_CornersWS[3], center); // Top
}

bool Frustum::Intersects(const BoundBox& worldBound) const
{
    const float cx = worldBound.Center.x, cy = worldBound.Center.y, cz = worldBound.Center.z;
    const float ex = worldBound.Extents.x, ey = worldBound.Extents.y, ez = worldBound.Extents.z;

    for (const XMFLOAT4& p : m_Planes)
    {
        const float nx = p.x, ny = p.y, nz = p.z;

        // AABB를 plane normal 방향으로 투영했을 때의 반지름
        const float r = ex * std::fabs(nx) + ey * std::fabs(ny) + ez * std::fabs(nz);

        // center to plane distance
        const float d = nx*cx + ny*cy + nz*cz + p.w;

        if (d < -r)
            return false;
    }
    return true;
}

XMVECTOR Frustum::GetCenter() const
{
    XMVECTOR center = XMVectorZero();
    for (int c = 0; c < 8; ++c)
        center += m_CornersWS[c];

    return center * (1.0f / 8.0f);
}

float Frustum::GetRadius() const
{
    const XMVECTOR center = GetCenter();
    float radius = 0.0f;

    for (int c = 0; c < 8; ++c)
    {
        const float dist = XMVectorGetX(XMVector3Length(m_CornersWS[c] - center));
        if (dist > radius) radius = dist;
    }
    return radius;
}

const XMVECTOR& Frustum::GetExtent(int32_t index) const
{
    return m_CornersWS[index];
}
