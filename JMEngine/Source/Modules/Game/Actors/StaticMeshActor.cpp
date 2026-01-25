#include "StaticMeshActor.h"


StaticMeshActor::StaticMeshActor()
{
    Root = CreateComponent<SceneComponent>();
    SetRootComponent(Root);

    MeshComp = CreateComponent<StaticMeshComponent>();
    MeshComp->AttachTo(Root);
}
