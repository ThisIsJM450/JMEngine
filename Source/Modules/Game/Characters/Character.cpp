#include "Character.h"

#include "../Components/CharacterMovementComponenet.h"
#include "../Skeletal/SkeletalMeshComponent.h"

Character::Character()
{
    m_Root = CreateComponent<SceneComponent>();
    SMComponent = CreateComponent<SkeletalMeshComponent>();
    SMComponent->AttachTo(GetRootComponent());
    
    MoveComponent = CreateComponent<CharacterMovementComponent>();
}