#pragma once

#include "../Actor.h"
#include "../Components/SceneComponent.h"
#include "../Components/StaticMeshComponent.h"

class StaticMeshActor : public Actor
{
public:
    StaticMeshActor();

    StaticMeshComponent* GetMeshComponent() const { return MeshComp; }

private:
    SceneComponent* Root = nullptr;
    StaticMeshComponent* MeshComp = nullptr;
};