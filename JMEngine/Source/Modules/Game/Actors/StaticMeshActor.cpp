#include "StaticMeshActor.h"


StaticMeshActor::StaticMeshActor()
{
    m_Root = CreateComponent<SceneComponent>();

    MeshComp = CreateComponent<StaticMeshComponent>();
    MeshComp->AttachTo(m_Root);
}
