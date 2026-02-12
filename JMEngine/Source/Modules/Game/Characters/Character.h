#pragma once
#include "../Actor.h"
class SkeletalMeshComponent;

class Character : public Actor
{
public:
    Character();
    SkeletalMeshComponent* GetSkeletalComponent() const { return SMComponent; }
    
protected:
    SkeletalMeshComponent* SMComponent;
};