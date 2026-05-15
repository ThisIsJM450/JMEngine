#include "ActorComponent.h"
#include "../Actor.h"

World* ActorComponent::GetWorld()
{
    Actor* owner = GetOwner();
    if (!owner) return nullptr;
    World* world = owner->GetWorld();
    return world;
}
