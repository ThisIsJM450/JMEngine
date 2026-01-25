#include "SpotLightActor.h"

SpotLightActor::SpotLightActor()
{
    Root = CreateComponent<SceneComponent>();
    SetRootComponent(Root);

    Light = CreateComponent<SpotLightComponent>();
    Light->AttachTo(Root);
}
