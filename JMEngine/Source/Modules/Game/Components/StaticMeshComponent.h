#pragma once

#include "SceneComponent.h"
#include <memory>

#include "../../Renderer/SceneProxy/StaticMeshSceneProxy.h"


struct MeshAsset;
class MaterialInstance;


class StaticMeshComponent : public SceneComponent
{
public:
    void OnRegister() override;      
    void OnUnregister() override;
    
    void SetMesh(const std::shared_ptr<MeshAsset>& mesh) { m_Mesh = mesh; MarkRenderStateDirty(); }
    //void SetMaterial(std::shared_ptr<MaterialInstance> mat) { m_MaterialInstance = std::move(mat); MarkRenderStateDirty(); }
    void SetMaterial(std::vector<std::shared_ptr<MaterialInstance>> mats);
    // void SetColor(const DirectX::XMFLOAT4& InColor) { color = InColor; }

    const MeshAsset* GetMesh() const { return m_Mesh.get(); }
    //Material* GetMaterial() const { return m_Material.get(); }
    //MaterialInstance* GetMaterialInstance() const { return m_MaterialInstance.get(); }
    std::vector<MaterialInstance*> GetMaterialInstances() const;

    bool CastShadow = true;
    bool ReceiveShadow = true;

    // 렌더러가 읽는 렌더 전용 데이터
    StaticMeshSceneProxy* GetProxy() const { return m_Proxy.get(); }
    void MarkRenderStateDirty();

    
    uint64_t meshID = 0;
private:
    std::shared_ptr<MeshAsset> m_Mesh = nullptr;
    //std::shared_ptr<Material> m_Material;
    ///std::shared_ptr<MaterialInstance> m_MaterialInstance;
    std::vector<std::shared_ptr<MaterialInstance>> m_Materials;
    std::unique_ptr<StaticMeshSceneProxy> m_Proxy;

    // DirectX::XMFLOAT4 color = {1, 1, 1, 1};
};
