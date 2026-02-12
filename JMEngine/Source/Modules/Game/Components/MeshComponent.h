#pragma once
#include <memory>
#include <vector>
#include <string>
#include <cstdint>

#include "SceneComponent.h"
#include "../../Core/CoreStruct.h"

class CpuMeshBase;
class World;
class MaterialInstance;

class  SceneProxyBase
{
public:
    SceneProxyBase() = default;
    virtual ~SceneProxyBase() = default;
    
    // Mesh key for MeshManager cache
    virtual int64_t GetMeshID() const = 0;

    // Mesh asset used by MeshManager::GetOrCreate(..., *mesh)
    virtual const CpuMeshBase* GetMesh() const = 0;

    // Materials (if empty, renderer will use fallback)
    virtual const std::vector<MaterialInstance*>& GetMaterialInstances() const = 0;

    // Object-to-world
    virtual DirectX::XMMATRIX GetWorldMatrix() const = 0;

    // Shadow flags
    virtual bool GetCastShadow() const = 0;
    virtual bool GetReceiveShadow() const = 0;
    
    BoundBox GetBounds() const;
    virtual bool IsCubeMap() const {return false;}

};

class MeshComponent : public SceneComponent
{
public:
    MeshComponent();
    ~MeshComponent() override = default;

    void OnRegister() override;
    void OnUnregister() override;
    SceneProxyBase* GetProxy() const { return m_Proxy.get(); }
    void MarkRenderStateDirty();


    // 공용 Material API
    void SetMaterial(std::vector<std::shared_ptr<MaterialInstance>> mats);
    std::vector<MaterialInstance*> GetMaterialInstances() const;

    // 공용 렌더 플래그
    bool CastShadow = true;
    bool ReceiveShadow = true;

    // 공용 렌더 핸들
    uint64_t meshID = 0;

protected:
    // 파생이 구현: Scene에 등록/해제
    virtual void RegisterToScene(World* world);
    virtual void UnregisterFromScene(World* world);

    // 파생이 구현: Proxy 생성 및 Mesh 데이터 채우기
    virtual std::unique_ptr<SceneProxyBase> CreateSceneProxy() = 0;
    virtual void FillProxyMeshData(SceneProxyBase* proxy) = 0;

    // 공용: Proxy 공통 필드 세팅 + materials 바인딩
    virtual void FillProxyCommonData(SceneProxyBase* proxy);


protected:
    std::vector<std::shared_ptr<MaterialInstance>> m_Materials;
    std::unique_ptr<SceneProxyBase> m_Proxy;
};
