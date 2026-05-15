#pragma once

#include "../Actor.h"
#include "../Components/StaticMeshComponent.h"

class StaticMeshActor : public Actor
{
public:
    StaticMeshActor();

    StaticMeshComponent* GetMeshComponent() const { return MeshComp; }

private:
    StaticMeshComponent* MeshComp = nullptr;
};