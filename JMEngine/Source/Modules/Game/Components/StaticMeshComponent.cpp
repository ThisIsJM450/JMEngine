#include "StaticMeshComponent.h"
#include "../World.h"
#include "../../Graphics/Material/MaterialInstance.h"
#include "../../Renderer/SceneProxy/StaticMeshSceneProxy.h"

StaticMeshComponent::StaticMeshComponent()
    : MeshComponent()
{
    TypeName = std::string("StaticMeshComponent");
}

void StaticMeshComponent::SetMesh(const std::shared_ptr<MeshAsset>& mesh)
{
    m_Mesh = mesh;
    MarkRenderStateDirty();
}

std::unique_ptr<SceneProxyBase> StaticMeshComponent::CreateSceneProxy()
{
    // Mesh가 있을 때만 프록시 생성
    if (!GetMesh())
        return nullptr;
    
    return std::unique_ptr<SceneProxyBase>(new StaticMeshSceneProxy());
}

void StaticMeshComponent::FillProxyCommonData(SceneProxyBase* proxy)
{
    // StaticMeshSceneProxy의 공통 필드 세팅
    StaticMeshSceneProxy* p = static_cast<StaticMeshSceneProxy*>(proxy);
    p->CastShadow = CastShadow;
    p->ReceiveShadow = ReceiveShadow;
    p->MeshID = meshID;
    p->materialInstances = GetMaterialInstances();
}

void StaticMeshComponent::FillProxyMeshData(SceneProxyBase* proxy)
{
    // StaticMeshSceneProxy에 Mesh/Owner 세팅
    if (!GetMesh())
    {
        // 메시가 없으면 프록시 무효
        return;
    }

    StaticMeshSceneProxy* p = static_cast<StaticMeshSceneProxy*>(proxy);
    p->Ownner = this;
    p->Mesh = GetMesh();
}
