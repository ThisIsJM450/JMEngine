#include "WorldQuery.h"

#include "../Actor.h"
#include "../Components/MeshComponent.h"

#include <DirectXCollision.h>
#include <algorithm>

namespace
{
    DirectX::XMFLOAT3 ComputeHitNormal(const BoundBox& bounds, const DirectX::XMFLOAT3& hitLocation)
    {
        const float localX = hitLocation.x - bounds.Center.x;
        const float localY = hitLocation.y - bounds.Center.y;
        const float localZ = hitLocation.z - bounds.Center.z;

        const float nx = (bounds.Extents.x > 1e-6f) ? std::abs(localX / bounds.Extents.x) : 0.0f;
        const float ny = (bounds.Extents.y > 1e-6f) ? std::abs(localY / bounds.Extents.y) : 0.0f;
        const float nz = (bounds.Extents.z > 1e-6f) ? std::abs(localZ / bounds.Extents.z) : 0.0f;

        if (nx >= ny && nx >= nz)
        {
            return DirectX::XMFLOAT3((localX >= 0.0f) ? 1.0f : -1.0f, 0.0f, 0.0f);
        }
        if (ny >= nx && ny >= nz)
        {
            return DirectX::XMFLOAT3(0.0f, (localY >= 0.0f) ? 1.0f : -1.0f, 0.0f);
        }
        return DirectX::XMFLOAT3(0.0f, 0.0f, (localZ >= 0.0f) ? 1.0f : -1.0f);
    }
}

void WorldQueryCollisionSystem::RegisterTraceableMesh(MeshComponent* meshComponent)
{
    if (!meshComponent)
    {
        return;
    }

    if (std::find(m_TraceableMeshes.begin(), m_TraceableMeshes.end(), meshComponent) == m_TraceableMeshes.end())
    {
        m_TraceableMeshes.push_back(meshComponent);
    }
}

void WorldQueryCollisionSystem::UnregisterTraceableMesh(MeshComponent* meshComponent)
{
    m_TraceableMeshes.erase(std::remove(m_TraceableMeshes.begin(), m_TraceableMeshes.end(), meshComponent), m_TraceableMeshes.end());
}

void WorldQueryCollisionSystem::Clear()
{
    m_TraceableMeshes.clear();
}

bool WorldQueryCollisionSystem::Raycast(
    const DirectX::XMFLOAT3& start,
    const DirectX::XMFLOAT3& end,
    RaycastHitResult& outHit,
    const RaycastQueryParams& queryParams) const
{
    using namespace DirectX;

    outHit = RaycastHitResult{};

    const XMVECTOR startV = XMLoadFloat3(&start);
    const XMVECTOR endV = XMLoadFloat3(&end);
    const XMVECTOR delta = XMVectorSubtract(endV, startV);
    const float segmentLength = XMVectorGetX(XMVector3Length(delta));
    if (segmentLength <= 1e-6f)
    {
        return false;
    }

    const XMVECTOR rayDir = XMVectorScale(delta, 1.0f / segmentLength);

    float bestDistance = segmentLength;
    bool bHit = false;

    for (MeshComponent* meshComponent : m_TraceableMeshes)
    {
        if (!meshComponent || meshComponent == queryParams.IgnoreComponent)
        {
            continue;
        }

        const Actor* owner = meshComponent->GetOwner();
        if (queryParams.IgnoreActor && owner == queryParams.IgnoreActor)
        {
            continue;
        }

        SceneProxyBase* proxy = meshComponent->GetProxy();
        if (!proxy)
        {
            continue;
        }

        if (queryParams.bIgnoreCubeMap && proxy->IsCubeMap())
        {
            continue;
        }

        const BoundBox bounds = proxy->GetBounds();
        const BoundingBox dxBox(bounds.Center, bounds.Extents);

        float distance = 0.0f;
        if (!dxBox.Intersects(startV, rayDir, distance))
        {
            continue;
        }

        if (distance < 0.0f || distance > bestDistance)
        {
            continue;
        }

        bestDistance = distance;
        bHit = true;

        outHit.bBlockingHit = true;
        outHit.Distance = distance;
        outHit.HitActor = owner;
        outHit.HitComponent = meshComponent;
        outHit.HitProxy = proxy;
        outHit.HitBounds = bounds;

        const XMVECTOR hitLocationV = XMVectorAdd(startV, XMVectorScale(rayDir, distance));
        XMStoreFloat3(&outHit.Location, hitLocationV);
        outHit.Normal = ComputeHitNormal(bounds, outHit.Location);
    }

    return bHit;
}
