#pragma once
#include "../Actor.h"


class ActorComponent : public Object
{
public:
    ActorComponent() : TypeName(std::string("ActorComponent")) {}
    virtual ~ActorComponent() = default;
    
    Actor* GetOwner() const { return m_Owner; }
    
    std::string GetTypeName() const { return TypeName; }

    virtual void OnRegister() {}
    virtual void OnUnregister() {}
    virtual void Tick(float deltaTime) {}

    World* GetWorld(); 
protected:
    friend class Actor;
    void SetOwner(Actor* o) { m_Owner = o; }
    std::string TypeName;

private:
    Actor* m_Owner = nullptr;

};
