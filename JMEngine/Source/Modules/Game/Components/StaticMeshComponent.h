// #pragma once
//
// #include "SceneComponent.h"
// #include <memory>
//
// #include "../../Renderer/SceneProxy/StaticMeshSceneProxy.h"
//
//
// struct MeshAsset;
// class MaterialInstance;
//
//
// class StaticMeshComponent : public SceneComponent
// {
// public:
//     StaticMeshComponent();
//
//     void OnRegister() override;      
//     void OnUnregister() override;
//     
//     void SetMesh(const std::shared_ptr<MeshAsset>& mesh) { m_Mesh = mesh; MarkRenderStateDirty(); }
//     void SetMaterial(std::vector<std::shared_ptr<MaterialInstance>> mats);
//
//
//     const MeshAsset* GetMesh() const { return m_Mesh.get(); }
//     std::vector<MaterialInstance*> GetMaterialInstances() const;
//
//     bool CastShadow = true;
//     bool ReceiveShadow = true;
//
//     // 렌더러가 읽는 렌더 전용 데이터
//     StaticMeshSceneProxy* GetProxy() const { return m_Proxy.get(); }
//     void MarkRenderStateDirty();
//
//     
//     uint64_t meshID = 0;
// private:
//     std::shared_ptr<MeshAsset> m_Mesh = nullptr;
//     std::vector<std::shared_ptr<MaterialInstance>> m_Materials;
//     std::unique_ptr<StaticMeshSceneProxy> m_Proxy;
//
// };
#pragma once
#include "MeshComponent.h"

class MeshAsset;
class StaticMeshSceneProxy;

class StaticMeshComponent : public MeshComponent
{
public:
    StaticMeshComponent();

    void SetMesh(const std::shared_ptr<MeshAsset>& mesh);
    const MeshAsset* GetMesh() const { return m_Mesh.get(); }

protected:
    std::unique_ptr<SceneProxyBase> CreateSceneProxy() override;
    void FillProxyCommonData(SceneProxyBase* proxy) override;
    void FillProxyMeshData(SceneProxyBase* proxy) override;

private:
    std::shared_ptr<MeshAsset> m_Mesh = nullptr;
};
