#pragma once

#include <DirectXMath.h>
#include <cstdint>


struct BoundBox;

class Frustum
{
public:
    // viewProj: "월드 -> 클립" 변환을 나타내는 행렬이어야 함
    // (CPP에서 row-vector 규약을 쓰더라도, 코너 기반 plane 생성이라 규약 의존이 적음)
    void Build(const DirectX::XMMATRIX& viewProj);

    bool Intersects(const BoundBox& worldBound) const;

    DirectX::XMVECTOR GetCenter() const;
    float GetRadius() const;

    const DirectX::XMVECTOR& GetExtent(int32_t index) const;

//private:
    // Planes: 0=Left, 1=Right, 2=Bottom, 3=Top, 4=Near, 5=Far
    DirectX::XMFLOAT4 m_Planes[6]{};
    DirectX::XMVECTOR m_CornersWS[8]{};

private:
    static void NormalizePlane(DirectX::XMFLOAT4& p);

    // 3점으로 plane 만들고, insidePoint가 안쪽이 되도록 normal 방향 정렬
    static DirectX::XMFLOAT4 PlaneFromPoints(
        DirectX::XMVECTOR a, DirectX::XMVECTOR b, DirectX::XMVECTOR c,
        DirectX::XMVECTOR insidePoint);
};
