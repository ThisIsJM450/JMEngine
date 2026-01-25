#include "DirectionalLightComponent.h"
#include "../World.h"
#include "../Actor.h"

void DirectionalLightComponent::OnRegister()
{
    World* world = GetWorld();
    if (!world) return;
    world->GetScene().AddDirectionalLight(this);
}

void DirectionalLightComponent::OnUnregister()
{
    World* world = GetWorld();
    if (!world) return;
    world->GetScene().RemoveDirectionalLight(this);
}
