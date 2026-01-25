#include "DirectionalLightActor.h"

DirectionalLightActor::DirectionalLightActor()
{
    Root = CreateComponent<SceneComponent>();
    SetRootComponent(Root);

    Light = CreateComponent<DirectionalLightComponent>();
    Light->AttachTo(Root);
}
