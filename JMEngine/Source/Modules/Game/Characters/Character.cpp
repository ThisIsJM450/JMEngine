#include "Character.h"

#include "../Skeletal/SkeletalMeshComponent.h"

Character::Character()
{
    m_Root = CreateComponent<SceneComponent>();
    SMComponent = CreateComponent<SkeletalMeshComponent>();
    SMComponent->AttachTo(GetRootComponent());
}
