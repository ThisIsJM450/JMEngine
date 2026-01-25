#pragma once
#include "../Actor.h"


class ActorComponent
{
public:
    virtual ~ActorComponent() = default;

    Actor* GetOwner() const { return m_Owner; }

    virtual void OnRegister() {}
    virtual void OnUnregister() {}
    virtual void Tick(float) {}

    World* GetWorld(); 
protected:
    friend class Actor;
    void SetOwner(Actor* o) { m_Owner = o; }

private:
    Actor* m_Owner = nullptr;
};
