#pragma once

#include "../Actor.h"
#include "../Components/SceneComponent.h"
#include "../Components/DirectionalLightComponent.h"

class DirectionalLightActor : public Actor
{
public:
    DirectionalLightActor();

    DirectionalLightComponent* GetLightComponent() const { return Light; }

private:
    SceneComponent* Root = nullptr;
    DirectionalLightComponent* Light = nullptr;
};