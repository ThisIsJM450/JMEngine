#pragma once

#include <memory>
#include <vector>

#include "../Core/Object.h"

class ActorComponent;
class SceneComponent;
class World;

class Actor : public Object
{
public:
    Actor() {}
    virtual ~Actor() = default;

    SceneComponent* GetRootComponent() const { return m_Root; }
    void SetRootComponent(SceneComponent* root) { m_Root = root; }
    const std::vector<std::shared_ptr<ActorComponent>>& GetComponents() const { return m_Components; }

    template<typename T, typename... Args>
    T* CreateComponent(Args&&... args)
    {
        auto sp = std::make_shared<T>(std::forward<Args>(args)...);
        T* raw = sp.get();
        raw->SetOwner(this);
        m_Components.emplace_back(std::move(sp));
        return raw;
    }

    virtual void BeginPlay();
    virtual void Tick(float dt);
    virtual void EndPlay();
    
    virtual World* GetWorld() { return m_World; }
    void SetWorld(World* world) { m_World = world; }
    
    uint32_t GetUniqueId() { return uniqueId; }
    void SetUniqueId(uint32_t InuniqueId) { uniqueId = InuniqueId; }

protected:
    SceneComponent* m_Root = nullptr;
    std::vector<std::shared_ptr<ActorComponent>> m_Components;

private:
    World* m_World = nullptr;
    uint32_t uniqueId = 0;
};
