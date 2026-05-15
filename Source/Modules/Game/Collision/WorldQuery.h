#pragma once

#include <DirectXMath.h>

#include "../../Core/CoreStruct.h"

#include <vector>

class Actor;
class MeshComponent;
class SceneProxyBase;

struct RaycastQueryParams
{
    const Actor* IgnoreActor = nullptr;
    const MeshComponent* IgnoreComponent = nullptr;
    bool bIgnoreCubeMap = true;
};

struct RaycastHitResult
{
    bool bBlockingHit = false;
    float Distance = 0.0f;
    DirectX::XMFLOAT3 Location = {0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 Normal = {0.0f, 1.0f, 0.0f};

    const Actor* HitActor = nullptr;
    const MeshComponent* HitComponent = nullptr;
    const SceneProxyBase* HitProxy = nullptr;
    BoundBox HitBounds{};
};

class WorldQueryCollisionSystem
{
public:
    void RegisterTraceableMesh(MeshComponent* meshComponent);
    void UnregisterTraceableMesh(MeshComponent* meshComponent);
    void Clear();

    bool Raycast(
        const DirectX::XMFLOAT3& start,
        const DirectX::XMFLOAT3& end,
        RaycastHitResult& outHit,
        const RaycastQueryParams& queryParams = {}) const;

private:
    std::vector<MeshComponent*> m_TraceableMeshes;
};
