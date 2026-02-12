#include "Actor.h"

#include "Components/ActorComponent.h"

void Actor::Tick(float dt)
{
    for (std::shared_ptr<ActorComponent>& c : m_Components)
    {
        c->Tick(dt);
    }
}

void Actor::EndPlay()
{
    for (auto& component : GetComponents())
    {
        component->OnUnregister();
    }
}

void Actor::BeginPlay()
{
    for (auto& component : GetComponents())
    {
        component->OnRegister();
    }
}
