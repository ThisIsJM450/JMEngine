#include "StaticMeshComponent.h"
#include "../World.h"
#include "../../Graphics/Material/MaterialInstance.h"
#include "../../Renderer/SceneProxy/StaticMeshSceneProxy.h"

void StaticMeshComponent::OnRegister()
{
    World* world = GetWorld();
    if (!world) return;

    world->GetScene().AddStaticMesh(this);
    MarkRenderStateDirty();
}

void StaticMeshComponent::OnUnregister()
{
    World* world = GetWorld();
    if (!world) return;

    world->GetScene().RemoveStaticMesh(this);
}

void StaticMeshComponent::SetMaterial(std::vector<std::shared_ptr<MaterialInstance>> mats)
{
    m_Materials = std::move(mats); 
    MarkRenderStateDirty();
}

std::vector<MaterialInstance*> StaticMeshComponent::GetMaterialInstances() const
{
    std::vector<MaterialInstance*> RetVal;
    for (std::shared_ptr<MaterialInstance> Mat : m_Materials)
    {
        if (Mat)
        {
            RetVal.push_back(Mat.get()); 
        }
    }
    
    return RetVal;
}

void StaticMeshComponent::MarkRenderStateDirty()
{
    // 현재는 지금은 단일 스레드 최소 구현
    // tODO: UE처럼 RenderThread에 커맨드로 넘기도록 수정
    if (GetMesh())
    {
        m_Proxy = std::make_unique<StaticMeshSceneProxy>();
        m_Proxy->Mesh = GetMesh();
        m_Proxy->CastShadow = CastShadow;
        m_Proxy->ReceiveShadow = ReceiveShadow;
        m_Proxy->WorldMatrix = GetWorldMatrix();
        m_Proxy->Bounds = m_Mesh->GetBoundBox().ToWorldBound(m_Proxy->WorldMatrix);
        m_Proxy->MeshID = meshID;
        // m_Proxy->color = color;
        //m_Proxy->materialInstance = GetMaterialInstance();
        m_Proxy->materialInstances = GetMaterialInstances();
    }
}
