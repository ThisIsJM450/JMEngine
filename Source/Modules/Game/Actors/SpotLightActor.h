#pragma once

#include "../Actor.h"
#include "../Components/SceneComponent.h"
#include "../Components/SpotLightComponent.h"

class SpotLightActor : public Actor
{
public:
    SpotLightActor();

    SpotLightComponent* GetLightComponent() const { return Light; }

private:
    SceneComponent* Root = nullptr;
    SpotLightComponent* Light = nullptr;
};