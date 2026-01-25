#pragma once

#include <memory>
#include <vector>
#include "../Scene/Scene.h"

class Actor;

class World : public Actor
{
public:
    template<typename T, typename... Args>
    T* SpawnActor(Args&&... args)
    {
        auto sp = std::make_shared<T>(std::forward<Args>(args)...);
        T* raw = sp.get();
        raw->SetWorld(this);
        m_Actors.emplace_back(std::move(sp));
        if (bBeginPlay)
        {
            raw->BeginPlay();
        }
        return raw;
    }

    void BeginPlay() override;
    void Tick(float dt) override;
    void EndPlay() override;

    virtual World* GetWorld() { return this; }

    const std::vector<std::shared_ptr<Actor>>& GetActors() const { return m_Actors; }

    Scene& GetScene() { return m_Scene; } 
    const Scene& GetScene() const { return m_Scene; }
    
private:
    std::vector<std::shared_ptr<Actor>> m_Actors;

    Scene m_Scene;

    bool bBeginPlay = false;
};
