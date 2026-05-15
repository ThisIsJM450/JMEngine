#pragma once
#include "../Actor.h"
class SkeletalMeshComponent;
class CharacterMovementComponent;

class Character : public Actor
{
public:
    Character();
    SkeletalMeshComponent* GetSkeletalComponent() const { return SMComponent; }
    CharacterMovementComponent* GetMovementComponent() const { return MoveComponent; }
    
protected:
    SkeletalMeshComponent* SMComponent;
    CharacterMovementComponent* MoveComponent = nullptr;
};