#include "MeshComponent.h"

#include "../MeshData.h"
#include "../World.h"
#include "../../Graphics/Material/MaterialInstance.h"

BoundBox SceneProxyBase::GetBounds() const
{
    if (const CpuMeshBase* CPUMesh = GetMesh())
    {
        return CPUMesh->GetBoundBox().ToWorldBound(GetWorldMatrix());
    }
    return BoundBox();
}

MeshComponent::MeshComponent() : SceneComponent()
{
    TypeName = std::string("MeshComponent");
}

void MeshComponent::OnRegister()
{
    World* world = GetWorld();
    if (!world) return;

    RegisterToScene(world);
    MarkRenderStateDirty();
}

void MeshComponent::OnUnregister()
{
    World* world = GetWorld();
    if (!world) return;

    UnregisterFromScene(world);

    // 필요하면 프록시 해제
    m_Proxy.reset();
}

void MeshComponent::SetMaterial(std::vector<std::shared_ptr<MaterialInstance>> mats)
{
    m_Materials = std::move(mats);
    MarkRenderStateDirty();
}

std::vector<MaterialInstance*> MeshComponent::GetMaterialInstances() const
{
    std::vector<MaterialInstance*> RetVal;
    RetVal.reserve(m_Materials.size());

    for (const std::shared_ptr<MaterialInstance>& Mat : m_Materials)
    {
        if (Mat)
            RetVal.push_back(Mat.get());
    }
    return RetVal;
}

void MeshComponent::RegisterToScene(World* world)
{
    if (!world) return;
    world->GetScene().AddMesh(this);
}

void MeshComponent::UnregisterFromScene(World* world)
{
    if (!world) return;
    world->GetScene().RemoveMesh(this);
}

void MeshComponent::FillProxyCommonData(SceneProxyBase* proxy)
{
    // 공통으로 들어갈 값들은 여기서 세팅
    // (단, proxy의 실제 타입마다 필드명이 다르면 파생에서 override 해도 됨)
    (void)proxy;
}

void MeshComponent::MarkRenderStateDirty()
{
    // 파생이 "메시가 유효한지" 판단은 FillProxyMeshData 내부에서 하게 하고,
    // 여기서는 항상 프록시 생성 시도 -> 유효하지 않으면 reset 하게 한다.

    std::unique_ptr<SceneProxyBase> newProxy = CreateSceneProxy();
    if (!newProxy)
    {
        m_Proxy.reset();
        return;
    }

    // 공용/파생 데이터 채우기
    FillProxyCommonData(newProxy.get());
    FillProxyMeshData(newProxy.get());

    // 최종 커밋
    m_Proxy = std::move(newProxy);
}
