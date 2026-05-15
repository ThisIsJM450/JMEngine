#pragma once

#include <memory>
#include <vector>
#include "../Scene/Scene.h"
#include "CameraManager.h"
#include "Collision/WorldQuery.h"

class Actor;
class CameraActor;

class World : public Actor
{
public:
    template<typename T, typename... Args>
    T* SpawnActor(const std::string& name, Args&&... args)
    {
        auto sp = std::make_shared<T>(std::forward<Args>(args)...);
        T* raw = sp.get();
        raw->SetWorld(this);
        raw->SetName(name);
        m_Actors.emplace_back(std::move(sp));
        raw->SetUniqueId(static_cast<uint32_t>(m_Actors.size()));
        if (bBeginPlay)
        {
            raw->BeginPlay();
        }
        return raw;
    }

    void BeginPlay() override;
    void Tick(float dt) override;
    void EndPlay() override;
    void ClearActors();

    virtual World* GetWorld() { return this; }

    const std::vector<std::shared_ptr<Actor>>& GetActors() const { return m_Actors; }

    CameraManager& GetCameraManager() { return m_CameraManager; }
    const CameraManager& GetCameraManager() const { return m_CameraManager; }
    void SetActiveCamera(CameraActor* camera) { m_CameraManager.SetActiveCamera(camera); }

    Scene& GetScene() { return m_Scene; } 
    const Scene& GetScene() const { return m_Scene; }

    WorldQueryCollisionSystem& GetWorldQuery() { return m_WorldQuery; }
    const WorldQueryCollisionSystem& GetWorldQuery() const { return m_WorldQuery; }

    bool Raycast(
        const DirectX::XMFLOAT3& start,
        const DirectX::XMFLOAT3& end,
        RaycastHitResult& outHit,
        const RaycastQueryParams& queryParams = {}) const
    {
        return m_WorldQuery.Raycast(start, end, outHit, queryParams);
    }
    
private:
    std::vector<std::shared_ptr<Actor>> m_Actors;

    CameraManager m_CameraManager;
    Scene m_Scene;
    WorldQueryCollisionSystem m_WorldQuery;

    bool bBeginPlay = false;
};
